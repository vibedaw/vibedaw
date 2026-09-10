#include "MainWindow.h"
#include "core/Constants.h"
#include "utils/Logger.h"

namespace vibedaw {

MainWindow::MainWindow(juce::String name, juce::MidiKeyboardState& keyboardState,
                       MidiManager& midiManager, Project& project)
    : DocumentWindow(name, juce::Colour(0xff2a2a2a), DocumentWindow::closeButton, true),
      project_(&project)
{
    auto content = std::make_unique<MainContent>(keyboardState, midiManager, project);
    mainContent_ = content.get();
    setContentOwned(content.release(), false);
    
    setResizable(true, true);
    setResizeLimits(600, 400, 10000, 10000);
    setUsingNativeTitleBar(true);
    
    auto& displays = juce::Desktop::getInstance().getDisplays();
    auto displayArea = displays.getDisplayForPoint(displays.getPrimaryDisplay()->userArea.getCentre())->userArea;
    int width = static_cast<int>(displayArea.getWidth() * 0.8f);
    int height = static_cast<int>(displayArea.getHeight() * 0.8f);
    centreWithSize(width, height);
    setVisible(true);
    updateProjectTitle();

    LOG_INFO("MainWindow: Created");
}

void MainWindow::updateProjectTitle() {
    if (project_ == nullptr) return;
    const auto project = project_->getProjectName() +
        (project_->isDirty() ? juce::String(" *") : juce::String());
    setName(project + " - " + juce::String(Constants::APP_NAME));
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
