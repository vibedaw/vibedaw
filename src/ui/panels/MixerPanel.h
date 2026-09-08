#pragma once

#include "Panel.h"
#include "project/Project.h"
#include "ui/mixer/MixerStrip.h"
#include "ui/mixer/MasterStrip.h"

namespace vibedaw {

class MixerPanel : public Panel, private ChannelList::Listener,
                   private Project::Listener, private juce::MultiTimer, private juce::ComponentListener {
public:
    explicit MixerPanel(Project&);
    ~MixerPanel() override;
    void resized() override;
    void onDisplayModeChanged(DisplayMode, DisplayMode) override;
    MixerStrip* getChannelStrip(int index);
    MasterStrip* getMasterStrip() { return &masterStrip; }
    int getNumChannels() const { return static_cast<int>(channelStrips.size()); }
private:
    void rebuildStrips();
    void refreshControls();
    void layoutStrips();
    void componentMovedOrResized(juce::Component&, bool, bool) override { layoutStrips(); }
    void timerCallback(int) override;
    void channelAdded(Channel*) override { rebuildStrips(); }
    void channelRemoved(int) override { rebuildStrips(); }
    void channelListChanged() override { rebuildStrips(); }
    void channelChanged(Channel*) override { refreshControls(); }
    void activeChannelChanged(int) override { refreshControls(); }

    Project& project;
    juce::Component stripContent;
    juce::Viewport* viewport = nullptr; // Owned as Panel content, including pop-out mode.
    MasterStrip masterStrip;
    std::vector<std::unique_ptr<MixerStrip>> channelStrips;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPanel)
};

} // namespace vibedaw
