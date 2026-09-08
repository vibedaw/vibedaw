#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <array>
#include "core/MixerState.h"

namespace vibedaw {

class LevelMeter : public juce::Component, private juce::Timer {
public:
    LevelMeter();
    ~LevelMeter() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setLevels(float leftLevel, float rightLevel);
    void poll(const StereoMeter& source);
    void setLevelsDecibels(float leftDb, float rightDb);
    
    void setDecayRate(float dbPerSecond) { decayRate = dbPerSecond; }
    
    void setMeterStyle(bool useGradient) { meterStyle = useGradient ? MeterStyle::Gradient : MeterStyle::Solid; }
    void setMeterWidth(int width) { meterWidth = width; }
    
    enum class MeterStyle {
        Solid,
        Gradient,
        Segmented
    };
    
    void setStyle(MeterStyle style) { meterStyle = style; }
    
private:
    friend struct LevelMeterTestAccess;
    void timerCallback() override;
    
    std::atomic<float> leftLevel{0.0f};
    std::atomic<float> rightLevel{0.0f};
    
    float leftDisplayLevel = 0.0f;
    float rightDisplayLevel = 0.0f;
    float leftPeakDisplay = 0.0f;
    float rightPeakDisplay = 0.0f;
    
    float decayRate = 80.0f;
    int meterWidth = 6;
    int meterSpacing = 2;
    MeterStyle meterStyle = MeterStyle::Gradient;
    
    double lastTime = 0.0;
    unsigned lastRevision = 0;
    bool hasSource = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

} // namespace vibedaw
