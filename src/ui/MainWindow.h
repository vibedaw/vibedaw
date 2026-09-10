#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MainContent.h"

namespace vibedaw {

class Project;

class MainWindow : public juce::DocumentWindow {
public:
    MainWindow(juce::String name, juce::MidiKeyboardState& keyboardState,
               MidiManager& midiManager, Project& project, AudioEngine& engine);
    ~MainWindow() override;

    void closeButtonPressed() override;
    bool keyPressed(const juce::KeyPress& key) override;
    // Reflects the current project name and dirty state in the title (T07).
    void updateProjectTitle();

private:
    Project* project_ = nullptr;
    MainContent* mainContent_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

} // namespace vibedaw
