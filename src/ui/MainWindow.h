#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MainContent.h"

namespace vibedaw {

class MainWindow : public juce::DocumentWindow {
public:
    MainWindow(juce::String name, juce::MidiKeyboardState& keyboardState, 
               MidiManager& midiManager, Project& project);
    ~MainWindow() override;
    
    void closeButtonPressed() override;
    bool keyPressed(const juce::KeyPress& key) override;
    
private:
    MainContent* mainContent_ = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

} // namespace vibedaw
