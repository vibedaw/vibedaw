#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Sidebar.h"

namespace vibedaw {

class SidebarTab : public juce::Button {
public:
    using Side = Sidebar::Side;

    explicit SidebarTab(Sidebar& sidebar);
    ~SidebarTab() override;

    Sidebar& sidebar() const { return sidebar_; }
    void activate();
    // Starts (or restarts) one 300 ms accent fade, without activating the sidebar.
    void flash();
    bool isFlashing() const { return flashAmount_ > 0.0f; }

    void paintButton(juce::Graphics& g, bool over, bool down) override;

    static constexpr int tabWidth = 28;

private:
    friend struct SidebarTabTestAccess;

    void clicked() override;
    void advanceFlash(double now); // Monotonic milliseconds, matching flashStartTime_.

    Sidebar& sidebar_;
    static constexpr double flashDurationMs_ = 300.0;
    double flashStartTime_ = 0.0;
    float flashAmount_ = 0.0f;
    juce::VBlankAttachment frameUpdates_ { this, [this] {
        advanceFlash(juce::Time::getMillisecondCounterHiRes());
    } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SidebarTab)
};

} // namespace vibedaw
