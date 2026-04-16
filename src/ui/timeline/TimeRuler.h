#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

class TimeRuler : public juce::Component {
public:
    TimeRuler();
    ~TimeRuler() override = default;
    
    void paint(juce::Graphics& g) override;
    
    void setTotalDuration(double duration);
    double getTotalDuration() const { return totalDuration; }
    
    void setPixelsPerSecond(double pps);
    double getPixelsPerSecond() const { return pixelsPerSecond; }
    
    void setScrollOffset(double offset);
    double getScrollOffset() const { return scrollOffset; }
    
    static constexpr int rulerHeight = 24;
    
private:
    void drawTimeMarkers(juce::Graphics& g, int width);
    juce::String formatTime(double seconds) const;
    
    double totalDuration = 300.0;
    double pixelsPerSecond = 50.0;
    double scrollOffset = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimeRuler)
};

} // namespace vibedaw
