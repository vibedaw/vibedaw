#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

namespace vibedaw {

class DawWindow : public juce::DocumentWindow, private juce::Timer {
public:
    DawWindow(const juce::String& title, int requiredButtons, bool addToDesktop = true);
    ~DawWindow() override;

    void maximiseButtonPressed() override;
    void resized() override;
    void lookAndFeelChanged() override;
    void mouseDrag(const juce::MouseEvent&) override;
    int getDesktopWindowStyleFlags() const override;

protected:
    struct MaximizedState {
        bool horizontal = false;
        bool vertical = false;
        bool matches(bool maximized) const {
            return maximized ? horizontal && vertical : !horizontal && !vertical;
        }
    };
    virtual std::optional<MaximizedState> readNativeMaximizedState();
    virtual bool requestNativeMaximizedState(bool maximized);

private:
    friend struct DawWindowTestAccess;
    struct NativeState;
    std::unique_ptr<NativeState> native_;
    struct ResizeBorder;
    std::unique_ptr<ResizeBorder> resizeBorder_;
    MaximizedState observed_;
    bool desired_ = false;
    std::optional<bool> inFlight_;
    double requestStarted_ = 0.0;

    void timerCallback() override;
    bool pollNativeState(double now);
    void sendMaximizeRequest(double now, bool force = false);
    void updateWindowControls();
    bool clientBoundsLocked() const { return inFlight_.has_value() || observed_.horizontal || observed_.vertical; }
};

} // namespace vibedaw
