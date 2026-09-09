#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Settings.h"
#include "Track.h"
#include "TrackList.h"
#include "Channel.h"
#include "ChannelList.h"
#include "ClipPool.h"
#include "core/AudioEngine.h"
#include "core/MidiManager.h"
#include "core/TransportState.h"

namespace vibedaw {

class ChannelMixer;

class Project : private ChannelList::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void activeChannelChanged(int newActiveIndex) = 0;
    };
    
    Project();
    ~Project();
    
    bool initialise(AudioEngine& audioEngine, MidiManager& midiManager);
    void shutdown();
    
    void setActiveChannel(int index);
    int getActiveChannel() const { return channelList.indexOfChannel(channelList.getChannelById(activeChannelId_)); }
    ChannelId getActiveChannelId() const { return activeChannelId_; }
    
    void addListener(Listener* listener);
    void removeListener(Listener* listener);
    
    TrackList& getTrackList() { return trackList; }
    const TrackList& getTrackList() const { return trackList; }
    
    ChannelList& getChannelList() { return channelList; }
    const ChannelList& getChannelList() const { return channelList; }
    MasterBus& getMasterBus() { return channelList.getMasterBus(); }
    
    ClipPool& getClipPool() { return clipPool; }
    const ClipPool& getClipPool() const { return clipPool; }
    
    Settings& getSettings() { return settings; }
    const Settings& getSettings() const { return settings; }
    
    void loadSettings();
    void saveSettings();
    
    // Invalid target creates and selects; a stable target replaces without selecting.
    bool loadPlugin(const juce::String& pluginPath, ChannelId target = InvalidChannelId);
    TransportState& getTransportState() { return transport; }
    
private:
    friend struct ProjectTestAccess;
    std::function<std::unique_ptr<PluginHost>(const juce::String&)> pluginLoader_;
    TransportState transport;
    TrackList trackList;
    ChannelList channelList;
    ClipPool clipPool;
    Settings settings;
    
    ChannelId activeChannelId_ = InvalidChannelId;
    void channelAdded(Channel*) override { channelListChanged(); }
    void channelRemoved(int) override { channelListChanged(); }
    void channelChanged(Channel* channel) override {
        if (channel->getId() == activeChannelId_) notifyActiveChannelChanged(getActiveChannel());
    }
    void channelListChanged() override;
    juce::ListenerList<Listener> listeners_;
    
    void notifyActiveChannelChanged(int newIndex);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Project)
};

} // namespace vibedaw
