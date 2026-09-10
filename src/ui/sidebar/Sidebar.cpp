#include "Sidebar.h"
#include "ui/Theme.h"
#include "ui/components/IconButton.h"

namespace vibedaw {

int Sidebar::nextId_ = 0;

Sidebar::Sidebar(const juce::String& name, Side side)
    : name_(name), side_(side)
{
    id_ = nextId_++;
    
    setOpaque(true);
    setInterceptsMouseClicks(true, true);
    
    titleLabel_.setText(name_, juce::dontSendNotification);
    titleLabel_.setColour(juce::Label::textColourId, theme::white);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);
    
    collapseButton_.setSymbol("x");
    collapseButton_.onClick = [this]() { toggle(); };
    addAndMakeVisible(collapseButton_);
    
    setSize(width_, 100);
}

Sidebar::~Sidebar() = default;

void Sidebar::setExpanded(bool expanded) {
    if (expanded_ == expanded) return;
    
    if (expanded) {
        width_ = preferredWidth_;
    } else {
        preferredWidth_ = width_;
    }
    
    expanded_ = expanded;
    
    if (listener_) {
        listener_->sidebarToggled(this, expanded_);
    }
}

void Sidebar::toggle() {
    setExpanded(!expanded_);
}

void Sidebar::setSidebarWidth(int width) {
    width = juce::jlimit(minWidth_, maxWidth_, width);
    if (width_ == width) return;
    
    width_ = width;
    if (expanded_) {
        preferredWidth_ = width;
    }
    
    if (listener_) {
        listener_->sidebarResized(this, width_);
    }
    
    resized();
}

void Sidebar::setContent(juce::Component* content) {
    content_.reset(content);
    if (content_) {
        addAndMakeVisible(content_.get());
    }
    resized();
}

void Sidebar::paint(juce::Graphics& g) {
    g.fillAll(theme::raised);
    
    g.setColour(theme::hairline);
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
    
    if (isOverResizeEdge_) {
        g.setColour(theme::controlSelected);
    } else {
        g.setColour(theme::borderStrong);
    }
    
    int resizeX = getResizeEdgeX();
    g.fillRect(resizeX, 0, resizeEdgeWidth, getHeight());
}

void Sidebar::paintOverChildren(juce::Graphics& g) {
    g.setColour(theme::controlSelected);
    if (side_ == Side::Left) {
        g.fillRect(getWidth() - 1, 0, 1, getHeight());
    } else {
        g.fillRect(0, 0, 1, getHeight());
    }
}

void Sidebar::resized() {
    auto bounds = getLocalBounds();
    
    int titleBarHeight = 28;
    auto titleBarBounds = bounds.removeFromTop(titleBarHeight);
    
    collapseButton_.setBounds(titleBarBounds.removeFromRight(22).reduced(1));
    titleLabel_.setBounds(titleBarBounds.reduced(8, 0));
    
    if (content_) {
        content_->setBounds(bounds);
    }
}

void Sidebar::mouseDown(const juce::MouseEvent& e) {
    if (isOverResizeEdge(e.x)) {
        isDraggingResize_ = true;
        dragStartX_ = e.x;
        dragStartWidth_ = width_;
    }
}

void Sidebar::mouseDrag(const juce::MouseEvent& e) {
    if (!isDraggingResize_) return;
    
    int deltaX = e.x - dragStartX_;
    int newWidth;
    
    if (side_ == Side::Left) {
        newWidth = dragStartWidth_ + deltaX;
    } else {
        newWidth = dragStartWidth_ - deltaX;
    }
    
    setSidebarWidth(newWidth);
}

void Sidebar::mouseUp(const juce::MouseEvent&) {
    isDraggingResize_ = false;
}

void Sidebar::mouseMove(const juce::MouseEvent& e) {
    bool wasOver = isOverResizeEdge_;
    isOverResizeEdge_ = isOverResizeEdge(e.x);
    
    if (wasOver != isOverResizeEdge_) {
        updateMouseCursor();
        repaint();
    }
}

void Sidebar::mouseExit(const juce::MouseEvent&) {
    isOverResizeEdge_ = false;
    updateMouseCursor();
    repaint();
}

bool Sidebar::isOverResizeEdge(int x) const {
    int resizeX = getResizeEdgeX();
    return x >= resizeX && x < resizeX + resizeEdgeWidth;
}

int Sidebar::getResizeEdgeX() const {
    if (side_ == Side::Left) {
        return getWidth() - resizeEdgeWidth;
    } else {
        return 0;
    }
}

void Sidebar::updateMouseCursor() {
    if (isOverResizeEdge_ || isDraggingResize_) {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    } else {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

} // namespace vibedaw
