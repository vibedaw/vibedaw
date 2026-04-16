#include "IconButton.h"

namespace vibedaw {

IconButton::IconButton(const juce::String& symbol)
    : symbol_(symbol)
{
    setInterceptsMouseClicks(true, false);
}

void IconButton::setSymbol(const juce::String& symbol) {
    if (symbol_ != symbol) {
        symbol_ = symbol;
        repaint();
    }
}

void IconButton::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(2);
    
    if (isMouseOver_) {
        g.setColour(juce::Colour(0xff444444));
        g.fillRoundedRectangle(bounds, 3.0f);
    }
    
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText(symbol_, getLocalBounds(), juce::Justification::centred);
}

void IconButton::mouseDown(const juce::MouseEvent&) {
    if (onClick) onClick();
}

void IconButton::mouseEnter(const juce::MouseEvent&) {
    isMouseOver_ = true;
    repaint();
}

void IconButton::mouseExit(const juce::MouseEvent&) {
    isMouseOver_ = false;
    repaint();
}

} // namespace vibedaw
