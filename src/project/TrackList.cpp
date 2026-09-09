#include "TrackList.h"

namespace vibedaw {

TrackList::TrackList() = default;

TrackList::~TrackList() = default;

Track* TrackList::getTrackById(const juce::String& id) const {
    for (const auto& track : tracks) if (track->getId() == id) return track.get();
    return nullptr;
}

ClipInstance* TrackList::commitPlacement(const juce::String& targetId, double beat,
                                        const juce::String& sourceTrackId, const juce::String& instanceId,
                                        std::unique_ptr<ClipInstance> candidate) {
    if (sourceTrackId.isEmpty() != instanceId.isEmpty()) return nullptr;
    auto* source = getTrackById(sourceTrackId);
    int index = -1;
    if (source) for (int i = 0; i < source->getNumClipInstances(); ++i)
        if (source->getClipInstance(i)->getId() == instanceId) index = i;
    if (sourceTrackId.isNotEmpty() && (index < 0 || candidate)) return nullptr;
    auto* instance = index >= 0 ? source->getClipInstance(index) : candidate.get();
    auto* target = getTrackById(targetId);
    if ((!target && targetId.isNotEmpty()) || !instance || !instance->isValid() ||
        !std::isfinite(beat) || beat < 0 || !std::isfinite(beat + instance->getDuration())) return nullptr;
    if (source && source == target) {
        instance->setStartTime(beat);
        return instance;
    }
    std::unique_ptr<Track> newTrack;
    if (!target) {
        newTrack = std::make_unique<Track>("Track " + juce::String(tracks.size() + 1));
        target = newTrack.get();
        tracks.reserve(tracks.size() + 1);
    }
    // Allocate everything before removing ownership. No listener sees a half move.
    target->clipInstances.reserve(target->clipInstances.size() + 1);
    std::function<void()> changed = [target] { target->sendChangeMessage(); };
    if (index >= 0) {
        candidate = std::move(source->clipInstances[index]);
        source->clipInstances.erase(source->clipInstances.begin() + index);
    }
    candidate->startTime_ = beat;
    candidate->onChange_ = std::move(changed);
    target->clipInstances.push_back(std::move(candidate));
    if (newTrack) tracks.push_back(std::move(newTrack));
    if (source) source->sendChangeMessage();
    target->sendChangeMessage();
    if (targetId.isEmpty()) {
        notifyTrackAdded(target);
        notifyTrackListChanged();
    }
    return instance;
}

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
