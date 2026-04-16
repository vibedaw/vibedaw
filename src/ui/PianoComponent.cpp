#include "PianoComponent.h"
#include "utils/Logger.h"

namespace vibedaw {

PianoComponent::PianoComponent(juce::MidiKeyboardState& state, MidiManager* manager)
    : juce::MidiKeyboardComponent(state, juce::MidiKeyboardComponent::horizontalKeyboard),
      midiManager(manager),
      keyboardState(state)
{
    setLowestVisibleKey(48);
    setKeyWidth(20.0f);
    setScrollButtonsVisible(true);
    setMidiChannel(1);
    
    if (midiManager) {
        midiManager->addListener(this);
    }
    
    LOG_INFO("PianoComponent: Created");
}

PianoComponent::~PianoComponent() {
    if (midiManager) {
        midiManager->removeListener(this);
    }
    LOG_INFO("PianoComponent: Destroyed");
}

void PianoComponent::handleMidiMessage(const juce::MidiMessage& message, int sampleOffset) {
    if (message.isNoteOn() || message.isNoteOff()) {
        juce::MessageManager::callAsync([this, message]() {
            int noteNumber = message.getNoteNumber();
            bool isOn = message.isNoteOn() && message.getVelocity() > 0;
            
            if (isOn) {
                keyboardState.noteOn(1, noteNumber, message.getVelocity() / 127.0f);
            } else {
                keyboardState.noteOff(1, noteNumber, 0.0f);
            }
        });
    }
}

void PianoComponent::setOctaveRange(int startOctave, int numOctaves) {
    int startNote = startOctave * 12 + 12;
    setLowestVisibleKey(startNote);
    LOG_INFO("PianoComponent: Octave range set to " + juce::String(startOctave) + "-" + juce::String(startOctave + numOctaves - 1));
}

} // namespace vibedaw
