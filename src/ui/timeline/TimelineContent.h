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
                        , public TrackList::Listener, private TransportListener
                        , public juce::DragAndDropTarget, private juce::Timer {
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
    std::function<ChannelId()> activeDestination;
    std::function<ClipId()> selectedClipSource;
    std::function<void(ClipId)> onEditSource;
    std::function<void(int, int)> onAutoScroll;

    // Context-menu/keyboard actions on the clicked or selected placement. All
    // resolve by stable track/instance IDs and no-op when the target is gone;
    // the menu bindings own their confirmation/prompt dialogs.
    bool setPlacementStartById(const juce::String& trackId, const juce::String& instanceId, double beat);
    bool assignPlacementDestinationById(const juce::String& trackId, const juce::String& instanceId, ChannelId destination);
    bool togglePlacementMuteById(const juce::String& trackId, const juce::String& instanceId);
    bool removePlacementById(const juce::String& trackId, const juce::String& instanceId);
    bool editPlacementSourceById(const juce::String& trackId, const juce::String& instanceId);
    bool removeSelectedPlacement();
    bool editSelectedPlacementSource();
    // Empty-space menu actions. A null target creates one new lane, mirroring
    // the drop contract; a vanished named target is a harmless no-op.
    bool pooledPlacementIsValid(double beat) const;
    bool placePooledClipHere(Track* target, double beat);
    bool isInterestedInDragSource(const SourceDetails&) override;
    bool shouldDrawDragImageWhenOver() override { return false; }
    void itemDragEnter(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

    struct Preview {
        bool visible = false, valid = false, newTrack = false;
        juce::String trackId, label;
        double beat = 0, duration = 0;
        int lane = -1;
        ChannelId destination = InvalidChannelId;
    };
    const Preview& getPreview() const { return preview; }
    
private:
    friend struct TimelineContentTestAccess;
    void timerCallback() override;
    void updatePreview(juce::Point<int>, bool bypass);
    void cancelDrag();
    void commitDrag(juce::Point<int>, bool bypass);
    ClipInstance* movingInstance() const;
    ClipInstance* hitInstance(juce::Point<int>, Track*&) const;
    ClipInstance* resolvePlacement(const juce::String& trackId, const juce::String& instanceId) const;
    ClipInstance* selectedPlacementInstance() const;
    void showContextMenuAt(juce::Point<int> point);
    juce::PopupMenu createPlacementMenu(Track& track, ClipInstance& instance) const;
    juce::PopupMenu createEmptySpaceMenu(const juce::String& targetId, bool newTrack, double beat) const;
    Preview preview;
    juce::var pooledGesture; // Retained across ordinary exit; explicit cancellation survives reentry.
    ClipId draggedClip = InvalidClipId;
    juce::String sourceTrackId, movingId;
    double grabOffset = 0;
    bool dragging = false;
    bool sourceWasResolved = false, destinationWasResolved = false;
    bool hadExternalSource = false;
    juce::Component::SafePointer<juce::Component> externalSource;
    juce::Point<int> dragPosition;
    void transportPositionChanged(double) override { repaint(); }
    void transportLoopChanged(bool enabled, double start, double end) override {
        juce::ignoreUnused(enabled, start, end);
        timeRuler->setLoopRegion(transport.getLoopRegion()); // Full region: carries the exists flag.
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
