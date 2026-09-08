#include "TimelineContent.h"
#include <cmath>

namespace vibedaw {

TimelineContent::TimelineContent(TrackList& list, ClipPool& pool, ChannelList& channelList, TransportState& state)
    : transport(state), trackList(list), clipPool(pool), channels(channelList)
{
    trackList.addListener(this);
    
    timeRuler = std::make_unique<TimeRuler>();
    timeRuler->onSeek = [this](double beats) { transport.setPositionInBeats(beats); };
    transport.addListener(this);
    addAndMakeVisible(*timeRuler);
    
    rebuildLanes();
    setOpaque(true);
}

TimelineContent::~TimelineContent() {
    transport.removeListener(this);
    trackList.removeListener(this);
}

void TimelineContent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void TimelineContent::paintOverChildren(juce::Graphics& g) {
    const double x = transport.getPositionInBeats() * pixelsPerBeat - horizontalScrollOffset;
    if (x < 0 || x >= getWidth()) return;
    g.setColour(juce::Colour(0xff00ff88));
    g.fillRect(static_cast<float>(x), 0.0f, 2.0f, static_cast<float>(getHeight()));
}

void TimelineContent::resized() {
    updateLayout();
    repaint();
}

void TimelineContent::setScrollOffset(int verticalOffset, double horizontalOffset) {
    verticalScrollOffset = verticalOffset;
    horizontalScrollOffset = horizontalOffset;
    
    timeRuler->setScrollOffset(horizontalOffset);
    
    for (auto& lane : lanes) {
        lane->setScrollOffset(horizontalOffset);
        lane->setPixelsPerBeat(pixelsPerBeat);
    }
    
    updateLayout();
    repaint();
}

int TimelineContent::getTotalHeight() const {
    return TimeRuler::rulerHeight + static_cast<int>(lanes.size()) * TimelineLane::defaultHeight;
}

int TimelineContent::getScrollableHeight() const {
    return static_cast<int>(lanes.size()) * TimelineLane::defaultHeight;
}

double TimelineContent::getTotalWidth() const {
    double beats = timeRuler->getTotalDuration();
    for (const auto& track : trackList.getTracks())
        for (const auto& instance : track->getClipInstances())
            if (instance->isValid()) beats = juce::jmax(beats, instance->getEndTime() + 4.0);
    // Bound the scrollable pixel extent so finite but enormous model times cannot
    // overflow GUI coordinates or stall beat-by-beat ruler iteration.
    return pixelsPerBeat * juce::jmin(beats, 1.0e9 / pixelsPerBeat);
}

void TimelineContent::setPixelsPerBeat(double value) {
    if (!std::isfinite(value) || value < 1.0) return;
    pixelsPerBeat = value;
    timeRuler->setPixelsPerBeat(value);
    
    for (auto& lane : lanes) {
        lane->setPixelsPerBeat(value);
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
    selectedTrackIndex = -1;
    rebuildLanes();
}

void TimelineContent::trackChanged(Track*) {
    repaint();
}

void TimelineContent::trackListChanged() {
    selectedTrackIndex = -1;
    rebuildLanes();
}

void TimelineContent::rebuildLanes() {
    lanes.clear();
    
    const auto& tracks = trackList.getTracks();
    for (size_t i = 0; i < tracks.size(); ++i) {
        auto lane = std::make_unique<TimelineLane>(tracks[i].get(), static_cast<int>(i));
        lane->setPixelsPerBeat(pixelsPerBeat);
        lane->setClipPool(&clipPool);
        lane->setChannelList(&channels);
        lane->onPlacementSelected = [this](int track, int placement) {
            setSelectedTrack(track);
            if (onTrackSelected) onTrackSelected(track);
            if (onPlacementSelected) onPlacementSelected(track, placement);
        };
        lane->setScrollOffset(horizontalScrollOffset);
        
        if (static_cast<int>(i) == selectedTrackIndex) {
            lane->setSelected(true);
        }
        
        addAndMakeVisible(*lane);
        lanes.push_back(std::move(lane));
    }
    
    updateLayout();
}

void TimelineContent::refresh() {
    for (auto& lane : lanes) lane->repaint();
    repaint();
}

void TimelineContent::updateLayout() {
    auto bounds = getLocalBounds();
    
    timeRuler->setBounds(0, 0, bounds.getWidth(), TimeRuler::rulerHeight);
    
    int laneY = TimeRuler::rulerHeight - verticalScrollOffset;
    for (auto& lane : lanes) {
        lane->setBounds(0, laneY, bounds.getWidth(), TimelineLane::defaultHeight);
        laneY += TimelineLane::defaultHeight;
    }
    timeRuler->toFront(false);
}

} // namespace vibedaw
