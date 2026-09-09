#include "SidebarTab.h"
#include "Sidebar.h"

namespace vibedaw {

SidebarTab::SidebarTab(Sidebar& sidebar)
    : sidebar_(sidebar),
      button_(sidebar_.getIconSymbol().isNotEmpty()
                  ? sidebar_.getIconSymbol()
                  : sidebar_.getName().substring(0, 1))
{
    setOpaque(true);
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);

    button_.onClick = [this]() { activate(); };
    button_.setTooltip(sidebar_.getName());
    setTooltip(sidebar_.getName());
    addAndMakeVisible(button_);

    setSize(tabWidth, tabWidth);
}

SidebarTab::~SidebarTab() = default;

void SidebarTab::activate() {
    sidebar_.toggle();
}

void SidebarTab::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff333333));

    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRect(getLocalBounds());
}

void SidebarTab::resized() {
    button_.setBounds(getLocalBounds());
}

void SidebarTab::mouseDown(const juce::MouseEvent&) {
    activate();
}

} // namespace vibedaw
