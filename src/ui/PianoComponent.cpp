#include "PianoComponent.h"
#include "utils/Logger.h"

namespace vibedaw {

PianoComponent::PianoComponent(juce::MidiKeyboardState& state, MidiManager* manager)
    : juce::MidiKeyboardComponent(state, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    juce::ignoreUnused(manager);
    setLowestVisibleKey(48);
    setKeyWidth(20.0f);
    setScrollButtonsVisible(true);
    setMidiChannel(1);
    
    
    LOG_INFO("PianoComponent: Created");
}

PianoComponent::~PianoComponent() {
    LOG_INFO("PianoComponent: Destroyed");
}

void PianoComponent::setOctaveRange(int startOctave, int numOctaves) {
    int startNote = startOctave * 12 + 12;
    setLowestVisibleKey(startNote);
    LOG_INFO("PianoComponent: Octave range set to " + juce::String(startOctave) + "-" + juce::String(startOctave + numOctaves - 1));
}

} // namespace vibedaw
