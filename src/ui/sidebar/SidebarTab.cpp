#include "SidebarTab.h"
#include "Sidebar.h"

namespace vibedaw {

SidebarTab::SidebarTab(Sidebar& sidebar)
    : sidebar_(sidebar)
{
    setOpaque(true);
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

SidebarTab::~SidebarTab() = default;

void SidebarTab::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff333333));
    
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRect(getLocalBounds());
    
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);
    
    auto text = sidebar_.getName();
    auto bounds = getLocalBounds().reduced(4);
    
    g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                   static_cast<float>(getWidth() / 2),
                                                   static_cast<float>(getHeight() / 2)));
    
    auto rotatedBounds = juce::Rectangle<int>(-getHeight() / 2, -getWidth() / 2, getHeight(), getWidth());
    g.drawText(text, rotatedBounds.reduced(4), juce::Justification::centred, true);
}

void SidebarTab::mouseDown(const juce::MouseEvent&) {
    sidebar_.toggle();
}

} // namespace vibedaw
