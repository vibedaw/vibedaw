#pragma once

#include "Panel.h"
#include "../timeline/TrackHeaderList.h"
#include "../timeline/TimelineContent.h"
#include "project/Project.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

class TimelinePanel : public Panel
                     , public juce::ScrollBar::Listener {
public:
    TimelinePanel(Project& project);
    ~TimelinePanel() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
private:
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;
    
    void layoutContent();
    void syncVerticalScroll();
    void syncHorizontalScroll();
    
    Project& project;
    TrackList& trackList;
    
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
