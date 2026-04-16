#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "core/MidiManager.h"

namespace vibedaw {

class PianoComponent : public juce::MidiKeyboardComponent,
                       public MidiListener {
public:
    PianoComponent(juce::MidiKeyboardState& state, MidiManager* midiManager);
    ~PianoComponent() override;
    
    void handleMidiMessage(const juce::MidiMessage& message, int sampleOffset) override;
    
    void setOctaveRange(int startOctave, int numOctaves);
    
private:
    MidiManager* midiManager = nullptr;
    juce::MidiKeyboardState& keyboardState;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoComponent)
};

} // namespace vibedaw
