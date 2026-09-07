#include "ChannelRackSidebar.h"
#include "project/Project.h"
#include "project/ChannelList.h"
#include "project/Channel.h"
#include "plugins/PluginHost.h"
#include "utils/Logger.h"

namespace vibedaw {

DragDropInfo DragDropInfo::fromDragDescription(const juce::var& description) {
    DragDropInfo info;
    
    if (!description.isString()) {
        return info;
    }
    
    juce::String desc = description.toString();
    
    if (desc.startsWith("sample://")) {
        info.type = DragSourceType::Sample;
        info.path = desc.fromFirstOccurrenceOf("sample://", false, false);
        info.name = juce::File(info.path).getFileNameWithoutExtension();
    } else if (desc.startsWith("preset://")) {
        info.type = DragSourceType::Preset;
        info.path = desc.fromFirstOccurrenceOf("preset://", false, false);
        info.name = juce::File(info.path).getFileNameWithoutExtension();
    } else if (desc.isNotEmpty()) {
        info.type = DragSourceType::Plugin;
        info.path = desc;
        info.name = juce::File(desc).getFileNameWithoutExtension();
    }
    
    return info;
}

ChannelRow::ChannelRow(Channel* channel, int index)
    : channel_(channel), index_(index)
{
    setInterceptsMouseClicks(true, false);
}

void ChannelRow::setSelected(bool selected) {
    if (isSelected_ != selected) {
        isSelected_ = selected;
        repaint();
    }
}

void ChannelRow::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    if (isDragOver_) {
        g.fillAll(juce::Colour(0xff3a5a3a));
    } else if (isSelected_) {
        g.fillAll(juce::Colour(0xff3a3a4a));
    } else {
        g.fillAll(juce::Colour(0xff2a2a2a));
    }
    
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);
    
    juce::String name = channel_ ? channel_->getName() : "Channel " + juce::String(index_ + 1);
    
    if (channel_ && channel_->hasPlugin()) {
        name += " [" + channel_->getPlugin()->getPluginName() + "]";
    }
    
    g.drawText(name, 8, 0, bounds.getWidth() - 16, bounds.getHeight(), 
               juce::Justification::centredLeft, true);
    
    if (isDragOver_) {
        g.setColour(juce::Colour(0xff00ff00));
        g.drawRect(bounds, 2);
    }
    
    g.setColour(juce::Colour(0xff444444));
    g.drawHorizontalLine(bounds.getHeight() - 1, 0.0f, static_cast<float>(bounds.getWidth()));
}

void ChannelRow::mouseDown(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    if (listener_ && channel_) {
        listener_->channelSelected(channel_);
    }
}

void ChannelRow::mouseUp(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
}

bool ChannelRow::isInterestedInDragSource(const SourceDetails& dragSourceDetails) {
    auto info = DragDropInfo::fromDragDescription(dragSourceDetails.description);
    return info.type != DragSourceType::Unknown;
}

void ChannelRow::itemDragEnter(const SourceDetails& dragSourceDetails) {
    isDragOver_ = true;
    pendingDragInfo_ = DragDropInfo::fromDragDescription(dragSourceDetails.description);
    repaint();
}

void ChannelRow::itemDragExit(const SourceDetails&) {
    isDragOver_ = false;
    pendingDragInfo_ = DragDropInfo();
    repaint();
}

void ChannelRow::itemDropped(const SourceDetails& dragSourceDetails) {
    isDragOver_ = false;
    repaint();
    
    auto info = DragDropInfo::fromDragDescription(dragSourceDetails.description);
    
    if (!listener_ || !channel_) {
        return;
    }
    
    switch (info.type) {
        case DragSourceType::Plugin:
            listener_->pluginDroppedOnChannel(channel_, info.path);
            break;
        case DragSourceType::Sample:
            listener_->sampleDroppedOnChannel(channel_, juce::File(info.path));
            break;
        default:
            break;
    }
}

ChannelRackContent::ChannelRackContent(Project& project)
    : project_(project)
{
    addChannelButton_.setButtonText("+ Add Channel");
    addChannelButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a5a3a));
    addChannelButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addChannelButton_.onClick = [this]() {
        auto* channel = project_.getChannelList().addChannel();
        rebuildChannelRows();
    };
    addAndMakeVisible(addChannelButton_);
    
    project_.getChannelList().addListener(this);
    rebuildChannelRows();
}

ChannelRackContent::~ChannelRackContent() {
    project_.getChannelList().removeListener(this);
}

void ChannelRackContent::refreshChannels() {
    rebuildChannelRows();
}

void ChannelRackContent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff252525));
}

void ChannelRackContent::resized() {
    auto bounds = getLocalBounds();
    
    addChannelButton_.setBounds(bounds.removeFromBottom(32).reduced(4));
    
    int y = 0;
    for (auto& row : channelRows_) {
        row->setBounds(0, y, bounds.getWidth(), ChannelRow::rowHeight);
        y += ChannelRow::rowHeight;
    }
}

void ChannelRackContent::channelAdded(Channel* channel) {
    rebuildChannelRows();
}

void ChannelRackContent::channelRemoved(int index) {
    rebuildChannelRows();
}

void ChannelRackContent::channelChanged(Channel* channel) {
    rebuildChannelRows();
}

void ChannelRackContent::channelListChanged() {
    rebuildChannelRows();
}

void ChannelRackContent::channelSelected(Channel* channel) {
    int index = project_.getChannelList().indexOfChannel(channel);
    selectChannel(index);
    project_.setActiveChannel(index);
}

void ChannelRackContent::pluginDroppedOnChannel(Channel* channel, const juce::String& pluginPath) {
    LOG_INFO("ChannelRack: Loading plugin '" + pluginPath + "' on channel");
    
    auto pluginHost = std::make_unique<PluginHost>();
    if (pluginHost->loadPlugin(pluginPath)) {
        channel->setPlugin(std::move(pluginHost));
        rebuildChannelRows();
    } else {
        LOG_ERROR("ChannelRack: Failed to load plugin: " + pluginPath);
    }
}

void ChannelRackContent::sampleDroppedOnChannel(Channel* channel, const juce::File& sampleFile) {
    LOG_INFO("ChannelRack: Sample '" + sampleFile.getFileName() + "' dropped on channel");
    channel->setSampleFile(sampleFile);
    rebuildChannelRows();
}

void ChannelRackContent::rebuildChannelRows() {
    for (auto& row : channelRows_) {
        removeChildComponent(row.get());
    }
    channelRows_.clear();
    
    auto& channelList = project_.getChannelList();
    int numChannels = channelList.getNumChannels();
    
    for (int i = 0; i < numChannels; ++i) {
        auto* channel = channelList.getChannel(i);
        auto row = std::make_unique<ChannelRow>(channel, i);
        row->setListener(this);
        row->setSelected(i == selectedChannelIndex_);
        addAndMakeVisible(*row);
        channelRows_.push_back(std::move(row));
    }
    
    resized();
}

void ChannelRackContent::selectChannel(int index) {
    if (selectedChannelIndex_ == index) {
        return;
    }
    
    selectedChannelIndex_ = index;
    
    for (int i = 0; i < static_cast<int>(channelRows_.size()); ++i) {
        channelRows_[i]->setSelected(i == selectedChannelIndex_);
    }
}

Sidebar* createChannelRackSidebar(Project& project) {
    auto* sidebar = new Sidebar("Channel Rack", Sidebar::Side::Left);
    sidebar->setMinWidth(150);
    sidebar->setMaxWidth(400);
    sidebar->setSidebarWidth(250);
    
    auto* content = new ChannelRackContent(project);
    sidebar->setContent(content);
    
    return sidebar;
}

} // namespace vibedaw