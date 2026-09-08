#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "core/TransportState.h"

namespace vibedaw {

class TimeRuler : public juce::Component {
public:
    TimeRuler();
    ~TimeRuler() override = default;
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    std::function<void(double)> onSeek;
    void setLoopRegion(LoopRegion value) { loop = value; repaint(); }
    
    void setTotalDuration(double duration);
    double getTotalDuration() const { return totalDuration; }
    
    void setPixelsPerBeat(double value);
    double getPixelsPerBeat() const { return pixelsPerBeat; }
    
    void setScrollOffset(double offset);
    double getScrollOffset() const { return scrollOffset; }
    
    static constexpr int rulerHeight = 24;
    
private:
    LoopRegion loop;
    double totalDuration = 64.0; // Quarter-note beats, not seconds.
    double pixelsPerBeat = 50.0;
    double scrollOffset = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimeRuler)
};

} // namespace vibedaw
