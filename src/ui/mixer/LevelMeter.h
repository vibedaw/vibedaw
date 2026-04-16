#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <array>

namespace vibedaw {

class LevelMeter : public juce::Component, private juce::Timer {
public:
    LevelMeter();
    ~LevelMeter() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setLevels(float leftLevel, float rightLevel);
    void setLevelsDecibels(float leftDb, float rightDb);
    
    void setHoldTime(int milliseconds) { holdTimeMs = milliseconds; }
    void setDecayRate(float dbPerSecond) { decayRate = dbPerSecond; }
    
    void setMeterStyle(bool useGradient) { useGradientStyle = useGradient; }
    void setMeterWidth(int width) { meterWidth = width; }
    
    enum class MeterStyle {
        Solid,
        Gradient,
        Segmented
    };
    
    void setStyle(MeterStyle style) { meterStyle = style; }
    
private:
    void timerCallback() override;
    float decayLevel(float currentLevel);
    
    std::atomic<float> leftLevel{0.0f};
    std::atomic<float> rightLevel{0.0f};
    std::atomic<float> leftPeak{0.0f};
    std::atomic<float> rightPeak{0.0f};
    
    float leftDisplayLevel = 0.0f;
    float rightDisplayLevel = 0.0f;
    float leftPeakDisplay = 0.0f;
    float rightPeakDisplay = 0.0f;
    
    int holdTimeMs = 2000;
    float decayRate = 20.0f;
    int meterWidth = 8;
    int meterSpacing = 2;
    bool useGradientStyle = true;
    MeterStyle meterStyle = MeterStyle::Gradient;
    
    double lastTime = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

} // namespace vibedaw
