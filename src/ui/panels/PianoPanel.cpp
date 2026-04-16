#include "PianoPanel.h"
#include "ui/PianoComponent.h"
#include "core/MidiManager.h"

namespace vibedaw {

PianoPanel::PianoPanel(juce::MidiKeyboardState& keyboardState, MidiManager* midiManager)
    : Panel("Piano")
{
    piano_ = std::make_unique<PianoComponent>(keyboardState, midiManager);
    setContentComponent(std::move(piano_));
    
    setPreferredHeight(150);
    setExpandedHeight(150);
    setMinHeight(24);
    setMaxHeight(300);
    setTitleBarHeight(24);
}

PianoPanel::~PianoPanel() = default;

void PianoPanel::paint(juce::Graphics& g) {
    Panel::paint(g);
}

void PianoPanel::resized() {
    Panel::resized();
    
    if (auto* content = getContentComponent()) {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(getTitleBarHeight());
        
        if (auto* piano = dynamic_cast<PianoComponent*>(content)) {
            int pianoWidth = piano->getTotalKeyboardWidth();
            int xOffset = juce::jmax(10, (bounds.getWidth() - pianoWidth) / 2);
            piano->setBounds(bounds.withTrimmedLeft(xOffset).withTrimmedRight(xOffset));
        } else {
            content->setBounds(bounds);
        }
    }
}

void PianoPanel::onPanelStateChanged(PanelState newState, PanelState oldState) {
    Panel::onPanelStateChanged(newState, oldState);
}

} // namespace vibedaw
