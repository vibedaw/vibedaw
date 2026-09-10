#include "ChannelRackSidebar.h"
#include "ui/Theme.h"
#include "project/Project.h"
#include "project/ChannelList.h"
#include "project/Channel.h"
#include "core/MixerState.h"
#include "plugins/PluginHost.h"
#include "ui/components/PluginButton.h"
#include "ui/components/TextPrompt.h"
#include "utils/Logger.h"

namespace vibedaw {

ChannelRow::ChannelRow(Channel* channel, int index)
    : channel_(channel), index_(index)
{
    setInterceptsMouseClicks(true, false);
}

void ChannelRow::setSelected(bool selected) {
    if (isSelected_ != selected) {
        isSelected_ = selected;
        repaint();
    }
}

void ChannelRow::pollMeter(const StereoMeter& source) {
    meterLeft_ = source.getLeft();
    meterRight_ = source.getRight();
    repaint();
}

void ChannelRow::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    g.fillAll(theme::raised);
    const auto card = bounds.toFloat().reduced(3.0f, 2.0f);
    theme::drawSurface(g, card,
        isDragOver_ ? theme::actionGreen : (isSelected_ ? theme::selectedSurface
            : (isMouseOver() ? theme::controlHover : theme::control)),
        theme::panelRadius, isDragOver_ ? theme::dropIndicator
            : (isSelected_ ? theme::accent.withAlpha(0.4f) : theme::border));
    if (isSelected_ || isDragOver_) {
        g.setColour(theme::accent);
        g.fillRoundedRectangle(4.0f, 8.0f, 2.0f, juce::jmax(0.0f, getHeight() - 16.0f), 1.0f);
    }

    // Icon tile tinted with the channel colour, carrying the initial.
    auto icon = juce::Rectangle<int>(10, 7, 20, 20);
    const auto tint = channel_ ? channel_->getColour() : theme::textSecondary;
    theme::drawSurface(g, icon.toFloat(), theme::deepWell.interpolatedWith(tint, 0.2f),
                       theme::controlRadius, tint.withAlpha(0.35f));
    g.setColour(tint.interpolatedWith(theme::textBright, 0.6f));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    const auto initial = (channel_ && !channel_->getName().isEmpty())
        ? juce::String::charToString(channel_->getName().toUpperCase()[0]) : juce::String("#");
    g.drawText(initial, icon, juce::Justification::centred);

    juce::String name = channel_ ? channel_->getName() : "Channel " + juce::String(index_ + 1);
    if (isDragOver_)
        name = pendingDragInfo_.type == DragSourceType::Plugin
            ? "Replace plugin: " + channel_->getName()
            : "Assign sample file (no playback)";
    g.setColour(theme::textBright);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(name, 36, 4, juce::jmax(0, kebabRect().getX() - 40), 15,
               juce::Justification::centredLeft, true);

    juce::String subtitle = "No instrument";
    if (channel_ && channel_->hasPlugin())
        subtitle = channel_->getPlugin()->getPluginName();
    else if (channel_ && channel_->getMissingPlugin())
        subtitle = "Missing: " + channel_->getMissingPlugin()->description.name;
    else if (channel_ && channel_->getSampleFile() != juce::File())
        subtitle = "Sample assigned (no playback)";
    g.setColour(channel_ && channel_->getMissingPlugin() ? theme::dangerText : theme::textSecondary);
    g.setFont(10.0f);
    g.drawText(subtitle, 36, 18, juce::jmax(0, muteRect().getX() - 40), 11,
               juce::Justification::centredLeft, true);

    g.setColour(theme::textSecondary);
    const auto menuCentre = kebabRect().getCentre().toFloat();
    for (float offset : {-4.0f, 0.0f, 4.0f})
        g.fillEllipse(menuCentre.x - 1.0f, menuCentre.y + offset - 1.0f, 2.0f, 2.0f);

    auto drawToggle = [&](const juce::Rectangle<int>& rect, const juce::String& label, bool active, juce::Colour activeColour) {
        auto r = rect.toFloat();
        theme::drawWell(g, r, 3.0f);
        if (active)
            theme::drawSurface(g, r, theme::deepWell.interpolatedWith(activeColour, 0.25f),
                               3.0f, activeColour.withAlpha(0.55f));
        g.setColour(active ? activeColour : theme::textSecondary);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(label, rect, juce::Justification::centred);
    };
    const bool muted = channel_ && channel_->isMuted();
    const bool soloed = channel_ && channel_->isSolo();
    drawToggle(muteRect(), "M", muted, theme::muteRed);
    drawToggle(soloRect(), "S", soloed, theme::accent);

    // Live meter: sample-peak envelope polled by the rack's timer.
    auto meter = meterRect();
    theme::drawWell(g, meter.toFloat(), 2.0f);
    meter.reduce(1, 1);
    const float left = juce::jlimit(0.0f, 1.0f, meterLeft_);
    const float right = juce::jlimit(0.0f, 1.0f, meterRight_);
    const int barHeight = (meter.getHeight() - 1) / 2;
    g.setColour(theme::meterLow);
    g.fillRect(meter.getX(), meter.getY(), static_cast<int>(meter.getWidth() * left), barHeight);
    g.fillRect(meter.getX(), meter.getY() + barHeight + 1, static_cast<int>(meter.getWidth() * right), barHeight);
}

juce::PopupMenu ChannelRow::createContextMenu() const {
    juce::PopupMenu menu;
    const auto reason = PluginButton::unavailableReason(channel_);
    menu.addItem("Open Plugin Editor", reason.isEmpty(), false, openEditor);
    if (reason.isNotEmpty()) menu.addSectionHeader(reason);
    menu.addSeparator();
    menu.addItem("Select for Live Audition & New Placements", true, isSelected_, selectAsActive);
    menu.addItem(juce::String(juce::CharPointer_UTF8("Rename Channel\xe2\x80\xa6")), true, false, renameRequested);
    menu.addItem(juce::String(juce::CharPointer_UTF8("Remove Channel\xe2\x80\xa6")), true, false, removeRequested);
    return menu;
}

void ChannelRow::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        auto menu = createContextMenu();
        // The action resolves its stable ID through a lifetime-checked rack owner.
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
        return;
    }
    if (channel_) {
        if (kebabRect().contains(e.position.toInt())) {
            auto menu = createContextMenu();
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
            return;
        }
        if (muteRect().contains(e.position.toInt())) {
            channel_->setMuted(!channel_->isMuted());
            return;
        }
        if (soloRect().contains(e.position.toInt())) {
            channel_->setSolo(!channel_->isSolo());
            return;
        }
    }
    if (listener_ && channel_) {
        listener_->channelSelected(channel_);
    }
}

void ChannelRow::mouseUp(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
}

bool ChannelRow::isInterestedInDragSource(const SourceDetails& dragSourceDetails) {
    auto info = DragDropInfo::fromDragDescription(dragSourceDetails.description);
    return channel_ && (info.type == DragSourceType::Plugin || info.type == DragSourceType::Sample);
}

void ChannelRow::itemDragEnter(const SourceDetails& dragSourceDetails) {
    isDragOver_ = isInterestedInDragSource(dragSourceDetails);
    pendingDragInfo_ = DragDropInfo::fromDragDescription(dragSourceDetails.description);
    repaint();
}

void ChannelRow::itemDragExit(const SourceDetails&) {
    isDragOver_ = false;
    pendingDragInfo_ = DragDropInfo();
    repaint();
}

void ChannelRow::itemDropped(const SourceDetails& dragSourceDetails) {
    isDragOver_ = false;
    repaint();
    
    auto info = DragDropInfo::fromDragDescription(dragSourceDetails.description);
    
    if (!listener_ || !channel_) {
        return;
    }
    
    switch (info.type) {
        case DragSourceType::Plugin:
            listener_->pluginDroppedOnChannel(channel_, info.path);
            break;
        case DragSourceType::Sample:
            listener_->sampleDroppedOnChannel(channel_, juce::File(info.path));
            break;
        default:
            break;
    }
}

ChannelRackContent::ChannelRackContent(Project& project)
    : project_(project)
{
    addChannelButton_.setButtonText("+ Add Channel");
    addChannelButton_.setTooltip("Maximum 128 channels. Structural edits briefly silence audio while callbacks are quiesced.");
    addChannelButton_.setColour(juce::TextButton::buttonColourId, theme::actionGreen);
    addChannelButton_.setColour(juce::TextButton::textColourOffId, theme::accent);
    addChannelButton_.onClick = [this]() {
        if (!project_.getChannelList().addChannel())
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Channel not added",
                "The channel limit has been reached.");
        rebuildChannelRows();
    };
    addAndMakeVisible(addChannelButton_);

    project_.getChannelList().addListener(this);
    project_.addListener(this);
    juce::MultiTimer::startTimer(0, 33);
    rebuildChannelRows();
}

ChannelRackContent::~ChannelRackContent() {
    juce::MultiTimer::stopTimer(0);
    project_.removeListener(this);
    project_.getChannelList().removeListener(this);
}

void ChannelRackContent::timerCallback(int) {
    for (auto& row : channelRows_)
        if (auto* channel = row->getChannel()) row->pollMeter(channel->getMeter());
}

void ChannelRackContent::refreshChannels() {
    rebuildChannelRows();
}

int ChannelRackContent::countPlacementsToChannel(ChannelId id) const {
    int count = 0;
    for (const auto& track : project_.getTrackList().getTracks())
        for (const auto& instance : track->getClipInstances())
            if (instance->getChannelId() == id) ++count;
    return count;
}

void ChannelRackContent::selectChannelById(ChannelId id) {
    if (auto* channel = project_.getChannelList().getChannelById(id))
        project_.setActiveChannel(project_.getChannelList().indexOfChannel(channel));
}

void ChannelRackContent::renameChannelById(ChannelId id, const juce::String& name) {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty()) return;
    if (auto* channel = project_.getChannelList().getChannelById(id)) channel->setName(trimmed);
}

void ChannelRackContent::removeChannelById(ChannelId id) {
    if (auto* channel = project_.getChannelList().getChannelById(id)) {
        const int index = project_.getChannelList().indexOfChannel(channel);
        if (index >= 0) project_.getChannelList().removeChannel(index);
    }
}

void ChannelRackContent::removeChannelWithConfirmation(ChannelId id) {
    auto* channel = project_.getChannelList().getChannelById(id);
    if (!channel) return;
    const int placements = countPlacementsToChannel(id);
    const auto name = channel->getName();
    confirmAsync("Remove Channel",
        "Channel \"" + name + "\" is the destination of " + juce::String(placements) +
            " placement(s). Removing it leaves those placements unresolved and silent; the plugin is unloaded.",
        "Remove", this, [safe = juce::Component::SafePointer<ChannelRackContent>(this), id] {
            if (safe != nullptr) safe->removeChannelById(id);
        });
}

void ChannelRackContent::paint(juce::Graphics& g) {
    g.fillAll(theme::raised);
    auto footer = getLocalBounds().withTrimmedTop(juce::jmax(0, getHeight() - 32));
    g.setColour(theme::panelBackground);
    g.fillRect(footer);
    g.setColour(theme::hairline);
    g.drawHorizontalLine(footer.getY(), 0.0f, static_cast<float>(getWidth()));
    if (isDragOver_) {
        auto area = getLocalBounds().withTrimmedTop(juce::jmin(
            static_cast<int>(channelRows_.size()) * ChannelRow::rowHeight, juce::jmax(0, getHeight() - 32)));
        g.setColour(theme::dropFill);
        g.fillRect(area);
        g.setColour(theme::dropEdge);
        g.drawRect(area, 2);
        g.drawText("Create instrument channel", area.reduced(6).withHeight(24), juce::Justification::centredLeft);
    } else if (channelRows_.empty()) {
        g.setColour(theme::textMuted);
        g.setFont(12.0f);
        g.drawFittedText("Right-click a plugin in the Browser, or drag one here,\nto create an instrument channel.",
            getLocalBounds().reduced(8, 24), juce::Justification::centred, 3);
    }
}

bool ChannelRackContent::isInterestedInDragSource(const SourceDetails& details) {
    // JUCE queries interest before converting coordinates, and again for exit
    // using the next target's coordinates. Geometry belongs only in local events.
    return DragDropInfo::fromDragDescription(details.description).type == DragSourceType::Plugin &&
        project_.getChannelList().getNumChannels() < ChannelList::maxChannels;
}

bool ChannelRackContent::isCreateDropPosition(juce::Point<int> position) const {
    if (!getLocalBounds().contains(position)) return false;
    // Rows own their complete half-open rectangle. The add-button area belongs
    // to creation; JUCE walks through that non-target child to this parent.
    for (const auto& row : channelRows_)
        if (row->getBounds().contains(position)) return false;
    return true;
}

void ChannelRackContent::itemDragEnter(const SourceDetails& details) { itemDragMove(details); }
void ChannelRackContent::itemDragMove(const SourceDetails& details) {
    isDragOver_ = isInterestedInDragSource(details) && isCreateDropPosition(details.localPosition);
    addChannelButton_.setButtonText(isDragOver_ ? "Create instrument channel" : "+ Add Channel");
    repaint();
}
void ChannelRackContent::itemDragExit(const SourceDetails&) {
    isDragOver_ = false;
    addChannelButton_.setButtonText("+ Add Channel");
    repaint();
}
void ChannelRackContent::itemDropped(const SourceDetails& details) {
    const bool accepted = isInterestedInDragSource(details) && isCreateDropPosition(details.localPosition);
    itemDragExit(details);
    if (accepted && !project_.loadPlugin(DragDropInfo::fromDragDescription(details.description).path))
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Plugin not loaded",
            "Loading failed or the 128-channel limit was reached. Only mono/stereo plugins are supported. No channel was created.");
}

void ChannelRackContent::resized() {
    auto bounds = getLocalBounds();
    
    addChannelButton_.setBounds(bounds.removeFromBottom(32).reduced(4));
    
    int y = 0;
    for (auto& row : channelRows_) {
        row->setBounds(0, y, bounds.getWidth(), juce::jlimit(0, ChannelRow::rowHeight, bounds.getHeight() - y));
        y += ChannelRow::rowHeight;
    }
}

void ChannelRackContent::channelAdded(Channel* channel) {
    rebuildChannelRows();
}

void ChannelRackContent::channelRemoved(int index) {
    rebuildChannelRows();
}

void ChannelRackContent::channelChanged(Channel* channel) {
    rebuildChannelRows();
}

void ChannelRackContent::channelListChanged() {
    rebuildChannelRows();
}

void ChannelRackContent::channelSelected(Channel* channel) {
    int index = project_.getChannelList().indexOfChannel(channel);
    selectChannel(index);
    project_.setActiveChannel(index);
}

void ChannelRackContent::pluginDroppedOnChannel(Channel* channel, const juce::String& pluginPath) {
    LOG_INFO("ChannelRack: Loading plugin '" + pluginPath + "' on channel");
    
    if (!channel || !project_.loadPlugin(pluginPath, channel->getId())) {
        LOG_ERROR("ChannelRack: Failed to load plugin: " + pluginPath);
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Plugin not loaded",
            "Loading failed. Only mono/stereo plugins are supported. The existing plugin is unchanged.");
    }
}

void ChannelRackContent::sampleDroppedOnChannel(Channel* channel, const juce::File& sampleFile) {
    LOG_INFO("ChannelRack: Sample '" + sampleFile.getFileName() + "' dropped on channel");
    channel->setSampleFile(sampleFile);
}

void ChannelRackContent::rebuildChannelRows() {
    itemDragExit(SourceDetails({}, nullptr, {}));
    selectedChannelIndex_ = project_.getActiveChannel();
    for (auto& row : channelRows_) {
        removeChildComponent(row.get());
    }
    channelRows_.clear();
    
    auto& channelList = project_.getChannelList();
    int numChannels = channelList.getNumChannels();
    addChannelButton_.setEnabled(numChannels < ChannelList::maxChannels);
    
    for (int i = 0; i < numChannels; ++i) {
        auto* channel = channelList.getChannel(i);
        auto row = std::make_unique<ChannelRow>(channel, i);
        row->openEditor = [safe = juce::Component::SafePointer<ChannelRackContent>(this), id = channel->getId()] {
            if (safe == nullptr) return;
            auto* target = safe->project_.getChannelList().getChannelById(id);
            if (target && target->getPlugin()) target->getPlugin()->openEditorWindow();
        };
        row->selectAsActive = [safe = juce::Component::SafePointer<ChannelRackContent>(this), id = channel->getId()] {
            if (safe != nullptr) safe->selectChannelById(id);
        };
        row->renameRequested = [safe = juce::Component::SafePointer<ChannelRackContent>(this),
                                id = channel->getId()] {
            if (safe == nullptr) return;
            // The channel may vanish while the action is pending: no prompt, no mutation.
            auto* target = safe->project_.getChannelList().getChannelById(id);
            if (!target) return;
            showTextPrompt("Rename Channel", "New channel name:", target->getName(), safe.getComponent(),
                [safe, id](const juce::String& name) {
                    if (safe != nullptr) safe->renameChannelById(id, name);
                });
        };
        row->removeRequested = [safe = juce::Component::SafePointer<ChannelRackContent>(this), id = channel->getId()] {
            if (safe != nullptr) safe->removeChannelWithConfirmation(id);
        };
        row->setListener(this);
        row->setSelected(i == selectedChannelIndex_);
        addAndMakeVisible(*row);
        channelRows_.push_back(std::move(row));
    }
    
    resized();
}

void ChannelRackContent::selectChannel(int index) {
    if (selectedChannelIndex_ == index) {
        return;
    }
    
    selectedChannelIndex_ = index;
    
    for (int i = 0; i < static_cast<int>(channelRows_.size()); ++i) {
        channelRows_[i]->setSelected(i == selectedChannelIndex_);
    }
}

Sidebar* createChannelRackSidebar(Project& project) {
    auto* sidebar = new Sidebar("Channel Rack", Sidebar::Side::Left);
    sidebar->setIconSymbol(juce::String(juce::CharPointer_UTF8("\xe2\x96\xa6")));
    sidebar->setMinWidth(150);
    sidebar->setMaxWidth(400);
    sidebar->setSidebarWidth(250);
    
    auto* content = new ChannelRackContent(project);
    sidebar->setContent(content);
    
    return sidebar;
}

} // namespace vibedaw
