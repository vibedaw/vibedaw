#include "BrowserSection.h"
#include "ui/Theme.h"

namespace vibedaw {

BrowserSection::BrowserSection(const juce::String& name)
    : sectionName_(name)
{
    titleLabel_.setText(sectionName_.toUpperCase(), juce::dontSendNotification);
    titleLabel_.setColour(juce::Label::textColourId, theme::accent);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setFont(juce::Font(10.5f, juce::Font::bold));
    titleLabel_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(titleLabel_);
    
    toggleButton_.setEdgeIndent(8);
    toggleButton_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    toggleButton_.onClick = [this]() { toggleExpanded(); };
    addAndMakeVisible(toggleButton_);
}

void BrowserSection::setExpanded(bool expanded) {
    if (expanded_ == expanded) return;
    
    expanded_ = expanded;
    
    if (expanded_) {
        contentHeight_ = preferredHeight_;
        if (listener_) listener_->sectionExpanded(this);
    } else {
        preferredHeight_ = contentHeight_;
        if (listener_) listener_->sectionCollapsed(this);
    }
    
    resized();
    repaint();
}

void BrowserSection::toggleExpanded() {
    setExpanded(!expanded_);
}

void BrowserSection::setContentHeight(int height) {
    height = juce::jlimit(minHeight_, maxHeight_, height);
    if (contentHeight_ == height) return;
    
    contentHeight_ = height;
    if (expanded_) {
        preferredHeight_ = height;
    }
    resized();
}

void BrowserSection::setContentComponent(juce::Component* comp) {
    if (contentComponent_ != nullptr) {
        removeChildComponent(contentComponent_);
    }
    contentComponent_ = comp;
    if (contentComponent_ != nullptr) {
        addAndMakeVisible(contentComponent_);
    }
}

void BrowserSection::paint(juce::Graphics& g) {
    g.fillAll(theme::browserBackground);
    
    auto titleBounds = getLocalBounds().removeFromTop(titleBarHeight);
    g.setColour(theme::raised);
    g.fillRect(titleBounds);
    
    g.setColour(theme::hairline);
    g.drawHorizontalLine(titleBarHeight - 1, 6.0f, static_cast<float>(getWidth() - 6));
    
    if (expanded_) {
        paintContent(g, getLocalBounds().withTrimmedTop(titleBarHeight));
    }
}

void BrowserSection::resized() {
    auto bounds = getLocalBounds();
    auto titleBounds = bounds.removeFromTop(titleBarHeight);
    
    toggleButton_.setBounds(titleBounds.removeFromLeft(titleBarHeight));
    titleLabel_.setBounds(titleBounds.withTrimmedRight(6));
    juce::Path chevron;
    if (expanded_) {
        chevron.startNewSubPath(0.0f, 0.0f);
        chevron.lineTo(4.0f, 4.0f);
        chevron.lineTo(8.0f, 0.0f);
    } else {
        chevron.startNewSubPath(0.0f, 0.0f);
        chevron.lineTo(4.0f, 4.0f);
        chevron.lineTo(0.0f, 8.0f);
    }
    juce::DrawablePath icon;
    icon.setPath(chevron);
    icon.setFill(juce::Colours::transparentBlack);
    icon.setStrokeFill(theme::accent);
    icon.setStrokeThickness(1.4f);
    juce::DrawablePath hoverIcon(icon);
    hoverIcon.setStrokeFill(theme::accent);
    toggleButton_.setImages(&icon, &hoverIcon, &hoverIcon);
    toggleButton_.setTooltip(expanded_ ? "Collapse " + sectionName_ : "Expand " + sectionName_);
    
    if (expanded_ && contentComponent_ != nullptr) {
        contentComponent_->setBounds(bounds.withHeight(contentHeight_));
    }
    
    resizedContent(bounds);
}

void BrowserSection::mouseDown(const juce::MouseEvent& e) {
    if (e.y < titleBarHeight) {
        toggleExpanded();
    }
}

} // namespace vibedaw
