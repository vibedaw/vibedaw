#pragma once

#include "Panel.h"
#include "project/Project.h"
#include "ui/mixer/MixerStrip.h"
#include "ui/mixer/MasterStrip.h"

namespace vibedaw {

class MixerPanel : public Panel, private ChannelList::Listener,
                   private juce::MultiTimer, private juce::ComponentListener {
public:
    explicit MixerPanel(Project&);
    ~MixerPanel() override;
    void resized() override;
    void onDisplayModeChanged(DisplayMode, DisplayMode) override;
    MixerStrip* getChannelStrip(int index);
    MasterStrip* getMasterStrip() { return &masterStrip; }
    int getNumChannels() const { return static_cast<int>(channelStrips.size()); }
    int getSelectedMixerChannelId() const { return selectedMixerChannelId; }
    juce::TextButton& getAddChannelButton() { return addChannelButton; }
    juce::TextButton& getRemoveChannelButton() { return removeChannelButton; }
    juce::Viewport& getViewport() { return viewport; }
private:
    void rebuildStrips();
    void refreshControls();
    void layoutStrips();
    void componentMovedOrResized(juce::Component&, bool, bool) override { layoutStrips(); }
    void timerCallback(int) override;
    void channelAdded(Channel*) override {}
    void channelRemoved(int) override {}
    void channelListChanged() override {}
    void channelChanged(Channel*) override {}
    void mixerChannelsChanged() override { rebuildStrips(); }
    void mixerChannelChanged(MixerChannel*) override { refreshControls(); }

    Project& project;
    juce::Component* mixerContent = nullptr; // Owned as Panel content, including pop-out mode.
    juce::Viewport viewport;
    juce::Component stripContent;
    juce::TextButton addChannelButton{"+ Add Channel"};
    juce::TextButton removeChannelButton{"Remove Selected"};
    int selectedMixerChannelId = -1;
    MasterStrip masterStrip;
    std::vector<std::unique_ptr<MixerStrip>> channelStrips;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPanel)
};

} // namespace vibedaw
