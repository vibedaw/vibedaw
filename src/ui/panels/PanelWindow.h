#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

class Panel;

class PanelWindow : public juce::DocumentWindow {
public:
    PanelWindow(Panel& owner, const juce::String& title);
    ~PanelWindow() override;
    
    void closeButtonPressed() override;
    
private:
    Panel& owner_;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanelWindow)
};

} // namespace vibedaw
