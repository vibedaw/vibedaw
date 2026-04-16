#include "MainWindow.h"
#include "utils/Logger.h"

namespace vibedaw {

MainWindow::MainWindow(juce::String name, juce::MidiKeyboardState& keyboardState,
                       MidiManager& midiManager, Project& project)
    : DocumentWindow(name, juce::Colour(0xff2a2a2a), DocumentWindow::closeButton, true)
{
    auto content = std::make_unique<MainContent>(keyboardState, midiManager, project);
    mainContent_ = content.get();
    setContentOwned(content.release(), false);
    
    setResizable(true, true);
    setResizeLimits(600, 400, 2000, 1200);
    setUsingNativeTitleBar(true);
    
    centreWithSize(800, 500);
    setVisible(true);
    
    LOG_INFO("MainWindow: Created");
}

MainWindow::~MainWindow() {
    LOG_INFO("MainWindow: Destroyed");
}

void MainWindow::closeButtonPressed() {
    LOG_INFO("MainWindow: Close button pressed");
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

bool MainWindow::keyPressed(const juce::KeyPress& key) {
    if (mainContent_ && mainContent_->handleKeyPress(key)) {
        return true;
    }
    return false;
}

} // namespace vibedaw
