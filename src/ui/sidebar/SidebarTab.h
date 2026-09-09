#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Sidebar.h"
#include "ui/components/IconButton.h"

namespace vibedaw {

class SidebarTab : public juce::Component,
                   public juce::SettableTooltipClient {
public:
    using Side = Sidebar::Side;

    explicit SidebarTab(Sidebar& sidebar);
    ~SidebarTab() override;

    Sidebar& sidebar() const { return sidebar_; }
    void activate();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    static constexpr int tabWidth = 28;

private:
    Sidebar& sidebar_;
    IconButton button_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidebarTab)
};

} // namespace vibedaw
