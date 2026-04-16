#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/components/IconButton.h"

namespace vibedaw {

class BrowserSection : public juce::Component {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void sectionExpanded(BrowserSection* section) = 0;
        virtual void sectionCollapsed(BrowserSection* section) = 0;
    };
    
    BrowserSection(const juce::String& name);
    ~BrowserSection() override = default;
    
    const juce::String& getSectionName() const { return sectionName_; }
    
    bool isExpanded() const { return expanded_; }
    void setExpanded(bool expanded);
    void toggleExpanded();
    
    int getContentHeight() const { return contentHeight_; }
    void setContentHeight(int height);
    
    int getPreferredHeight() const { return preferredHeight_; }
    void setPreferredHeight(int height) { preferredHeight_ = height; }
    
    int getMinHeight() const { return minHeight_; }
    void setMinHeight(int min) { minHeight_ = min; }
    
    int getMaxHeight() const { return maxHeight_; }
    void setMaxHeight(int max) { maxHeight_ = max; }
    
    void setListener(Listener* listener) { listener_ = listener; }
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    
    static constexpr int titleBarHeight = 24;
    static constexpr int defaultContentHeight = 150;
    
protected:
    virtual void paintContent(juce::Graphics& g, juce::Rectangle<int> bounds) = 0;
    virtual void resizedContent(juce::Rectangle<int> bounds) = 0;
    
    juce::Component* getContentComponent() { return contentComponent_; }
    void setContentComponent(juce::Component* comp);
    
private:
    juce::String sectionName_;
    bool expanded_ = true;
    int contentHeight_ = defaultContentHeight;
    int preferredHeight_ = defaultContentHeight;
    int minHeight_ = 50;
    int maxHeight_ = 400;
    
    juce::Component* contentComponent_ = nullptr;
    juce::Label titleLabel_;
    IconButton toggleButton_;
    
    Listener* listener_ = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowserSection)
};

} // namespace vibedaw
