#include "TimelinePanel.h"
#include "ui/Theme.h"
#include "project/Clip.h"
#include <cmath>

namespace vibedaw {

class TimelinePanel::AddTrackButton : public juce::Component {
public:
    AddTrackButton() {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat();
        
        if (isDown) {
            g.fillAll(theme::controlActive);
        } else if (isOver) {
            g.fillAll(theme::controlDim);
        } else {
            g.fillAll(theme::control);
        }
        
        g.setColour(theme::textFaint);
        g.drawRect(bounds, 1.0f);
        
        g.setColour(theme::textDefault);
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
    content->activeDestination = [this] { return project.getActiveChannelId(); };
    content->onEditSource = [this](ClipId id) { if (onEditSource) onEditSource(id); };
    content->selectedClipSource = [this] { return selectedClip; };
    content->onAutoScroll = [this](int dx, int dy) {
        horizontalScrollBar->setCurrentRangeStart(horizontalScrollBar->getCurrentRangeStart() + dx, juce::dontSendNotification);
        verticalScrollBar->setCurrentRangeStart(verticalScrollBar->getCurrentRangeStart() + dy, juce::dontSendNotification);
        syncVerticalScroll();
    };

    juce::Component* controls[] = { &placementStatus };
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
    refreshPlacementControls();
}

void TimelinePanel::refreshPlacementControls() {
    auto* source = project.getClipPool().getClip(selectedClip);
    auto* track = trackList.getTrack(selectedTrack);
    auto* channel = project.getChannelList().getChannelById(project.getActiveChannelId());
    const bool validChannel = channel && channel->getType() == Channel::Type::Instrument;
    juce::String status = (source ? source->getName() : "Select a MIDI source in Clips") + juce::String(" | ") +
        (track ? track->getName() : "Select a track") + " | " +
        (validChannel ? "To: " + channel->getName() + " (#" + juce::String(channel->getId()) + ")" : "Select an instrument in Channel Rack");
    if (validChannel && !channel->hasPlugin()) status += " | Destination has no plugin";
    placementStatus.setText(status, juce::dontSendNotification);
    placementStatus.setTooltip(status +
        "\nDrag Clips onto the timeline to place; drag placements to move. Snap: 1/16 note, Alt bypasses."
        "\nDouble-click a placement to edit its shared source. Right-click placements, tracks, or empty space for actions."
        "\nDelete removes the selected placement; Ctrl+E edits its shared source.");
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
    Panel::paint(g);
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
    bounds = bounds.reduced(4, 0).withTrimmedBottom(4);
    placementStatus.setBounds(bounds.removeFromTop(24));

    int availableWidth = juce::jmax(0, bounds.getWidth() - scrollBarWidth);
    int availableHeight = juce::jmax(0, bounds.getHeight() - scrollBarWidth);
    
    int totalHeight = content->getScrollableHeight();
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
