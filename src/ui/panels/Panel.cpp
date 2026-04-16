#include "Panel.h"
#include "PanelTitleBar.h"
#include "PanelWindow.h"
#include "PanelContainer.h"

namespace vibedaw {

Panel::Panel(const juce::String& name)
    : panelName_(name)
{
    setOpaque(true);
    titleBar_ = std::make_unique<PanelTitleBar>(*this);
    addAndMakeVisible(*titleBar_);
}

Panel::~Panel() = default;

void Panel::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a1a));
    
    if (focused_) {
        g.setColour(juce::Colour(0xff5a8a9a));
        g.drawRect(getLocalBounds(), 1);
    }
}

void Panel::resized() {
    updateLayout();
}

juce::String Panel::getPanelName() const {
    return panelName_;
}

DisplayMode Panel::getDisplayMode() const {
    return displayMode_;
}

void Panel::setDisplayMode(DisplayMode mode) {
    if (displayMode_ == mode) return;
    
    auto oldMode = displayMode_;
    displayMode_ = mode;
    
    onDisplayModeChanged(mode, oldMode);
    
    switch (mode) {
        case DisplayMode::Flex:
            showInFlexMode();
            break;
        case DisplayMode::Floating:
            showInFloatingMode();
            break;
        case DisplayMode::PopOut:
            showInPopOutMode();
            break;
    }
}

bool Panel::isPanelVisible() const {
    return panelVisible_;
}

void Panel::setPanelVisible(bool visible) {
    if (panelVisible_ == visible) return;
    panelVisible_ = visible;
    
    onVisibilityChanged(visible);
    
    if (displayMode_ == DisplayMode::PopOut && !visible) {
        if (popOutWindow_) {
            popOutWindow_.reset();
        }
    }
    
    if (parentContainer_) {
        parentContainer_->resized();
    }
}

PanelState Panel::getPanelState() const {
    return panelState_;
}

bool Panel::isCollapsed() const {
    return panelState_ == PanelState::Collapsed;
}

void Panel::setCollapsed(bool collapsed, bool animate) {
    bool currentlyCollapsed = (panelState_ == PanelState::Collapsed);
    if (currentlyCollapsed == collapsed) return;
    
    PanelState oldState = panelState_;
    
    if (!animate) {
        panelState_ = collapsed ? PanelState::Collapsed : PanelState::Expanded;
        
        if (collapsed) {
            animatedHeight_ = 0;
        } else {
            animatedHeight_ = -1;
        }
        
        onPanelStateChanged(panelState_, oldState);
        updateLayout();
        
        if (parentContainer_) {
            parentContainer_->resized();
        }
        
        if (onStateChange) {
            onStateChange();
        }
        return;
    }
    
    panelState_ = collapsed ? PanelState::Collapsing : PanelState::Expanding;
    onPanelStateChanged(panelState_, oldState);
    
    animateToHeight(0);
}

void Panel::toggleCollapsed() {
    setCollapsed(!isCollapsed());
}

int Panel::getPreferredHeight() const {
    return getCurrentHeight();
}

int Panel::getCurrentHeight() const {
    if (panelState_ == PanelState::Collapsed) {
        return titleBarHeight_;
    }
    if (panelState_ == PanelState::Collapsing || panelState_ == PanelState::Expanding) {
        return animatedHeight_;
    }
    return preferredHeight_;
}

void Panel::setPreferredHeight(int height) {
    preferredHeight_ = juce::jlimit(minHeight_, maxHeight_, height);
    expandedHeight_ = preferredHeight_;
}

int Panel::getExpandedHeight() const {
    return expandedHeight_;
}

void Panel::setExpandedHeight(int height) {
    expandedHeight_ = height;
}

bool Panel::isFlexFill() const {
    return flexFill_;
}

void Panel::setFlexFill(bool fill) {
    flexFill_ = fill;
}

int Panel::getMinHeight() const {
    return minHeight_;
}

void Panel::setMinHeight(int height) {
    minHeight_ = height;
}

int Panel::getMaxHeight() const {
    return maxHeight_;
}

void Panel::setMaxHeight(int height) {
    maxHeight_ = height;
}

void Panel::setContentComponent(std::unique_ptr<juce::Component> content) {
    if (content_) {
        removeChildComponent(content_.get());
    }
    content_ = std::move(content);
    if (content_) {
        addAndMakeVisible(*content_);
    }
    updateLayout();
}

juce::Component* Panel::getContentComponent() const {
    return content_.get();
}

void Panel::setTitleBarHeight(int height) {
    titleBarHeight_ = height;
    updateLayout();
}

int Panel::getTitleBarHeight() const {
    return titleBarHeight_;
}

void Panel::onDisplayModeChanged(DisplayMode, DisplayMode) {}

void Panel::onPanelStateChanged(PanelState, PanelState) {}

void Panel::onVisibilityChanged(bool) {}

void Panel::setParentContainer(PanelContainer* container) {
    parentContainer_ = container;
}

PanelContainer* Panel::getParentContainer() const {
    return parentContainer_;
}

PanelTitleBar* Panel::getTitleBar() const {
    return titleBar_.get();
}

bool Panel::isFocused() const {
    return focused_;
}

void Panel::setFocused(bool focused) {
    if (focused_ == focused) return;
    focused_ = focused;
    repaint();
    if (titleBar_) {
        titleBar_->repaint();
    }
}

PanelWindowState Panel::getWindowState() const {
    return windowState_;
}

void Panel::setWindowState(PanelWindowState state) {
    if (windowState_ == state) return;
    
    if (state == PanelWindowState::Restored) {
        restore();
    } else if (state == PanelWindowState::Minimized) {
        minimize();
    } else if (state == PanelWindowState::Maximized) {
        maximize();
    }
}

void Panel::minimize() {
    if (windowState_ == PanelWindowState::Minimized) return;
    
    if (windowState_ == PanelWindowState::Restored) {
        restoredHeight_ = preferredHeight_;
    }
    
    windowState_ = PanelWindowState::Minimized;
    setCollapsed(true, false);
}

void Panel::restore() {
    if (windowState_ == PanelWindowState::Restored) return;
    
    windowState_ = PanelWindowState::Restored;
    setPreferredHeight(restoredHeight_);
    setCollapsed(false, false);
    
    if (parentContainer_) {
        parentContainer_->onPanelRestored(this);
    }
}

void Panel::maximize() {
    if (windowState_ == PanelWindowState::Maximized) return;
    
    if (windowState_ == PanelWindowState::Restored) {
        restoredHeight_ = preferredHeight_;
    }
    
    windowState_ = PanelWindowState::Maximized;
    setCollapsed(false, false);
    
    if (parentContainer_) {
        parentContainer_->onPanelMaximized(this);
    }
}

void Panel::updateLayout() {
    auto bounds = getLocalBounds();
    
    titleBar_->setBounds(bounds.removeFromTop(titleBarHeight_));
    
    if (panelState_ == PanelState::Collapsed) {
        if (content_) {
            content_->setVisible(false);
        }
        return;
    }
    
    int contentHeight = bounds.getHeight();
    if (panelState_ == PanelState::Collapsing || panelState_ == PanelState::Expanding) {
        contentHeight = juce::jmax(0, animatedHeight_ - titleBarHeight_);
    }
    
    if (contentHeight > 0 && content_) {
        content_->setVisible(true);
        content_->setBounds(bounds.removeFromTop(contentHeight));
    } else if (content_) {
        content_->setVisible(false);
    }
}

void Panel::showInFlexMode() {
    setVisible(true);
    if (popOutWindow_) {
        popOutWindow_.reset();
    }
}

void Panel::showInFloatingMode() {
    setVisible(true);
    if (popOutWindow_) {
        popOutWindow_.reset();
    }
}

void Panel::showInPopOutMode() {
    if (!popOutWindow_) {
        popOutWindow_ = std::make_unique<PanelWindow>(*this, panelName_);
        
        if (content_) {
            removeChildComponent(content_.get());
            popOutWindow_->setContentNonOwned(content_.get(), true);
        }
    }
    setVisible(false);
}

void Panel::hidePanel() {
    setVisible(false);
    if (popOutWindow_) {
        popOutWindow_.reset();
    }
}

void Panel::animateToHeight(int) {
    animationStartTime_ = juce::Time::getMillisecondCounterHiRes();
    
    if (!isTimerRunning()) {
        startTimerHz(60);
    }
}

void Panel::timerCallback() {
    double now = juce::Time::getMillisecondCounterHiRes();
    double elapsed = now - animationStartTime_;
    double progress = elapsed / animationDurationMs_;
    
    if (progress >= 1.0) {
        stopTimer();
        progress = 1.0;
    }
    
    float eased = static_cast<float>(progress);
    
    bool expanding = (panelState_ == PanelState::Expanding);
    int startHeight = expanding ? titleBarHeight_ : preferredHeight_;
    int endHeight = expanding ? preferredHeight_ : titleBarHeight_;
    int heightDelta = endHeight - startHeight;
    
    animatedHeight_ = startHeight + static_cast<int>(heightDelta * eased);
    
    updateLayout();
    
    if (parentContainer_) {
        parentContainer_->resized();
    }
    
    if (progress >= 1.0) {
        animatedHeight_ = endHeight;
        
        PanelState oldState = panelState_;
        panelState_ = expanding ? PanelState::Expanded : PanelState::Collapsed;
        
        onPanelStateChanged(panelState_, oldState);
        
        updateLayout();
        
        if (parentContainer_) {
            parentContainer_->resized();
        }
        
        if (onStateChange) {
            onStateChange();
        }
    }
}

} // namespace vibedaw
