#include "MixerPanel.h"
#include "ui/Theme.h"
#include "ui/components/TextPrompt.h"

namespace vibedaw {

MixerPanel::MixerPanel(Project& owner) : Panel("Mixer | Peak, linear 0..1 FS"), project(owner) {
    setPreferredHeight(240);
    setExpandedHeight(240);
    setMinHeight(100);
    setMaxHeight(400);
    auto content = std::make_unique<juce::Component>();
    mixerContent = content.get();
    content->addAndMakeVisible(viewport);
    content->addAndMakeVisible(addChannelButton);
    content->addAndMakeVisible(removeChannelButton);
    addChannelButton.setTooltip("Add an independent audio mixer channel (maximum 128). Route instruments to it using their Output controls; multiple instruments can share it.");
    removeChannelButton.setTooltip("Remove the selected mixer channel. Its instruments return to Master; live instrument selection is unchanged.");
    addChannelButton.setColour(juce::TextButton::buttonColourId, theme::actionGreen);
    addChannelButton.setColour(juce::TextButton::textColourOffId, theme::accent);
    addChannelButton.onClick = [this] {
        if (auto* channel = project.getChannelList().addMixerChannel()) {
            selectedMixerChannelId = channel->getId();
            refreshControls();
            viewport.setViewPosition(juce::jmax(0, stripContent.getWidth() - viewport.getViewWidth()),
                                     viewport.getViewPositionY());
        }
    };
    removeChannelButton.onClick = [this] {
        auto& list = project.getChannelList();
        if (list.getMixerChannelById(selectedMixerChannelId))
            list.removeMixerChannel(selectedMixerChannelId);
    };
    viewport.setViewedComponent(&stripContent, false);
    viewport.setScrollBarsShown(true, true);
    for (auto* bar : {&viewport.getVerticalScrollBar(), &viewport.getHorizontalScrollBar()}) {
        bar->setColour(juce::ScrollBar::backgroundColourId, theme::deepWell);
        bar->setColour(juce::ScrollBar::thumbColourId, theme::controlSelected);
    }
    viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    setContentComponent(std::move(content));
    mixerContent->addComponentListener(this);
    stripContent.addAndMakeVisible(masterStrip);
    masterStrip.onVolumeChanged = [this](float gain) { project.getMasterBus().setGain(gain); };
    masterStrip.onMuteToggled = [this](bool muted) { project.getMasterBus().setMuted(muted); };
    project.getChannelList().addListener(this);
    rebuildStrips();
    juce::MultiTimer::startTimer(0, 33);
}

MixerPanel::~MixerPanel() {
    juce::MultiTimer::stopTimer(0);
    project.getChannelList().removeListener(this);
    mixerContent->removeComponentListener(this);
    viewport.setViewedComponent(nullptr, false);
}

void MixerPanel::rebuildStrips() {
    channelStrips.clear();
    auto& channels = project.getChannelList();
    if (!channels.getMixerChannelById(selectedMixerChannelId))
        selectedMixerChannelId = -1;
    for (const auto& channel : channels.getMixerChannels()) {
        const auto id = channel->getId();
        auto strip = std::make_unique<MixerStrip>(id);
        // Never capture a borrowed channel or a reorderable index in a UI action.
        strip->onVolumeChanged = [this, id](float value) {
            if (auto* c = project.getChannelList().getMixerChannelById(id)) c->setVolume(value);
        };
        strip->onPanChanged = [this, id](float value) {
            if (auto* c = project.getChannelList().getMixerChannelById(id)) c->setPan(value);
        };
        strip->onMuteToggled = [this, id](bool value) {
            if (auto* c = project.getChannelList().getMixerChannelById(id)) c->setMuted(value);
        };
        strip->onSoloToggled = [this, id](bool value) {
            if (auto* c = project.getChannelList().getMixerChannelById(id)) c->setSolo(value);
        };
        strip->onStripSelected = [this, id] {
            if (project.getChannelList().getMixerChannelById(id)) {
                selectedMixerChannelId = id;
                refreshControls();
            }
        };
        strip->onRenameRequested = [safe = juce::Component::SafePointer<MixerPanel>(this), id] {
            if (safe == nullptr) return;
            auto* channel = safe->project.getChannelList().getMixerChannelById(id);
            if (!channel) return;
            showTextPrompt("Rename Mixer Channel", "New mixer channel name:", channel->getName(), safe.getComponent(),
                [safe, id](const juce::String& name) {
                    if (safe == nullptr || name.trim().isEmpty()) return;
                    if (auto* target = safe->project.getChannelList().getMixerChannelById(id))
                        target->setName(name.trim());
                });
        };
        stripContent.addAndMakeVisible(*strip);
        channelStrips.push_back(std::move(strip));
    }
    refreshControls();
    resized();
}

void MixerPanel::refreshControls() {
    for (auto& strip : channelStrips) {
        if (auto* c = project.getChannelList().getMixerChannelById(strip->getChannelId())) {
            strip->setTrackName(c->getName());
            strip->setTrackColour(c->getColour());
            strip->setVolume(c->getVolume());
            strip->setPan(c->getPan());
            strip->setMuted(c->isMuted());
            strip->setSolo(c->isSolo());
            strip->setSelected(c->getId() == selectedMixerChannelId);
        }
    }
    masterStrip.setVolume(project.getMasterBus().getGain());
    masterStrip.setMuted(project.getMasterBus().isMuted());
    addChannelButton.setEnabled(project.getChannelList().getNumMixerChannels() < ChannelList::maxMixerChannels);
    removeChannelButton.setEnabled(project.getChannelList().getMixerChannelById(selectedMixerChannelId) != nullptr);
}

void MixerPanel::timerCallback(int) {
    // Message-thread polling also picks up external master edits, with no feedback.
    refreshControls();
    for (auto& strip : channelStrips)
        if (auto* c = project.getChannelList().getMixerChannelById(strip->getChannelId()))
            strip->pollMeter(c->getMeter());
    masterStrip.pollMeter(project.getMasterBus().meter);
}

void MixerPanel::resized() {
    // A pop-out owns the content bounds; rebuilding strips must not resize it
    // from the hidden dock. Dock return reparents the content before this call.
    if (!mixerContent || mixerContent->getParentComponent() == this) Panel::resized();
    layoutStrips();
}

void MixerPanel::onDisplayModeChanged(DisplayMode mode, DisplayMode oldMode) {
    if (oldMode == DisplayMode::PopOut && mode != DisplayMode::PopOut) {
        addAndMakeVisible(mixerContent);
        resized();
    }
}

void MixerPanel::layoutStrips() {
    if (!mixerContent) return;
    auto bounds = mixerContent->getLocalBounds();
    auto toolbar = bounds.removeFromTop(28).reduced(2);
    const int buttonWidth = juce::jmin(124, juce::jmax(0, (toolbar.getWidth() - 4) / 2));
    addChannelButton.setBounds(toolbar.removeFromLeft(buttonWidth));
    toolbar.removeFromLeft(4);
    removeChannelButton.setBounds(toolbar.removeFromLeft(buttonWidth));
    viewport.setBounds(bounds);
    constexpr int width = 96;
    const int height = juce::jmax(180, viewport.getHeight() - viewport.getScrollBarThickness());
    stripContent.setSize((getNumChannels() + 1) * (width + 1), height);
    masterStrip.setBounds(0, 0, width, height);
    int x = width + 1;
    for (auto& strip : channelStrips) {
        strip->setBounds(x, 0, width, height);
        x += width + 1;
    }
}

MixerStrip* MixerPanel::getChannelStrip(int index) {
    return index >= 0 && index < getNumChannels() ? channelStrips[static_cast<size_t>(index)].get() : nullptr;
}

} // namespace vibedaw
