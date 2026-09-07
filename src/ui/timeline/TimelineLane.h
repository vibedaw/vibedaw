#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "project/Clip.h"

namespace vibedaw {

class Track;
class ClipPool;

class TimelineLane : public juce::Component {
public:
    TimelineLane(Track* track, int index);
    ~TimelineLane() override = default;
    
    void paint(juce::Graphics& g) override;
    
    void setPixelsPerSecond(double pps);
    double getPixelsPerSecond() const { return pixelsPerSecond; }
    
    void setScrollOffset(double offset);
    double getScrollOffset() const { return scrollOffset; }
    
    void setSelected(bool selected);
    void setClipPool(ClipPool* pool);
    
    int getTrackIndex() const { return trackIndex; }
    
    static constexpr int defaultHeight = 64;
    
private:
    void drawClips(juce::Graphics& g);
    void drawClip(juce::Graphics& g, const Clip* clip, int x, int width);
    
    Track* track = nullptr;
    ClipPool* clipPool = nullptr;
    int trackIndex = 0;
    double pixelsPerSecond = 50.0;
    double scrollOffset = 0.0;
    bool selected = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineLane)
};

} // namespace vibedaw
