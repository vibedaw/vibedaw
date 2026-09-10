#include "Sidebar.h"
#include "ui/Theme.h"
#include "ui/components/IconButton.h"

namespace vibedaw {

int Sidebar::nextId_ = 0;

Sidebar::Sidebar(const juce::String& name, Side side)
    : name_(name), side_(side)
{
    id_ = nextId_++;
    
    setOpaque(false);
    setInterceptsMouseClicks(true, true);
    
    titleLabel_.setText(name_, juce::dontSendNotification);
    titleLabel_.setColour(juce::Label::textColourId, theme::textBright);
    titleLabel_.setFont(juce::Font(12.0f, juce::Font::bold));
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
    auto frame = getLocalBounds().toFloat().reduced(2.0f, 0.0f);
    theme::drawSurface(g, frame, theme::panelBackground, theme::panelRadius);
    theme::drawSurface(g, frame.reduced(1.0f).withHeight(26.0f), theme::raised,
                       theme::panelRadius - 1.0f, theme::transparent);
    g.setColour(theme::hairline);
    g.drawHorizontalLine(27, 8.0f, juce::jmax(8.0f, getWidth() - 8.0f));
}

void Sidebar::paintOverChildren(juce::Graphics& g) {
    if (!isOverResizeEdge_ && !isDraggingResize_) return;
    g.setColour(theme::accent.withAlpha(0.65f));
    const float x = static_cast<float>(getResizeEdgeX() + resizeEdgeWidth / 2);
    g.drawVerticalLine(juce::roundToInt(x), 6.0f, juce::jmax(6.0f, getHeight() - 6.0f));
}

void Sidebar::resized() {
    auto bounds = getLocalBounds().reduced(6, 0);
    
    int titleBarHeight = 28;
    auto titleBarBounds = bounds.removeFromTop(titleBarHeight);
    
    collapseButton_.setBounds(titleBarBounds.removeFromRight(22).reduced(0, 3));
    titleLabel_.setBounds(titleBarBounds.reduced(2, 0));
    
    if (content_) {
        content_->setBounds(bounds.withTrimmedBottom(4));
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
