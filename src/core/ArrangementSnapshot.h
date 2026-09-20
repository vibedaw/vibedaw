#pragma once

#include "AudioBoundary.h"
#include "project/TrackList.h"
#include "project/ClipPool.h"
#include "project/ChannelList.h"
#include <algorithm>
#include <cmath>
#include <tuple>

namespace vibedaw {

enum class MidiEventOrigin { Live, Arrangement, Recording };

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
        bool expression = false;
        unsigned char data[3]{};
        ChannelId destination = InvalidChannelId;
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
                                              const ChannelList& channels, unsigned revision,
                                              ClipId excludedClip = InvalidClipId,
                                              ChannelId excludedChannel = InvalidChannelId,
                                              double excludedStart = -1.0,
                                              const juce::String& excludedPlacement = {}) {
    ArrangementSnapshot result;
    result.revision = revision;
    for (const auto& track : tracks.getTracks()) {
        for (const auto& placement : track->getClipInstances()) {
            if ((!excludedPlacement.isEmpty() && placement->getId() == excludedPlacement) ||
                (excludedPlacement.isEmpty() && placement->getClipId() == excludedClip &&
                (excludedChannel == InvalidChannelId || placement->getChannelId() == excludedChannel) &&
                (excludedStart < 0 || placement->getStartTime() == excludedStart))) continue;
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
                    result.events.clear();
                    result.overflow = true;
                    return result;
                }
                result.notes.push_back({start, end,
                    placement->getChannelId(), juce::jlimit(0, 127, note.getPitch()),
                    juce::jlimit(1, 127, note.getVelocity()), juce::jlimit(1, 16, note.getChannel())});
            }
            for (const auto& expression : source->getExpressionEvents()) {
                if (!expression.isValid() || expression.beat >= length) continue;
                if (result.events.size() == MidiClip::maxExpressionEvents) {
                    result.notes.clear(); result.events.clear(); result.overflow = true;
                    return result;
                }
                ArrangementSnapshot::Event event;
                event.beat = placement->getStartTime() + expression.beat;
                event.expression = true;
                event.data[0] = static_cast<unsigned char>(expression.status);
                event.data[1] = static_cast<unsigned char>(expression.data1);
                event.data[2] = static_cast<unsigned char>(expression.data2);
                event.destination = placement->getChannelId();
                result.events.push_back(event);
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
        const int key = (note.midiChannel - 1) * 128 + note.pitch;
        const auto token = static_cast<unsigned>(i + 1);
        result.events.push_back({note.start, note.end, token, key, note.velocity, false, {}, note.destination});
        result.events.push_back({note.end, note.end, token, key, 0, false, {}, note.destination});
    }
    std::stable_sort(result.events.begin(), result.events.end(), [](const auto& a, const auto& b) {
        const int ap = a.expression ? 1 : (a.velocity ? 2 : 0);
        const int bp = b.expression ? 1 : (b.velocity ? 2 : 0);
        return std::tie(a.destination, a.beat, ap) < std::tie(b.destination, b.beat, bp);
    });
    for (size_t i = 0; i < result.events.size(); ++i) {
        const auto id = result.events[i].destination;
        if (result.destinations.empty() || result.destinations.back().id != id) {
            if (!result.destinations.empty()) result.destinations.back().end = i;
            result.destinations.push_back({id, i, 0});
        }
    }
    if (!result.destinations.empty()) result.destinations.back().end = result.events.size();
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
