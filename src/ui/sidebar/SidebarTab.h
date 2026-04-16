#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Sidebar.h"

namespace vibedaw {

class SidebarTab : public juce::Component {
public:
    using Side = Sidebar::Side;
    
    SidebarTab(Sidebar& sidebar);
    ~SidebarTab() override;
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    
    static constexpr int tabWidth = 28;
    
private:
    Sidebar& sidebar_;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidebarTab)
};

} // namespace vibedaw
