#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

enum class DisplayMode {
    Flex,
    Floating,
    PopOut
};

enum class PanelState {
    Expanded,
    Collapsed,
    Collapsing,
    Expanding
};

enum class PanelWindowState {
    Minimized,
    Restored,
    Maximized
};

class PanelTitleBar;
class PanelWindow;
class PanelContainer;

class Panel : public juce::Component, private juce::Timer {
public:
    Panel(const juce::String& name);
    ~Panel() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    juce::String getPanelName() const;
    
    DisplayMode getDisplayMode() const;
    void setDisplayMode(DisplayMode mode);
    
    bool isPanelVisible() const;
    void setPanelVisible(bool visible);
    
    PanelState getPanelState() const;
    bool isCollapsed() const;
    void setCollapsed(bool collapsed, bool animate = true);
    void toggleCollapsed();
    
    int getPreferredHeight() const;
    void setPreferredHeight(int height);
    
    int getExpandedHeight() const;
    void setExpandedHeight(int height);
    
    int getCurrentHeight() const;
    
    bool isFlexFill() const;
    void setFlexFill(bool fill);
    
    int getMinHeight() const;
    void setMinHeight(int height);
    
    int getMaxHeight() const;
    void setMaxHeight(int height);
    
    void setContentComponent(std::unique_ptr<juce::Component> content);
    juce::Component* getContentComponent() const;
    
    void setTitleBarHeight(int height);
    int getTitleBarHeight() const;
    
    virtual void onDisplayModeChanged(DisplayMode newMode, DisplayMode oldMode);
    virtual void onPanelStateChanged(PanelState newState, PanelState oldState);
    virtual void onVisibilityChanged(bool visible);
    
    void setParentContainer(PanelContainer* container);
    PanelContainer* getParentContainer() const;
    
    bool isFocused() const;
    void setFocused(bool focused);
    
    PanelWindowState getWindowState() const;
    void setWindowState(PanelWindowState state);
    void minimize();
    void restore();
    void maximize();
    
    std::function<void()> onStateChange;
    
protected:
    PanelTitleBar* getTitleBar() const;
    virtual void resizeContent(juce::Rectangle<int> bounds);
    
private:
    juce::String panelName_;
    DisplayMode displayMode_ = DisplayMode::Flex;
    PanelState panelState_ = PanelState::Expanded;
    bool panelVisible_ = true;
    int preferredHeight_ = 200;
    int expandedHeight_ = 200;
    bool flexFill_ = false;
    int minHeight_ = 50;
    int maxHeight_ = 800;
    int titleBarHeight_ = 24;
    
    PanelContainer* parentContainer_ = nullptr;
    bool focused_ = false;
    PanelWindowState windowState_ = PanelWindowState::Restored;
    int restoredHeight_ = 200;
    
    std::unique_ptr<PanelTitleBar> titleBar_;
    std::unique_ptr<juce::Component> content_;
    std::unique_ptr<PanelWindow> popOutWindow_;
    
    int animatedHeight_ = -1;
    int targetHeight_ = -1;
    double animationStartTime_ = 0.0;
    int animationDurationMs_ = 200;
    
    void timerCallback() override;
    void updateLayout();
    void showInFlexMode();
    void showInFloatingMode();
    void showInPopOutMode();
    void hidePanel();
    void animateToHeight(int targetHeight);
    
    friend class PanelTitleBar;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Panel)
};

} // namespace vibedaw
