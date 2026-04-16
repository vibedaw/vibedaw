#include "FloatingPianoPanel.h"
#include "utils/Logger.h"

namespace vibedaw {

FloatingPianoPanel::FloatingPianoPanel(juce::MidiKeyboardState& keyboardState, MidiManager* midiManager)
    : isShowing(false)
{
    piano = std::make_unique<PianoComponent>(keyboardState, midiManager);
    addAndMakeVisible(*piano);
    
    setInterceptsMouseClicks(true, true);
    setAlwaysOnTop(true);
    setVisible(false);
    
    LOG_INFO("FloatingPianoPanel: Created");
}

FloatingPianoPanel::~FloatingPianoPanel() {
    LOG_INFO("FloatingPianoPanel: Destroyed");
}

void FloatingPianoPanel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xdd1a1a1a));
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 8.0f, 2.0f);
}

void FloatingPianoPanel::resized() {
    auto bounds = getLocalBounds();
    
    int pianoWidth = piano->getTotalKeyboardWidth();
    int availableWidth = bounds.getWidth();
    
    int xOffset = (availableWidth - pianoWidth) / 2;
    xOffset = juce::jmax(10, xOffset);
    
    piano->setBounds(xOffset, 10, pianoWidth, bounds.getHeight() - 20);
}

void FloatingPianoPanel::show() {
    if (isShowing) return;
    isShowing = true;
    setVisible(true);
    if (auto* parent = getParentComponent()) {
        int targetY = parent->getHeight() - panelHeight;
        setBounds(0, parent->getHeight(), getWidth(), panelHeight);
        animateToPosition(targetY);
    }
    LOG_INFO("FloatingPianoPanel: Showing");
}

void FloatingPianoPanel::hide() {
    if (!isShowing) return;
    isShowing = false;
    if (auto* parent = getParentComponent()) {
        animateToPosition(parent->getHeight());
    }
    LOG_INFO("FloatingPianoPanel: Hiding");
}

void FloatingPianoPanel::toggle() {
    if (isShowing) {
        hide();
    } else {
        show();
    }
}

bool FloatingPianoPanel::isVisibleAndShowing() const {
    return isShowing;
}

void FloatingPianoPanel::setPanelHeight(int height) {
    panelHeight = height;
}

void FloatingPianoPanel::animateToPosition(int targetY) {
    auto& animator = juce::Desktop::getInstance().getAnimator();
    
    auto bounds = getBounds();
    animator.animateComponent(this,
                              bounds.withY(targetY),
                              1.0f,
                              250,
                              false,
                              1.0,
                              1.0);
}

} // namespace vibedaw
