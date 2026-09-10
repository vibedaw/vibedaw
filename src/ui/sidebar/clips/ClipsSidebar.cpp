#include "ClipsSidebar.h"
#include "ui/Theme.h"
#include "ui/components/ClipMiniPreview.h"
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
    auto bounds = getLocalBounds().toFloat().reduced(3.0f, 3.0f);
    theme::drawSurface(g, bounds, isSelected_ ? theme::selectedSurface
        : (isMouseOver() ? theme::controlHover : theme::control), theme::panelRadius,
        isSelected_ ? theme::accent.withAlpha(0.65f) : theme::border);
    if (isSelected_) {
        g.setColour(theme::accent);
        g.fillRoundedRectangle(bounds.getX() + 1.0f, bounds.getY() + 8.0f,
                               2.0f, bounds.getHeight() - 16.0f, 1.0f);
    }

    auto textArea = bounds.reduced(8.0f, 6.0f).toNearestInt();
    // Reserve the menu column so the thumbnail can never paint over its hit area.
    textArea.setRight(kebabRect().getX() - 4);
    auto previewArea = textArea.removeFromRight(juce::jlimit(28, 64, textArea.getWidth() / 3));
    textArea.removeFromRight(8);
    auto titleArea = textArea.removeFromTop(16);

    juce::String name = clip_ ? clip_->getName() : "Clip " + juce::String(index_ + 1);
    if (clip_ && clip_->isMuted()) name += " [Muted]";
    g.setColour(clip_ && clip_->isMuted() ? theme::textSecondary : theme::textBright);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(name, titleArea, juce::Justification::centredLeft, true);

    juce::String metadata = "Missing source";
    if (clip_) {
        if (auto* midi = dynamic_cast<MidiClip*>(clip_)) {
            const double bars = clip_->getDuration() / 4.0;
            metadata = juce::String(bars, bars == std::floor(bars) ? 0 : 1) + (bars == 1.0 ? " bar - " : " bars - ") +
                       juce::String(midi->getNumNotes()) + (midi->getNumNotes() == 1 ? " note" : " notes");
        } else if (clip_->getType() == Clip::Type::Audio) {
            metadata = "Audio";
        } else {
            metadata = "Pattern";
        }
    }
    g.setColour(theme::textSecondary);
    g.setFont(10.0f);
    g.drawText(metadata, textArea, juce::Justification::centredLeft, true);

    g.setColour(theme::textSecondary);
    const auto menuCentre = kebabRect().getCentre().toFloat();
    for (float offset : {-4.0f, 0.0f, 4.0f})
        g.fillEllipse(menuCentre.x - 1.0f, menuCentre.y + offset - 1.0f, 2.0f, 2.0f);

    theme::drawWell(g, previewArea.toFloat(), theme::controlRadius);
    if (auto* midi = dynamic_cast<MidiClip*>(clip_)) {
        const auto grid = previewArea.toFloat().reduced(3.0f);
        g.setColour(theme::hairline.withAlpha(0.6f));
        for (int i = 1; i < 4; ++i)
            g.drawVerticalLine(juce::roundToInt(grid.getX() + grid.getWidth() * i / 4.0f),
                               grid.getY(), grid.getBottom());
        ClipMiniPreview::drawMiniNotes(g, *midi, midi->getDuration(),
                                       grid, clip_->isMuted());
    }
}

void ClipRow::mouseDown(const juce::MouseEvent& e) {
    if (kebabRect().contains(e.position.toInt())) {
        createContextMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
        return;
    }
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
    addClipButton_.setColour(juce::TextButton::buttonColourId, theme::actionGreen);
    addClipButton_.setColour(juce::TextButton::textColourOffId, theme::accent);
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
    deleteClipButton_.setColour(juce::TextButton::buttonColourId, theme::control);
    deleteClipButton_.setColour(juce::TextButton::textColourOffId, theme::textSecondary);
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

bool ClipsContent::rowMatchesFilter(const ClipRow& row) const {
    auto* clip = row.getClip();
    if (filterTab_ == 1) return clip && clip->getType() == Clip::Type::Audio;
    return clip && (clip->getType() == Clip::Type::Midi || clip->getType() == Clip::Type::Pattern);
}

void ClipsContent::paint(juce::Graphics& g) {
    g.fillAll(theme::raised);

    // The segmented well and its manual hit rectangles share the layout in resized().
    theme::drawWell(g, getLocalBounds().withHeight(tabRowHeight).reduced(2, 2).toFloat(),
                    theme::controlRadius);
    auto drawTab = [&](const juce::Rectangle<int>& bounds, const juce::String& label, bool active) {
        auto rect = bounds.toFloat();
        if (active)
            theme::drawSurface(g, rect, theme::selectedSurface, theme::controlRadius,
                               theme::accent.withAlpha(0.3f));
        g.setColour(active ? theme::accent : theme::textSecondary);
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawText(label, bounds, juce::Justification::centred);
    };
    drawTab(midiTabBounds_, "MIDI", filterTab_ == 0);
    drawTab(audioTabBounds_, "AUDIO", filterTab_ == 1);

    auto footer = getLocalBounds().withTrimmedTop(juce::jmax(0, getHeight() - 32));
    g.setColour(theme::panelBackground);
    g.fillRect(footer);
    g.setColour(theme::hairline);
    g.drawHorizontalLine(footer.getY(), 0.0f, static_cast<float>(getWidth()));

    int visibleRows = 0;
    for (const auto& row : clipRows_) {
        if (row->isVisible()) ++visibleRows;
    }
    if (visibleRows == 0) {
        g.setColour(theme::textMuted);
        g.setFont(12.0f);
        auto area = getLocalBounds().withTrimmedTop(tabRowHeight).reduced(8, 40);
        if (clipRows_.empty()) {
            g.drawFittedText("Create a MIDI clip with + New Clip.\nDrag it onto the timeline to place it;\ndouble-click or right-click to edit.",
                area, juce::Justification::centred, 4);
        } else {
            g.drawFittedText("No clips in this tab.", area, juce::Justification::centred, 2);
        }
    }
}

void ClipsContent::mouseDown(const juce::MouseEvent& e) {
    const auto position = e.position.toInt();
    if (midiTabBounds_.contains(position)) {
        if (filterTab_ != 0) {
            filterTab_ = 0;
            for (auto& row : clipRows_) row->setVisible(rowMatchesFilter(*row));
            resized();
        }
    } else if (audioTabBounds_.contains(position)) {
        if (filterTab_ != 1) {
            filterTab_ = 1;
            for (auto& row : clipRows_) row->setVisible(rowMatchesFilter(*row));
            resized();
        }
    }
}

void ClipsContent::resized() {
    auto bounds = getLocalBounds();
    auto tabs = bounds.withHeight(tabRowHeight).reduced(2, 2);
    midiTabBounds_ = tabs.removeFromLeft(tabs.getWidth() / 2).reduced(2, 1);
    audioTabBounds_ = tabs.reduced(2, 1);

    auto buttons = bounds.removeFromBottom(32);
    deleteClipButton_.setBounds(buttons.removeFromRight(buttons.getWidth() / 2).reduced(3, 4));
    addClipButton_.setBounds(buttons.reduced(3, 4));

    auto listArea = bounds.withTrimmedTop(tabRowHeight);
    int y = listArea.getY();
    for (auto& row : clipRows_) {
        row->setVisible(rowMatchesFilter(*row));
        if (!row->isVisible()) continue;
        row->setBounds(0, y, bounds.getWidth(), ClipRow::rowHeight);
        y += ClipRow::rowHeight;
    }
    repaint();
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
