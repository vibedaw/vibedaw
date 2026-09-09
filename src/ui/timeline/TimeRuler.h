#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "core/TransportState.h"

namespace vibedaw {

// Time ruler with seek clicks and DAW-conventional loop-region dragging.
// Plain drag creates a replacement loop region (anywhere, even inside the
// existing one); Shift+drag inside the region moves it with preserved length;
// plain drag near an edge resizes it. The gesture previews locally and
// publishes exactly one validated, snapped region via onLoopCommitted on
// mouseUp (loop edits are seek-like discontinuities, so nothing is published
// per drag frame). The mouse cursor previews the pending action: crosshair =
// create, dragging hand = shift-move, left/right arrows = edge resize.
class TimeRuler : public juce::Component {
public:
    TimeRuler();
    ~TimeRuler() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;

    std::function<void(double)> onSeek;
    // Validated, snapped [start, end) in beats; the owner applies it to
    // TransportState (and enables looping). Never called with an invalid region.
    std::function<void(double, double)> onLoopCommitted;
    void setLoopRegion(LoopRegion value) { loop = value; repaint(); }
    const LoopRegion& getLoopRegion() const { return loop; }

    // Offline test observability: the in-flight preview gesture.
    bool isDraggingLoop() const { return previewActive; }
    const LoopRegion& getLoopPreview() const { return preview; }

    void setTotalDuration(double duration);
    double getTotalDuration() const { return totalDuration; }

    void setPixelsPerBeat(double value);
    double getPixelsPerBeat() const { return pixelsPerBeat; }

    void setScrollOffset(double offset);
    double getScrollOffset() const { return scrollOffset; }

    static constexpr int rulerHeight = 24;
    static constexpr float dragThresholdPx = 5.0f;

private:
    enum class Gesture { none, pendingSeek, create, move, resizeStart, resizeEnd };
    double beatAt(juce::Point<float> position) const;
    double snap(double beat, bool bypass) const;
    void promote(const juce::MouseEvent& event);
    void updatePreview(double beat, bool bypass);
    void updateCursor(double beat, bool shiftHeld);
    juce::MouseCursor cursorForGesture() const;

    LoopRegion loop;
    LoopRegion preview;
    bool previewActive = false;
    Gesture gesture = Gesture::none;
    double anchorBeat = 0.0;
    double originalStart = 0.0, originalEnd = 0.0;
    juce::Point<float> downPosition;
    double totalDuration = 64.0; // Quarter-note beats, not seconds.
    double pixelsPerBeat = 50.0;
    double scrollOffset = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimeRuler)
};

} // namespace vibedaw