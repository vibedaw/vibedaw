#include "Icons.h"
#include <array>

namespace vibedaw {

// Tabler Icons v1.28 (metronome v2.40) outline path data, MIT License.
// Copyright (c) Paweł Kuna. Multiple subpaths are concatenated per icon; each
// begins with M, so concatenation preserves the source geometry.
namespace {

const char* const iconPaths[] = {
    "M20 5v14l-12 -7l12 -7M4 5l0 14",                                        // player-skip-back
    "M21 5v14l-8 -7l8 -7M10 5v14l-8 -7l8 -7",                                // player-track-prev
    "M5 7a2 2 0 0 1 2 -2h10a2 2 0 0 1 2 2v10a2 2 0 0 1 -2 2h-10a2 2 0 0 1 "
        "-2 -2l0 -10",                                                       // player-stop
    "M7 4v16l13 -8l-13 -8",                                                  // player-play
    "M5 12a7 7 0 1 0 14 0a7 7 0 1 0 -14 0",                                  // player-record
    "M3 5v14l8 -7l-8 -7M14 5v14l8 -7l-8 -7",                                 // player-track-next
    "M4 12v-3a3 3 0 0 1 3 -3h13m-3 -3l3 3l-3 3M20 12v3a3 3 0 0 1 -3 3h-13"
        "m3 3l-3 -3l3 -3",                                                   // repeat
    "M14.153 8.188l-.72 -3.236a2.493 2.493 0 0 0 -4.867 0l-3.025 13.614a2 2 0 0 0 "
        "1.952 2.434h7.014a2 2 0 0 0 1.952 -2.434l-.524 -2.357m-4.935 1.791l9 -13"
        "M19 5a1 1 0 1 0 2 0a1 1 0 1 0 -2 0"                                 // metronome
};

std::array<juce::Path, 8> buildPaths() {
    std::array<juce::Path, 8> paths{};
    for (size_t i = 0; i < paths.size(); ++i)
        paths[i] = juce::Drawable::parseSVGPath(iconPaths[i]);
    return paths;
}

} // namespace

const juce::Path& Icons::path(IconId id) {
    static const std::array<juce::Path, 8> paths = buildPaths();
    return paths[static_cast<size_t>(id)];
}

void Icons::draw(juce::Graphics& g, IconId id, juce::Colour colour,
                 juce::Rectangle<float> bounds) {
    const auto& source = path(id);
    const float scale = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 24.0f;
    const float centreX = bounds.getCentreX(), centreY = bounds.getCentreY();
    // x' = scale * x + (centre - 12 * scale) centres the 24x24 source box.
    const auto transform = juce::AffineTransform(scale, 0.0f, centreX - 12.0f * scale,
                                                 0.0f, scale, centreY - 12.0f * scale);
    juce::Path scaled;
    scaled.addPath(source, transform);
    juce::Path stroked;
    juce::PathStrokeType(2.0f * scale, juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded)
        .createStrokedPath(stroked, scaled);
    g.setColour(colour);
    g.fillPath(stroked);
}

} // namespace vibedaw