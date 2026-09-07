#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

class TimeRulerComponent : public juce::Component {
public:
    TimeRulerComponent();
    ~TimeRulerComponent() override = default;
    
    void setPixelsPerBeat(int pixels);
    int getPixelsPerBeat() const { return pixelsPerBeat_; }
    
    void setTimeOffset(double beats);
    double getTimeOffset() const { return timeOffset_; }
    
    void setBeatsPerMeasure(int beats);
    void setTempo(double bpm);
    
    void paint(juce::Graphics& g) override;
    
    static constexpr int defaultHeight = 24;
    
private:
    int pixelsPerBeat_ = 80;
    double timeOffset_ = 0.0;
    int beatsPerMeasure_ = 4;
    double tempo_ = 120.0;
    
    juce::String formatTime(double beats) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimeRulerComponent)
};

} // namespace vibedaw