#pragma once

#include "Panel.h"
#include "../timeline/TrackHeaderList.h"
#include "../timeline/TimelineContent.h"
#include "project/Project.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

class TimelinePanel : public Panel
                     , public juce::ScrollBar::Listener
                     , private ClipPool::Listener
                     , private ChannelList::Listener
                     , private TrackList::Listener
                     , private Project::Listener
                     , private juce::ChangeListener {
public:
    TimelinePanel(Project& project);
    ~TimelinePanel() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setSelectedClip(ClipId id);
    std::function<void(ClipId)> onEditSource;
    
private:
    void refreshPlacementControls();
    void selectPlacement(int trackIndex, int instanceIndex);
    ClipInstance* selectedPlacement() const;
    void clipAdded(ClipId, Clip*) override { refreshPlacementControls(); }
    void clipRemoved(ClipId) override { refreshPlacementControls(); }
    void clipChanged(ClipId, Clip*) override { refreshPlacementControls(); }
    void channelAdded(Channel*) override { refreshPlacementControls(); }
    void channelRemoved(int) override { refreshPlacementControls(); }
    void channelChanged(Channel*) override { refreshPlacementControls(); }
    void channelListChanged() override { refreshPlacementControls(); }
    void activeChannelChanged(int) override { refreshPlacementControls(); }
    void trackAdded(Track*) override;
    void trackRemoved(int) override;
    void trackChanged(Track*) override { refreshPlacementControls(); }
    void trackListChanged() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override { refreshPlacementControls(); }
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;
    
    void layoutContent();
    void syncScroll();
    
    Project& project;
    TrackList& trackList;
    ClipId selectedClip = InvalidClipId;
    int selectedTrack = -1;
    juce::Label placementStatus;
    
    std::unique_ptr<TrackHeaderList> headerList;
    std::unique_ptr<TimelineContent> content;
    
    std::unique_ptr<juce::ScrollBar> verticalScrollBar;
    std::unique_ptr<juce::ScrollBar> horizontalScrollBar;
    
    class AddTrackButton;
    std::unique_ptr<AddTrackButton> addTrackButton;
    
    int headerWidth = TrackHeaderList::defaultWidth;
    int scrollBarWidth = 16;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelinePanel)
};

} // namespace vibedaw
