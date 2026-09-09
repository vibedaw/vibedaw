#include "ChannelRackSidebar.h"
#include "project/Project.h"
#include "project/ChannelList.h"
#include "project/Channel.h"
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

void ChannelRow::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    if (isDragOver_) {
        g.fillAll(juce::Colour(0xff3a5a3a));
    } else if (isSelected_) {
        g.fillAll(juce::Colour(0xff3a3a4a));
    } else {
        g.fillAll(juce::Colour(0xff2a2a2a));
    }
    
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);
    
    juce::String name = channel_ ? channel_->getName() : "Channel " + juce::String(index_ + 1);
    
    if (channel_ && channel_->hasPlugin()) {
        name += " [" + channel_->getPlugin()->getPluginName() + "]";
    }
    if (isDragOver_)
        name = pendingDragInfo_.type == DragSourceType::Plugin
            ? "Replace plugin: " + channel_->getName()
            : "Assign sample file (no playback)";
    
    g.drawText(name, 8, 0, bounds.getWidth() - 16, bounds.getHeight(), 
               juce::Justification::centredLeft, true);
    
    if (isDragOver_) {
        g.setColour(juce::Colour(0xff00ff00));
        g.drawRect(bounds, 2);
    }
    
    g.setColour(juce::Colour(0xff444444));
    g.drawHorizontalLine(bounds.getHeight() - 1, 0.0f, static_cast<float>(bounds.getWidth()));
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
    addChannelButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a5a3a));
    addChannelButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addChannelButton_.onClick = [this]() {
        if (!project_.getChannelList().addChannel())
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Channel not added",
                "The channel limit has been reached.");
        rebuildChannelRows();
    };
    addAndMakeVisible(addChannelButton_);
    
    project_.getChannelList().addListener(this);
    project_.addListener(this);
    rebuildChannelRows();
}

ChannelRackContent::~ChannelRackContent() {
    project_.removeListener(this);
    project_.getChannelList().removeListener(this);
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
    g.fillAll(juce::Colour(0xff252525));
    if (isDragOver_) {
        auto area = getLocalBounds().withTrimmedTop(juce::jmin(
            static_cast<int>(channelRows_.size()) * ChannelRow::rowHeight, juce::jmax(0, getHeight() - 32)));
        g.setColour(juce::Colour(0xff395875));
        g.fillRect(area);
        g.setColour(juce::Colours::lightblue);
        g.drawRect(area, 2);
        g.drawText("Create instrument channel", area.reduced(6).withHeight(24), juce::Justification::centredLeft);
    } else if (channelRows_.empty()) {
        g.setColour(juce::Colour(0xff777777));
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
