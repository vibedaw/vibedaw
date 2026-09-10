#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw::theme {

// Surfaces, darkest to lightest.
inline const juce::Colour deepWell{0xff080f15};        // meter wells, darkest recesses
inline const juce::Colour windowBackground{0xff0a1117}; // workspace gutters
inline const juce::Colour panelBackground{0xff0e1821};  // panel bodies, unselected strips, lanes
inline const juce::Colour browserBackground{0xff101a23};
inline const juce::Colour controlDim{0xff17232e};
inline const juce::Colour controlActive{0xff253b49};
inline const juce::Colour controlSelected{0xff23504b};
inline const juce::Colour controlPressed{0xff426071};
inline const juce::Colour raised{0xff131f29};
inline const juce::Colour control{0xff192733};
inline const juce::Colour controlHover{0xff243847};
inline const juce::Colour selectedLane{0xff122730};
inline const juce::Colour selectedSurface{0xff17333a};
inline const juce::Colour overlayWindow{0xee0e1821};
inline const juce::Colour transparent{0x00000000};

// Lines and borders.
inline const juce::Colour hairline{0xff1b2a36};
inline const juce::Colour border{0xff253847};
inline const juce::Colour borderStrong{0xff3a5366};
inline const juce::Colour tickMark{0xff496173};
inline const juce::Colour separator{0xff304554};
inline const juce::Colour gridSub{0xff182732};          // sub-beat grid lines
inline const juce::Colour gridBar{0xff2a4050};          // lane bar lines
inline const juce::Colour gridBarStrong{0xff344d60};
inline const juce::Colour gridMeasure{0xff486174};

// Text.
inline const juce::Colour white{0xffffffff};
inline const juce::Colour black{0xff000000};
inline const juce::Colour textBright{0xffe3edf5};
inline const juce::Colour textDefault{0xffb7cbdc};
inline const juce::Colour textSecondary{0xff8ea7bb};
inline const juce::Colour textMuted{0xff7992a5};
inline const juce::Colour textSubtle{0xff6d8598};
inline const juce::Colour textFaint{0xff5f778a};
inline const juce::Colour textDim{0xff506779};
inline const juce::Colour rulerBarNumber{0xffc7d8e7};

// Accent and actions.
inline const juce::Colour accent{0xff00d996};           // playhead, transport digits, active play
inline const juce::Colour actionGreen{0xff103c32};      // add/new button background
inline const juce::Colour toggleActiveBackground{0xff08754f};
inline const juce::Colour highlightBackground{0xff184b48};

// Transport states.
inline const juce::Colour danger{0xfff34e62};           // record icon
inline const juce::Colour dangerDim{0xff65303c};
inline const juce::Colour dangerText{0xffff94a0};
inline const juce::Colour muteRed{0xffed6b76};
inline const juce::Colour muteGreen{0xff36dda5};

// Drag and drop.
inline const juce::Colour dropIndicator{0xff00d996};
inline const juce::Colour dropFill{0xff193f48};
inline const juce::Colour dropEdge{0xff62d9cb};
inline const juce::Colour dragValid{0xff62d9cb};
inline const juce::Colour dragInvalid{0xfff0ae73};

// Loop region.
inline const juce::Colour loopFillActive{0xff174138};
inline const juce::Colour loopEdgeActive{0xff00d996};
inline const juce::Colour loopFillIdle{0xff1c2b35};
inline const juce::Colour loopEdgeIdle{0xff5a788d};
inline const juce::Colour loopFillPreview{0xff235550};
inline const juce::Colour loopEdgePreview{0xff70e5c0};
inline const juce::Colour accentBlue{0xff678ff0};

// Piano roll.
inline const juce::Colour rollKeyHeldBlack{0xff12684e};
inline const juce::Colour rollKeyHeldWhite{0xff53dba6};
inline const juce::Colour noteDefault{0xff27c58e};
inline const juce::Colour noteHover{0xff65dfb4};
inline const juce::Colour noteSelected{0xffefbf65};

// Meter zones: low < 60% <= mid < 85% <= high.
inline const juce::Colour meterLow{0xff00d996};
inline const juce::Colour meterMid{0xffd1cd65};
inline const juce::Colour meterHigh{0xfff06470};
inline const juce::Colour meterPeak{0xffdef6ed};

// Clip states.
inline const juce::Colour unresolvedClip{0xffad6464};

// Default header strip colours when a model carries none.
inline const juce::Colour headerStripDefault{0xff64899f};
inline const juce::Colour trackHeaderDefault{0xff91afc2};

// Floating panel chrome.
inline const juce::Colour floatingPanelBorder{0xff448b8b};
inline const juce::Colour titleBarActive{0xff1b3440};

inline constexpr float panelRadius = 6.0f;
inline constexpr float controlRadius = 4.0f;

// A shallow bevel and a close shadow keep dense controls distinct without gloss.
inline void drawSurface(juce::Graphics& g, juce::Rectangle<float> bounds,
                        juce::Colour background, float radius = panelRadius,
                        juce::Colour outline = border) {
    if (bounds.isEmpty()) return;
    g.setColour(black.withAlpha(0.22f));
    g.fillRoundedRectangle(bounds.translated(0.0f, 1.0f), radius);
    g.setGradientFill(juce::ColourGradient(background.brighter(0.045f), bounds.getTopLeft(),
                                          background.darker(0.08f), bounds.getBottomLeft(), false));
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(outline);
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
    if (bounds.getWidth() > radius * 2.0f) {
        g.setColour(white.withAlpha(0.035f));
        g.drawHorizontalLine(juce::roundToInt(bounds.getY() + 1.0f),
                             bounds.getX() + radius, bounds.getRight() - radius);
    }
}

inline void drawWell(juce::Graphics& g, juce::Rectangle<float> bounds,
                     float radius = controlRadius) {
    if (bounds.isEmpty()) return;
    g.setGradientFill(juce::ColourGradient(deepWell, bounds.getTopLeft(),
                                          windowBackground, bounds.getBottomLeft(), false));
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
}

// Shared paint helpers.
inline void fillPanel(juce::Graphics& g, const juce::Component& c, juce::Colour background) {
    g.fillAll(background);
    g.setColour(border);
    g.drawRect(c.getLocalBounds());
}

} // namespace vibedaw::theme
