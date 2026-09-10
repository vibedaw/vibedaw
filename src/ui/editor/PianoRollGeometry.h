#pragma once

#include <algorithm>
#include <cmath>

namespace vibedaw {

// Single transform shared by the piano-roll grid, keyboard, ruler, and hit
// testing. The viewport is the only scroll source: the grid lives at
// grid-local coordinates and the viewport translates it, while the keyboard
// and ruler apply the viewport's pixel scroll offsets directly.
struct PianoRollGeometry {
    int lowestNote = 0;
    int numKeys = 128;
    int keyHeight = 12;
    double pixelsPerBeat = 80.0;

    int highestNote() const { return lowestNote + numKeys - 1; }
    int gridHeight() const { return numKeys * keyHeight; }

    // Row 0 is the top row (highest pitch); row increases downward.
    int rowFromPitch(int pitch) const { return highestNote() - pitch; }
    int pitchFromRow(int row) const { return highestNote() - row; }

    int yFromPitch(int pitch, int scrollY) const {
        return (highestNote() - pitch) * keyHeight - scrollY;
    }
    int pitchFromY(int y, int scrollY) const {
        return highestNote() - floorDiv(y + scrollY, keyHeight);
    }

    static double beatFromX(double x, double scale) { return x / scale; }
    static double xFromBeat(double beat, double scale) { return beat * scale; }

    static int floorDiv(int value, int divisor) {
        int quotient = value / divisor;
        int remainder = value % divisor;
        if (remainder != 0 && ((remainder < 0) != (divisor < 0))) --quotient;
        return quotient;
    }
};

// Editable content extent in quarter-note beats: the longest of the clip
// length, the last note end, and a minimum, plus an editing margin so the
// rightmost content is not pinned to the edge of the scrollable area.
inline double pianoRollContentBeats(double clipLength, double lastNoteEnd,
                                    double minBeats = 8.0, double margin = 4.0) {
    double beats = minBeats;
    if (std::isfinite(clipLength)) beats = std::max(beats, clipLength);
    if (std::isfinite(lastNoteEnd)) beats = std::max(beats, lastNoteEnd);
    if (!std::isfinite(beats) || beats <= 0.0) beats = minBeats;
    return beats + margin;
}

// Arrangement -> source-local beat for playhead following a pooled MIDI source.
// A source plays once from a placement: local beat zero maps to the placement
// start and stays valid only up to the shorter of the source and placement
// lengths. Returns false in a gap, silent tail, or with no placement.
inline bool sourceLocalBeat(double arrangementBeat, double placementStart,
                            double placementDuration, double sourceDuration,
                            double& localBeat) {
    const double local = arrangementBeat - placementStart;
    const double limit = std::min(placementDuration, sourceDuration);
    if (!std::isfinite(local) || !std::isfinite(limit) || limit <= 0.0 ||
        local < 0.0 || local >= limit)
        return false;
    localBeat = local;
    return true;
}

// Follow scroll: keep the playhead inside the comfort band [bandLow, bandHigh]
// of the visible area, recentering when it leaves. Returns the new viewport X,
// or -1 when no scroll is needed. Clamped to the scrollable content.
inline double followedScrollX(double playheadX, double viewX, double visibleWidth,
                              double contentWidth, double bandLow = 0.1,
                              double bandHigh = 0.9) {
    if (!std::isfinite(playheadX) || !std::isfinite(viewX) || visibleWidth <= 0.0)
        return -1.0;
    const double low = viewX + visibleWidth * bandLow;
    const double high = viewX + visibleWidth * bandHigh;
    if (playheadX >= low && playheadX <= high) return -1.0;
    const double maxX = std::max(0.0, contentWidth - visibleWidth);
    double target = std::min(std::max(playheadX - visibleWidth * 0.5, 0.0), maxX);
    if (std::abs(target - viewX) < 0.5) return -1.0;
    return target;
}

} // namespace vibedaw
