#include "PianoPanel.h"
#include "ui/PianoComponent.h"
#include "ui/Theme.h"
#include "core/MidiManager.h"

namespace vibedaw {

PianoPanel::PianoPanel(juce::MidiKeyboardState& keyboardState, MidiManager* midiManager)
    : Panel("Piano")
{
    piano_ = std::make_unique<PianoComponent>(keyboardState, midiManager);
    setContentComponent(std::move(piano_));

    octaveLabel_.setJustificationType(juce::Justification::centred);
    octaveLabel_.setColour(juce::Label::textColourId, theme::textDefault);
    octaveLabel_.setFont(juce::Font(12.0f, juce::Font::bold));
    addAndMakeVisible(octaveLabel_);

    for (auto* button : {&octaveDown_, &octaveUp_}) {
        button->setColour(juce::TextButton::buttonColourId, theme::control);
        button->setColour(juce::TextButton::textColourOffId, theme::textDefault);
        addAndMakeVisible(*button);
    }
    octaveDown_.onClick = [this] { shiftOctave(-1); };
    octaveUp_.onClick = [this] { shiftOctave(1); };

    velocitySlider_.setRange(1.0, 127.0, 1.0);
    velocitySlider_.setValue(100.0, juce::dontSendNotification);
    velocitySlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 30, 18);
    velocitySlider_.setColour(juce::Slider::backgroundColourId, theme::deepWell);
    velocitySlider_.setColour(juce::Slider::trackColourId, theme::accent.withAlpha(0.5f));
    velocitySlider_.setColour(juce::Slider::thumbColourId, theme::accent);
    velocitySlider_.setColour(juce::Slider::textBoxTextColourId, theme::textBright);
    velocitySlider_.setColour(juce::Slider::textBoxBackgroundColourId, theme::deepWell);
    velocitySlider_.setColour(juce::Slider::textBoxOutlineColourId, theme::border);
    if (auto* piano = dynamic_cast<PianoComponent*>(getContentComponent()))
        piano->setVelocity(100.0f / 127.0f, false);
    velocitySlider_.onValueChange = [this] {
        if (auto* piano = dynamic_cast<PianoComponent*>(getContentComponent()))
            piano->setVelocity(static_cast<float>(velocitySlider_.getValue()) / 127.0f, false);
    };
    addAndMakeVisible(velocitySlider_);

    setPreferredHeight(150);
    setExpandedHeight(150);
    setMinHeight(24);
    setMaxHeight(300);
    setTitleBarHeight(24);
}

PianoPanel::~PianoPanel() = default;

void PianoPanel::paint(juce::Graphics& g) {
    Panel::paint(g);
    auto bounds = getLocalBounds();
    bounds.removeFromTop(getTitleBarHeight());
    bounds = bounds.reduced(4, 0).withTrimmedBottom(4);
    theme::drawSurface(g, bounds.withWidth(150).toFloat().reduced(4.0f), theme::raised);
    auto controls = bounds.removeFromLeft(150).reduced(8, 4);

    g.setColour(theme::textSecondary);
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText("OCTAVE", controls.removeFromTop(12), juce::Justification::centredLeft);

    controls.removeFromTop(24);
    controls.removeFromTop(4);

    g.drawText("VELOCITY", controls.removeFromTop(12), juce::Justification::centredLeft);
}

void PianoPanel::resized() {
    Panel::resized();

    auto bounds = getLocalBounds();
    bounds.removeFromTop(getTitleBarHeight());
    bounds = bounds.reduced(4, 0).withTrimmedBottom(4);
    auto controls = bounds.removeFromLeft(150).reduced(8, 4);

    controls.removeFromTop(12);
    auto octaveRow = controls.removeFromTop(24);
    octaveDown_.setBounds(octaveRow.removeFromLeft(24).reduced(1));
    octaveUp_.setBounds(octaveRow.removeFromRight(24).reduced(1));
    octaveLabel_.setBounds(octaveRow.reduced(2, 0));
    controls.removeFromTop(4 + 12);

    velocitySlider_.setBounds(controls.removeFromTop(22));

    if (auto* content = getContentComponent()) {
        if (auto* piano = dynamic_cast<PianoComponent*>(content)) {
            int pianoWidth = piano->getTotalKeyboardWidth();
            int xOffset = juce::jmax(10, (bounds.getWidth() - pianoWidth) / 2);
            piano->setBounds(bounds.withTrimmedLeft(xOffset).withTrimmedRight(xOffset));
        } else {
            content->setBounds(bounds);
        }
    }
    updateOctaveLabel();
}

void PianoPanel::shiftOctave(int direction) {
    if (auto* piano = dynamic_cast<PianoComponent*>(getContentComponent())) {
        piano->setLowestVisibleKey(juce::jlimit(0, 127, piano->getLowestVisibleKey() + direction * 12));
        updateOctaveLabel();
    }
}

void PianoPanel::updateOctaveLabel() {
    if (auto* piano = dynamic_cast<PianoComponent*>(getContentComponent())) {
        octaveLabel_.setText(juce::String(piano->getLowestVisibleKey() / 12 - 1), juce::dontSendNotification);
    }
}

void PianoPanel::onPanelStateChanged(PanelState newState, PanelState oldState) {
    Panel::onPanelStateChanged(newState, oldState);
}

} // namespace vibedaw
