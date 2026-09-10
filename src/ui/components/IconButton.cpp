#include "IconButton.h"
#include "ui/Theme.h"

namespace vibedaw {

IconButton::IconButton(const juce::String& symbol)
    : symbol_(symbol)
{
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
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
        theme::drawSurface(g, bounds, theme::controlHover, theme::controlRadius);
    }
    
    g.setColour(isMouseOver_ ? theme::textBright : theme::textSecondary);
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
