#include "PanelWindow.h"
#include "Panel.h"

namespace vibedaw {

PanelWindow::PanelWindow(Panel& owner, const juce::String& title)
    : DocumentWindow(title, juce::Colours::darkgrey, DocumentWindow::closeButton, true),
      owner_(owner)
{
    setResizable(true, true);
    setUsingNativeTitleBar(true);
    setSize(600, 400);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
}

PanelWindow::~PanelWindow() = default;

void PanelWindow::closeButtonPressed() {
    owner_.setDisplayMode(DisplayMode::Flex);
    owner_.setPanelVisible(false);
}

} // namespace vibedaw
