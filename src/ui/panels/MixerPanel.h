#pragma once

#include "Panel.h"
#include "ui/mixer/MixerStrip.h"
#include "ui/mixer/MasterStrip.h"
#include <vector>
#include <functional>

namespace vibedaw {

class Project;

class MixerPanel : public Panel {
public:
    MixerPanel();
    ~MixerPanel() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setProject(Project* project);
    
    void setNumChannels(int numChannels);
    int getNumChannels() const { return numChannels; }
    
    void setNumSends(int numSends);
    int getNumSends() const { return numSends; }
    
    MixerStrip* getChannelStrip(int index);
    MixerStrip* getSendStrip(int index);
    MasterStrip* getMasterStrip();
    
    void selectChannel(int index);
    int getSelectedChannel() const { return selectedChannel; }
    
    void updateChannelLevels(int index, float left, float right);
    void updateMasterLevels(float left, float right);
    
    std::function<void(int, float)> onChannelVolumeChanged;
    std::function<void(int, float)> onChannelPanChanged;
    std::function<void(int, bool)> onChannelMuteChanged;
    std::function<void(int, bool)> onChannelSoloChanged;
    std::function<void(int, int, float)> onSendLevelChanged;
    std::function<void(float)> onMasterVolumeChanged;
    
private:
    void setupStrips();
    void layoutStrips();
    void updateMetersFromProject();
    
    class MeterTimer;
    
    Project* project = nullptr;
    
    int numChannels = 16;
    int numSends = 4;
    int selectedChannel = -1;
    
    std::vector<std::unique_ptr<MixerStrip>> channelStrips;
    std::vector<std::unique_ptr<MixerStrip>> sendStrips;
    std::unique_ptr<MasterStrip> masterStrip;
    std::unique_ptr<MeterTimer> meterTimer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPanel)
};

} // namespace vibedaw
