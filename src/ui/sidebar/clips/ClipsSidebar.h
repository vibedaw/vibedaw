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
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    
    static constexpr int rowHeight = 28;
    
private:
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
        virtual void clipOpened(ClipId clipId, Clip* clip) = 0;
    };
    
    ClipsContent(Project& project);
    ~ClipsContent();
    
    void setClipsListener(Listener* listener) { clipsListener_ = listener; }
    
    void refreshClips();
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void clipAdded(ClipId clipId, Clip* clip) override;
    void clipRemoved(ClipId clipId) override;
    void clipChanged(ClipId clipId, Clip* clip) override;
    
    void clipSelected(ClipId clipId, Clip* clip) override;
    void clipOpened(ClipId clipId, Clip* clip) override;
    
private:
    Project& project_;
    Listener* clipsListener_ = nullptr;
    
    std::vector<std::unique_ptr<ClipRow>> clipRows_;
    juce::TextButton addClipButton_;
    
    int selectedClipIndex_ = -1;
    
    void rebuildClipRows();
    void selectClip(int index);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipsContent)
};

Sidebar* createClipsSidebar(Project& project);

} // namespace vibedaw