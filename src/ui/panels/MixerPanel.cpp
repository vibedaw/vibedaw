#include "MixerPanel.h"

namespace vibedaw {

MixerPanel::MixerPanel(Project& owner) : Panel("Mixer | Peak, linear 0..1 FS"), project(owner) {
    setPreferredHeight(240);
    setExpandedHeight(240);
    setMinHeight(100);
    setMaxHeight(400);
    auto content = std::make_unique<juce::Viewport>();
    viewport = content.get();
    viewport->setViewedComponent(&stripContent, false);
    viewport->setScrollBarsShown(true, true);
    viewport->setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
    setContentComponent(std::move(content));
    viewport->addComponentListener(this);
    stripContent.addAndMakeVisible(masterStrip);
    masterStrip.onVolumeChanged = [this](float gain) { project.getMasterBus().setGain(gain); };
    masterStrip.onMuteToggled = [this](bool muted) { project.getMasterBus().setMuted(muted); };
    project.getChannelList().addListener(this);
    project.addListener(this);
    rebuildStrips();
    juce::MultiTimer::startTimer(0, 33);
}

MixerPanel::~MixerPanel() {
    juce::MultiTimer::stopTimer(0);
    project.removeListener(this);
    project.getChannelList().removeListener(this);
    viewport->removeComponentListener(this);
    viewport->setViewedComponent(nullptr, false);
}

void MixerPanel::rebuildStrips() {
    channelStrips.clear();
    auto& channels = project.getChannelList();
    for (const auto& channel : channels.getChannels()) {
        const auto id = channel->getId();
        auto strip = std::make_unique<MixerStrip>(id);
        // Never capture a borrowed channel or a reorderable index in a UI action.
        strip->onVolumeChanged = [this, id](float value) {
            if (auto* c = project.getChannelList().getChannelById(id)) c->setVolume(value);
        };
        strip->onPanChanged = [this, id](float value) {
            if (auto* c = project.getChannelList().getChannelById(id)) c->setPan(value);
        };
        strip->onMuteToggled = [this, id](bool value) {
            if (auto* c = project.getChannelList().getChannelById(id)) c->setMuted(value);
        };
        strip->onSoloToggled = [this, id](bool value) {
            if (auto* c = project.getChannelList().getChannelById(id)) c->setSolo(value);
        };
        strip->onStripSelected = [this, id] {
            auto& list = project.getChannelList();
            if (auto* c = list.getChannelById(id)) project.setActiveChannel(list.indexOfChannel(c));
        };
        stripContent.addAndMakeVisible(*strip);
        channelStrips.push_back(std::move(strip));
    }
    refreshControls();
    resized();
}

void MixerPanel::refreshControls() {
    for (auto& strip : channelStrips) {
        if (auto* c = project.getChannelList().getChannelById(strip->getChannelId())) {
            strip->setTrackName(c->getName());
            strip->setTrackColour(c->getColour());
            strip->setVolume(c->getVolume());
            strip->setPan(c->getPan());
            strip->setMuted(c->isMuted());
            strip->setSolo(c->isSolo());
            strip->setSelected(c->getId() == project.getActiveChannelId());
        }
    }
    masterStrip.setVolume(project.getMasterBus().getGain());
    masterStrip.setMuted(project.getMasterBus().isMuted());
}

void MixerPanel::timerCallback(int) {
    // Message-thread polling also picks up external master edits, with no feedback.
    refreshControls();
    for (auto& strip : channelStrips)
        if (auto* c = project.getChannelList().getChannelById(strip->getChannelId()))
            strip->pollMeter(c->getMeter());
    masterStrip.pollMeter(project.getMasterBus().meter);
}

void MixerPanel::resized() {
    // A pop-out owns the viewport bounds; rebuilding strips must not resize it
    // from the hidden dock. Dock return reparents the viewport before this call.
    if (!viewport || viewport->getParentComponent() == this) Panel::resized();
    layoutStrips();
}

void MixerPanel::onDisplayModeChanged(DisplayMode mode, DisplayMode oldMode) {
    if (oldMode == DisplayMode::PopOut && mode != DisplayMode::PopOut) {
        addAndMakeVisible(viewport);
        resized();
    }
}

void MixerPanel::layoutStrips() {
    if (!viewport) return;
    constexpr int width = 96;
    const int height = juce::jmax(180, viewport->getHeight() - viewport->getScrollBarThickness());
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
