#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/components/IconButton.h"

namespace vibedaw {

class Sidebar : public juce::Component {
public:
    enum class Side { Left, Right };
    
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void sidebarToggled(Sidebar* sidebar, bool expanded) = 0;
        virtual void sidebarResized(Sidebar* sidebar, int newWidth) = 0;
    };
    
    Sidebar(const juce::String& name, Side side = Side::Left);
    ~Sidebar() override;
    
    juce::String getName() const { return name_; }
    int getId() const { return id_; }
    
    bool isExpanded() const { return expanded_; }
    bool isCollapsed() const { return !expanded_; }
    void setExpanded(bool expanded);
    void toggle();
    
    int getSidebarWidth() const { return width_; }
    void setSidebarWidth(int width);
    int getMinWidth() const { return minWidth_; }
    int getMaxWidth() const { return maxWidth_; }
    void setMinWidth(int min) { minWidth_ = min; }
    void setMaxWidth(int max) { maxWidth_ = max; }
    
    void setContent(juce::Component* content);
    juce::Component* getContent() const { return content_.get(); }
    
    Side getSide() const { return side_; }
    
    void setListener(Listener* listener) { listener_ = listener; }
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    
    static constexpr int defaultWidth = 250;
    static constexpr int defaultMinWidth = 150;
    static constexpr int defaultMaxWidth = 500;
    static constexpr int collapseTabWidth = 28;
    static constexpr int resizeEdgeWidth = 4;
    
private:
    juce::String name_;
    int id_;
    Side side_;
    bool expanded_ = true;
    int width_ = defaultWidth;
    int minWidth_ = defaultMinWidth;
    int maxWidth_ = defaultMaxWidth;
    int preferredWidth_ = defaultWidth;
    
    std::unique_ptr<juce::Component> content_;
    std::unique_ptr<juce::Component> titleBar_;
    juce::Label titleLabel_;
    IconButton collapseButton_;
    
    bool isDraggingResize_ = false;
    int dragStartX_ = 0;
    int dragStartWidth_ = 0;
    bool isOverResizeEdge_ = false;
    
    Listener* listener_ = nullptr;
    
    static int nextId_;
    
    bool isOverResizeEdge(int x) const;
    int getResizeEdgeX() const;
    void updateMouseCursor();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Sidebar)
};

} // namespace vibedaw
