#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Track.h"
#include <vector>
#include <functional>

namespace vibedaw {

class TrackList {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void trackAdded(Track* track) = 0;
        virtual void trackRemoved(int index) = 0;
        virtual void trackChanged(Track* track) = 0;
        virtual void trackListChanged() = 0;
    };
    
    TrackList();
    ~TrackList();
    
    Track* addTrack(const juce::String& name = {});
    // Project-load restore: creates a track with an exact saved UUID (T07/T12).
    // Validates id non-empty and unique. Message thread only.
    Track* restoreTrack(const juce::String& id, const juce::String& name);
    Track* getTrackById(const juce::String& id) const;
    // Message thread only. Empty target creates a lane; empty source requires a
    // candidate whose source/routing the caller has validated. Moves allow placeholders.
    ClipInstance* commitPlacement(const juce::String& targetId, double beat,
                                 const juce::String& sourceTrackId, const juce::String& instanceId,
                                 std::unique_ptr<ClipInstance> candidate = {});
    void removeTrack(int index);
    void clearTracks();
    
    int getNumTracks() const { return static_cast<int>(tracks.size()); }
    Track* getTrack(int index) const;
    const std::vector<std::unique_ptr<Track>>& getTracks() const { return tracks; }
    
    void moveTrack(int fromIndex, int toIndex);
    
    int indexOfTrack(const Track* track) const;
    
    void addListener(Listener* listener);
    void removeListener(Listener* listener);
    
private:
    std::vector<std::unique_ptr<Track>> tracks;
    juce::ListenerList<Listener> listeners;
    
    void notifyTrackAdded(Track* track);
    void notifyTrackRemoved(int index);
    void notifyTrackChanged(Track* track);
    void notifyTrackListChanged();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackList)
};

} // namespace vibedaw
