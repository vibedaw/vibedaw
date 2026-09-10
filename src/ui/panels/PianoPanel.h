#pragma once

#include "Panel.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace vibedaw {

class MidiManager;
class PianoComponent;

class PianoPanel : public Panel {
public:
    PianoPanel(juce::MidiKeyboardState& keyboardState, MidiManager* midiManager);
    ~PianoPanel() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void onPanelStateChanged(PanelState newState, PanelState oldState) override;
    
private:
    std::unique_ptr<PianoComponent> piano_;
    juce::TextButton octaveDown_{"-"}, octaveUp_{"+"};
    juce::Label octaveLabel_;
    juce::Slider velocitySlider_;

    void setupFloatingMode();
    void shiftOctave(int direction);
    void updateOctaveLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoPanel)
};

} // namespace vibedaw
