#include "ChannelRackSidebar.h"
#include "project/Project.h"
#include "project/TrackList.h"
#include "project/Track.h"
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

TrackRow::TrackRow(Track* track, int index)
    : track_(track), index_(index)
{
    setInterceptsMouseClicks(true, false);
}

void TrackRow::setSelected(bool selected) {
    if (isSelected_ != selected) {
        isSelected_ = selected;
        repaint();
    }
}

void TrackRow::paint(juce::Graphics& g) {
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
    
    juce::String name = track_ ? track_->getName() : "Track " + juce::String(index_ + 1);
    
    if (track_ && track_->hasPlugin()) {
        name += " [" + track_->getPlugin()->getPluginName() + "]";
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

void TrackRow::mouseDown(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    if (listener_ && track_) {
        listener_->trackSelected(track_);
    }
}

void TrackRow::mouseUp(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
}

bool TrackRow::isInterestedInDragSource(const SourceDetails& dragSourceDetails) {
    auto info = DragDropInfo::fromDragDescription(dragSourceDetails.description);
    return info.type != DragSourceType::Unknown;
}

void TrackRow::itemDragEnter(const SourceDetails& dragSourceDetails) {
    isDragOver_ = true;
    pendingDragInfo_ = DragDropInfo::fromDragDescription(dragSourceDetails.description);
    repaint();
}

void TrackRow::itemDragExit(const SourceDetails&) {
    isDragOver_ = false;
    pendingDragInfo_ = DragDropInfo();
    repaint();
}

void TrackRow::itemDropped(const SourceDetails& dragSourceDetails) {
    isDragOver_ = false;
    repaint();
    
    auto info = DragDropInfo::fromDragDescription(dragSourceDetails.description);
    
    if (!listener_ || !track_) {
        return;
    }
    
    switch (info.type) {
        case DragSourceType::Plugin:
            listener_->pluginDroppedOnTrack(track_, info.path);
            break;
        case DragSourceType::Sample:
            listener_->sampleDroppedOnTrack(track_, juce::File(info.path));
            break;
        default:
            break;
    }
}

ChannelRackContent::ChannelRackContent(Project& project)
    : project_(project)
{
    addTrackButton_.setButtonText("+ Add Track");
    addTrackButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a5a3a));
    addTrackButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addTrackButton_.onClick = [this]() {
        project_.getTrackList().addTrack();
        rebuildTrackRows();
    };
    addAndMakeVisible(addTrackButton_);
    
    rebuildTrackRows();
}

ChannelRackContent::~ChannelRackContent() = default;

void ChannelRackContent::refreshTracks() {
    rebuildTrackRows();
}

void ChannelRackContent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff252525));
}

void ChannelRackContent::resized() {
    auto bounds = getLocalBounds();
    
    addTrackButton_.setBounds(bounds.removeFromBottom(32).reduced(4));
    
    int y = 0;
    for (auto& row : trackRows_) {
        row->setBounds(0, y, bounds.getWidth(), TrackRow::rowHeight);
        y += TrackRow::rowHeight;
    }
}

void ChannelRackContent::trackSelected(Track* track) {
    int index = -1;
    auto& tracks = project_.getTrackList().getTracks();
    for (int i = 0; i < static_cast<int>(tracks.size()); ++i) {
        if (tracks[i].get() == track) {
            index = i;
            break;
        }
    }
    selectTrack(index);
}

void ChannelRackContent::pluginDroppedOnTrack(Track* track, const juce::String& pluginPath) {
    LOG_INFO("ChannelRack: Loading plugin '" + pluginPath + "' on track");
    
    auto pluginHost = std::make_unique<PluginHost>();
    if (pluginHost->loadPlugin(pluginPath)) {
        track->setPlugin(std::move(pluginHost));
        rebuildTrackRows();
    } else {
        LOG_ERROR("ChannelRack: Failed to load plugin: " + pluginPath);
    }
}

void ChannelRackContent::sampleDroppedOnTrack(Track* track, const juce::File& sampleFile) {
    LOG_INFO("ChannelRack: Sample '" + sampleFile.getFileName() + "' dropped on track");
    
    if (channelListener_) {
        channelListener_->trackCreatedFromSample(sampleFile);
    }
    
    rebuildTrackRows();
}

void ChannelRackContent::rebuildTrackRows() {
    for (auto& row : trackRows_) {
        removeChildComponent(row.get());
    }
    trackRows_.clear();
    
    auto& trackList = project_.getTrackList();
    int numTracks = trackList.getNumTracks();
    
    for (int i = 0; i < numTracks; ++i) {
        auto* track = trackList.getTrack(i);
        auto row = std::make_unique<TrackRow>(track, i);
        row->setListener(this);
        row->setSelected(i == selectedTrackIndex_);
        addAndMakeVisible(*row);
        trackRows_.push_back(std::move(row));
    }
    
    resized();
}

void ChannelRackContent::selectTrack(int index) {
    if (selectedTrackIndex_ == index) {
        return;
    }
    
    selectedTrackIndex_ = index;
    
    for (int i = 0; i < static_cast<int>(trackRows_.size()); ++i) {
        trackRows_[i]->setSelected(i == selectedTrackIndex_);
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
