#include "TimelinePanel.h"
#include "project/Clip.h"

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
    
    addTrackButton = std::make_unique<AddTrackButton>();
    addTrackButton->onClick = [this] {
        int trackCount = trackList.getNumTracks();
        trackList.addTrack("Track " + juce::String(trackCount + 1));
    };
    addAndMakeVisible(*addTrackButton);
    
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
