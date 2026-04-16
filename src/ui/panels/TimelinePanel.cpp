#include "TimelinePanel.h"
#include "project/Clip.h"

namespace vibedaw {

TimelinePanel::TimelinePanel(Project& proj)
    : Panel("Timeline")
    , project(proj)
    , trackList(proj.getTrackList())
{
    setFlexFill(true);
    setMinHeight(100);
    
    headerList = std::make_unique<TrackHeaderList>(trackList);
    addAndMakeVisible(*headerList);
    
    content = std::make_unique<TimelineContent>(trackList);
    addAndMakeVisible(*content);
    
    verticalScrollBar = std::make_unique<juce::ScrollBar>(true);
    verticalScrollBar->addListener(this);
    addAndMakeVisible(*verticalScrollBar);
    
    horizontalScrollBar = std::make_unique<juce::ScrollBar>(false);
    horizontalScrollBar->addListener(this);
    addAndMakeVisible(*horizontalScrollBar);
    
    cornerComponent = std::make_unique<juce::Component>();
    cornerComponent->setOpaque(true);
    addAndMakeVisible(*cornerComponent);
    
    headerList->onTrackSelected = [this](int index) {
        content->setSelectedTrack(index);
    };
    content->onTrackSelected = [this](int index) {
        headerList->setSelectedTrack(index);
    };
    
    trackList.addTrack("Audio 1");
    trackList.addTrack("MIDI 1");
    trackList.addTrack("Synth 1");
    
    if (trackList.getNumTracks() > 0) {
        auto* track = trackList.getTrack(0);
        if (track) {
            track->addClip(std::make_unique<AudioClip>(0.0, 5.0));
            track->addClip(std::make_unique<AudioClip>(8.0, 3.0));
        }
    }
    if (trackList.getNumTracks() > 1) {
        auto* track = trackList.getTrack(1);
        if (track) {
            track->addClip(std::make_unique<MidiClip>(2.0, 4.0));
            track->addClip(std::make_unique<MidiClip>(10.0, 6.0));
        }
    }
    if (trackList.getNumTracks() > 2) {
        auto* track = trackList.getTrack(2);
        if (track) {
            track->addClip(std::make_unique<PatternClip>(0.0, 16.0));
        }
    }
}

TimelinePanel::~TimelinePanel() = default;

void TimelinePanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void TimelinePanel::resized() {
    Panel::resized();
    layoutContent();
}

void TimelinePanel::scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) {
    if (scrollBar == verticalScrollBar.get()) {
        syncVerticalScroll();
    } else if (scrollBar == horizontalScrollBar.get()) {
        syncHorizontalScroll();
    }
}

void TimelinePanel::layoutContent() {
    auto bounds = getLocalBounds();
    bounds.removeFromTop(getTitleBarHeight());
    
    int availableWidth = bounds.getWidth() - scrollBarWidth;
    int availableHeight = bounds.getHeight() - scrollBarWidth;
    
    int totalHeight = headerList->getScrollableHeight();
    int visibleHeight = availableHeight - TimeRuler::rulerHeight;
    
    int totalWidth = static_cast<int>(content->getTotalWidth());
    int visibleWidth = availableWidth - headerWidth;
    
    verticalScrollBar->setRangeLimits({0.0, static_cast<double>(totalHeight)});
    verticalScrollBar->setCurrentRange({verticalScrollBar->getCurrentRangeStart(), static_cast<double>(visibleHeight)});
    
    horizontalScrollBar->setRangeLimits({0.0, static_cast<double>(totalWidth)});
    horizontalScrollBar->setCurrentRange({horizontalScrollBar->getCurrentRangeStart(), static_cast<double>(visibleWidth)});
    
    headerList->setBounds(bounds.getX(), bounds.getY(), headerWidth, availableHeight);
    
    content->setBounds(bounds.getX() + headerWidth, bounds.getY(), 
                       availableWidth - headerWidth, availableHeight);
    
    verticalScrollBar->setBounds(bounds.getRight() - scrollBarWidth, bounds.getY(), 
                                   scrollBarWidth, availableHeight - scrollBarWidth);
    
    horizontalScrollBar->setBounds(bounds.getX() + headerWidth, 
                                    bounds.getBottom() - scrollBarWidth,
                                    availableWidth - headerWidth, scrollBarWidth);
    
    cornerComponent->setBounds(bounds.getX(), bounds.getBottom() - scrollBarWidth,
                                headerWidth, scrollBarWidth);
    
    syncVerticalScroll();
    syncHorizontalScroll();
}

void TimelinePanel::syncVerticalScroll() {
    int scrollOffset = static_cast<int>(verticalScrollBar->getCurrentRangeStart());
    headerList->setScrollOffset(scrollOffset);
    content->setScrollOffset(scrollOffset, horizontalScrollBar->getCurrentRangeStart());
}

void TimelinePanel::syncHorizontalScroll() {
    double scrollOffset = horizontalScrollBar->getCurrentRangeStart();
    content->setScrollOffset(static_cast<int>(verticalScrollBar->getCurrentRangeStart()), scrollOffset);
}

} // namespace vibedaw
