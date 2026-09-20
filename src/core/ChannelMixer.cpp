#include "ChannelMixer.h"
#include "MidiRecorder.h"
#include "project/Channel.h"
#include <cmath>
#include <limits>

namespace vibedaw {

ChannelMixer::ChannelMixer(ChannelList& list, TrackList& tracks, ClipPool& clips, TransportState& state,
                           MidiRecorder* recordingService)
    : channelList(list), arrangement(tracks, clips, list), transport(state), recorder(recordingService) {
    liveExpressionValues.fill(-1);
    channelList.addListener(this);
}

ChannelMixer::~ChannelMixer() { channelList.removeListener(this); }

void ChannelMixer::setActiveChannel(int index) {
    auto* channel = channelList.getChannel(index);
    activeChannelId.store(channel ? channel->getId() : InvalidChannelId);
}

void ChannelMixer::prepareToPlay(double sampleRate, int blockSize) {
    AudioQuiescence::Edit edit;
    if (recorder) recorder->renderInterrupted();
    clockInterrupted = true;
    if (!std::isfinite(sampleRate) || sampleRate <= 0 || blockSize <= 0) {
        releaseResources();
        return;
    }
    currentSampleRate = sampleRate;
    currentBlockSize = juce::jmax(1, blockSize);
    scratch.setSize(2, currentBlockSize);
    for (auto& bus : mixerScratch) bus.setSize(2, currentBlockSize);
    channelMidi.ensureSize(32768);
    isPrepared = true;
    channelList.getMasterBus().meter.prepare(sampleRate);
    mixerChannelsChanged();
    previousActive = InvalidChannelId;
    for (const auto& channel : channelList.getChannels())
        channel->prepareToPlay(sampleRate, currentBlockSize);
}

void ChannelMixer::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    const bool syntheticInput = inputCleanup, lostInput = inputLost, interruptedInput = inputInterrupted;
    inputCleanup = inputLost = inputInterrupted = false;
    buffer.clear();
    if (!isPrepared || buffer.getNumSamples() <= 0 || buffer.getNumSamples() > currentBlockSize ||
        buffer.getNumChannels() < 1 || buffer.getNumChannels() > 2) {
        ++overflowCount;
        midiMessages.clear();
        lastRevision = 0;
        clockInterrupted = true;
        if (recorder && recorder->renderActive()) recorder->renderInterrupted();
        channelList.getMasterBus().meter.clear();
        for (const auto& channel : channelList.getChannels()) channel->meter.clear();
        for (const auto& channel : channelList.getMixerChannels()) channel->meter.clear();
        return;
    }
    const auto& songSnapshot = arrangement.acquire();
    const bool session = recorder && recorder->renderActive();
    const auto& snapshot = session ? recorder->renderArrangement() : songSnapshot;
    auto& playbackTransport = session ? recorder->renderTransport() : transport;
    const auto generation = recorder ? recorder->renderGeneration() : 0;
    const bool sessionChanged = clockTransport != &playbackTransport || generation != recorderGeneration;
    if (sessionChanged) {
        // Distinct TransportStates have unrelated command generations. A source
        // revision within the same transport only cleans voices; it must not rewind
        // to that state's last (possibly long-ago) seek command.
        if (clockTransport != &playbackTransport) clock = TransportClock{};
        clockTransport = &playbackTransport;
        recorderGeneration = generation;
        nextArrangementEvent.fill(0);
    }
    const auto active = session ? recorder->renderChannel() : activeChannelId.load();
    bool panic = sessionChanged || snapshot.revision != lastRevision;
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
    const auto timing = clock.beginBlock(playbackTransport, buffer.getNumSamples(), currentSampleRate,
                                         clockInterrupted || panic);
    clockInterrupted = false;
    panic = panic || timing.discontinuity || timing.spanOverflow;
    if (timing.spanOverflow) { ++overflowCount; clockInterrupted = true; }
    // This is the untouched ingress buffer: never record scheduled accompaniment
    // or mixer-generated cleanup. Engine panic CCs are rejected by the recorder.
    if (active != previousActive || (previousSession && !session) || (previousPlaying && !timing.playing) || lostInput ||
        (previousSession && session && sessionChanged && !recorder->renderHasStarted()))
        liveExpressionValues.fill(-1);
    if (session && sessionChanged) recorder->primeLiveExpression(liveExpressionValues);
    if (session && !channelList.getChannelById(active)) recorder->renderDeliveryFailed();
    if (session) recorder->render(timing, midiMessages, !discardInput && !lostInput, syntheticInput,
                                  interruptedInput && !sessionChanged);
    if (active != previousActive || !session || !timing.playing || lostInput)
        liveExpressionOwned.fill(false);
    const double samplesPerBeat = currentSampleRate * 60.0 / timing.tempo;
    // Floor to the containing sample; snap floating-point noise at integer edges.
    // Ownership comes from the consumed index, NOT a fresh lower-bound at zero:
    // an event deferred by one block may round below zero in the next (also at a
    // tempo change). Such a pending event belongs to sample zero, never a gap.
    const double epsilon = 1.0e-7 + 8 * std::numeric_limits<double>::epsilon() *
                           std::abs(timing.startBeats) * samplesPerBeat;
    const auto sampleAt = [&](double beat, const TransportClock::Block::Span& span) {
        if (&span == &timing.spans[timing.spanCount - 1] && beat == span.end)
            return static_cast<double>(timing.numSamples);
        return std::floor(span.sampleOffset + (beat - span.start) * samplesPerBeat + epsilon);
    };
    float* pointers[]{scratch.getWritePointer(0), scratch.getWritePointer(1)};
    // Keep the prepared stereo plugin storage even for a mono output bus.
    juce::AudioBuffer<float> block(pointers, 2, buffer.getNumSamples());
    juce::AudioBuffer<float> meteredBlock(pointers, buffer.getNumChannels(), buffer.getNumSamples());
    const auto& mixerChannels = channelList.getMixerChannels();
    std::array<MixerChannel::Controls, ChannelList::maxMixerChannels> mixerControls;
    bool anyMixerSolo = false;
    for (size_t i = 0; i < mixerChannels.size(); ++i) {
        mixerControls[i] = mixerChannels[i]->acquireControls();
        anyMixerSolo = anyMixerSolo || mixerControls[i].solo;
        mixerScratch[i].clear(0, buffer.getNumSamples());
    }
    std::array<Channel::Controls, ChannelList::maxChannels> controls;
    size_t channelIndex = 0;
    bool anySolo = false;
    for (const auto& channel : channelList.getChannels()) {
        controls[channelIndex] = channel->controls.acquire();
        anySolo = anySolo || controls[channelIndex].solo;
        ++channelIndex;
    }
    channelIndex = 0;
    size_t controllerChaseWork = 0;
    constexpr size_t maxControllerChaseWork = 1048576;
    for (const auto& channel : channelList.getChannels()) {
        const auto state = controls[channelIndex++];
        auto expressionSlot = std::find_if(expressionOwnership.begin(), expressionOwnership.end(),
            [&](const auto& entry) { return entry.id == channel->getId(); });
        if (expressionSlot == expressionOwnership.end()) {
            expressionSlot = std::find_if(expressionOwnership.begin(), expressionOwnership.end(),
                [](const auto& entry) { return entry.id == InvalidChannelId; });
            jassert(expressionSlot != expressionOwnership.end());
            if (expressionSlot == expressionOwnership.end()) continue;
            expressionSlot->id = channel->getId();
        }
        auto& ownedExpression = *expressionSlot;
        const bool sessionTarget = session && channel->getId() == active;
        const bool suppressed = state.muted || (anySolo && !state.solo);
        const bool suppressing = suppressed && !channel->wasSuppressed;
        channel->wasSuppressed = suppressed;
        block.clear();
        channelMidi.clear();
        const bool auditionChanged = active != previousActive && channel->getId() == previousActive;
        // The clip workspace uses timed MIDI cleanup for compliant instruments.
        // Re-arming must not turn a previously used pedal into an async reset.
        bool resetVoices = ((panic && !sessionTarget) || auditionChanged || suppressing) && channel->needsVoiceReset();
        auto counts = channel->deliveredNotes;
        unsigned normalCount = 0;
        auto& voices = channel->arrangementVoices;
        bool arrangementSinceReset = channel->arrangementSinceReset;
        unsigned expressionCleanupCount = 0;
        bool expressionCleanupRemaining = false;
        const auto cleanupExpression = [&] {
            expressionCleanupCount = 0;
            expressionCleanupRemaining = false;
            for (unsigned lane = 0; lane < ownedExpression.lanes.size(); ++lane) {
                if (!ownedExpression.lanes[lane]) continue;
                if (expressionCleanupCount == Channel::maxLiveEvents) { expressionCleanupRemaining = true; continue; }
                const int cc = static_cast<int>(lane % 129), ch = static_cast<int>(lane / 129 + 1);
                const int value = cc == 7 ? 100 : (cc == 10 ? 64 : (cc == 11 ? 127 : 0));
                channelMidi.addEvent(cc == 128 ? juce::MidiMessage::pitchWheel(ch, 8192)
                    : juce::MidiMessage::controllerEvent(ch, cc, value), 0);
                ++expressionCleanupCount;
            }
            ownedExpression.cleanupPending = true;
        };
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
            cleanupExpression();
        };
        if (panic || resetVoices || suppressing || ownedExpression.cleanupPending) cleanupAll();
        else if (auditionChanged) {
            // Release only live deliveries: choosing an audition destination must
            // not cut an independent arrangement lifetime on the previous channel.
            for (unsigned key = 0; key < counts.size(); ++key) {
                const unsigned arrangementCount = voices[key] != 0 ? 1 : 0;
                for (unsigned n = arrangementCount; n < counts[key]; ++n)
                    channelMidi.addEvent(juce::MidiMessage::noteOff(key / 128 + 1, key % 128), 0);
                counts[key] = static_cast<unsigned short>(arrangementCount);
            }
            cleanupExpression();
        }
        unsigned count = 0;
        bool overflow = false;
        const bool ready = !discardInput && !resetVoices && !expressionCleanupRemaining &&
            !channel->isVoiceResetPending() && channel->hasPlugin();
        const bool available = ready && !suppressed;
        if (sessionTarget && timing.playing && !available) recorder->renderDeliveryFailed();
        if (!available) voices.fill(0);
        if (!timing.spanOverflow) {
            const auto destination = std::lower_bound(snapshot.destinations.begin(), snapshot.destinations.end(),
                channel->getId(), [](const auto& d, ChannelId id) { return d.id < id; });
            if (destination != snapshot.destinations.end() && destination->id == channel->getId()) {
                const auto index = static_cast<size_t>(destination - snapshot.destinations.begin());
                auto& next = nextArrangementEvent[index];
                const auto destinationEnd = snapshot.events.begin() + destination->end;
                if (panic)
                    next = static_cast<size_t>(std::lower_bound(snapshot.events.begin() + destination->begin,
                        destinationEnd, timing.startBeats, [](const auto& e, double beat) { return e.beat < beat; })
                        - snapshot.events.begin());
                for (unsigned s = 0; s < timing.spanCount; ++s) {
                    const auto& span = timing.spans[s];
                    if (span.wrap) {
                        next = static_cast<size_t>(std::lower_bound(snapshot.events.begin() + destination->begin,
                            destinationEnd, span.start, [](const auto& e, double beat) { return e.beat < beat; })
                            - snapshot.events.begin());
                        // Like deferred note events, a carried wrap belongs to sample
                        // zero even when a tempo change maps its fractional origin negative.
                        if (available && count < scheduled.size())
                            scheduled[count++] = {static_cast<int>(std::max(0.0, sampleAt(span.start, span))), -1, 0, {}};
                        else if (available) overflow = true;
                    }
                    if (available && ((panic && s == 0) || span.wrap)) {
                        const size_t work = 2 * (destination->end - destination->begin) + 16 * 129;
                        if (work > maxControllerChaseWork - controllerChaseWork) { overflow = true; break; }
                        controllerChaseWork += work;
                        std::array<int, 16 * 129> latest;
                        std::array<bool, 16 * 129> lanes{};
                        latest.fill(-1);
                        for (size_t i = destination->begin; i < destination->end; ++i) {
                            const auto& e = snapshot.events[i];
                            if (!e.expression) continue;
                            if ((e.data[0] & 0xf0) == 0xb0 && e.data[1] >= 120) continue; // Commands, not restorable state.
                            const int lane = (e.data[0] & 15) * 129 + ((e.data[0] & 0xf0) == 0xe0 ? 128 : e.data[1]);
                            lanes[lane] = true;
                            if (e.beat >= span.start) continue;
                            latest[lane] = static_cast<int>(i);
                        }
                        // A backward seek/wrap before a lane's first event must
                        // not retain its value from the previous pass.
                        for (unsigned lane = 0; lane < lanes.size(); ++lane) {
                            if (!lanes[lane] || latest[lane] >= 0) continue;
                            if (count == scheduled.size()) { overflow = true; break; }
                            const int cc = static_cast<int>(lane % 129);
                            const int value = cc == 7 ? 100 : (cc == 10 || cc == 128 ? 64 : (cc == 11 ? 127 : 0));
                            auto& restored = scheduled[count++];
                            restored = {static_cast<int>(std::max(0.0, sampleAt(span.start, span))), 3, 0,
                                {static_cast<unsigned char>((cc == 128 ? 0xe0 : 0xb0) | (lane / 129)),
                                 static_cast<unsigned char>(cc == 128 ? 0 : cc), static_cast<unsigned char>(value)}};
                            restored.origin = MidiEventOrigin::Arrangement;
                        }
                        for (size_t i = destination->begin; i < destination->end; ++i) {
                            const auto& e = snapshot.events[i];
                            if (!e.expression) continue;
                            const int lane = (e.data[0] & 15) * 129 + ((e.data[0] & 0xf0) == 0xe0 ? 128 : e.data[1]);
                            if (latest[lane] != static_cast<int>(i)) continue;
                            if (count == scheduled.size()) { overflow = true; break; }
                            auto& restored = scheduled[count++];
                            restored = {static_cast<int>(std::max(0.0, sampleAt(span.start, span))), 3, 0,
                                {e.data[0], e.data[1], e.data[2]}};
                            restored.origin = MidiEventOrigin::Arrangement;
                        }
                    }
                    const auto begin = snapshot.events.begin() + next;
                    const auto end = std::partition_point(begin, destinationEnd, [&](const auto& e) {
                        return e.beat < span.end && sampleAt(e.beat, span) < timing.numSamples;
                    });
                    next = static_cast<size_t>(end - snapshot.events.begin());
                    if (!available || overflow) continue;
                    for (auto event = begin; event != end; ++event) {
                        const auto sample = std::max(0.0, sampleAt(event->beat, span));
                        // A note shorter than one quantized sample is silent, not off-then-on.
                        if (!event->expression && event->velocity && sampleAt(std::min(event->end, span.end), span) <= sample) continue;
                        if (count == scheduled.size()) { overflow = true; break; }
                        scheduled[count++] = {static_cast<int>(sample), 3, event->token,
                            {static_cast<unsigned char>((event->velocity ? 0x90 : 0x80) | (event->key / 128)),
                             static_cast<unsigned char>(event->key % 128), static_cast<unsigned char>(event->velocity)}};
                        auto& queued = scheduled[count - 1];
                        queued.origin = MidiEventOrigin::Arrangement;
                        if (event->expression) std::copy_n(event->data, 3, queued.data);
                    }
                }
            }
        }
        if (session && channel->getId() == active && available) {
            overflow = overflow || recorder->renderOverflowed();
            for (unsigned i = 0; i < recorder->renderEventCount(); ++i) {
                if (count == scheduled.size()) { overflow = true; break; }
                const auto& event = recorder->renderEvents()[i];
                auto& queued = scheduled[count++];
                queued = {event.sample, event.size, event.token, {event.data[0], event.data[1], event.data[2]}};
                queued.origin = MidiEventOrigin::Recording;
                if (event.physical) queued.origin = MidiEventOrigin::Live;
            }
        }
        std::array<int, 16 * 128> liveAttacks;
        liveAttacks.fill(-1);
        if (ready && channel->getId() == active && !syntheticInput)
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
                // Cancel same-sample
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
            if (e.size == -1) return -1;
            if (isOn(e)) return 2;
            return e.size == 3 && ((e.data[0] & 0xf0) == 0x80 || (e.data[0] & 0xf0) == 0x90) ? 0 : 1;
        };
        for (unsigned i = 0; i < count; ++i) scheduled[i].order = i;
        const auto eventOrder = [&](const auto& a, const auto& b) {
            // Live attacks win ownership; live controllers have the final word
            // over accompaniment at the same sample. Preserve ties within each origin.
            const auto originOrder = [&](const auto& e) {
                return priority(e) == 1 ? e.origin == MidiEventOrigin::Live : e.origin != MidiEventOrigin::Live;
            };
            return std::make_tuple(a.sample, priority(a), originOrder(a), a.order) <
                   std::make_tuple(b.sample, priority(b), originOrder(b), b.order);
        };
        std::sort(scheduled.begin(), scheduled.begin() + count, eventOrder);
        const auto append = [&](const unsigned char* data, int size, int sample) {
            if (normalCount >= merged.size() - expressionCleanupCount) { overflow = true; return; }
            auto& event = merged[normalCount++];
            event = {sample, size, 0, {}};
            event.order = normalCount;
            std::copy_n(data, size, event.data);
        };
        for (unsigned i = 0; i < count && !overflow && !resetVoices; ++i) {
            const auto& e = scheduled[i];
            if (e.size == -1) {
                for (unsigned key = 0; key < voices.size(); ++key) {
                    if (!voices[key]) continue;
                    const unsigned char release[]{static_cast<unsigned char>(0x80 | (key / 128)),
                                                  static_cast<unsigned char>(key % 128), 0};
                    append(release, 3, e.sample);
                    voices[key] = 0;
                    if (counts[key]) --counts[key];
                }
                if (sessionTarget) {
                    // MIDI-compliant instruments release pedal-held tails at the
                    // actual wrap sample. Physical values follow as ordered live
                    // events from the recorder, before the next pass's attacks.
                    for (int ch = 0; ch < 16; ++ch)
                        for (const int pedalCC : {64, 66}) {
                            const unsigned char up[]{static_cast<unsigned char>(0xb0 | ch),
                                static_cast<unsigned char>(pedalCC), 0};
                            append(up, 3, e.sample);
                        }
                    continue;
                }
                // A pedal may outlive explicit offs, including notes attacked in
                // this block. Reuse the conservative off-audio reset, never CC-only panic.
                bool pedal = channel->sustainSeen;
                for (const auto input : midiMessages)
                    pedal = pedal || (channel->getId() == active && input.numBytes == 3 &&
                        (input.data[0] & 0xf0) == 0xb0 && (input.data[1] == 64 || input.data[1] == 66) && input.data[2] >= 64);
                for (unsigned j = 0; j < i; ++j) {
                    const auto& prior = scheduled[j];
                    pedal = pedal || (prior.size == 3 && (prior.data[0] & 0xf0) == 0xb0 &&
                        (prior.data[1] == 64 || prior.data[1] == 66) && prior.data[2] >= 64);
                }
                if (pedal && arrangementSinceReset) {
                    resetVoices = true;
                }
                continue;
            }
            if (e.size == 0) continue;
            const auto status = e.data[0] & 0xf0;
            const unsigned key = (e.data[0] & 15) * 128 + (e.data[1] & 127);
            const bool on = isOn(e);
            const bool off = e.size == 3 && (status == 0x80 || (status == 0x90 && !on));
            if (sessionTarget && e.size == 3 && (status == 0xe0 || (status == 0xb0 && e.data[1] < 120))) {
                const int lane = (e.data[0] & 15) * 129 + (status == 0xe0 ? 128 : e.data[1]);
                if (e.origin == MidiEventOrigin::Live) liveExpressionOwned[lane] = true;
                else if (recorder->isRecording() && liveExpressionOwned[lane]) continue;
            }
            if (e.origin != MidiEventOrigin::Live && (on || off)) {
                if (on) {
                    if (counts[key] != 0) continue; // Live owns this pitch until its release; no chase.
                    voices[key] = e.token;
                    arrangementSinceReset = true;
                } else {
                    if (voices[key] != e.token) continue; // Missed/preempted attack or seek.
                    voices[key] = 0;
                }
            } else if (e.origin == MidiEventOrigin::Live && on && voices[key]) {
                const unsigned char release[]{static_cast<unsigned char>(0x80 | (key / 128)),
                                              static_cast<unsigned char>(key % 128), 0};
                append(release, 3, e.sample);
                voices[key] = 0;
                --counts[key];
            } else if (e.origin == MidiEventOrigin::Live && off && voices[key]) continue; // A stale live release cannot kill arrangement.
            if (on) ++counts[key];
            else if (off && counts[key]) --counts[key];
            append(e.data, e.size, e.sample);
        }
        std::sort(merged.begin(), merged.begin() + normalCount, eventOrder);
        for (unsigned i = 0; i < normalCount; ++i)
            channelMidi.addEvent(merged[i].data, merged[i].size, merged[i].sample);
        if (overflow || resetVoices || !channel->canDeliverMidi(channelMidi)) {
            if (sessionTarget && timing.playing)
                recorder->renderDeliveryFailed();
            if (!resetVoices) ++overflowCount;
            resetVoices = resetVoices || channel->needsVoiceReset();
            cleanupAll(); // Reject the whole destination batch, never a note-on prefix.
        } else channel->arrangementSinceReset = arrangementSinceReset;
        if (!channel->isVoiceResetPending() && channel->hasPlugin()) {
            for (const auto event : channelMidi) {
                if (event.numBytes != 3) continue;
                const int kind = event.data[0] & 0xf0;
                if (kind != 0xe0 && !(kind == 0xb0 && event.data[1] < 120)) continue;
                const int cc = kind == 0xe0 ? 128 : event.data[1];
                const int value = cc == 128 ? event.data[1] + 128 * event.data[2] : event.data[2];
                const int neutral = cc == 128 ? 8192 : (cc == 7 ? 100 : (cc == 10 ? 64 : (cc == 11 ? 127 : 0)));
                ownedExpression.lanes[(event.data[0] & 15) * 129 + cc] = value != neutral;
            }
            ownedExpression.cleanupPending = expressionCleanupRemaining;
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
        // Audio destinations are independent of MIDI ownership. Rerouting a held
        // note moves its continuing audio, without cleaning or retriggering it.
        auto destination = mixerChannels.size();
        for (size_t i = 0; i < mixerChannels.size(); ++i)
            if (mixerChannels[i]->getId() == channel->getMixerTrackId()) {
                destination = i;
                break;
            }
        if (destination < mixerChannels.size()) {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                mixerScratch[destination].addFrom(ch, 0, block, ch, 0, buffer.getNumSamples());
        } else if (!anyMixerSolo) {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.addFrom(ch, 0, block, ch, 0, buffer.getNumSamples());
        }
    }
    for (size_t i = 0; i < mixerChannels.size(); ++i) {
        float* busPointers[]{mixerScratch[i].getWritePointer(0), mixerScratch[i].getWritePointer(1)};
        juce::AudioBuffer<float> bus(busPointers, buffer.getNumChannels(), buffer.getNumSamples());
        const auto state = mixerControls[i];
        if (state.muted || (anyMixerSolo && !state.solo) || state.volume == 0) bus.clear();
        else {
            const float pan = buffer.getNumChannels() == 1 ? 0.0f : state.pan;
            bus.applyGain(0, 0, bus.getNumSamples(), state.volume * (1.0f - juce::jmax(0.0f, pan)));
            if (bus.getNumChannels() > 1)
                bus.applyGain(1, 0, bus.getNumSamples(), state.volume * (1.0f + juce::jmin(0.0f, pan)));
        }
        mixerChannels[i]->meter.update(bus);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom(ch, 0, bus, ch, 0, buffer.getNumSamples());
    }
    if (!metronome.render(buffer, timing)) ++overflowCount;
    channelList.getMasterBus().process(buffer);
    previousActive = active;
    previousSession = session;
    previousPlaying = timing.playing;
    if (!syntheticInput && !discardInput && !lostInput)
        for (const auto input : midiMessages) {
            if (input.numBytes != 3 || input.data[1] > 127 || input.data[2] > 127) continue;
            const int kind = input.data[0] & 0xf0;
            if (kind != 0xe0 && !(kind == 0xb0 && input.data[1] < 120)) continue;
            const int lane = (input.data[0] & 15) * 129 + (kind == 0xe0 ? 128 : input.data[1]);
            liveExpressionValues[lane] = kind == 0xe0 ? input.data[1] + 128 * input.data[2] : input.data[2];
        }
    midiMessages.clear();
    clock.endBlock(playbackTransport, timing);
}

void ChannelMixer::releaseResources() {
    AudioQuiescence::Edit edit;
    if (recorder) recorder->renderInterrupted();
    isPrepared = false;
    clockInterrupted = true;
    channelList.getMasterBus().meter.clear();
    for (const auto& channel : channelList.getMixerChannels()) channel->meter.clear();
    for (const auto& channel : channelList.getChannels()) channel->releaseResources();
}
void ChannelMixer::channelAdded(Channel* channel) {
    if (isPrepared && channel) channel->prepareToPlay(currentSampleRate, currentBlockSize);
}
void ChannelMixer::channelRemoved(int) { channelListChanged(); }
void ChannelMixer::channelChanged(Channel*) {}
void ChannelMixer::channelListChanged() {
    for (auto& entry : expressionOwnership)
        if (entry.id != InvalidChannelId && !channelList.getChannelById(entry.id)) entry = {};
}
void ChannelMixer::mixerChannelsChanged() {
    // Structural notifications run under the list's quiescence edit; storage is
    // already reserved for every possible destination in prepareToPlay.
    if (isPrepared)
        for (const auto& channel : channelList.getMixerChannels()) channel->meter.prepare(currentSampleRate);
}

} // namespace vibedaw
