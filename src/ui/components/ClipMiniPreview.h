#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "project/Clip.h"
#include "ui/Theme.h"

namespace vibedaw::ClipMiniPreview {

inline bool isBlackKey(int pitch) {
    const int pitchClass = ((pitch % 12) + 12) % 12;
    return pitchClass == 1 || pitchClass == 3 || pitchClass == 6 || pitchClass == 8 || pitchClass == 10;
}

// Shared mini piano-roll painter for MIDI clip previews (timeline lanes, clip
// cards). Fits the pitch range of the clip so dense material stays readable.
// Horizontal mapping: beat-accurate when pixelsPerBeat > 0 (the caller's area
// must then correspond to the placement), otherwise the clip's full duration
// is fitted into the area (sidebar thumbnails).
inline void drawMiniNotes(juce::Graphics& g, const MidiClip& clip, double viewDuration,
                          juce::Rectangle<float> area, bool muted, double pixelsPerBeat = 0.0) {
    const auto& notes = clip.getNotes();
    if (notes.empty() || viewDuration <= 0.0 || area.getWidth() < 2.0f || area.getHeight() < 4.0f) return;

    int minPitch = Note::maxPitch;
    int maxPitch = Note::minPitch;
    for (const auto& note : notes) {
        minPitch = juce::jmin(minPitch, note.getPitch());
        maxPitch = juce::jmax(maxPitch, note.getPitch());
    }
    int span = maxPitch - minPitch + 1;
    if (span < 12) {
        const int pad = (12 - span) / 2 + 1;
        minPitch = juce::jmax(Note::minPitch, minPitch - pad);
        maxPitch = juce::jmin(Note::maxPitch, maxPitch + pad);
        span = maxPitch - minPitch + 1;
    }

    const float rowHeight = area.getHeight() / static_cast<float>(span);
    const float scale = pixelsPerBeat > 0.0
        ? static_cast<float>(pixelsPerBeat)
        : area.getWidth() / static_cast<float>(viewDuration);

    auto rowBounds = [&](int pitch) {
        const float fromTop = static_cast<float>(maxPitch - pitch) * rowHeight;
        return juce::Rectangle<float>(area.getX(), area.getY() + fromTop, area.getWidth(), rowHeight);
    };

    for (int pitch = minPitch; pitch <= maxPitch; ++pitch) {
        if (!isBlackKey(pitch)) continue;
        g.setColour(theme::black.withAlpha(muted ? 0.10f : 0.15f));
        g.fillRect(rowBounds(pitch).withTrimmedBottom(rowHeight / 2.0f));
    }

    const double clipEnd = juce::jmin(clip.getDuration(), viewDuration);
    for (const auto& note : notes) {
        const double end = juce::jmin(note.getEndTime(), clipEnd);
        if (note.getStartTime() >= end || note.getStartTime() >= viewDuration) continue;
        const float left = area.getX() + static_cast<float>(note.getStartTime()) * scale;
        const float right = area.getX() + static_cast<float>(juce::jmin(end, viewDuration)) * scale;
        const float width = juce::jmax(2.0f, right - left);
        auto noteRect = rowBounds(note.getPitch());
        noteRect.setY(noteRect.getY() + juce::jmax(0.0f, (rowHeight - 3.0f) / 2.0f));
        noteRect.setHeight(juce::jmin(3.0f, juce::jmax(2.0f, rowHeight - 1.0f)));
        noteRect.setLeft(juce::jmax(area.getX(), left));
        noteRect.setRight(juce::jmin(area.getRight(), right));
        if (noteRect.getWidth() <= 0.0f) continue;

        float alpha = 0.35f + 0.55f * (static_cast<float>(note.getVelocity()) / Note::maxVelocity);
        if (note.isMuted()) alpha *= 0.35f;
        if (muted) alpha *= 0.5f;
        g.setColour(theme::white.withAlpha(juce::jlimit(0.1f, 1.0f, alpha)));
        g.fillRoundedRectangle(noteRect, 1.0f);
    }
}

} // namespace vibedaw::ClipMiniPreview