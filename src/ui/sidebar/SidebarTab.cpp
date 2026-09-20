#include "SidebarTab.h"
#include "ui/Theme.h"

namespace vibedaw {

SidebarTab::SidebarTab(Sidebar& sidebar)
    : juce::Button(sidebar.getName()), sidebar_(sidebar)
{
    setOpaque(false);
    setInterceptsMouseClicks(true, false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(true);

    setTooltip("Restore " + sidebar_.getName());

    setSize(tabWidth, tabWidth);
}

SidebarTab::~SidebarTab() = default;

void SidebarTab::activate() {
    // Toggling can synchronously destroy this tab; do not access members afterwards.
    sidebar_.toggle();
}

void SidebarTab::clicked() {
    activate();
}

void SidebarTab::flash() {
    flashStartTime_ = juce::Time::getMillisecondCounterHiRes();
    flashAmount_ = 1.0f;
    repaint();
}

void SidebarTab::advanceFlash(double now) {
    if (!isFlashing()) return;

    const auto progress = juce::jlimit(0.0, 1.0, (now - flashStartTime_) / flashDurationMs_);
    flashAmount_ = static_cast<float>(1.0 - progress);
    repaint();
}

void SidebarTab::paintButton(juce::Graphics& g, bool over, bool down) {
    const bool focused = hasKeyboardFocus(true);
    const auto background = down ? theme::controlPressed
                                : (over ? theme::controlHover : theme::raised);
    const auto outline = focused ? theme::accent : (over ? theme::borderStrong : theme::border);
    const auto foreground = over || focused ? theme::textBright : theme::textDefault;
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    theme::drawSurface(g, bounds,
                       background.interpolatedWith(theme::accent, flashAmount_ * 0.3f),
                       theme::controlRadius, outline.interpolatedWith(theme::accent, flashAmount_));
    Icons::draw(g, sidebar_.getIcon(),
                foreground.interpolatedWith(theme::accent, flashAmount_)
                    .withMultipliedAlpha(isEnabled() ? 1.0f : 0.45f),
                bounds.withSizeKeepingCentre(20.0f, 20.0f));
}

} // namespace vibedaw
