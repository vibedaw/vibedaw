#include "TimelinePanel.h"
#include "project/Clip.h"
#include <cmath>
#include <cstdlib>

namespace vibedaw {

class TimelinePanel::AddTrackButton : public juce::Component {
public:
    AddTrackButton() {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        
        if (isDown) {
            g.fillAll(juce::Colour(0xff404040));
        } else if (isOver) {
            g.fillAll(juce::Colour(0xff353535));
        } else {
            g.fillAll(juce::Colour(0xff2a2a2a));
        }
        
        g.setColour(juce::Colour(0xff666666));
        g.drawRect(bounds, 1.0f);
        
        g.setColour(juce::Colour(0xffaaaaaa));
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText("+", bounds, juce::Justification::centred);
    }
    
    void mouseEnter(const juce::MouseEvent&) override {
        isOver = true;
        repaint();
    }
    
    void mouseExit(const juce::MouseEvent&) override {
        isOver = false;
        repaint();
    }
    
    void mouseDown(const juce::MouseEvent&) override {
        isDown = true;
        repaint();
    }
    
    void mouseUp(const juce::MouseEvent&) override {
        isDown = false;
        repaint();
        if (onClick) onClick();
    }
    
    std::function<void()> onClick;
    
private:
    bool isOver = false;
    bool isDown = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AddTrackButton)
};

TimelinePanel::TimelinePanel(Project& proj)
    : Panel("Timeline")
    , project(proj)
    , trackList(proj.getTrackList())
{
    setFlexFill(true);
    setMinHeight(200);
    
    headerList = std::make_unique<TrackHeaderList>(trackList);
    addAndMakeVisible(*headerList);
    
    content = std::make_unique<TimelineContent>(trackList, project.getClipPool(), project.getChannelList(),
                                               project.getTransportState());
    addAndMakeVisible(*content);
    
    verticalScrollBar = std::make_unique<juce::ScrollBar>(true);
    verticalScrollBar->addListener(this);
    addAndMakeVisible(*verticalScrollBar);
    
    horizontalScrollBar = std::make_unique<juce::ScrollBar>(false);
    horizontalScrollBar->addListener(this);
    addAndMakeVisible(*horizontalScrollBar);
    
    addTrackButton = std::make_unique<AddTrackButton>();
    addTrackButton->onClick = [this] {
        int trackCount = trackList.getNumTracks();
        trackList.addTrack("Track " + juce::String(trackCount + 1));
    };
    addAndMakeVisible(*addTrackButton);
    
    headerList->onTrackSelected = [this](int index) {
        content->setSelectedTrack(index);
        selectPlacement(index, -1);
    };
    content->onTrackSelected = [this](int index) {
        headerList->setSelectedTrack(index);
    };
    content->onPlacementSelected = [this](int track, int placement) { selectPlacement(track, placement); };
    content->onExtentChanged = [this] { layoutContent(); };

    startLabel.setText("Start beat", juce::dontSendNotification);
    startBeat.setText("0", false);
    startBeat.onTextChange = [this] { refreshPlacementControls(); };
    placeButton.setTooltip("Place the selected pooled MIDI source on the selected track, routed to the active instrument.");
    moveButton.setTooltip("Move the selected placement to Start beat (quarter-note beats, zero-based).");
    routeButton.setTooltip("Assign the selected placement to the active instrument. Other placements are unchanged.");
    placeButton.onClick = [this] {
        double beat;
        auto* source = project.getClipPool().getClip(selectedClip);
        auto* track = trackList.getTrack(selectedTrack);
        auto* channel = project.getChannelList().getChannelById(project.getActiveChannelId());
        if (!readStartBeat(beat) || !track || !source || source->getType() != Clip::Type::Midi ||
            !channel || channel->getType() != Channel::Type::Instrument ||
            !std::isfinite(source->getDuration()) || source->getDuration() <= 0.0 ||
            !std::isfinite(beat + source->getDuration())) return;
        track->addClipInstance(std::make_unique<ClipInstance>(selectedClip, channel->getId(), beat, source->getDuration()));
        selectPlacement(selectedTrack, track->getNumClipInstances() - 1);
    };
    moveButton.onClick = [this] {
        double beat;
        if (auto* instance = selectedPlacement())
            if (readStartBeat(beat)) instance->setStartTime(beat);
        refreshPlacementControls();
    };
    deleteButton.onClick = [this] {
        if (auto* track = trackList.getTrack(selectedTrack)) {
            for (int i = track->getNumClipInstances() - 1; i >= 0; --i)
                if (track->getClipInstance(i)->isSelected()) track->removeClipInstance(i);
        }
        refreshPlacementControls();
    };
    routeButton.onClick = [this] {
        auto* channel = project.getChannelList().getChannelById(project.getActiveChannelId());
        if (auto* instance = selectedPlacement())
            if (channel && channel->getType() == Channel::Type::Instrument) instance->setChannelId(channel->getId());
        refreshPlacementControls();
    };
    muteButton.onClick = [this] {
        if (auto* instance = selectedPlacement()) instance->setMuted(muteButton.getToggleState());
        refreshPlacementControls();
    };
    juce::Component* controls[] = { &placementStatus, &startLabel, &startBeat, &placeButton,
                                   &moveButton, &deleteButton, &routeButton, &muteButton };
    for (auto* component : controls)
        addAndMakeVisible(*component);
    project.getClipPool().addListener(this);
    project.getChannelList().addListener(this);
    project.addListener(this);
    trackList.addListener(this);
    for (const auto& track : trackList.getTracks()) track->addChangeListener(this);
    refreshPlacementControls();
}

TimelinePanel::~TimelinePanel() {
    for (const auto& track : trackList.getTracks()) track->removeChangeListener(this);
    trackList.removeListener(this);
    project.removeListener(this);
    project.getChannelList().removeListener(this);
    project.getClipPool().removeListener(this);
}

void TimelinePanel::setSelectedClip(ClipId id) {
    selectedClip = id;
    refreshPlacementControls();
}

bool TimelinePanel::readStartBeat(double& beat) const {
    auto text = startBeat.getText().trim();
    const auto* begin = text.toRawUTF8();
    char* end = nullptr;
    beat = std::strtod(begin, &end);
    return end != begin && *end == '\0' && std::isfinite(beat) && beat >= 0.0;
}

ClipInstance* TimelinePanel::selectedPlacement() const {
    if (auto* track = trackList.getTrack(selectedTrack))
        for (const auto& instance : track->getClipInstances())
            if (instance->isSelected()) return instance.get();
    return nullptr;
}

void TimelinePanel::selectPlacement(int trackIndex, int instanceIndex) {
    selectedTrack = trackIndex;
    for (int t = 0; t < trackList.getNumTracks(); ++t) {
        auto* track = trackList.getTrack(t);
        for (int i = 0; i < track->getNumClipInstances(); ++i)
            track->getClipInstance(i)->setSelected(t == trackIndex && i == instanceIndex);
    }
    if (auto* instance = selectedPlacement()) startBeat.setText(juce::String(instance->getStartTime(), 6), false);
    refreshPlacementControls();
}

void TimelinePanel::refreshPlacementControls() {
    auto* source = project.getClipPool().getClip(selectedClip);
    auto* track = trackList.getTrack(selectedTrack);
    auto* channel = project.getChannelList().getChannelById(project.getActiveChannelId());
    auto* instance = selectedPlacement();
    double beat;
    const bool validBeat = readStartBeat(beat);
    const bool validSource = source && source->getType() == Clip::Type::Midi &&
                             std::isfinite(source->getDuration()) && source->getDuration() > 0.0;
    const bool validChannel = channel && channel->getType() == Channel::Type::Instrument;
    const bool canPlace = validSource && track && validChannel && validBeat && std::isfinite(beat + source->getDuration());
    placeButton.setEnabled(canPlace);
    moveButton.setEnabled(instance && validBeat && std::isfinite(beat + instance->getDuration()));
    deleteButton.setEnabled(instance != nullptr);
    routeButton.setEnabled(instance && validChannel);
    muteButton.setEnabled(instance != nullptr);
    muteButton.setToggleState(instance && instance->isMuted(), juce::dontSendNotification);
    juce::String status = (source ? source->getName() : "Select a MIDI source in Clips") + juce::String(" | ") +
        (track ? track->getName() : "Select a track") + " | " +
        (validChannel ? "To: " + channel->getName() + " (#" + juce::String(channel->getId()) + ")" : "Select an instrument in Channel Rack");
    if (!validBeat) status += " | Start must be a finite, non-negative beat";
    else if (source && !validSource) status += " | Requires a MIDI source with positive length";
    if (validChannel && !channel->hasPlugin()) status += " | Destination has no plugin";
    placementStatus.setText(status, juce::dontSendNotification);
    placementStatus.setTooltip(status);
    content->refresh();
    layoutContent();
}

void TimelinePanel::trackAdded(Track* track) {
    track->addChangeListener(this);
    refreshPlacementControls();
}

void TimelinePanel::trackRemoved(int) { trackListChanged(); }

void TimelinePanel::trackListChanged() {
    selectedTrack = -1;
    headerList->setSelectedTrack(-1);
    content->setSelectedTrack(-1);
    for (const auto& track : trackList.getTracks()) track->addChangeListener(this);
    selectPlacement(-1, -1);
}

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
    placementStatus.setBounds(bounds.removeFromTop(24));
    auto controls = bounds.removeFromTop(30).reduced(2);
    const int unit = juce::jmax(1, controls.getWidth() / 8);
    startLabel.setBounds(controls.removeFromLeft(unit));
    startBeat.setBounds(controls.removeFromLeft(unit));
    placeButton.setBounds(controls.removeFromLeft(unit));
    moveButton.setBounds(controls.removeFromLeft(unit));
    deleteButton.setBounds(controls.removeFromLeft(unit));
    routeButton.setBounds(controls.removeFromLeft(unit));
    muteButton.setBounds(controls);
    
    int availableWidth = juce::jmax(0, bounds.getWidth() - scrollBarWidth);
    int availableHeight = juce::jmax(0, bounds.getHeight() - scrollBarWidth);
    
    int totalHeight = headerList->getScrollableHeight();
    int visibleHeight = juce::jmax(0, availableHeight - TimeRuler::rulerHeight);
    
    double totalWidth = content->getTotalWidth();
    int visibleWidth = juce::jmax(0, availableWidth - headerWidth);
    
    verticalScrollBar->setRangeLimits({0.0, static_cast<double>(totalHeight)});
    verticalScrollBar->setCurrentRange({verticalScrollBar->getCurrentRangeStart(), static_cast<double>(visibleHeight)});
    
    horizontalScrollBar->setRangeLimits({0.0, static_cast<double>(totalWidth)});
    horizontalScrollBar->setCurrentRange({horizontalScrollBar->getCurrentRangeStart(), static_cast<double>(visibleWidth)});
    
    headerList->setBounds(bounds.getX(), bounds.getY(), headerWidth, availableHeight);
    
    content->setBounds(bounds.getX() + headerWidth, bounds.getY(), 
                       visibleWidth, availableHeight);
    
    verticalScrollBar->setBounds(bounds.getRight() - scrollBarWidth, bounds.getY(), 
                                   scrollBarWidth, availableHeight);
    
    horizontalScrollBar->setBounds(bounds.getX() + headerWidth, 
                                    bounds.getBottom() - scrollBarWidth,
                                    visibleWidth, scrollBarWidth);
    
    addTrackButton->setBounds(bounds.getX(), bounds.getBottom() - scrollBarWidth,
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
