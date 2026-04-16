#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PianoComponent.h"
#include "core/MidiManager.h"

namespace vibedaw {

class FloatingPianoPanel : public juce::Component {
public:
    FloatingPianoPanel(juce::MidiKeyboardState& keyboardState, MidiManager* midiManager);
    ~FloatingPianoPanel() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void show();
    void hide();
    void toggle();
    bool isVisibleAndShowing() const;
    
    void setPanelHeight(int height);
    
private:
    void animateToPosition(int targetY);
    
    std::unique_ptr<PianoComponent> piano;
    int panelHeight = 150;
    bool isShowing = false;
    bool animationInProgress = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FloatingPianoPanel)
};

} // namespace vibedaw
