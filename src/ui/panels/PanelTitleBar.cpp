#include "PanelTitleBar.h"
#include "ui/Theme.h"
#include "Panel.h"
#include "PanelContainer.h"

namespace vibedaw {

PanelTitleBar::PanelTitleBar(Panel& owner)
    : owner_(owner)
{
    setOpaque(false);
    collapseBtn_ = std::make_unique<IconButton>("-");
    collapseBtn_->setTooltip("Collapse panel");
    collapseBtn_->onClick = [this]() {
        owner_.setCollapsed(true);
    };
    addAndMakeVisible(*collapseBtn_);
    
    expandBtn_ = std::make_unique<IconButton>("+");
    expandBtn_->setTooltip("Expand panel");
    expandBtn_->onClick = [this]() {
        owner_.setCollapsed(false);
    };
    addChildComponent(*expandBtn_);
    
    setInterceptsMouseClicks(true, true);
}

PanelTitleBar::~PanelTitleBar() = default;

void PanelTitleBar::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    theme::drawSurface(g, bounds.toFloat(),
                       owner_.isFocused() ? theme::titleBarActive : theme::raised,
                       theme::panelRadius - 1.0f, theme::transparent);
    
    g.setColour(theme::textBright);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    
    auto textBounds = bounds.reduced(8, 0);
    textBounds.removeFromRight(28);
    g.drawText(owner_.getPanelName(), textBounds, juce::Justification::centredLeft);
    
    if (!owner_.isCollapsed()) {
        g.setColour(theme::hairline);
        g.drawHorizontalLine(bounds.getBottom() - 1, 5.0f, juce::jmax(5.0f, bounds.getWidth() - 5.0f));
    }
    
    updateButtonVisibility();
}

void PanelTitleBar::resized() {
    auto bounds = getLocalBounds();
    auto buttonBounds = bounds.removeFromRight(26).reduced(2, 1);
    collapseBtn_->setBounds(buttonBounds);
    expandBtn_->setBounds(buttonBounds);
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
