#include "VibeSynthEditor.h"
#include "VibeSynthProcessor.h"
#include "ui/Theme.h"

namespace vibedaw {

VibeSynthEditor::VibeSynthEditor(VibeSynthProcessor& processor)
    : juce::AudioProcessorEditor(processor),
      processor_(processor),
      osc1Wave(processor_.getParameterState(), "osc1Wave", "Wave", {"Sine", "Saw", "Square", "Triangle"}),
      osc2Wave(processor_.getParameterState(), "osc2Wave", "Wave", {"Sine", "Saw", "Square", "Triangle"}),
      filterType(processor_.getParameterState(), "filterType", "Mode", {"Low-pass", "High-pass"}),
      osc1Octave(processor_.getParameterState(), "osc1Octave", "Octave"),
      osc1Detune(processor_.getParameterState(), "osc1Detune", "Detune"),
      osc2Octave(processor_.getParameterState(), "osc2Octave", "Octave"),
      osc2Detune(processor_.getParameterState(), "osc2Detune", "Detune"),
      oscMix(processor_.getParameterState(), "oscMix", "Mix"),
      attack(processor_.getParameterState(), "attack", "Attack"),
      decay(processor_.getParameterState(), "decay", "Decay"),
      sustain(processor_.getParameterState(), "sustain", "Sustain"),
      release(processor_.getParameterState(), "release", "Release"),
      cutoff(processor_.getParameterState(), "cutoff", "Cutoff"),
      resonance(processor_.getParameterState(), "resonance", "Resonance"),
      gain(processor_.getParameterState(), "gain", "Gain")
{
    setOpaque(true);

    for (auto* title : { &osc1Title, &osc2Title, &envTitle, &filterTitle, &outputTitle }) {
        title->setJustificationType(juce::Justification::centred);
        title->setFont(juce::Font(11.5f, juce::Font::bold));
        title->setColour(juce::Label::textColourId, theme::textDefault);
        addAndMakeVisible(title);
    }
    for (auto* picker : { &osc1Wave, &osc2Wave, &filterType }) {
        addAndMakeVisible(picker->box);
        addAndMakeVisible(picker->name);
    }
    for (auto* knob : { &osc1Octave, &osc1Detune, &osc2Octave, &osc2Detune, &oscMix,
                        &attack, &decay, &sustain, &release, &cutoff, &resonance, &gain }) {
        addAndMakeVisible(knob->slider);
        addAndMakeVisible(knob->name);
    }

    setSize(600, 400);
}

void VibeSynthEditor::paint(juce::Graphics& g) {
    theme::fillPanel(g, *this, theme::panelBackground);
}

void VibeSynthEditor::resized() {
    auto area = getLocalBounds().reduced(14, 12);
    const int columnWidth = area.getWidth() / 5;

    auto placeTitle = [](juce::Label& title, juce::Rectangle<int> bounds) {
        title.setBounds(bounds.removeFromTop(18));
    };
    auto placePicker = [](Picker& picker, juce::Rectangle<int> bounds) {
        picker.name.setBounds(bounds.removeFromTop(15));
        picker.box.setBounds(bounds.removeFromTop(24).reduced(6, 0));
    };
    auto placeKnob = [](Knob& knob, juce::Rectangle<int> bounds) {
        auto cell = bounds.removeFromTop(84);
        knob.slider.setBounds(cell.removeFromTop(68));
        knob.name.setBounds(cell);
    };

    auto first = area.removeFromLeft(columnWidth);
    placeTitle(osc1Title, first);
    placePicker(osc1Wave, first);
    placeKnob(osc1Octave, first);
    placeKnob(osc1Detune, first);

    auto second = area.removeFromLeft(columnWidth);
    placeTitle(osc2Title, second);
    placePicker(osc2Wave, second);
    placeKnob(osc2Octave, second);
    placeKnob(osc2Detune, second);

    auto third = area.removeFromLeft(columnWidth);
    placeTitle(envTitle, third);
    placeKnob(attack, third);
    placeKnob(decay, third);
    placeKnob(sustain, third);
    placeKnob(release, third);

    auto fourth = area.removeFromLeft(columnWidth);
    placeTitle(filterTitle, fourth);
    placePicker(filterType, fourth);
    placeKnob(cutoff, fourth);
    placeKnob(resonance, fourth);

    auto fifth = area.removeFromLeft(columnWidth);
    placeTitle(outputTitle, fifth);
    placeKnob(oscMix, fifth);
    placeKnob(gain, fifth);
}

} // namespace vibedaw
