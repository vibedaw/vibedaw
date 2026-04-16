#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Settings.h"
#include "Track.h"
#include "TrackList.h"
#include "core/AudioEngine.h"
#include "core/MidiManager.h"

namespace vibedaw {

class Project {
public:
    Project();
    ~Project();
    
    bool initialise(AudioEngine& audioEngine, MidiManager& midiManager);
    void shutdown();
    
    Track* getMasterTrack() { return masterTrack.get(); }
    const Track* getMasterTrack() const { return masterTrack.get(); }
    
    TrackList& getTrackList() { return trackList; }
    const TrackList& getTrackList() const { return trackList; }
    
    Settings& getSettings() { return settings; }
    const Settings& getSettings() const { return settings; }
    
    void loadSettings();
    void saveSettings();
    
    bool loadPlugin(const juce::String& pluginPath);
    
    juce::AudioProcessor* getProcessorGraph();
    
private:
    std::unique_ptr<Track> masterTrack;
    TrackList trackList;
    Settings settings;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Project)
};

} // namespace vibedaw
