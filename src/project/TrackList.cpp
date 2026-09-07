#include "TrackList.h"

namespace vibedaw {

TrackList::TrackList() = default;

TrackList::~TrackList() = default;

Track* TrackList::addTrack(const juce::String& name) {
    auto trackName = name.isEmpty() 
        ? "Track " + juce::String(tracks.size() + 1) 
        : name;
    
    auto track = std::make_unique<Track>(trackName);
    auto* ptr = track.get();
    tracks.push_back(std::move(track));
    
    notifyTrackAdded(ptr);
    notifyTrackListChanged();
    
    return tracks.back().get();
}

void TrackList::removeTrack(int index) {
    if (index >= 0 && index < static_cast<int>(tracks.size())) {
        tracks.erase(tracks.begin() + index);
        notifyTrackRemoved(index);
        notifyTrackListChanged();
    }
}

void TrackList::clearTracks() {
    tracks.clear();
    notifyTrackListChanged();
}

Track* TrackList::getTrack(int index) const {
    if (index >= 0 && index < static_cast<int>(tracks.size())) {
        return tracks[index].get();
    }
    return nullptr;
}

void TrackList::moveTrack(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= static_cast<int>(tracks.size()) ||
        toIndex < 0 || toIndex >= static_cast<int>(tracks.size()) ||
        fromIndex == toIndex) {
        return;
    }
    
    auto track = std::move(tracks[fromIndex]);
    tracks.erase(tracks.begin() + fromIndex);
    tracks.insert(tracks.begin() + toIndex, std::move(track));
    
    notifyTrackListChanged();
}

int TrackList::indexOfTrack(const Track* track) const {
    for (int i = 0; i < static_cast<int>(tracks.size()); ++i) {
        if (tracks[i].get() == track) {
            return i;
        }
    }
    return -1;
}

void TrackList::addListener(Listener* listener) {
    listeners.add(listener);
}

void TrackList::removeListener(Listener* listener) {
    listeners.remove(listener);
}

void TrackList::notifyTrackAdded(Track* track) {
    listeners.call([track](Listener& l) { l.trackAdded(track); });
}

void TrackList::notifyTrackRemoved(int index) {
    listeners.call([index](Listener& l) { l.trackRemoved(index); });
}

void TrackList::notifyTrackChanged(Track* track) {
    listeners.call([track](Listener& l) { l.trackChanged(track); });
}

void TrackList::notifyTrackListChanged() {
    listeners.call([](Listener& l) { l.trackListChanged(); });
}

} // namespace vibedaw
