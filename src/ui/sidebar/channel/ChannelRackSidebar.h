#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Sidebar.h"
#include <vector>

namespace vibedaw {

class Project;
class Track;

enum class DragSourceType {
    Plugin,
    Sample,
    Preset,
    Unknown
};

struct DragDropInfo {
    DragSourceType type = DragSourceType::Unknown;
    juce::String path;
    juce::String name;
    
    static DragDropInfo fromDragDescription(const juce::var& description);
};

class TrackRow : public juce::Component, public juce::DragAndDropTarget {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void trackSelected(Track* track) = 0;
        virtual void pluginDroppedOnTrack(Track* track, const juce::String& pluginPath) = 0;
        virtual void sampleDroppedOnTrack(Track* track, const juce::File& sampleFile) = 0;
    };
    
    TrackRow(Track* track, int index);
    ~TrackRow() override = default;
    
    void setListener(Listener* listener) { listener_ = listener; }
    void setSelected(bool selected);
    bool isSelected() const { return isSelected_; }
    
    Track* getTrack() const { return track_; }
    int getIndex() const { return index_; }
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;
    
    static constexpr int rowHeight = 28;
    
private:
    Track* track_;
    int index_;
    Listener* listener_ = nullptr;
    bool isSelected_ = false;
    bool isDragOver_ = false;
    DragDropInfo pendingDragInfo_;
};

class ChannelRackContent : public juce::Component,
                            public TrackRow::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void trackCreated(const juce::String& pluginPath) = 0;
        virtual void trackCreatedFromSample(const juce::File& sampleFile) = 0;
    };
    
    ChannelRackContent(Project& project);
    ~ChannelRackContent() override;
    
    void setChannelListener(Listener* listener) { channelListener_ = listener; }
    
    void refreshTracks();
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void trackSelected(Track* track) override;
    void pluginDroppedOnTrack(Track* track, const juce::String& pluginPath) override;
    void sampleDroppedOnTrack(Track* track, const juce::File& sampleFile) override;
    
private:
    Project& project_;
    Listener* channelListener_ = nullptr;
    
    std::vector<std::unique_ptr<TrackRow>> trackRows_;
    juce::TextButton addTrackButton_;
    
    int selectedTrackIndex_ = -1;
    
    void rebuildTrackRows();
    void selectTrack(int index);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelRackContent)
};

Sidebar* createChannelRackSidebar(Project& project);

} // namespace vibedaw
