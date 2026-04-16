#include "PanelContainer.h"
#include "Panel.h"

namespace vibedaw {

PanelContainer::PanelContainer() {
    setInterceptsMouseClicks(true, true);
    setWantsKeyboardFocus(true);
}

PanelContainer::~PanelContainer() = default;

void PanelContainer::addPanel(Panel* panel) {
    if (panel == nullptr) return;
    panel->setParentContainer(this);
    panels_.add(panel);
    addAndMakeVisible(panel);
    layoutPanels();
}

void PanelContainer::removePanel(Panel* panel) {
    if (panel == nullptr) return;
    panel->setParentContainer(nullptr);
    removeChildComponent(panel);
    panels_.removeObject(panel);
    layoutPanels();
}

int PanelContainer::getPanelCount() const {
    return panels_.size();
}

Panel* PanelContainer::getPanel(int index) const {
    if (index >= 0 && index < panels_.size()) {
        return panels_[index];
    }
    return nullptr;
}

void PanelContainer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff0a0a0a));
    
    auto visiblePanels = getNumVisiblePanels();
    
    for (int i = 0; i < visiblePanels - 1; ++i) {
        auto* panel = getVisiblePanel(i);
        if (panel == nullptr) continue;
        
        int splitterY = panel->getBottom();
        
        if (i == draggedSplitterIndex_) {
            g.setColour(juce::Colour(0xff555555));
        } else {
            g.setColour(juce::Colour(0xff333333));
        }
        
        g.fillRect(0, splitterY, getWidth(), getSplitterHeight());
    }
}

void PanelContainer::resized() {
    layoutPanels();
}

void PanelContainer::mouseDown(const juce::MouseEvent& e) {
    draggedSplitterIndex_ = getSplitterIndexAt(e.y);
    
    if (draggedSplitterIndex_ >= 0) {
        splitterDragStartY_ = e.y;
        
        if (auto* upperPanel = getVisiblePanel(draggedSplitterIndex_)) {
            splitterDragStartHeights_[0] = upperPanel->getHeight();
        }
        if (auto* lowerPanel = getVisiblePanel(draggedSplitterIndex_ + 1)) {
            splitterDragStartHeights_[1] = lowerPanel->getHeight();
        }
    }
}

void PanelContainer::mouseDrag(const juce::MouseEvent& e) {
    if (draggedSplitterIndex_ < 0) return;
    
    int deltaY = e.y - splitterDragStartY_;
    
    auto* upperPanel = getVisiblePanel(draggedSplitterIndex_);
    auto* lowerPanel = getVisiblePanel(draggedSplitterIndex_ + 1);
    
    if (upperPanel == nullptr || lowerPanel == nullptr) return;
    
    if (upperPanel->isCollapsed() || lowerPanel->isCollapsed()) {
        return;
    }
    
    int newUpperHeight = splitterDragStartHeights_[0] + deltaY;
    
    int upperMin = upperPanel->getTitleBarHeight();
    int upperMax = upperPanel->isFlexFill() ? splitterDragStartHeights_[0] + splitterDragStartHeights_[1] - lowerPanel->getMinHeight() : upperPanel->getMaxHeight();
    
    newUpperHeight = juce::jlimit(upperMin, upperMax, newUpperHeight);
    
    if (upperPanel->isFlexFill()) {
        int totalAvailable = splitterDragStartHeights_[0] + splitterDragStartHeights_[1];
        newUpperHeight = juce::jlimit(upperMin, totalAvailable - lowerPanel->getTitleBarHeight(), newUpperHeight);
        upperPanel->setExpandedHeight(newUpperHeight);
    } else {
        upperPanel->setPreferredHeight(newUpperHeight);
        upperPanel->setExpandedHeight(newUpperHeight);
    }
    
    layoutPanels();
    repaint();
}

void PanelContainer::mouseUp(const juce::MouseEvent&) {
    draggedSplitterIndex_ = -1;
    repaint();
}

int PanelContainer::getSplitterIndexAt(int y) const {
    int currentY = 0;
    auto visiblePanels = getNumVisiblePanels();
    
    for (int i = 0; i < visiblePanels - 1; ++i) {
        auto* panel = getVisiblePanel(i);
        if (panel == nullptr) continue;
        
        currentY += panel->getHeight();
        
        if (y >= currentY && y <= currentY + getSplitterHeight()) {
            return i;
        }
        
        currentY += getSplitterHeight();
    }
    
    return -1;
}

bool PanelContainer::isOverSplitter(int y) const {
    return getSplitterIndexAt(y) >= 0;
}

int PanelContainer::getNumVisiblePanels() const {
    int count = 0;
    for (auto* panel : panels_) {
        if (panel->isPanelVisible() && panel->getDisplayMode() == DisplayMode::Flex) {
            count++;
        }
    }
    return count;
}

Panel* PanelContainer::getVisiblePanel(int visibleIndex) const {
    int currentIndex = 0;
    for (auto* panel : panels_) {
        if (panel->isPanelVisible() && panel->getDisplayMode() == DisplayMode::Flex) {
            if (currentIndex == visibleIndex) {
                return panel;
            }
            currentIndex++;
        }
    }
    return nullptr;
}

void PanelContainer::layoutPanels() {
    auto bounds = getLocalBounds();
    int totalHeight = bounds.getHeight();
    
    int visibleCount = getNumVisiblePanels();
    
    if (visibleCount == 0) return;
    
    int totalSplitterHeight = (visibleCount - 1) * getSplitterHeight();
    int availableHeight = totalHeight - totalSplitterHeight;
    
    int flexFillCount = 0;
    int fixedHeight = 0;
    
    for (int i = 0; i < visibleCount; ++i) {
        auto* panel = getVisiblePanel(i);
        if (panel == nullptr) continue;
        
        if (panel->isFlexFill() && !panel->isCollapsed()) {
            flexFillCount++;
        } else {
            fixedHeight += panel->getCurrentHeight();
        }
    }
    
    int flexFillHeight = 0;
    if (flexFillCount > 0) {
        int remainingHeight = availableHeight - fixedHeight;
        flexFillHeight = remainingHeight / flexFillCount;
    }
    
    int currentY = 0;
    int visibleIndex = 0;
    
    for (auto* panel : panels_) {
        if (!panel->isPanelVisible() || panel->getDisplayMode() != DisplayMode::Flex) {
            panel->setVisible(false);
            continue;
        }
        
        panel->setVisible(true);
        
        int panelHeight;
        if (panel->isCollapsed()) {
            panelHeight = panel->getTitleBarHeight();
        } else if (panel->isFlexFill()) {
            panelHeight = flexFillHeight;
        } else {
            panelHeight = panel->getCurrentHeight();
        }
        
        panel->setBounds(0, currentY, bounds.getWidth(), panelHeight);
        currentY += panelHeight;
        
        if (visibleIndex < visibleCount - 1) {
            currentY += getSplitterHeight();
        }
        
        visibleIndex++;
    }
}

} // namespace vibedaw
