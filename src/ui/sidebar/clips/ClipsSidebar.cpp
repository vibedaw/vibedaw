#include "ClipsSidebar.h"
#include "project/Project.h"
#include "project/ClipPool.h"
#include "project/Clip.h"
#include "utils/Logger.h"

namespace vibedaw {

ClipRow::ClipRow(ClipId clipId, Clip* clip, int index)
    : clipId_(clipId), clip_(clip), index_(index)
{
    setInterceptsMouseClicks(true, false);
}

void ClipRow::setSelected(bool selected) {
    if (isSelected_ != selected) {
        isSelected_ = selected;
        repaint();
    }
}

void ClipRow::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    if (isSelected_) {
        g.fillAll(juce::Colour(0xff3a3a4a));
    } else {
        g.fillAll(juce::Colour(0xff2a2a2a));
    }
    
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);
    
    juce::String name = clip_ ? clip_->getName() : "Clip " + juce::String(index_ + 1);
    
    juce::String typeStr;
    if (clip_) {
        switch (clip_->getType()) {
            case Clip::Type::Audio: typeStr = "[Audio]"; break;
            case Clip::Type::Midi: typeStr = "[MIDI]"; break;
            case Clip::Type::Pattern: typeStr = "[Pattern]"; break;
        }
        name += " " + typeStr;
    }
    
    g.drawText(name, 8, 0, bounds.getWidth() - 16, bounds.getHeight(), 
               juce::Justification::centredLeft, true);
    
    g.setColour(juce::Colour(0xff444444));
    g.drawHorizontalLine(bounds.getHeight() - 1, 0.0f, static_cast<float>(bounds.getWidth()));
}

void ClipRow::mouseDown(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    if (listener_ && clip_) {
        listener_->clipSelected(clipId_, clip_);
    }
}

void ClipRow::mouseDoubleClick(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    if (listener_ && clip_) {
        listener_->clipOpened(clipId_, clip_);
    }
}

ClipsContent::ClipsContent(Project& project)
    : project_(project)
{
    addClipButton_.setButtonText("+ New Clip");
    addClipButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a5a3a));
    addClipButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addClipButton_.onClick = [this]() {
        auto newClip = std::make_unique<MidiClip>(0.0, 4.0);
        newClip->setName("Clip " + juce::String(project_.getClipPool().getNumClips() + 1));
        auto* clipPtr = newClip.get();
        auto clipId = project_.getClipPool().addClip(std::move(newClip));
        if (clipId == InvalidClipId) return;
        clipSelected(clipId, clipPtr);
        if (clipsListener_) {
            clipsListener_->clipCreated(clipId, clipPtr);
        }
    };
    addAndMakeVisible(addClipButton_);
    deleteClipButton_.setEnabled(false);
    deleteClipButton_.setTooltip("Delete the selected source. Its placements remain as unresolved placeholders.");
    deleteClipButton_.onClick = [this] { project_.getClipPool().removeClip(selectedClipId_); };
    addAndMakeVisible(deleteClipButton_);
    
    project_.getClipPool().addListener(this);
    rebuildClipRows();
}

ClipsContent::~ClipsContent() {
    project_.getClipPool().removeListener(this);
}

void ClipsContent::refreshClips() {
    rebuildClipRows();
}

void ClipsContent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff252525));
}

void ClipsContent::resized() {
    auto bounds = getLocalBounds();
    
    auto buttons = bounds.removeFromBottom(32);
    deleteClipButton_.setBounds(buttons.removeFromRight(buttons.getWidth() / 2).reduced(2));
    addClipButton_.setBounds(buttons.reduced(2));
    
    int y = 0;
    for (auto& row : clipRows_) {
        row->setBounds(0, y, bounds.getWidth(), ClipRow::rowHeight);
        y += ClipRow::rowHeight;
    }
}

void ClipsContent::clipAdded(ClipId clipId, Clip* clip) {
    rebuildClipRows();
}

void ClipsContent::clipRemoved(ClipId clipId) {
    if (selectedClipId_ == clipId) {
        selectedClipId_ = InvalidClipId;
        deleteClipButton_.setEnabled(false);
        if (clipsListener_) clipsListener_->clipSelected(InvalidClipId, nullptr);
    }
    rebuildClipRows();
}

void ClipsContent::clipChanged(ClipId clipId, Clip* clip) {
    juce::ignoreUnused(clipId, clip);
    for (auto& row : clipRows_) row->repaint();
}

void ClipsContent::clipSelected(ClipId clipId, Clip* clip) {
    if (clipsListener_) clipsListener_->clipSelected(clipId, clip);
    auto& clipPool = project_.getClipPool();
    const auto& clips = clipPool.getClips();
    
    for (int i = 0; i < static_cast<int>(clips.size()); ++i) {
        if (clips[i].first == clipId) {
            selectClip(i);
            break;
        }
    }
}

void ClipsContent::clipOpened(ClipId clipId, Clip* clip) {
    if (clipsListener_ && clip) {
        clipsListener_->clipOpened(clipId, clip);
    }
}

void ClipsContent::rebuildClipRows() {
    for (auto& row : clipRows_) {
        removeChildComponent(row.get());
    }
    clipRows_.clear();
    
    auto& clipPool = project_.getClipPool();
    const auto& clips = clipPool.getClips();
    
    for (int i = 0; i < static_cast<int>(clips.size()); ++i) {
        auto clipId = clips[i].first;
        auto* clip = clips[i].second.get();
        auto row = std::make_unique<ClipRow>(clipId, clip, i);
        row->setListener(this);
        row->setSelected(clipId == selectedClipId_);
        addAndMakeVisible(*row);
        clipRows_.push_back(std::move(row));
    }
    
    resized();
}

void ClipsContent::selectClip(int index) {
    const auto& clips = project_.getClipPool().getClips();
    selectedClipId_ = index >= 0 && index < static_cast<int>(clips.size()) ? clips[index].first : InvalidClipId;
    deleteClipButton_.setEnabled(selectedClipId_ != InvalidClipId);
    
    for (int i = 0; i < static_cast<int>(clipRows_.size()); ++i) {
        clipRows_[i]->setSelected(clipRows_[i]->getClipId() == selectedClipId_);
    }
}

Sidebar* createClipsSidebar(Project& project) {
    auto* sidebar = new Sidebar("Clips", Sidebar::Side::Right);
    sidebar->setMinWidth(150);
    sidebar->setMaxWidth(400);
    sidebar->setSidebarWidth(250);
    
    auto* content = new ClipsContent(project);
    sidebar->setContent(content);
    
    return sidebar;
}

} // namespace vibedaw
