#include "TimelineContent.h"

namespace vibedaw {

TimelineContent::TimelineContent(TrackList& list)
    : trackList(list)
{
    trackList.addListener(this);
    
    timeRuler = std::make_unique<TimeRuler>();
    addAndMakeVisible(*timeRuler);
    
    rebuildLanes();
    setOpaque(true);
}

TimelineContent::~TimelineContent() {
    trackList.removeListener(this);
}

void TimelineContent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void TimelineContent::resized() {
    updateLayout();
}

void TimelineContent::setScrollOffset(int verticalOffset, double horizontalOffset) {
    verticalScrollOffset = verticalOffset;
    horizontalScrollOffset = horizontalOffset;
    
    timeRuler->setScrollOffset(horizontalOffset);
    
    for (auto& lane : lanes) {
        lane->setScrollOffset(horizontalOffset);
        lane->setPixelsPerSecond(pixelsPerSecond);
    }
    
    updateLayout();
}

int TimelineContent::getTotalHeight() const {
    return TimeRuler::rulerHeight + static_cast<int>(lanes.size()) * TimelineLane::defaultHeight;
}

int TimelineContent::getScrollableHeight() const {
    return static_cast<int>(lanes.size()) * TimelineLane::defaultHeight;
}

double TimelineContent::getTotalWidth() const {
    return pixelsPerSecond * timeRuler->getTotalDuration();
}

void TimelineContent::setPixelsPerSecond(double pps) {
    pixelsPerSecond = pps;
    timeRuler->setPixelsPerSecond(pps);
    
    for (auto& lane : lanes) {
        lane->setPixelsPerSecond(pps);
    }
    
    repaint();
}

void TimelineContent::setSelectedTrack(int index) {
    if (selectedTrackIndex == index) return;
    
    if (selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(lanes.size())) {
        lanes[selectedTrackIndex]->setSelected(false);
    }
    
    selectedTrackIndex = index;
    
    if (selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(lanes.size())) {
        lanes[selectedTrackIndex]->setSelected(true);
    }
}

void TimelineContent::trackAdded(Track*) {
    rebuildLanes();
}

void TimelineContent::trackRemoved(int) {
    rebuildLanes();
}

void TimelineContent::trackChanged(Track*) {
    repaint();
}

void TimelineContent::trackListChanged() {
    rebuildLanes();
}

void TimelineContent::rebuildLanes() {
    lanes.clear();
    
    const auto& tracks = trackList.getTracks();
    for (size_t i = 0; i < tracks.size(); ++i) {
        auto lane = std::make_unique<TimelineLane>(tracks[i].get(), static_cast<int>(i));
        lane->setPixelsPerSecond(pixelsPerSecond);
        lane->setScrollOffset(horizontalScrollOffset);
        
        if (static_cast<int>(i) == selectedTrackIndex) {
            lane->setSelected(true);
        }
        
        addAndMakeVisible(*lane);
        lanes.push_back(std::move(lane));
    }
    
    updateLayout();
}

void TimelineContent::updateLayout() {
    auto bounds = getLocalBounds();
    
    timeRuler->setBounds(0, 0, bounds.getWidth(), TimeRuler::rulerHeight);
    
    int laneY = TimeRuler::rulerHeight - verticalScrollOffset;
    for (auto& lane : lanes) {
        lane->setBounds(0, laneY, bounds.getWidth(), TimelineLane::defaultHeight);
        laneY += TimelineLane::defaultHeight;
    }
}

} // namespace vibedaw
