#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Sidebar.h"
#include "project/ChannelList.h"
#include "project/Project.h"
#include <vector>

namespace vibedaw {

class Project;
class Channel;

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

class ChannelRow : public juce::Component, public juce::DragAndDropTarget {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void channelSelected(Channel* channel) = 0;
        virtual void pluginDroppedOnChannel(Channel* channel, const juce::String& pluginPath) = 0;
        virtual void sampleDroppedOnChannel(Channel* channel, const juce::File& sampleFile) = 0;
    };
    
    ChannelRow(Channel* channel, int index);
    ~ChannelRow() override = default;
    
    void setListener(Listener* listener) { listener_ = listener; }
    void setSelected(bool selected);
    bool isSelected() const { return isSelected_; }
    
    Channel* getChannel() const { return channel_; }
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
    Channel* channel_;
    int index_;
    Listener* listener_ = nullptr;
    bool isSelected_ = false;
    bool isDragOver_ = false;
    DragDropInfo pendingDragInfo_;
};

class ChannelRackContent : public juce::Component,
                           private Project::Listener,
                           public ChannelRow::Listener,
                           public ChannelList::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void channelCreated(const juce::String& pluginPath) = 0;
        virtual void channelCreatedFromSample(const juce::File& sampleFile) = 0;
    };
    
    ChannelRackContent(Project& project);
    ~ChannelRackContent();
    
    void setChannelListener(Listener* listener) { channelListener_ = listener; }
    
    void refreshChannels();
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void channelAdded(Channel* channel) override;
    void channelRemoved(int index) override;
    void channelChanged(Channel* channel) override;
    void channelListChanged() override;
    
    void channelSelected(Channel* channel) override;
    void pluginDroppedOnChannel(Channel* channel, const juce::String& pluginPath) override;
    void sampleDroppedOnChannel(Channel* channel, const juce::File& sampleFile) override;
    
private:
    void activeChannelChanged(int index) override { selectChannel(index); }
    Project& project_;
    Listener* channelListener_ = nullptr;
    
    std::vector<std::unique_ptr<ChannelRow>> channelRows_;
    juce::TextButton addChannelButton_;
    
    int selectedChannelIndex_ = -1;
    
    void rebuildChannelRows();
    void selectChannel(int index);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelRackContent)
};

Sidebar* createChannelRackSidebar(Project& project);

} // namespace vibedaw
