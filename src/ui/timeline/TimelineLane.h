#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "project/Clip.h"
#include <functional>

namespace vibedaw {

class Track;
class ClipPool;
class ChannelList;

class TimelineLane : public juce::Component {
public:
    TimelineLane(Track* track, int index);
    ~TimelineLane() override = default;
    
    void paint(juce::Graphics& g) override;
    
    void setPixelsPerBeat(double value);
    double getPixelsPerBeat() const { return pixelsPerBeat; }
    void mouseDown(const juce::MouseEvent& e) override;
    std::function<void(int, int)> onPlacementSelected;
    void setChannelList(ChannelList* list) { channels = list; }
    
    void setScrollOffset(double offset);
    double getScrollOffset() const { return scrollOffset; }
    
    void setSelected(bool selected);
    void setClipPool(ClipPool* pool);
    
    int getTrackIndex() const { return trackIndex; }
    
    static constexpr int defaultHeight = 64;
    
private:
    void drawClips(juce::Graphics& g);
    
    Track* track = nullptr;
    ClipPool* clipPool = nullptr;
    ChannelList* channels = nullptr;
    int trackIndex = 0;
    double pixelsPerBeat = 50.0;
    double scrollOffset = 0.0;
    bool selected = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineLane)
};

} // namespace vibedaw
