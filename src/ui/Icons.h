#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

// Vendored Tabler Icons (https://tabler.io/icons), MIT License. Outline set:
// 24x24 viewBox, stroke-width 2, round caps/joins. Path data is parsed once
// and stroked at paint time, so icons are resolution-independent and tintable.
enum class IconId {
    returnToStart, // player-skip-back
    rewind,        // player-track-prev
    stop,          // player-stop
    play,          // player-play
    record,        // player-record
    fastForward,   // player-track-next
    loop,          // repeat
    metronome,     // metronome
    continuous,    // infinity
    takes,         // stack-2
    overdub        // playlist-add
};

class Icons {
public:
    static const juce::Path& path(IconId id);
    // Strokes the icon scaled from its 24x24 source space into bounds.
    static void draw(juce::Graphics& g, IconId id, juce::Colour colour,
                     juce::Rectangle<float> bounds);
};

} // namespace vibedaw
