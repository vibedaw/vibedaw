#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

class Panel;

class PanelTitleBar : public juce::Component {
public:
    PanelTitleBar(Panel& owner);
    ~PanelTitleBar() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    
private:
    class IconButton;
    
    Panel& owner_;
    
    std::unique_ptr<IconButton> collapseBtn_;
    std::unique_ptr<IconButton> expandBtn_;
    
    void showContextMenu();
    void updateButtonVisibility();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanelTitleBar)
};

} // namespace vibedaw
