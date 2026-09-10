#include "PianoComponent.h"
#include "ui/Theme.h"
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
    setColour(whiteNoteColourId, juce::Colour(0xffe3e8e6));
    setColour(blackNoteColourId, theme::deepWell);
    setColour(keyDownOverlayColourId, theme::accent.withAlpha(0.55f));
    setColour(mouseOverKeyOverlayColourId, theme::accent.withAlpha(0.12f));
    setColour(shadowColourId, theme::black.withAlpha(0.4f));
    setColour(upDownButtonBackgroundColourId, theme::control);
    setColour(upDownButtonArrowColourId, theme::textSecondary);

    LOG_INFO("PianoComponent: Created");
}

PianoComponent::~PianoComponent() {
    LOG_INFO("PianoComponent: Destroyed");
}

void PianoComponent::drawWhiteNote(int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                   bool isDown, bool isOver, juce::Colour, juce::Colour) {
    const auto ivory = juce::Colour(0xffe3e8e6);
    const auto top = isDown ? theme::accent.darker(0.15f) : isOver ? ivory.interpolatedWith(theme::accent, 0.12f) : ivory;
    const auto bottom = isDown ? theme::accent.brighter(0.2f) : juce::Colour(0xffaab8c2);
    g.setGradientFill(juce::ColourGradient(top, area.getX(), area.getY(),
                                         bottom, area.getX(), area.getBottom(), false));
    g.fillRect(area);
    g.setColour(theme::deepWell.withAlpha(0.7f));
    g.drawRect(area, 0.7f);
    g.setColour(theme::white.withAlpha(isDown ? 0.25f : 0.6f));
    g.drawLine(area.getX() + 1.5f, area.getY(), area.getX() + 1.5f, area.getBottom() - 4.0f);
    g.setColour(theme::deepWell.withAlpha(0.18f));
    g.fillRect(area.withTrimmedTop(juce::jmax(0.0f, area.getHeight() - 4.0f)));
    if (midiNoteNumber % 12 == 0) {
        g.setColour(theme::deepWell.withAlpha(0.75f));
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(getWhiteNoteText(midiNoteNumber),
                   area.withTrimmedTop(juce::jmax(0.0f, area.getHeight() - 22.0f)), juce::Justification::centred);
    }
}

void PianoComponent::drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> area,
                                   bool isDown, bool isOver, juce::Colour) {
    g.setColour(theme::black.withAlpha(0.5f));
    g.fillRoundedRectangle(area.translated(1.0f, 2.0f), 2.0f);
    const auto top = isDown ? theme::accent.darker(0.25f) : isOver ? theme::controlSelected : theme::controlHover;
    const auto bottom = isDown ? theme::accent.darker(0.55f) : theme::deepWell;
    g.setGradientFill(juce::ColourGradient(top, area.getX(), area.getY(),
                                         bottom, area.getX(), area.getBottom(), false));
    g.fillRoundedRectangle(area.reduced(0.5f, 0.0f), 2.0f);
    g.setColour((isDown ? theme::accent : theme::textSecondary).withAlpha(0.35f));
    g.drawRoundedRectangle(area.reduced(1.0f), 2.0f, 0.7f);
    g.setColour((isDown ? theme::accent : theme::controlSelected).withAlpha(0.7f));
    g.fillRoundedRectangle(area.reduced(2.0f, 0.0f).withTrimmedTop(juce::jmax(0.0f, area.getHeight() - 7.0f)), 1.0f);
}

void PianoComponent::setOctaveRange(int startOctave, int numOctaves) {
    int startNote = startOctave * 12 + 12;
    setLowestVisibleKey(startNote);
    LOG_INFO("PianoComponent: Octave range set to " + juce::String(startOctave) + "-" + juce::String(startOctave + numOctaves - 1));
}

} // namespace vibedaw
