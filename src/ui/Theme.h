#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw::theme {

// Surfaces, darkest to lightest.
inline const juce::Colour deepWell{0xff0d0d0d};        // meter wells, darkest recesses
inline const juce::Colour windowBackground{0xff1a1a1a}; // app window, transport bar, fader wells
inline const juce::Colour panelBackground{0xff1e1e1e};  // panel bodies, unselected strips, lanes
inline const juce::Colour browserBackground{0xff202020};
inline const juce::Colour controlDim{0xff353535};       // idle toggles, editor ruler surface
inline const juce::Colour controlActive{0xff404040};    // pressed surfaces, strong row hairlines
inline const juce::Colour controlSelected{0xff555555};  // toggled small buttons
inline const juce::Colour controlPressed{0xff666666};   // held small buttons, fader cap fill
inline const juce::Colour raised{0xff252525};           // raised containers: sidebars, popovers, grids
inline const juce::Colour control{0xff2a2a2a};          // buttons, rows, title bars at rest
inline const juce::Colour controlHover{0xff3a3a3a};     // hovered buttons, raised control fills
inline const juce::Colour selectedLane{0xff2a2a3a};     // selected timeline lane
inline const juce::Colour selectedSurface{0xff3a3a4a};  // selected rows/headers
inline const juce::Colour overlayWindow{0xdd1a1a1a};    // translucent floating windows
inline const juce::Colour transparent{0x00000000};

// Lines and borders.
inline const juce::Colour hairline{0xff333333};
inline const juce::Colour border{0xff3a3a3a};
inline const juce::Colour borderStrong{0xff444444};
inline const juce::Colour tickMark{0xff4a4a4a};
inline const juce::Colour separator{0xff505050};
inline const juce::Colour gridSub{0xff383838};          // sub-beat grid lines
inline const juce::Colour gridBar{0xff484848};          // lane bar lines
inline const juce::Colour gridBarStrong{0xff454545};    // editor ruler bar lines
inline const juce::Colour gridMeasure{0xff606060};      // measure lines, keyboard edge

// Text.
inline const juce::Colour white{0xffffffff};
inline const juce::Colour black{0xff000000};
inline const juce::Colour textBright{0xffcccccc};
inline const juce::Colour textDefault{0xffaaaaaa};
inline const juce::Colour textSecondary{0xff888888};
inline const juce::Colour textMuted{0xff777777};
inline const juce::Colour textSubtle{0xff707070};
inline const juce::Colour textFaint{0xff666666};
inline const juce::Colour textDim{0xff606060};
inline const juce::Colour rulerBarNumber{0xffccddff};

// Accent and actions.
inline const juce::Colour accent{0xff00ff88};           // playhead, transport digits, active play
inline const juce::Colour actionGreen{0xff3a5a3a};      // add/new button background
inline const juce::Colour toggleActiveBackground{0xff4a8a4a}; // engaged transport toggle
inline const juce::Colour highlightBackground{0xff3a5a7a};    // selection highlight, loop/metronome active

// Transport states.
inline const juce::Colour danger{0xffff4444};           // record icon
inline const juce::Colour dangerDim{0xff8a4a4a};        // engaged record background
inline const juce::Colour dangerText{0xffff8888};       // invalid input text
inline const juce::Colour muteRed{0xffd94a4a};
inline const juce::Colour muteGreen{0xff4ad94a};

// Drag and drop.
inline const juce::Colour dropIndicator{0xff00ff00};
inline const juce::Colour dropFill{0xff395875};
inline const juce::Colour dropEdge{0xffadd8e6};         // light blue drop outline
inline const juce::Colour dragValid{0xff70baff};
inline const juce::Colour dragInvalid{0xffffaa70};

// Loop region.
inline const juce::Colour loopFillActive{0xff365978};
inline const juce::Colour loopEdgeActive{0xff66aaff};
inline const juce::Colour loopFillIdle{0xff383e44};
inline const juce::Colour loopEdgeIdle{0xff667788};
inline const juce::Colour loopFillPreview{0xff3d6e8f};
inline const juce::Colour loopEdgePreview{0xff8fc4ff};
inline const juce::Colour accentBlue{0xff66aaff};

// Piano roll.
inline const juce::Colour rollKeyHeldBlack{0xff4466aa};
inline const juce::Colour rollKeyHeldWhite{0xff6688cc};
inline const juce::Colour noteDefault{0xff6ad94a};
inline const juce::Colour noteHover{0xff88aa55};
inline const juce::Colour noteSelected{0xffee9933};

// Meters (neutral grey scale; zones low < 60% <= mid < 85% <= high).
inline const juce::Colour meterLow{0xff666666};
inline const juce::Colour meterMid{0xffaaaaaa};
inline const juce::Colour meterHigh{0xffffffff};
inline const juce::Colour meterPeak{0xffffffff};

// Clip states.
inline const juce::Colour unresolvedClip{0xffad6464};

// Default header strip colours when a model carries none.
inline const juce::Colour headerStripDefault{0xff888888};
inline const juce::Colour trackHeaderDefault{0xffaaaaaa};

// Floating panel chrome.
inline const juce::Colour floatingPanelBorder{0xff5a8a9a};
inline const juce::Colour titleBarActive{0xff3a5a6a};

// Shared paint helpers.
inline void fillPanel(juce::Graphics& g, const juce::Component& c, juce::Colour background) {
    g.fillAll(background);
    g.setColour(border);
    g.drawRect(c.getLocalBounds());
}

} // namespace vibedaw::theme