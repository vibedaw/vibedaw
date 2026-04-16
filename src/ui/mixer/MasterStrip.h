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
    
    void setLevels(float left, float right);
    
    std::function<void(float)> onVolumeChanged;
    
private:
    class MasterFader;
    void updateComponentPositions();
    
    float volume = 0.85f;
    
    std::unique_ptr<LevelMeter> leftMeter;
    std::unique_ptr<LevelMeter> rightMeter;
    std::unique_ptr<MasterFader> fader;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterStrip)
};

} // namespace vibedaw
