#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Sidebar.h"
#include "project/ClipPool.h"
#include <vector>

namespace vibedaw {

class Project;
class Clip;

class ClipRow : public juce::Component {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void clipSelected(ClipId clipId, Clip* clip) = 0;
        virtual void clipOpened(ClipId clipId, Clip* clip) = 0;
    };
    
    ClipRow(ClipId clipId, Clip* clip, int index);
    ~ClipRow() override = default;
    
    void setListener(Listener* listener) { listener_ = listener; }
    void setSelected(bool selected);
    bool isSelected() const { return isSelected_; }
    
    ClipId getClipId() const { return clipId_; }
    Clip* getClip() const { return clip_; }
    int getIndex() const { return index_; }
    
    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent&) override { pressActive_ = false; }
    juce::var getDragDescription() const;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    juce::PopupMenu createContextMenu() const;
    // Delayed menu actions resolve stable IDs through the lifetime-checked pool.
    std::function<void()> editSource;
    std::function<void()> renameRequested;
    std::function<void()> deleteSourceRequested;

    static constexpr int rowHeight = 56;

private:
    friend struct ClipRowTestAccess;
    juce::Rectangle<int> kebabRect() const { return {getWidth() - 22, 2, 20, 20}; }
    juce::var dragDescription_;
    bool pressActive_ = false, dragStarted_ = false;
    std::function<void(const juce::var&, bool)> dragStarter_;
    ClipId clipId_;
    Clip* clip_;
    int index_;
    Listener* listener_ = nullptr;
    bool isSelected_ = false;
};

class ClipsContent : public juce::Component,
                     public ClipRow::Listener,
                     public ClipPool::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void clipCreated(ClipId clipId, Clip* clip) = 0;
        virtual void clipSelected(ClipId clipId, Clip* clip) = 0;
        virtual void clipOpened(ClipId clipId, Clip* clip) = 0;
    };
    
    ClipsContent(Project& project);
    ~ClipsContent();
    
    void setClipsListener(Listener* listener) { clipsListener_ = listener; }

    void refreshClips();

    // Context-menu actions; all resolve by stable ClipId and no-op when the
    // source is gone. Confirmation dialogs live in the menu bindings only.
    void renameClipById(ClipId clipId, const juce::String& name);
    void deleteSourceById(ClipId clipId);
    void deleteSourceWithConfirmation(ClipId clipId);
    int countPlacementsOfClip(ClipId clipId) const;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

    void clipAdded(ClipId clipId, Clip* clip) override;
    void clipRemoved(ClipId clipId) override;
    void clipChanged(ClipId clipId, Clip* clip) override;
    
    void clipSelected(ClipId clipId, Clip* clip) override;
    void clipOpened(ClipId clipId, Clip* clip) override;
    
private:
    friend struct ClipsContentTestAccess;
    Project& project_;
    Listener* clipsListener_ = nullptr;
    
    std::vector<std::unique_ptr<ClipRow>> clipRows_;
    juce::TextButton addClipButton_;
    juce::TextButton deleteClipButton_{"Delete Source"};

    // MIDI/AUDIO filter tabs are painted with manual hit-testing (no child
    // components) so offline child-index contracts stay stable.
    int filterTab_ = 0; // 0 = MIDI (incl. Pattern), 1 = Audio.
    juce::Rectangle<int> midiTabBounds_;
    juce::Rectangle<int> audioTabBounds_;
    static constexpr int tabRowHeight = 26;

    ClipId selectedClipId_ = InvalidClipId;

    void rebuildClipRows();
    void selectClip(int index);
    bool rowMatchesFilter(const ClipRow& row) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipsContent)
};

Sidebar* createClipsSidebar(Project& project);

} // namespace vibedaw
