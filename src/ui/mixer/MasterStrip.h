#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LevelMeter.h"
#include <functional>

namespace vibedaw {

class MasterStrip : public juce::Component {
public:
    MasterStrip();
    ~MasterStrip() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setVolume(float volume);
    float getVolume() const { return volume; }
    void setMuted(bool value) { muteButton.setToggleState(value, juce::dontSendNotification); }
    bool isMuted() const { return muteButton.getToggleState(); }
    
    void pollMeter(const StereoMeter& source);
    
    std::function<void(float)> onVolumeChanged;
    std::function<void(bool)> onMuteToggled;
    
private:
    class MasterFader;
    void updateComponentPositions();
    
    float volume = 1.0f;
    juce::TextButton muteButton{"M"};
    
    std::unique_ptr<LevelMeter> leftMeter;
    std::unique_ptr<MasterFader> fader;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterStrip)
};

} // namespace vibedaw
