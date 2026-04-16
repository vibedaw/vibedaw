#include "PanelTitleBar.h"
#include "Panel.h"
#include "PanelContainer.h"

namespace vibedaw {

class PanelTitleBar::IconButton : public juce::Component {
public:
    IconButton(const juce::String& symbol) : symbol_(symbol) {
        setInterceptsMouseClicks(true, false);
    }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(2);
        
        if (isMouseOver()) {
            g.setColour(juce::Colour(0xff444444));
            g.fillRoundedRectangle(bounds, 3.0f);
        }
        
        g.setColour(juce::Colour(0xffaaaaaa));
        g.setFont(juce::Font(12.0f, juce::Font::plain));
        g.drawText(symbol_, getLocalBounds(), juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent&) override {
        if (onClick) onClick();
    }
    
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    
    std::function<void()> onClick;
    
private:
    juce::String symbol_;
};

PanelTitleBar::PanelTitleBar(Panel& owner)
    : owner_(owner)
{
    collapseBtn_ = std::make_unique<IconButton>("-");
    collapseBtn_->onClick = [this]() {
        owner_.setCollapsed(true);
    };
    addAndMakeVisible(*collapseBtn_);
    
    expandBtn_ = std::make_unique<IconButton>("+");
    expandBtn_->onClick = [this]() {
        owner_.setCollapsed(false);
    };
    addChildComponent(*expandBtn_);
    
    setInterceptsMouseClicks(true, true);
}

PanelTitleBar::~PanelTitleBar() = default;

void PanelTitleBar::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    g.fillAll(juce::Colour(0xff2a2a2a));
    
    g.setColour(juce::Colour(0xffcccccc));
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    
    auto textBounds = bounds.reduced(8, 0);
    textBounds.removeFromRight(60);
    g.drawText(owner_.getPanelName(), textBounds, juce::Justification::centredLeft);
    
    g.setColour(juce::Colour(0xff444444));
    g.drawLine(bounds.getX(), bounds.getBottom() - 1, bounds.getRight(), bounds.getBottom() - 1, 1.0f);
    
    updateButtonVisibility();
}

void PanelTitleBar::resized() {
    auto bounds = getLocalBounds();
    auto rightArea = bounds.removeFromRight(50).reduced(4, 3);
    
    collapseBtn_->setBounds(rightArea.removeFromLeft(22));
    expandBtn_->setBounds(rightArea.removeFromLeft(22));
}

void PanelTitleBar::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isRightButtonDown()) {
        showContextMenu();
    } else if (e.mods.isLeftButtonDown() && !collapseBtn_->getBounds().contains(e.getPosition()) 
               && !expandBtn_->getBounds().contains(e.getPosition())) {
        owner_.toggleCollapsed();
    }
}

void PanelTitleBar::showContextMenu() {
    auto menu = juce::PopupMenu();
    
    menu.addItem("Hide Panel", [this]() {
        owner_.setPanelVisible(false);
    });
    
    menu.addSeparator();
    
    auto displayMode = owner_.getDisplayMode();
    
    menu.addItem("Flex Mode", displayMode == DisplayMode::Flex, false, [this]() {
        owner_.setDisplayMode(DisplayMode::Flex);
    });
    
    menu.addItem("Floating Mode", displayMode == DisplayMode::Floating, false, [this]() {
        owner_.setDisplayMode(DisplayMode::Floating);
    });
    
    menu.addItem("Pop Out Window", displayMode == DisplayMode::PopOut, false, [this]() {
        owner_.setDisplayMode(DisplayMode::PopOut);
    });
    
    menu.addSeparator();
    
    if (owner_.isCollapsed()) {
        menu.addItem("Expand Panel", [this]() {
            owner_.setCollapsed(false);
        });
    } else {
        menu.addItem("Collapse Panel", [this]() {
            owner_.setCollapsed(true);
        });
    }
    
    menu.addSeparator();
    
    menu.addItem("Reset to Default Height", [this]() {
        owner_.setPreferredHeight(200);
        owner_.setCollapsed(false, false);
        if (auto* container = owner_.getParentContainer()) {
            container->resized();
        }
    });
    
    menu.showMenuAsync(juce::PopupMenu::Options());
}

void PanelTitleBar::updateButtonVisibility() {
    bool collapsed = owner_.isCollapsed();
    collapseBtn_->setVisible(!collapsed);
    expandBtn_->setVisible(collapsed);
}

} // namespace vibedaw
