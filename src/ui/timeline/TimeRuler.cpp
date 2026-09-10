#include "TimeRuler.h"
#include "ui/Theme.h"
#include <cmath>

namespace vibedaw {

TimeRuler::TimeRuler() { setOpaque(true); }

void TimeRuler::paint(juce::Graphics& g) {
    g.fillAll(theme::control);
    const auto drawRegion = [&](const LoopRegion& region, juce::Colour fill,
                                juce::Colour edge, float barHeight) {
        const float left = static_cast<float>(juce::jlimit(0.0, static_cast<double>(getWidth()),
            region.startBeats * pixelsPerBeat - scrollOffset));
        const float right = static_cast<float>(juce::jlimit(0.0, static_cast<double>(getWidth()),
            region.endBeats * pixelsPerBeat - scrollOffset));
        if (right <= left) return;
        g.setColour(fill.withAlpha(0.65f));
        g.fillRect(left, 0.0f, right - left, static_cast<float>(getHeight()));
        g.setColour(edge);
        g.fillRect(left, 0.0f, right - left, barHeight);
    };
    // A set region is accented when enabled, dim when disabled; a cleared
    // (non-existent) region paints nothing at all.
    if (loop.exists)
        drawRegion(loop, juce::Colour(loop.enabled ? theme::loopFillActive : theme::loopFillIdle),
                   juce::Colour(loop.enabled ? theme::loopEdgeActive : theme::loopEdgeIdle), 3.0f);
    if (previewActive)
        drawRegion(preview, theme::loopFillPreview, theme::loopEdgePreview, 4.0f);
    const double interval = pixelsPerBeat < 25.0 ? 4.0 : 1.0;
    const double first = std::ceil(scrollOffset / pixelsPerBeat / interval) * interval;
    const double end = (scrollOffset + getWidth()) / pixelsPerBeat;
    for (double beat = first; beat <= end; beat += interval) {
        int x = static_cast<int>(beat * pixelsPerBeat - scrollOffset);
        const bool bar = std::fmod(beat, 4.0) == 0.0;
        g.setColour(bar ? theme::gridBar : theme::hairline);
        g.drawVerticalLine(x, static_cast<float>(getHeight() - (bar ? 9 : 5)), static_cast<float>(getHeight()));
        g.setColour(bar ? theme::textBright : theme::textSecondary);
        g.setFont(juce::Font(10.0f, bar ? juce::Font::bold : juce::Font::plain));
        g.drawText("b" + juce::String(beat, 0), x + 5, 2,
                   juce::jmax(1, static_cast<int>(juce::jmin(76.0, pixelsPerBeat * interval)) - 6), getHeight() - 5,
                   juce::Justification::left, true);
    }
    g.setColour(theme::border);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
    if (previewActive) {
        // Edge handles make resize affordance explicit during the gesture.
        const float left = static_cast<float>(preview.startBeats * pixelsPerBeat - scrollOffset);
        const float right = static_cast<float>(preview.endBeats * pixelsPerBeat - scrollOffset);
        g.setColour(theme::rulerBarNumber);
        if (left >= 0.0f && left <= getWidth()) g.fillRect(left - 1.0f, 0.0f, 2.0f, static_cast<float>(getHeight()));
        if (right >= 0.0f && right <= getWidth()) g.fillRect(right - 1.0f, 0.0f, 2.0f, static_cast<float>(getHeight()));
    }
}

void TimeRuler::setTotalDuration(double beats) {
    if (std::isfinite(beats) && beats > 0.0) totalDuration = beats;
    repaint();
}

double TimeRuler::beatAt(juce::Point<float> position) const {
    return juce::jmax(0.0, (position.x + scrollOffset) / pixelsPerBeat);
}

double TimeRuler::snap(double beat, bool bypass) const {
    if (bypass) return beat;
    return std::round(beat * 16.0) / 16.0; // 1/16 qn grid, matching T12 drags.
}

void TimeRuler::mouseDown(const juce::MouseEvent& event) {
    if (!event.mods.isLeftButtonDown()) return;
    anchorBeat = beatAt(event.position);
    downPosition = event.position;
    gesture = Gesture::pendingSeek;
}

void TimeRuler::mouseMove(const juce::MouseEvent& event) {
    if (gesture == Gesture::none)
        updateCursor(beatAt(event.position), event.mods.isShiftDown());
}

void TimeRuler::mouseDrag(const juce::MouseEvent& event) {
    if (gesture == Gesture::none) return;
    if (gesture == Gesture::pendingSeek) {
        if (std::abs(event.position.getX() - downPosition.getX()) < dragThresholdPx) return;
        promote(event);
    }
    updatePreview(beatAt(event.position), event.mods.isAltDown());
    repaint();
}

juce::MouseCursor TimeRuler::cursorForGesture() const {
    switch (gesture) {
        case Gesture::move: return juce::MouseCursor(juce::MouseCursor::DraggingHandCursor);
        case Gesture::resizeStart:
        case Gesture::resizeEnd: return juce::MouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        case Gesture::create: return juce::MouseCursor(juce::MouseCursor::CrosshairCursor);
        default: return juce::MouseCursor(juce::MouseCursor::NormalCursor);
    }
}

void TimeRuler::updateCursor(double beat, bool shiftHeld) {
    if (previewActive) {
        setMouseCursor(cursorForGesture());
        return;
    }
    const double tolerance = dragThresholdPx / pixelsPerBeat;
    juce::MouseCursor::StandardCursorType type = juce::MouseCursor::CrosshairCursor;
    if (shiftHeld && loop.exists && beat > loop.startBeats && beat < loop.endBeats)
        type = juce::MouseCursor::DraggingHandCursor;
    else if (!shiftHeld && loop.exists &&
             (std::abs(beat - loop.startBeats) <= tolerance ||
              std::abs(beat - loop.endBeats) <= tolerance))
        type = juce::MouseCursor::LeftRightResizeCursor;
    setMouseCursor(juce::MouseCursor(type));
}

// Plain drag creates a replacement region; Shift+drag inside the body moves;
// plain drag on an edge resizes. Shift only changes behaviour where a move
// is possible (inside an existing, visible region — dim included). Any drag
// with no region (cleared) creates.
void TimeRuler::promote(const juce::MouseEvent& event) {
    const double tolerance = dragThresholdPx / pixelsPerBeat;
    const bool shift = event.mods.isShiftDown();
    const bool inside = loop.exists && anchorBeat > loop.startBeats && anchorBeat < loop.endBeats;
    const bool nearStart = loop.exists && std::abs(anchorBeat - loop.startBeats) <= tolerance;
    const bool nearEnd = loop.exists && std::abs(anchorBeat - loop.endBeats) <= tolerance;
    if (shift && inside) {
        gesture = Gesture::move;
        originalStart = loop.startBeats;
        originalEnd = loop.endBeats;
    } else if (!shift && nearStart) gesture = Gesture::resizeStart;
    else if (!shift && nearEnd) gesture = Gesture::resizeEnd;
    else gesture = Gesture::create;
    preview = loop;
    preview.enabled = true;
    previewActive = true;
    setMouseCursor(cursorForGesture());
}

void TimeRuler::updatePreview(double beat, bool bypass) {
    const double minLength = bypass ? TransportState::minLoopBeats : 1.0 / 16.0;
    switch (gesture) {
        case Gesture::create: {
            double start = snap(juce::jmin(anchorBeat, beat), bypass);
            double end = snap(juce::jmax(anchorBeat, beat), bypass);
            if (end - start < minLength) {
                if (beat >= anchorBeat) end = start + minLength;
                else start = juce::jmax(0.0, end - minLength);
            }
            preview.startBeats = start;
            preview.endBeats = end;
            break;
        }
        case Gesture::move: {
            const double length = originalEnd - originalStart;
            double start = originalStart + (beat - anchorBeat);
            if (!bypass) start = snap(start, false);
            start = juce::jmax(0.0, start);
            double end = start + length;
            if (end > TransportState::maxPositionBeats) {
                end = TransportState::maxPositionBeats;
                start = juce::jmax(0.0, end - length);
            }
            preview.startBeats = start;
            preview.endBeats = end;
            break;
        }
        case Gesture::resizeStart: {
            double start = snap(beat, bypass);
            start = juce::jlimit(0.0, preview.endBeats - minLength, start);
            preview.startBeats = start;
            break;
        }
        case Gesture::resizeEnd: {
            double end = snap(beat, bypass);
            end = juce::jlimit(preview.startBeats + minLength, TransportState::maxPositionBeats, end);
            preview.endBeats = end;
            break;
        }
        default: break;
    }
}

void TimeRuler::mouseUp(const juce::MouseEvent& event) {
    if (gesture == Gesture::pendingSeek) {
        if (onSeek) onSeek(anchorBeat);
    } else if (previewActive && onLoopCommitted &&
               TransportState::validLoopRegion(preview.startBeats, preview.endBeats)) {
        onLoopCommitted(preview.startBeats, preview.endBeats);
    }
    gesture = Gesture::none;
    previewActive = false;
    updateCursor(beatAt(event.position), event.mods.isShiftDown());
    repaint();
}

void TimeRuler::setPixelsPerBeat(double value) {
    if (std::isfinite(value) && value >= 1.0) pixelsPerBeat = value;
    repaint();
}
void TimeRuler::setScrollOffset(double offset) { scrollOffset = offset; repaint(); }

} // namespace vibedaw
