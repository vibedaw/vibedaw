#include "ChannelMixer.h"
#include "project/Channel.h"
#include <cmath>
#include <limits>

namespace vibedaw {

ChannelMixer::ChannelMixer(ChannelList& list, TrackList& tracks, ClipPool& clips, TransportState& state)
    : channelList(list), arrangement(tracks, clips, list), transport(state) {
    channelList.addListener(this);
}

ChannelMixer::~ChannelMixer() { channelList.removeListener(this); }

void ChannelMixer::setActiveChannel(int index) {
    auto* channel = channelList.getChannel(index);
    activeChannelId.store(channel ? channel->getId() : InvalidChannelId);
}

void ChannelMixer::prepareToPlay(double sampleRate, int blockSize) {
    AudioQuiescence::Edit edit;
    clockInterrupted = true;
    if (!std::isfinite(sampleRate) || sampleRate <= 0 || blockSize <= 0) {
        releaseResources();
        return;
    }
    currentSampleRate = sampleRate;
    currentBlockSize = juce::jmax(1, blockSize);
    scratch.setSize(2, currentBlockSize);
    channelMidi.ensureSize(32768);
    isPrepared = true;
    channelList.getMasterBus().meter.prepare(sampleRate);
    previousActive = InvalidChannelId;
    for (const auto& channel : channelList.getChannels())
        channel->prepareToPlay(sampleRate, currentBlockSize);
}

void ChannelMixer::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    buffer.clear();
    if (!isPrepared || buffer.getNumSamples() <= 0 || buffer.getNumSamples() > currentBlockSize ||
        buffer.getNumChannels() < 1 || buffer.getNumChannels() > 2) {
        ++overflowCount;
        midiMessages.clear();
        lastRevision = 0;
        clockInterrupted = true;
        channelList.getMasterBus().meter.clear();
        for (const auto& channel : channelList.getChannels()) channel->meter.clear();
        return;
    }
    const auto& snapshot = arrangement.acquire();
    const auto active = activeChannelId.load();
    bool panic = snapshot.revision != lastRevision;
    bool discardInput = false;
    lastRevision = snapshot.revision;
    unsigned events = 0;
    for (const auto metadata : midiMessages) {
        if (++events > Channel::maxLiveEvents || metadata.numBytes > 3) {
            ++overflowCount; panic = true; discardInput = true; break;
        }
        if (metadata.numBytes == 3 && (metadata.data[0] & 0xf0) == 0xb0 &&
            (metadata.data[1] == 120 || metadata.data[1] == 123))
            panic = true;
    }
    const auto timing = clock.beginBlock(transport, buffer.getNumSamples(), currentSampleRate,
                                         clockInterrupted || panic);
    clockInterrupted = false;
    panic = panic || timing.discontinuity;
    const double samplesPerBeat = currentSampleRate * 60.0 / timing.tempo;
    // Floor to the containing sample; snap floating-point noise at integer edges.
    // Ownership comes from the consumed index, NOT a fresh lower-bound at zero:
    // an event deferred by one block may round below zero in the next (also at a
    // tempo change). Such a pending event belongs to sample zero, never a gap.
    const double epsilon = 1.0e-7 + 8 * std::numeric_limits<double>::epsilon() *
                           std::abs(timing.startBeats) * samplesPerBeat;
    const auto sampleAt = [&](double beat) {
        if (beat == timing.startBeats) return 0.0;
        if (timing.playing && beat == timing.endBeats) return static_cast<double>(timing.numSamples);
        return std::floor((beat - timing.startBeats) * samplesPerBeat + epsilon);
    };
    std::array<size_t, ChannelList::maxChannels> blockEventBegins{};
    for (size_t i = 0; i < snapshot.destinations.size(); ++i) {
        const auto& destination = snapshot.destinations[i];
        const auto end = snapshot.events.begin() + destination.end;
        auto& next = nextArrangementEvent[i];
        if (panic)
            next = static_cast<size_t>(std::lower_bound(snapshot.events.begin() + destination.begin, end,
                timing.startBeats, [](const auto& e, double beat) { return e.beat < beat; }) - snapshot.events.begin());
        blockEventBegins[i] = next;
        if (timing.playing)
            next = static_cast<size_t>(std::partition_point(snapshot.events.begin() + next, end,
                [&](const auto& e) { return e.beat < timing.endBeats && sampleAt(e.beat) < timing.numSamples; })
                - snapshot.events.begin());
        // Advance even for unavailable/overflowing destinations: dropped attacks
        // are not retried later, and their tokenless releases remain harmless.
    }
    float* pointers[]{scratch.getWritePointer(0), scratch.getWritePointer(1)};
    // Keep the prepared stereo plugin storage even for a mono output bus.
    juce::AudioBuffer<float> block(pointers, 2, buffer.getNumSamples());
    juce::AudioBuffer<float> meteredBlock(pointers, buffer.getNumChannels(), buffer.getNumSamples());
    std::array<Channel::Controls, ChannelList::maxChannels> controls;
    size_t channelIndex = 0;
    bool anySolo = false;
    for (const auto& channel : channelList.getChannels()) {
        controls[channelIndex] = channel->controls.acquire();
        anySolo = anySolo || controls[channelIndex].solo;
        ++channelIndex;
    }
    channelIndex = 0;
    for (const auto& channel : channelList.getChannels()) {
        const auto state = controls[channelIndex++];
        const bool suppressed = state.muted || (anySolo && !state.solo);
        const bool suppressing = suppressed && !channel->wasSuppressed;
        channel->wasSuppressed = suppressed;
        block.clear();
        channelMidi.clear();
        const bool auditionChanged = active != previousActive && channel->getId() == previousActive;
        bool resetVoices = (panic || auditionChanged || suppressing) && channel->needsVoiceReset();
        auto counts = channel->deliveredNotes;
        unsigned normalCount = 0;
        auto& voices = channel->arrangementVoices;
        const auto cleanupAll = [&] {
            channelMidi.clear();
            channel->appendNoteCleanup(channelMidi);
            counts.fill(0);
            voices.fill(0);
            for (int ch = 1; ch <= 16; ++ch) {
                channelMidi.addEvent(juce::MidiMessage::controllerEvent(ch, 64, 0), 0);
                channelMidi.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
                channelMidi.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
            }
        };
        if (panic || resetVoices || suppressing) cleanupAll();
        else if (auditionChanged) {
            // Release only live deliveries: choosing an audition destination must
            // not cut an independent arrangement lifetime on the previous channel.
            for (unsigned key = 0; key < counts.size(); ++key) {
                const unsigned arrangementCount = voices[key] != 0 ? 1 : 0;
                for (unsigned n = arrangementCount; n < counts[key]; ++n)
                    channelMidi.addEvent(juce::MidiMessage::noteOff(key / 128 + 1, key % 128), 0);
                counts[key] = static_cast<unsigned short>(arrangementCount);
            }
        }
        unsigned count = 0;
        bool overflow = false;
        const bool ready = !discardInput && !resetVoices && !channel->isVoiceResetPending() && channel->hasPlugin();
        const bool available = ready && !suppressed;
        if (!available) voices.fill(0);
        if (available && timing.playing) {
            const auto destination = std::lower_bound(snapshot.destinations.begin(), snapshot.destinations.end(),
                channel->getId(), [](const auto& d, ChannelId id) { return d.id < id; });
            if (destination != snapshot.destinations.end() && destination->id == channel->getId()) {
                const auto index = static_cast<size_t>(destination - snapshot.destinations.begin());
                const auto end = snapshot.events.begin() + nextArrangementEvent[index];
                auto event = snapshot.events.begin() + blockEventBegins[index];
                for (; event != end; ++event) {
                    const auto sample = std::max(0.0, sampleAt(event->beat));
                    // A note shorter than one quantized sample is silent, not off-then-on.
                    if (event->velocity && sampleAt(event->end) <= sample) continue;
                    if (count == scheduled.size()) { overflow = true; break; }
                    scheduled[count++] = {static_cast<int>(sample), 3, event->token,
                        {static_cast<unsigned char>((event->velocity ? 0x90 : 0x80) | (event->key / 128)),
                         static_cast<unsigned char>(event->key % 128), static_cast<unsigned char>(event->velocity)}};
                }
            }
        }
        std::array<int, 16 * 128> liveAttacks;
        liveAttacks.fill(-1);
        if (ready && channel->getId() == active)
            for (const auto metadata : midiMessages) {
                const int sample = juce::jlimit(0, buffer.getNumSamples() - 1, metadata.samplePosition);
                const int key = metadata.numBytes == 3 ? (metadata.data[0] & 15) * 128 + (metadata.data[1] & 127) : 0;
                const int status = metadata.data[0] & 0xf0;
                const bool on = metadata.numBytes == 3 && status == 0x90 && metadata.data[2] != 0;
                const bool off = metadata.numBytes == 3 && (status == 0x80 || (status == 0x90 && !on));
                // Suppression already cleaned note ownership. Keep controller state
                // current (especially pedal-up), without creating muted voices.
                if (suppressed && (on || off)) continue;
                auto& attack = liveAttacks[key];
                // Engine ingress quantizes live messages to zero. Cancel same-sample
                // on->off pairs before off-first sorting, or a fast tap could stick.
                if (off && attack >= 0 && scheduled[attack].sample == sample) {
                    auto& pending = scheduled[attack];
                    pending.size = 0;
                    attack = pending.previousAttack;
                    continue;
                }
                if (count == scheduled.size()) { overflow = true; break; }
                auto& event = scheduled[count++];
                event = {sample, metadata.numBytes, 0, {}};
                std::copy_n(metadata.data, metadata.numBytes, event.data);
                if (on) { event.previousAttack = attack; attack = static_cast<int>(count - 1); }
            }
        const auto isOn = [](const auto& e) { return e.size == 3 && (e.data[0] & 0xf0) == 0x90 && e.data[2] != 0; };
        const auto priority = [&](const auto& e) {
            if (isOn(e)) return 2;
            return e.size == 3 && ((e.data[0] & 0xf0) == 0x80 || (e.data[0] & 0xf0) == 0x90) ? 0 : 1;
        };
        for (unsigned i = 0; i < count; ++i) scheduled[i].order = i;
        const auto eventOrder = [&](const auto& a, const auto& b) {
            // Releases/controllers precede attacks, live attacks precede arrangement.
            return std::make_tuple(a.sample, priority(a), a.token != 0, a.order) <
                   std::make_tuple(b.sample, priority(b), b.token != 0, b.order);
        };
        std::sort(scheduled.begin(), scheduled.begin() + count, eventOrder);
        const auto append = [&](const unsigned char* data, int size, int sample) {
            if (normalCount == merged.size()) { overflow = true; return; }
            auto& event = merged[normalCount++];
            event = {sample, size, 0, {}};
            event.order = normalCount;
            std::copy_n(data, size, event.data);
        };
        for (unsigned i = 0; i < count && !overflow; ++i) {
            const auto& e = scheduled[i];
            if (e.size == 0) continue;
            const auto status = e.data[0] & 0xf0;
            const unsigned key = (e.data[0] & 15) * 128 + (e.data[1] & 127);
            const bool on = isOn(e);
            const bool off = e.size == 3 && (status == 0x80 || (status == 0x90 && !on));
            if (e.token) {
                if (on) {
                    if (counts[key] != 0) continue; // Live owns this pitch until its release; no chase.
                    voices[key] = e.token;
                } else {
                    if (voices[key] != e.token) continue; // Missed/preempted attack or seek.
                    voices[key] = 0;
                }
            } else if (on && voices[key]) {
                const unsigned char release[]{static_cast<unsigned char>(0x80 | (key / 128)),
                                              static_cast<unsigned char>(key % 128), 0};
                append(release, 3, e.sample);
                voices[key] = 0;
                --counts[key];
            } else if (off && voices[key]) continue; // A stale live release cannot kill arrangement.
            if (on) ++counts[key];
            else if (off && counts[key]) --counts[key];
            append(e.data, e.size, e.sample);
        }
        std::sort(merged.begin(), merged.begin() + normalCount, eventOrder);
        for (unsigned i = 0; i < normalCount; ++i)
            channelMidi.addEvent(merged[i].data, merged[i].size, merged[i].sample);
        if (overflow || !channel->canDeliverMidi(channelMidi)) {
            ++overflowCount;
            resetVoices = channel->needsVoiceReset();
            cleanupAll(); // Reject the whole destination batch, never a note-on prefix.
        }
        // Suppression cleans once, consumes/drops attacks without chase, but keeps
        // processing tails and lifecycle. Controls were captured once for this block.
        channel->processWithControls(block, channelMidi, suppressed ? 0.0f : state.volume,
                                     buffer.getNumChannels() == 1 ? 0.0f : state.pan);
        if (resetVoices) {
            channel->requestVoiceReset();
            block.clear(); // Sustained voices stay silent until the off-audio reset completes.
        }
        channel->meter.update(meteredBlock);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom(ch, 0, block, ch, 0, buffer.getNumSamples());
    }
    // T05 metronome injection point: add click audio here, before master gain/mute.
    channelList.getMasterBus().process(buffer);
    previousActive = active;
    midiMessages.clear();
    clock.endBlock(transport, timing);
}

void ChannelMixer::releaseResources() {
    AudioQuiescence::Edit edit;
    isPrepared = false;
    clockInterrupted = true;
    channelList.getMasterBus().meter.clear();
    for (const auto& channel : channelList.getChannels()) channel->releaseResources();
}
void ChannelMixer::channelAdded(Channel* channel) {
    if (isPrepared && channel) channel->prepareToPlay(currentSampleRate, currentBlockSize);
}
void ChannelMixer::channelRemoved(int) {}
void ChannelMixer::channelChanged(Channel*) {}
void ChannelMixer::channelListChanged() {}

} // namespace vibedaw
