#include "BrowserSection.h"

namespace vibedaw {

BrowserSection::BrowserSection(const juce::String& name)
    : sectionName_(name)
{
    titleLabel_.setText(sectionName_, juce::dontSendNotification);
    titleLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setFont(juce::Font(12.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel_);
    
    toggleButton_.setSymbol("-");
    toggleButton_.onClick = [this]() { toggleExpanded(); };
    addAndMakeVisible(toggleButton_);
}

void BrowserSection::setExpanded(bool expanded) {
    if (expanded_ == expanded) return;
    
    expanded_ = expanded;
    toggleButton_.setSymbol(expanded_ ? "-" : "+");
    
    if (expanded_) {
        contentHeight_ = preferredHeight_;
        if (listener_) listener_->sectionExpanded(this);
    } else {
        preferredHeight_ = contentHeight_;
        if (listener_) listener_->sectionCollapsed(this);
    }
    
    resized();
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
    g.fillAll(juce::Colour(0xff2a2a2a));
    
    auto titleBounds = getLocalBounds().removeFromTop(titleBarHeight);
    g.setColour(juce::Colour(0xff333333));
    g.fillRect(titleBounds);
    
    g.setColour(juce::Colour(0xff444444));
    g.drawHorizontalLine(titleBarHeight, 0.0f, static_cast<float>(getWidth()));
    
    if (expanded_) {
        paintContent(g, getLocalBounds().withTrimmedTop(titleBarHeight));
    }
}

void BrowserSection::resized() {
    auto bounds = getLocalBounds();
    auto titleBounds = bounds.removeFromTop(titleBarHeight);
    
    toggleButton_.setBounds(titleBounds.removeFromRight(titleBarHeight));
    titleLabel_.setBounds(titleBounds.reduced(4, 0));
    
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
