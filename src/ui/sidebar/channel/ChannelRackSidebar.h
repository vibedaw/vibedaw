#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Sidebar.h"
#include "project/ChannelList.h"
#include "project/Project.h"
#include "ui/DragPayload.h"
#include <vector>

namespace vibedaw {

class Project;
class Channel;

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
    juce::PopupMenu createContextMenu() const;
    // Delayed menu actions resolve stable IDs through the lifetime-checked rack.
    std::function<void()> openEditor;
    std::function<void()> selectAsActive;
    std::function<void()> renameRequested;
    std::function<void()> removeRequested;
    
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    void pollMeter(const StereoMeter& source);

    static constexpr int rowHeight = 40;

private:
    juce::Rectangle<int> kebabRect() const { return {getWidth() - 20, 2, 18, 18}; }
    juce::Rectangle<int> muteRect() const { return {getWidth() - 66, getHeight() - 17, 18, 14}; }
    juce::Rectangle<int> soloRect() const { return {getWidth() - 42, getHeight() - 17, 18, 14}; }
    juce::Rectangle<int> meterRect() const { return {6, getHeight() - 13, getWidth() - 96, 7}; }
    float meterLeft_ = 0.0f, meterRight_ = 0.0f;
    Channel* channel_;
    int index_;
    Listener* listener_ = nullptr;
    bool isSelected_ = false;
    bool isDragOver_ = false;
    DragDropInfo pendingDragInfo_;
};

class ChannelRackContent : public juce::Component,
                           public juce::DragAndDropTarget,
                           private Project::Listener,
                           public ChannelRow::Listener,
                           public ChannelList::Listener,
                           private juce::MultiTimer {
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

    // Context-menu actions; all resolve by stable ChannelId and no-op when the
    // channel is gone. Confirmation dialogs live in the menu bindings only.
    void selectChannelById(ChannelId id);
    void renameChannelById(ChannelId id, const juce::String& name);
    void removeChannelById(ChannelId id);
    void removeChannelWithConfirmation(ChannelId id);
    int countPlacementsToChannel(ChannelId id) const;
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDragEnter(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    
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
    bool isCreateDropPosition(juce::Point<int> position) const;
    void timerCallback(int timerId) override;
    void activeChannelChanged(int index) override { selectChannel(index); }
    Project& project_;
    Listener* channelListener_ = nullptr;
    
    std::vector<std::unique_ptr<ChannelRow>> channelRows_;
    juce::TextButton addChannelButton_;
    
    int selectedChannelIndex_ = -1;
    bool isDragOver_ = false;
    
    void rebuildChannelRows();
    void selectChannel(int index);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelRackContent)
};

Sidebar* createChannelRackSidebar(Project& project);

} // namespace vibedaw
