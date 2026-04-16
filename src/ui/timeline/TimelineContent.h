#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TimeRuler.h"
#include "TimelineLane.h"
#include "project/TrackList.h"
#include <vector>
#include <functional>

namespace vibedaw {

class TimelineContent : public juce::Component
                        , public TrackList::Listener {
public:
    TimelineContent(TrackList& trackList);
    ~TimelineContent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setScrollOffset(int verticalOffset, double horizontalOffset);
    int getTotalHeight() const;
    int getScrollableHeight() const;
    double getTotalWidth() const;
    
    void setPixelsPerSecond(double pps);
    double getPixelsPerSecond() const { return pixelsPerSecond; }
    
    void setSelectedTrack(int index);
    
    std::function<void(int)> onTrackSelected;
    
private:
    void trackAdded(Track* track) override;
    void trackRemoved(int index) override;
    void trackChanged(Track* track) override;
    void trackListChanged() override;
    
    void rebuildLanes();
    void updateLayout();
    
    TrackList& trackList;
    std::unique_ptr<TimeRuler> timeRuler;
    std::vector<std::unique_ptr<TimelineLane>> lanes;
    
    int verticalScrollOffset = 0;
    double horizontalScrollOffset = 0.0;
    double pixelsPerSecond = 50.0;
    int selectedTrackIndex = -1;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineContent)
};

} // namespace vibedaw
