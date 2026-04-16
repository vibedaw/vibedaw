#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TrackHeader.h"
#include "project/TrackList.h"
#include <vector>
#include <functional>

namespace vibedaw {

class TrackHeaderList : public juce::Component
                       , public TrackList::Listener {
public:
    TrackHeaderList(TrackList& trackList);
    ~TrackHeaderList() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setScrollOffset(int offset);
    int getTotalHeight() const;
    int getScrollableHeight() const;
    
    void setSelectedTrack(int index);
    int getSelectedTrack() const { return selectedTrackIndex; }
    
    std::function<void(int)> onTrackSelected;
    std::function<void(int, bool)> onTrackMuteToggled;
    std::function<void(int, bool)> onTrackSoloToggled;
    
    static constexpr int defaultWidth = 150;
    static constexpr int headerHeight = 24;
    
private:
    void trackAdded(Track* track) override;
    void trackRemoved(int index) override;
    void trackChanged(Track* track) override;
    void trackListChanged() override;
    
    void rebuildHeaders();
    void updateHeaderPositions();
    
    TrackList& trackList;
    std::vector<std::unique_ptr<TrackHeader>> headers;
    int scrollOffset = 0;
    int selectedTrackIndex = -1;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackHeaderList)
};

} // namespace vibedaw
