#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

class IconButton : public juce::Component {
public:
    explicit IconButton(const juce::String& symbol = {});
    ~IconButton() override = default;
    
    void setSymbol(const juce::String& symbol);
    const juce::String& getSymbol() const { return symbol_; }
    
    std::function<void()> onClick;
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    
private:
    juce::String symbol_;
    bool isMouseOver_ = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IconButton)
};

} // namespace vibedaw
