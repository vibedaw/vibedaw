#pragma once

#include "AudioBoundary.h"
#include "project/TrackList.h"
#include "project/ClipPool.h"
#include "project/ChannelList.h"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace vibedaw {

struct RenderNote {
    double start = 0, end = 0;
    ChannelId destination = InvalidChannelId;
    int pitch = 60, velocity = 100, midiChannel = 1;
};
struct ArrangementSnapshot {
    struct Event {
        double beat = 0, end = 0;
        unsigned token = 0;
        int key = 0, velocity = 0; // velocity zero is a release.
    };
    struct Destination {
        ChannelId id = InvalidChannelId;
        size_t begin = 0, end = 0;
    };
    unsigned revision = 0;
    bool overflow = false;
    std::vector<RenderNote> notes;
    std::vector<Event> events;
    std::vector<Destination> destinations;
    static constexpr size_t maxNotes = 65536;
};

// Message-thread compiler. No model pointer is retained in the published data.
inline ArrangementSnapshot compileArrangement(const TrackList& tracks, const ClipPool& clips,
                                              const ChannelList& channels, unsigned revision) {
    ArrangementSnapshot result;
    result.revision = revision;
    for (const auto& track : tracks.getTracks()) {
        for (const auto& placement : track->getClipInstances()) {
            auto* source = dynamic_cast<MidiClip*>(clips.getClip(placement->getClipId()));
            if (!source || source->isMuted() || placement->isMuted() ||
                !channels.getChannelById(placement->getChannelId())) continue;
            const double length = std::min(source->getDuration(), placement->getDuration());
            for (const auto& note : source->getNotes()) {
                if (note.isMuted() || note.getVelocity() <= 0 || note.getStartTime() < 0 ||
                    !std::isfinite(note.getStartTime()) || !std::isfinite(note.getEndTime()) ||
                    note.getStartTime() >= length) continue;
                const double start = placement->getStartTime() + note.getStartTime();
                const double end = placement->getStartTime() + std::min(length, note.getEndTime());
                if (!std::isfinite(start) || !std::isfinite(end) || !(end > start)) continue;
                if (result.notes.size() == ArrangementSnapshot::maxNotes) {
                    result.notes.clear();
                    result.overflow = true;
                    return result;
                }
                result.notes.push_back({start, end,
                    placement->getChannelId(), juce::jlimit(0, 127, note.getPitch()),
                    juce::jlimit(1, 127, note.getVelocity()), juce::jlimit(1, 16, note.getChannel())});
            }
        }
    }
    std::stable_sort(result.notes.begin(), result.notes.end(), [](const auto& a, const auto& b) {
        return a.start < b.start;
    });
    auto voices = result.notes;
    std::stable_sort(voices.begin(), voices.end(), [](const auto& a, const auto& b) {
        return std::tie(a.destination, a.midiChannel, a.pitch, a.start) <
               std::tie(b.destination, b.midiChannel, b.pitch, b.start);
    });
    // Union strictly overlapping same-key intervals. First attack supplies velocity;
    // touching intervals remain separate attacks. Source/track order breaks ties.
    size_t count = 0;
    for (const auto note : voices) {
        if (count && voices[count - 1].destination == note.destination &&
            voices[count - 1].midiChannel == note.midiChannel &&
            voices[count - 1].pitch == note.pitch && note.start < voices[count - 1].end)
            voices[count - 1].end = std::max(voices[count - 1].end, note.end);
        else voices[count++] = note;
    }
    for (size_t i = 0; i < count; ++i) {
        const auto& note = voices[i];
        if (result.destinations.empty() || result.destinations.back().id != note.destination) {
            if (!result.destinations.empty()) result.destinations.back().end = result.events.size();
            result.destinations.push_back({note.destination, result.events.size(), 0});
        }
        const int key = (note.midiChannel - 1) * 128 + note.pitch;
        const auto token = static_cast<unsigned>(i + 1);
        result.events.push_back({note.start, note.end, token, key, note.velocity});
        result.events.push_back({note.end, note.end, token, key, 0});
    }
    if (!result.destinations.empty()) result.destinations.back().end = result.events.size();
    for (const auto& destination : result.destinations)
        std::sort(result.events.begin() + destination.begin, result.events.begin() + destination.end,
            [](const auto& a, const auto& b) {
                return std::tie(a.beat, a.velocity, a.token) < std::tie(b.beat, b.velocity, b.token);
            });
    return result;
}

class ArrangementPublisher : private TrackList::Listener, private ClipPool::Listener,
                             private ChannelList::Listener, private juce::ChangeListener,
                             private juce::AsyncUpdater {
public:
    ArrangementPublisher(TrackList& t, ClipPool& p, ChannelList& c) : tracks(t), clips(p), channels(c) {
        tracks.addListener(this); clips.addListener(this); channels.addListener(this);
        for (const auto& track : tracks.getTracks()) track->addChangeListener(this);
        publishNow();
    }
    ~ArrangementPublisher() override {
        cancelPendingUpdate();
        for (const auto& track : tracks.getTracks()) track->removeChangeListener(this);
        tracks.removeListener(this); clips.removeListener(this); channels.removeListener(this);
    }
    const ArrangementSnapshot& acquire() noexcept { return snapshots.acquire(); }
    bool hasOverflow() const { return overflow; } // Message thread only.
    void publishNow() {
        auto snapshot = compileArrangement(tracks, clips, channels, ++revision);
        overflow = snapshot.overflow;
        snapshots.publish(snapshot);
    }
private:
    void handleAsyncUpdate() override { publishNow(); }
    void changeListenerCallback(juce::ChangeBroadcaster*) override { triggerAsyncUpdate(); }
    void trackAdded(Track* t) override { t->addChangeListener(this); triggerAsyncUpdate(); }
    void trackRemoved(int) override { triggerAsyncUpdate(); }
    void trackChanged(Track*) override { triggerAsyncUpdate(); }
    void trackListChanged() override { triggerAsyncUpdate(); }
    void clipAdded(ClipId, Clip*) override { triggerAsyncUpdate(); }
    void clipRemoved(ClipId) override { triggerAsyncUpdate(); }
    void clipChanged(ClipId, Clip*) override { triggerAsyncUpdate(); }
    void channelAdded(Channel*) override { triggerAsyncUpdate(); }
    void channelRemoved(int) override { triggerAsyncUpdate(); }
    // Mixer controls/cosmetics do not change compiled timing or destinations.
    // Plugin replacement already clears the channel's delivered/token ledgers.
    void channelChanged(Channel*) override {}
    void channelListChanged() override { triggerAsyncUpdate(); }
    TrackList& tracks;
    ClipPool& clips;
    ChannelList& channels;
    LatestState<ArrangementSnapshot> snapshots;
    unsigned revision = 0;
    bool overflow = false;
};

} // namespace vibedaw
