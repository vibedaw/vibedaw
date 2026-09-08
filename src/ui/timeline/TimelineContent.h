#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TimeRuler.h"
#include "TimelineLane.h"
#include "project/TrackList.h"
#include "core/TransportState.h"
#include <vector>
#include <functional>

namespace vibedaw {

class TimelineContent : public juce::Component
                        , public TrackList::Listener, private TransportListener {
public:
    TimelineContent(TrackList& trackList, ClipPool& pool, ChannelList& channels, TransportState& transport);
    ~TimelineContent() override;
    
    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    
    void setScrollOffset(int verticalOffset, double horizontalOffset);
    int getTotalHeight() const;
    int getScrollableHeight() const;
    double getTotalWidth() const;
    
    void setPixelsPerBeat(double value);
    double getPixelsPerBeat() const { return pixelsPerBeat; }
    
    void setSelectedTrack(int index);
    
    std::function<void(int)> onTrackSelected;
    std::function<void(int, int)> onPlacementSelected;
    std::function<void()> onExtentChanged;
    void refresh();
    
private:
    void transportPositionChanged(double) override { repaint(); }
    void transportLoopChanged(bool enabled, double start, double end) override {
        timeRuler->setLoopRegion({enabled, start, end});
        if (onExtentChanged) onExtentChanged();
    }
    TransportState& transport;
    void trackAdded(Track* track) override;
    void trackRemoved(int index) override;
    void trackChanged(Track* track) override;
    void trackListChanged() override;
    
    void rebuildLanes();
    void updateLayout();
    
    TrackList& trackList;
    ClipPool& clipPool;
    ChannelList& channels;
    std::unique_ptr<TimeRuler> timeRuler;
    std::vector<std::unique_ptr<TimelineLane>> lanes;
    
    int verticalScrollOffset = 0;
    double horizontalScrollOffset = 0.0;
    double pixelsPerBeat = 50.0;
    int selectedTrackIndex = -1;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineContent)
};

} // namespace vibedaw
