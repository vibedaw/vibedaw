#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace vibedaw {

class Panel;

class PanelContainer : public juce::Component {
public:
    PanelContainer();
    ~PanelContainer() override;
    
    void addPanel(Panel* panel);
    void removePanel(Panel* panel);
    int getPanelCount() const;
    Panel* getPanel(int index) const;
    
    void setFocusedPanel(Panel* panel);
    Panel* getFocusedPanel() const;
    void focusPanelByIndex(int index);
    int getFocusedPanelIndex() const;
    
    void resizeFocusedPanel(int deltaHeight);
    void minimizeFocusedPanel();
    void restoreFocusedPanel();
    void maximizeFocusedPanel();
    
    void onPanelRestored(Panel* panel);
    void onPanelMaximized(Panel* panel);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    
private:
    juce::OwnedArray<Panel> panels_;
    Panel* focusedPanel_ = nullptr;
    Panel* maximizedPanel_ = nullptr;
    std::vector<int> splitterPositions_;
    int draggedSplitterIndex_ = -1;
    int splitterDragStartY_ = 0;
    int splitterDragStartHeights_[2] = {0, 0};
    
    int getSplitterHeight() const { return 4; }
    int getSplitterIndexAt(int y) const;
    bool isOverSplitter(int y) const;
    
    int getNumVisiblePanels() const;
    Panel* getVisiblePanel(int visibleIndex) const;
    
    void layoutPanels();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanelContainer)
};

} // namespace vibedaw
