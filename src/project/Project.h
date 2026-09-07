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

namespace vibedaw {

class ChannelMixer;

class Project {
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
    int getActiveChannel() const { return activeChannelIndex_; }
    
    void addListener(Listener* listener);
    void removeListener(Listener* listener);
    
    TrackList& getTrackList() { return trackList; }
    const TrackList& getTrackList() const { return trackList; }
    
    ChannelList& getChannelList() { return channelList; }
    const ChannelList& getChannelList() const { return channelList; }
    
    ClipPool& getClipPool() { return clipPool; }
    const ClipPool& getClipPool() const { return clipPool; }
    
    Settings& getSettings() { return settings; }
    const Settings& getSettings() const { return settings; }
    
    void loadSettings();
    void saveSettings();
    
    bool loadPlugin(const juce::String& pluginPath);
    
private:
    TrackList trackList;
    ChannelList channelList;
    ClipPool clipPool;
    Settings settings;
    
    int activeChannelIndex_ = -1;
    juce::ListenerList<Listener> listeners_;
    
    void notifyActiveChannelChanged(int newIndex);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Project)
};

} // namespace vibedaw