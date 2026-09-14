#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/Theme.h"

namespace vibedaw {

class VibeSynthProcessor;

// Compact knob/combo editor for the built-in synth; opens through the existing
// PluginWindow pop-out.
class VibeSynthEditor : public juce::AudioProcessorEditor {
public:
    explicit VibeSynthEditor(VibeSynthProcessor& processor);
    ~VibeSynthEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Knob {
        juce::Slider slider;
        juce::Label name;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;

        Knob(juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
             const juce::String& label)
            : slider(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
              attachment(state, parameterId, slider) {
            name.setText(label, juce::dontSendNotification);
            name.setJustificationType(juce::Justification::centred);
            name.setFont(juce::Font(11.0f));
            name.setColour(juce::Label::textColourId, theme::textSecondary);
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
            slider.setColour(juce::Slider::rotarySliderFillColourId, theme::accent);
            slider.setColour(juce::Slider::rotarySliderOutlineColourId, theme::controlHover);
            slider.setColour(juce::Slider::textBoxTextColourId, theme::textDefault);
            slider.setColour(juce::Slider::textBoxBackgroundColourId, theme::controlDim);
            slider.setColour(juce::Slider::textBoxOutlineColourId, theme::hairline);
            slider.setNumDecimalPlacesToDisplay(2);
        }
    };

    struct Picker {
        juce::ComboBox box;
        juce::Label name;
        juce::AudioProcessorValueTreeState::ComboBoxAttachment attachment;

        Picker(juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
               const juce::String& label, const juce::StringArray& items)
            : attachment(state, parameterId, box) {
            for (int i = 0; i < items.size(); ++i) box.addItem(items[i], i + 1);
            name.setText(label, juce::dontSendNotification);
            name.setJustificationType(juce::Justification::centred);
            name.setFont(juce::Font(11.0f));
            name.setColour(juce::Label::textColourId, theme::textSecondary);
            box.setColour(juce::ComboBox::backgroundColourId, theme::controlDim);
            box.setColour(juce::ComboBox::outlineColourId, theme::hairline);
            box.setColour(juce::ComboBox::textColourId, theme::textDefault);
            box.setColour(juce::ComboBox::arrowColourId, theme::textSecondary);
        }
    };

    VibeSynthProcessor& processor_;

    juce::Label osc1Title{"osc1Title", "OSC 1"};
    juce::Label osc2Title{"osc2Title", "OSC 2"};
    juce::Label envTitle{"envTitle", "ENVELOPE"};
    juce::Label filterTitle{"filterTitle", "FILTER"};
    juce::Label outputTitle{"outputTitle", "OUTPUT"};

    Picker osc1Wave, osc2Wave, filterType;
    Knob osc1Octave, osc1Detune, osc2Octave, osc2Detune, oscMix;
    Knob attack, decay, sustain, release;
    Knob cutoff, resonance, gain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VibeSynthEditor)
};

} // namespace vibedaw
