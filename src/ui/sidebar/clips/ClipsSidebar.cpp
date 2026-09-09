#include "ClipsSidebar.h"
#include "project/Project.h"
#include "project/ClipPool.h"
#include "project/Clip.h"
#include "utils/Logger.h"
#include "ui/DragPayload.h"
#include "ui/components/TextPrompt.h"

namespace vibedaw {

ClipRow::ClipRow(ClipId clipId, Clip* clip, int index)
    : clipId_(clipId), clip_(clip), index_(index)
{
    setInterceptsMouseClicks(true, false);
    if (clip_ && clip_->getType() == Clip::Type::Midi) dragDescription_ = DragDropInfo::clip(clipId_);
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
    DragDropInfo::cancelClip(dragDescription_);
    dragDescription_ = clip_ && clip_->getType() == Clip::Type::Midi ? DragDropInfo::clip(clipId_) : juce::var();
    pressActive_ = e.mods.isLeftButtonDown() && !e.mods.isPopupMenu();
    dragStarted_ = false;
    if (e.mods.isPopupMenu()) {
        createContextMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
        return;
    }
    if (listener_ && clip_) {
        listener_->clipSelected(clipId_, clip_);
    }
}

void ClipRow::mouseDoubleClick(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu() || e.mouseWasDraggedSinceMouseDown()) return;
    if (listener_ && clip_) {
        listener_->clipOpened(clipId_, clip_);
    }
}

juce::var ClipRow::getDragDescription() const {
    return dragDescription_;
}

void ClipRow::mouseDrag(const juce::MouseEvent& e) {
    if (!pressActive_ || dragStarted_ || !e.mods.isLeftButtonDown() || e.getDistanceFromDragStart() < 5) return;
    const auto payload = getDragDescription();
    if (payload.isVoid() || DragDropInfo::clipCancelled(payload)) return;
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
    if (!dragStarter_ && (!container || container->isDragAndDropActive())) return;
    // Escape can delete JUCE's drag image while this mouse press is still down.
    dragStarted_ = true;
    constexpr bool acrossWindows = true;
    if (dragStarter_) dragStarter_(payload, acrossWindows);
    else container->startDragging(payload, this, juce::ScaledImage(), acrossWindows);
}

juce::PopupMenu ClipRow::createContextMenu() const {
    juce::PopupMenu menu;
    menu.addItem("Edit in Piano Roll", clip_ && clip_->getType() == Clip::Type::Midi, false, editSource);
    menu.addItem(juce::String(juce::CharPointer_UTF8("Rename Clip\xe2\x80\xa6")), clip_ != nullptr, false, renameRequested);
    menu.addSeparator();
    menu.addItem(juce::String(juce::CharPointer_UTF8("Delete Source\xe2\x80\xa6")), clip_ != nullptr, false, deleteSourceRequested);
    return menu;
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
    // Same confirmed, impact-warned path as the row context menu (T14 parity).
    deleteClipButton_.setTooltip("Delete the selected source. Its placements remain as unresolved placeholders.");
    deleteClipButton_.onClick = [this] { deleteSourceWithConfirmation(selectedClipId_); };
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

int ClipsContent::countPlacementsOfClip(ClipId clipId) const {
    int count = 0;
    for (const auto& track : project_.getTrackList().getTracks())
        for (const auto& instance : track->getClipInstances())
            if (instance->getClipId() == clipId) ++count;
    return count;
}

void ClipsContent::renameClipById(ClipId clipId, const juce::String& name) {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty()) return;
    if (auto* clip = project_.getClipPool().getClip(clipId)) clip->setName(trimmed);
}

void ClipsContent::deleteSourceById(ClipId clipId) {
    project_.getClipPool().removeClip(clipId);
}

void ClipsContent::deleteSourceWithConfirmation(ClipId clipId) {
    auto* clip = project_.getClipPool().getClip(clipId);
    if (!clip) return;
    confirmAsync("Delete Source",
        "The MIDI source \"" + clip->getName() + "\" is used by " + juce::String(countPlacementsOfClip(clipId)) +
            " placement(s). Deleting it leaves those placements as unresolved placeholders (visible and silent), "
            "and any open editors close. Other clips are unaffected.",
        "Delete Source", this, [safe = juce::Component::SafePointer<ClipsContent>(this), clipId] {
            if (safe != nullptr) safe->deleteSourceById(clipId);
        });
}

void ClipsContent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff252525));
    if (clipRows_.empty()) {
        g.setColour(juce::Colour(0xff777777));
        g.setFont(12.0f);
        g.drawFittedText("Create a MIDI clip with + New Clip.\nDrag it onto the timeline to place it;\ndouble-click or right-click to edit.",
            getLocalBounds().reduced(8, 40), juce::Justification::centred, 4);
    }
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
    clip = project_.getClipPool().getClip(clipId);
    if (clip) clipSelected(clipId, clip);
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
        row->editSource = [safe = juce::Component::SafePointer<ClipsContent>(this), clipId] {
            if (safe != nullptr) safe->clipOpened(clipId, nullptr);
        };
        row->renameRequested = [safe = juce::Component::SafePointer<ClipsContent>(this), id = clipId] {
            if (safe == nullptr) return;
            // The source may vanish while the action is pending: no prompt, no mutation.
            auto* target = safe->project_.getClipPool().getClip(id);
            if (!target) return;
            showTextPrompt("Rename Clip", "New clip name:", target->getName(), safe.getComponent(),
                [safe, id](const juce::String& name) {
                    if (safe != nullptr) safe->renameClipById(id, name);
                });
        };
        row->deleteSourceRequested = [safe = juce::Component::SafePointer<ClipsContent>(this), id = clipId] {
            if (safe != nullptr) safe->deleteSourceWithConfirmation(id);
        };
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
    sidebar->setIconSymbol(juce::String(juce::CharPointer_UTF8("\xe2\x99\xaa")));
    sidebar->setMinWidth(150);
    sidebar->setMaxWidth(400);
    sidebar->setSidebarWidth(250);
    
    auto* content = new ClipsContent(project);
    sidebar->setContent(content);
    
    return sidebar;
}

} // namespace vibedaw
