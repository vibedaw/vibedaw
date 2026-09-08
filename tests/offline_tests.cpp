#include "core/AudioBoundary.h"
#include "core/ChannelMixer.h"
#include "project/Project.h"
#include "plugins/PluginHost.h"
#include "ui/panels/MixerPanel.h"
#include "ui/TransportComponent.h"
#include "ui/panels/TimelinePanel.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <cstdlib>
#include <new>

thread_local bool rendering = false;
thread_local unsigned renderAllocations = 0, renderDeletions = 0;
void* operator new(std::size_t n) {
    if (rendering) ++renderAllocations;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { if (rendering && p) ++renderDeletions; std::free(p); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

using namespace vibedaw;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)

namespace vibedaw {
struct AudioEngineTestAccess {
    static void prepare(AudioEngine& engine, double rate, int samples) { engine.prepare(rate, samples); }
    static void render(AudioEngine& engine, juce::AudioBuffer<float>& buffer) {
        engine.audioDeviceIOCallbackWithContext(nullptr, 0, buffer.getArrayOfWritePointers(),
            buffer.getNumChannels(), buffer.getNumSamples(), {});
    }
    static void renderSamples(AudioEngine& engine, juce::AudioBuffer<float>& buffer, int samples) {
        engine.audioDeviceIOCallbackWithContext(nullptr, 0, buffer.getArrayOfWritePointers(),
            buffer.getNumChannels(), samples, {});
    }
};
struct ChannelTestAccess { static void resetVoices(Channel& channel) { channel.timerCallback(); } };
struct MidiManagerTestAccess { static void drain(MidiManager& manager) { manager.timerCallback(); } };
struct LevelMeterTestAccess {
    static float tick(LevelMeter& meter, double seconds) {
        meter.lastTime = juce::Time::getMillisecondCounterHiRes() - seconds * 1000;
        meter.timerCallback();
        return meter.leftDisplayLevel;
    }
};
}

static void boundaryTests() {
    BoundedQueue<unsigned, 4> queue;
    for (unsigned i = 0; i < 4; ++i) CHECK(queue.push(i));
    CHECK(!queue.push(99));
    unsigned value = 0;
    for (unsigned i = 0; i < 4; ++i) { CHECK(queue.pop(value)); CHECK(value == i); }
    CHECK(!queue.pop(value));
    for (unsigned i = 0; i < 10000; ++i) { CHECK(queue.push(i)); CHECK(queue.pop(value)); CHECK(value == i); }

    struct State { unsigned a = 0, b = 0; std::vector<unsigned> data; };
    LatestState<State> mailbox;
    mailbox.publish({1, 1, {1}});
    const auto& held = mailbox.acquire();
    for (unsigned i = 2; i < 1000; ++i) mailbox.publish({i, i, {i}});
    CHECK(held.a == 1 && held.data[0] == 1);
    CHECK(mailbox.acquire().a == 999);
    std::atomic<bool> finished{false};
    std::thread producer([&] {
        for (unsigned i = 1000; i < 100000; ++i) mailbox.publish({i, i, {i}});
        finished.store(true);
    });
    do {
        const auto& state = mailbox.acquire();
        CHECK(state.a == state.b && state.data[0] == state.a);
    } while (!finished.load());
    producer.join();
    CHECK(mailbox.acquire().a == 99999);
    rendering = true;
    mailbox.acquire();
    rendering = false;
    CHECK(renderAllocations == 0 && renderDeletions == 0);

    auto& gate = AudioQuiescence::instance();
    CHECK(gate.enter());
    std::atomic<bool> started{false}, edited{false};
    std::thread writer([&] { started.store(true); AudioQuiescence::Edit edit; edited.store(true); });
    while (!started.load()) std::this_thread::yield();
    CHECK(!edited.load());
    gate.leave();
    writer.join();
    CHECK(edited.load());
    { AudioQuiescence::Edit edit; AudioQuiescence::Edit nested; CHECK(!gate.enter()); }
    CHECK(gate.enter()); gate.leave();
}

struct Changes : juce::ChangeListener {
    int count = 0;
    void changeListenerCallback(juce::ChangeBroadcaster*) override { ++count; }
};
struct ClipChanges : Clip::Listener {
    int changes = 0, invalidations = 0;
    void clipChanged() override { ++changes; }
    void notesInvalidated() override { ++invalidations; }
};
struct PoolChanges : ClipPool::Listener {
    explicit PoolChanges(ClipPool& p) : pool(p) { pool.addListener(this); }
    ~PoolChanges() override { pool.removeListener(this); }
    ClipPool& pool;
    int before = 0, after = 0;
    void clipAdded(ClipId, Clip*) override {}
    void clipChanged(ClipId, Clip*) override {}
    void clipWillBeRemoved(ClipId id) override { CHECK(pool.getClip(id)); ++before; }
    void clipRemoved(ClipId id) override { CHECK(!pool.getClip(id)); ++after; }
};
struct ChannelChanges : ChannelList::Listener {
    int changed = 0;
    ChannelId last = -1;
    void channelAdded(Channel*) override {}
    void channelRemoved(int) override {}
    void channelListChanged() override {}
    void channelChanged(Channel* c) override {
        CHECK(juce::MessageManager::getInstance()->isThisTheMessageThread());
        ++changed; last = c->getId();
    }
};

struct Probe {
    struct Event {
        long long time;
        int offset, channel, pitch, velocity;
        bool on;
    };
    Probe() { events.reserve(20000); }
    std::vector<Event> events;
    long long samples = 0;
    int blocks = 0, notes = 0, noteOffs = 0, sounding = 0, destroyed = 0, resets = 0;
    int lastInputCount = 0, lastOffSample = -1, editorCalls = 0;
    bool invalidInputOffset = false;
    bool destroyedOnAudio = false, resetOnAudio = false, resetQuiescent = false, sustain = false;
    bool prepared = false;
    bool constantOutput = false;
    float leftOutput = 0.25f, rightOutput = 0.25f;
    int resetsWhileUnprepared = 0;
    std::function<void()> duringReset;
    std::array<unsigned, 2048> held{};
};
class OfflineInstrument : public juce::AudioPluginInstance {
public:
    explicit OfflineInstrument(Probe& p) : probe(p) {}
    ~OfflineInstrument() override { ++probe.destroyed; probe.destroyedOnAudio = rendering; }
    const juce::String getName() const override { return "Offline instrument"; }
    void fillInPluginDescription(juce::PluginDescription&) const override {}
    void prepareToPlay(double, int) override { probe.prepared = true; }
    void releaseResources() override { probe.prepared = false; }
    void reset() override {
        ++probe.resets; probe.resetOnAudio = rendering;
        if (!probe.prepared) ++probe.resetsWhileUnprepared;
        const bool admitted = AudioQuiescence::instance().enter();
        probe.resetQuiescent = !admitted;
        if (admitted) AudioQuiescence::instance().leave();
        if (probe.duringReset) probe.duringReset();
        probe.sounding = 0; probe.held.fill(0);
        // Pedal parameter deliberately survives reset, like some mapped controls.
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
        ++probe.blocks;
        probe.lastInputCount = midi.getNumEvents();
        unsigned converted = 0;
        for (const auto metadata : midi) {
            probe.invalidInputOffset = probe.invalidInputOffset || metadata.samplePosition < 0 ||
                metadata.samplePosition >= buffer.getNumSamples();
            if (++converted > 2048) break; // JUCE counts ignored CCs too.
            if (metadata.numBytes != 3) continue;
            const auto status = metadata.data[0] & 0xf0;
            const auto index = (metadata.data[0] & 15) * 128 + (metadata.data[1] & 127);
            if ((status == 0x80 || status == 0x90) && probe.events.size() < probe.events.capacity())
                probe.events.push_back({probe.samples + metadata.samplePosition, metadata.samplePosition,
                    (metadata.data[0] & 15) + 1, metadata.data[1] & 127, metadata.data[2],
                    status == 0x90 && metadata.data[2] != 0});
            if (status == 0x90 && metadata.data[2] != 0) {
                ++probe.notes; ++probe.sounding; ++probe.held[index];
            } else if (status == 0x80 || status == 0x90) {
                ++probe.noteOffs; probe.lastOffSample = metadata.samplePosition;
                if (probe.held[index]) {
                    --probe.held[index];
                    if (!probe.sustain) --probe.sounding;
                }
            } else if (status == 0xb0 && metadata.data[1] == 64 && metadata.data[2] >= 64) {
                probe.sustain = true;
            }
            // Intentionally ignore ALL cleanup CCs, including sustain-off.
        }
        buffer.clear();
        probe.samples += buffer.getNumSamples();
        if (probe.sounding != 0 || probe.constantOutput)
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                juce::FloatVectorOperations::fill(buffer.getWritePointer(ch),
                    ch == 0 ? probe.leftOutput : probe.rightOutput, buffer.getNumSamples());
    }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { ++probe.editorCalls; return nullptr; }
    bool hasEditor() const override { return true; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
private:
    Probe& probe;
};

static void renderTests() {
    Probe first, second;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState transport;
    auto* a = channels.addChannel("A");
    auto* b = channels.addChannel("B");
    a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
    b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
    ChannelMixer mixer(channels, tracks, clips, transport);
    mixer.prepareToPlay(48000, 64);
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    midi.ensureSize(32768);
    auto render = [&] {
        CHECK(AudioQuiescence::instance().enter());
        rendering = true;
        mixer.processBlock(buffer, midi);
        rendering = false;
        AudioQuiescence::instance().leave();
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    };
    mixer.setActiveChannel(0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    render();
    CHECK(first.notes == 1 && second.notes == 0);
    const auto priorOffs = first.noteOffs;
    mixer.setActiveChannel(1);
    midi.addEvent(juce::MidiMessage::noteOn(1, 62, 0.5f), 0);
    render();
    CHECK(first.noteOffs == priorOffs + 1 && first.sounding == 0 && second.notes == 1);
    CHECK(first.blocks == 2 && second.blocks == 2);
    b->setMuted(true);
    midi.addEvent(juce::MidiMessage::noteOff(1, 62), 0);
    render(); CHECK(second.blocks == 3);
    CHECK(second.sounding == 0);
    b->setMuted(false); // T04 drops attacks while suppressed; resume on a fresh attack.
    channels.moveChannel(1, 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.5f), 0);
    render(); CHECK(second.notes == 2 && first.notes == 1);
    const auto stopOffs = second.noteOffs;
    transport.stop(); transport.setPlaying(true);
    render(); CHECK(second.noteOffs == stopOffs + 1 && second.sounding == 0);
    for (int i = 0; i < 2049; ++i) midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    render(); CHECK(second.notes == 2 && mixer.getOverflowCount() == 1);
    channels.removeChannel(0);
    CHECK(second.destroyed == 1 && !second.destroyedOnAudio);
    render();
    a->setPlugin(nullptr);
    CHECK(first.destroyed == 1 && !first.destroyedOnAudio);
    channels.clearChannels(); render();
    mixer.releaseResources();

    AudioEngine engine; // Never initialise: no audio device or application launch.
    AudioEngineTestAccess::prepare(engine, 48000, 64);
    engine.setProcessor(&mixer);
    auto deviceRender = [&] {
        rendering = true;
        AudioEngineTestAccess::render(engine, buffer);
        rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    };
    deviceRender();
    for (int i = 0; i < 2049; ++i)
        engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOn(1, 60, 0.5f));
    CHECK(engine.getMidiOverflowCount() == 1);
    deviceRender();
    { AudioQuiescence::Edit edit; deviceRender(); }
    CHECK(engine.getSkippedBlockCount() == 1);
    deviceRender();
    AudioEngineTestAccess::prepare(engine, 96000, 32);
    deviceRender(); CHECK(engine.getSkippedBlockCount() == 2);
    CHECK(buffer.getMagnitude(0, 64) == 0);
    engine.clearProcessor();

    auto* destination = channels.addChannel();
    auto dense = std::make_unique<MidiClip>(0, 4);
    for (size_t i = 0; i <= ArrangementSnapshot::maxNotes; ++i) dense->addNote(Note(60, 0, 1));
    auto id = clips.addClip(std::move(dense));
    tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, destination->getId(), 0, 4));
    const auto saturated = compileArrangement(tracks, clips, channels, 9);
    CHECK(saturated.overflow && saturated.notes.empty() && saturated.revision == 9);
}

static void reviewRegressionTests() {
    Probe probe;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState transport;
    auto* channel = channels.addChannel();
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
    CHECK(!channel->getPlugin()->hasEditor());
    CHECK(!channel->getPlugin()->createEditor() && probe.editorCalls == 0);
    ChannelMixer mixer(channels, tracks, clips, transport);
    mixer.setActiveChannel(0);
    AudioEngine engine;
    AudioEngineTestAccess::prepare(engine, 48000, 64);
    engine.setProcessor(&mixer);
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;
    midi.ensureSize(32768);
    auto render = [&] {
        CHECK(AudioQuiescence::instance().enter());
        rendering = true;
        mixer.processBlock(buffer, midi);
        rendering = false;
        AudioQuiescence::instance().leave();
        CHECK(renderAllocations == 0 && renderDeletions == 0);
        CHECK(probe.lastInputCount <= 2048);
    };
    auto deviceRender = [&] {
        rendering = true;
        AudioEngineTestAccess::render(engine, buffer);
        rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    };
    deviceRender(); // Consume preparation cleanup before delivering notes.
    auto holdMaximum = [&] {
        for (unsigned i = 0; i < Channel::maxDeliveredNotes; ++i) {
            midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
            if ((i + 1) % Channel::maxLiveEvents == 0) render();
        }
        if (!midi.isEmpty()) render();
        CHECK(probe.sounding == static_cast<int>(Channel::maxDeliveredNotes));
    };
    holdMaximum();
    transport.stop();
    for (unsigned i = 0; i < Channel::maxLiveEvents - 1; ++i)
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 63);
    render();
    CHECK(probe.lastInputCount == 2048 && probe.lastOffSample == 63);
    CHECK(probe.noteOffs == 1025 && probe.sounding == 0);

    // A full 2048-event live batch cannot consume the cleanup reserve. Reject the
    // entire batch, not a prefix ending before its late note-off.
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render();
    auto notesBefore = probe.notes;
    auto overflowBefore = mixer.getOverflowCount();
    for (int i = 0; i < 2047; ++i) midi.addEvent(juce::MidiMessage::noteOn(1, 61, 0.5f), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 63);
    render();
    CHECK(probe.notes == notesBefore && probe.sounding == 0);
    CHECK(mixer.getOverflowCount() == overflowBefore + 1);
    holdMaximum();
    notesBefore = probe.notes;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render();
    CHECK(probe.notes == notesBefore && probe.sounding == 0); // Outstanding-note cap, including repeats.

    // Unmapped cleanup CCs cannot release a sustained voice. Silence immediately,
    // then reset via the message-thread polling seam under actual quiescence.
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render();
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0); render();
    CHECK(probe.sounding == 1);
    transport.stop(); render();
    CHECK(channel->isVoiceResetPending() && probe.sounding == 1);
    CHECK(buffer.getMagnitude(0, 64) == 0);
    const auto blocksBefore = probe.blocks;
    render(); CHECK(probe.blocks == blocksBefore && buffer.getMagnitude(0, 64) == 0);
    const auto skipsBeforeReset = engine.getSkippedBlockCount();
    probe.duringReset = [&] { deviceRender(); };
    ChannelTestAccess::resetVoices(*channel);
    probe.duringReset = {};
    CHECK(!channel->isVoiceResetPending() && probe.sounding == 0);
    CHECK(probe.resets == 1 && !probe.resetOnAudio && probe.resetQuiescent);
    CHECK(engine.getSkippedBlockCount() == skipsBeforeReset + 1);
    const auto blocksAfterReset = probe.blocks;
    for (int i = 0; i < 3; ++i) {
        deviceRender(); // Sticky panic from the skipped callback, with no fresh MIDI.
        CHECK(!channel->isVoiceResetPending());
        ChannelTestAccess::resetVoices(*channel);
    }
    CHECK(probe.resets == 1 && probe.blocks == blocksAfterReset + 3);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render();
    transport.stop(); render();
    CHECK(channel->isVoiceResetPending()); // Pedal mapping can persist across reset.
    const auto overflowDuringReset = engine.getMidiOverflowCount();
    const auto notesBeforeReset = probe.notes;
    probe.duringReset = [&] {
        deviceRender();
        const unsigned char payload[]{1, 2};
        engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::createSysExMessage(payload, 2));
        engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOn(1, 62, 0.5f));
    };
    ChannelTestAccess::resetVoices(*channel);
    probe.duringReset = {};
    CHECK(probe.resets == 2 && probe.sounding == 0);
    CHECK(engine.getMidiOverflowCount() == overflowDuringReset + 1);
    deviceRender();
    CHECK(probe.notes == notesBeforeReset && !channel->isVoiceResetPending());
    ChannelTestAccess::resetVoices(*channel);
    CHECK(probe.resets == 2); // Independent ingress panic was not cleared to break the loop.

    // A reset pending at device release must not reactivate an unprepared plugin.
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render();
    transport.stop(); render();
    CHECK(channel->isVoiceResetPending());
    mixer.releaseResources();
    CHECK(!channel->isPrepared() && !probe.prepared);
    ChannelTestAccess::resetVoices(*channel);
    ChannelTestAccess::resetVoices(*channel);
    CHECK(probe.resets == 2 && probe.resetsWhileUnprepared == 0);
    CHECK(channel->isVoiceResetPending() && !probe.prepared);
    probe.duringReset = [&] { deviceRender(); };
    mixer.prepareToPlay(48000, 64);
    probe.duringReset = {};
    CHECK(probe.resets == 3 && probe.resetsWhileUnprepared == 0);
    CHECK(channel->isPrepared() && probe.prepared && !channel->isVoiceResetPending());
    CHECK(probe.sounding == 0 && probe.resetQuiescent && !probe.resetOnAudio);
    deviceRender(); ChannelTestAccess::resetVoices(*channel);
    CHECK(probe.resets == 3 && !channel->isVoiceResetPending());

    // New instance clears the conservative pedal latch; editor creation is still denied.
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
    probe.sustain = false;
    juce::MidiKeyboardState keyboard;
    keyboard.addListener(&engine);
    MidiManager manager;
    manager.setAudioDestination(engine, keyboard);
    keyboard.noteOn(1, 65, 0.5f); deviceRender();
    CHECK(probe.sounding == 1 && keyboard.isNoteOn(1, 65));
    const auto ingressOverflow = engine.getMidiOverflowCount();
    for (int i = 0; i < 2049; ++i) {
        manager.sendMidiMessage(juce::MidiMessage::controllerEvent(1, 7, 100));
        if (i % 500 == 499) deviceRender();
    }
    deviceRender(); // Audio ingress never saturates, only the feedback queue does.
    CHECK(engine.getMidiOverflowCount() == ingressOverflow && probe.sounding == 1);
    MidiManagerTestAccess::drain(manager);
    CHECK(!keyboard.isNoteOn(1, 65));
    const auto offsBeforeFeedbackPanic = probe.noteOffs;
    deviceRender();
    CHECK(probe.sounding == 0 && probe.noteOffs == offsBeforeFeedbackPanic + 1);

    keyboard.noteOn(1, 66, 0.5f); deviceRender();
    const auto skipsBefore = engine.getSkippedBlockCount();
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i) buffer.setSample(ch, i, 0.375f);
    rendering = true;
    AudioEngineTestAccess::renderSamples(engine, buffer, -1);
    AudioEngineTestAccess::renderSamples(engine, buffer, 0);
    rendering = false;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i) CHECK(buffer.getSample(ch, i) == 0.375f);
    CHECK(engine.getSkippedBlockCount() == skipsBefore + 2);
    deviceRender(); CHECK(probe.sounding == 0); // Nonpositive callbacks still request cleanup.
    keyboard.removeListener(&engine);
    engine.clearProcessor();
}

static void modelTests() {
    Project project;
    auto& channels = project.getChannelList();
    auto& pool = project.getClipPool();
    auto& tracks = project.getTrackList();
    auto& transport = project.getTransportState();
    PoolChanges deletion(pool);
    auto* a = channels.addChannel("A");
    auto* b = channels.addChannel("B");
    const auto aId = a->getId(), bId = b->getId();
    project.setActiveChannel(0);
    auto source = std::make_unique<MidiClip>(5, 4);
    auto* midi = source.get();
    midi->setLoopEnabled(true);
    ClipChanges clipChanges;
    midi->addListener(&clipChanges);
    midi->addNote(Note(62, 3.5, 1.5));
    midi->addNote(Note(60, 0, 1));
    midi->addNote(Note(64, 4, 1));
    CHECK(clipChanges.invalidations == 3);
    auto* borrowed = midi->findNoteAt(0, 60);
    midi->updateNote(borrowed, Note(61, 0, 1));
    CHECK(midi->findNoteAt(0, 61) == borrowed);
    CHECK(clipChanges.invalidations == 3 && clipChanges.changes == 4);
    const auto sourceId = pool.addClip(std::move(source));
    auto* x = tracks.addTrack("X");
    auto* y = tracks.addTrack("Y");
    Changes trackChanges;
    x->addChangeListener(&trackChanges);
    x->addClipInstance(std::make_unique<ClipInstance>(sourceId, aId, 8, 6));
    x->addClipInstance(std::make_unique<ClipInstance>(sourceId, bId, 0, 2));
    y->addClipInstance(std::make_unique<ClipInstance>(sourceId, aId, 0, 4));
    x->sendSynchronousChangeMessage();
    CHECK(trackChanges.count > 0);
    channels.moveChannel(0, 1);
    CHECK(channels.getChannel(0) == b && channels.getChannelById(aId) == a);
    CHECK(project.getActiveChannelId() == aId && project.getActiveChannel() == 1);
    CHECK(x->getClipInstance(0)->getChannelId() == aId);
    CHECK(x->getClipInstance(1)->getChannelId() == bId);
    CHECK(y->getClipInstance(0)->getChannelId() == aId);
    auto snapshot = compileArrangement(tracks, pool, channels, 1);
    CHECK(snapshot.notes.size() == 5);
    CHECK(snapshot.notes.back().start == 11.5 && snapshot.notes.back().end == 12);
    CHECK(snapshot.notes.front().pitch == 61);
    ArrangementPublisher publisher(tracks, pool, channels);
    const auto& old = publisher.acquire();
    const auto revision = old.revision;
    midi->clearNotes();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    CHECK(old.revision == revision && old.notes.size() == 5);
    CHECK(publisher.acquire().notes.empty());
    midi->addNote(Note(60, 0, 1));
    const auto previousRevision = publisher.acquire().revision;
    x->getClipInstance(0)->setMuted(true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    CHECK(publisher.acquire().revision != previousRevision);
    x->getClipInstance(0)->setMuted(false);
    x->removeClipInstance(1);
    CHECK(pool.getClip(sourceId) == midi && y->getNumClipInstances() == 1);

    for (double tempo : {120.0, 60.0, 90.0}) {
        transport.setTempo(tempo); transport.setPositionInBeats(4);
        CHECK(std::abs(transport.getPosition() - 240.0 / tempo) < 1e-10);
        CHECK(std::abs(transport.getPositionInBeats() - 4) < 1e-10);
        CHECK(midi->getDuration() == 4 && x->getClipInstance(0)->getStartTime() == 8);
    }
    transport.stop();
    transport.setPlaying(true);
    for (int i = 0; i < 10000; ++i) transport.setPosition(i);
    CHECK(transport.acquireControl().stopGeneration != 0);
    CHECK(transport.acquireControl().playing);
    transport.setLoopRegion(2, 6); transport.setLoopEnabled(true);
    CHECK(transport.acquireControl().loop.endBeats == 6);
    transport.publishRenderPosition({2.5, 120, 48000, {}, false, 7, 0});
    CHECK(transport.acquireRenderPosition().beats == 2.5);
    CHECK(transport.acquireRenderPosition().revision == 7);
    ClipInstance interval(sourceId, aId, 8, 4);
    CHECK(interval.containsTime(8) && !interval.containsTime(12) && !interval.overlapsRange(12, 16));
    CHECK(!Note(60, 0, 1).containsTime(1));
    for (double bad : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        interval.setStartTime(bad); CHECK(interval.getStartTime() == 8);
        interval.setDuration(bad); CHECK(interval.getDuration() == 4);
        CHECK(!midi->addNote(Note(60, bad, 1)));
        x->addClipInstance(std::make_unique<ClipInstance>(sourceId, aId, bad, 1));
    }
    x->addClipInstance(nullptr);
    x->addClipInstance(std::make_unique<ClipInstance>(-1, aId, 0, 1));
    CHECK(x->getNumClipInstances() == 1);
    CHECK(pool.addClip(nullptr) == InvalidClipId);
    AudioClip audio(0, 2); CHECK(audio.getDuration() == 2);

    ChannelChanges changes;
    channels.addListener(&changes);
    a->setName("renamed"); a->setMuted(true); a->setPlugin(nullptr);
    a->setSampleFile({}); a->setColour(juce::Colours::red); a->setVolume(0.5f);
    a->setPan(0.25f); a->setMixerTrackId(8);
    a->sendSynchronousChangeMessage();
    CHECK(changes.changed > 0 && changes.last == aId);
    const auto count = changes.changed;
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer events;
    a->prepareToPlay(48000, 64); buffer.clear(); a->processBlock(buffer, events); a->releaseResources();
    CHECK(changes.changed == count);
    a->setMuted(false); channels.moveChannel(1, 0); a->sendSynchronousChangeMessage();
    CHECK(changes.last == aId);
    a->setName("pending deletion");
    channels.removeChannel(1);
    CHECK(project.getActiveChannelId() == aId && project.getActiveChannel() == 0);
    channels.removeChannel(0);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    CHECK(project.getActiveChannelId() == -1 && project.getActiveChannel() == -1);
    CHECK(compileArrangement(tracks, pool, channels, 8).notes.empty());
    project.setActiveChannel(999); CHECK(project.getActiveChannel() == -1);
    auto* c = channels.addChannel(); CHECK(c->getId() != aId && c->getId() != bId);
    CHECK(!channels.getChannelById(aId) && !channels.getChannelById(-1));
    channels.clearChannels(); channels.addChannel();
    CHECK(!channels.getChannelById(aId));
    channels.removeListener(&changes);
    midi->removeListener(&clipChanges);
    pool.removeClip(sourceId);
    CHECK(x->getClipInstance(0)->getClipId() == sourceId);
    auto next = pool.addClip(std::make_unique<MidiClip>()); CHECK(next != sourceId);
    pool.clearClips();
    CHECK(pool.addClip(std::make_unique<MidiClip>()) != next);
    pool.removeClip(sourceId);
    CHECK(deletion.before == deletion.after && deletion.before == 2);
    x->removeChangeListener(&trackChanges);

    ChannelMixer mixer(channels, tracks, pool, transport);
    mixer.prepareToPlay(48000, 64);
    for (int i = 0; i < 2100; ++i) events.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    mixer.processBlock(buffer, events);
    CHECK(mixer.getOverflowCount() == 1 && events.isEmpty());
    juce::AudioBuffer<float> oversized(2, 65);
    mixer.processBlock(oversized, events); CHECK(mixer.getOverflowCount() == 2);
    channels.clearChannels(); mixer.processBlock(buffer, events);
    CHECK(buffer.getMagnitude(0, 64) == 0);
    mixer.releaseResources(); mixer.prepareToPlay(96000, 32); mixer.releaseResources();
}

static void transportClockTests() {
    constexpr double tolerance = 1e-10; // Quarter-note beats for all sample-count comparisons.
    for (int size : {1, 7, 64, 127, 512, 4096, 96000}) {
        TransportState state;
        TransportClock clock;
        state.setPlaying(true);
        double end = 0;
        for (int rendered = 0; rendered < 96000;) {
            const int count = std::min(size, 96000 - rendered);
            rendering = true;
            const auto block = clock.beginBlock(state, count, 48000);
            clock.endBlock(state, block);
            rendering = false;
            CHECK(block.startBeats == end && block.numSamples == count);
            end = block.endBeats;
            rendered += count;
        }
        CHECK(std::abs(end - 4.0) < tolerance);
        CHECK(state.getPositionInBeats() == 0); // No polling, no UI clock, no listener side effects.
        state.pollRenderPosition();
        CHECK(std::abs(state.getPositionInBeats() - 4.0) < tolerance);
    }

    {
        TransportState longRun;
        TransportClock clock;
        longRun.setTempo(137);
        longRun.setPlaying(true);
        const int total = 44100 * 3600;
        for (int samples = 0; samples < total;) {
            int count = std::min(127, total - samples);
            auto block = clock.beginBlock(longRun, count, 44100);
            clock.endBlock(longRun, block);
            samples += count;
        }
        longRun.pollRenderPosition();
        CHECK(std::abs(longRun.getPositionInBeats() - 137.0 * 60) < tolerance);
    }

    struct Listener : TransportListener {
        int positions = 0;
        void transportPositionChanged(double) override {
            CHECK(!rendering && juce::MessageManager::getInstance()->isThisTheMessageThread());
            ++positions;
        }
    } listener;
    TransportState state;
    TransportClock clock;
    state.addListener(&listener);
    auto render = [&](int count = 480, double rate = 48000) {
        rendering = true;
        auto block = clock.beginBlock(state, count, rate);
        clock.endBlock(state, block);
        rendering = false;
        return block;
    };
    CHECK(render().endBeats == 0);
    state.setPositionInBeats(8);
    auto block = render();
    CHECK(block.startBeats == 8 && block.endBeats == 8 && block.discontinuity);
    auto revision = block.revision;
    state.setPlaying(true);
    block = render();
    CHECK(block.startBeats == 8 && std::abs(block.endBeats - 8.02) < tolerance);
    CHECK(!block.discontinuity && block.revision == revision);
    // A seek made after rendering but before polling cannot be overwritten by old feedback.
    state.setPositionInBeats(32);
    state.pollRenderPosition();
    CHECK(state.getPositionInBeats() == 32);
    state.setTempo(60);
    state.setTimeSignature(6, 8);
    block = render();
    CHECK(block.startBeats == 32 && block.tempo == 60 && block.meter.denominator == 8);
    CHECK(std::abs(block.endBeats - 32.01) < tolerance && block.discontinuity);
    state.pollRenderPosition();
    const auto beforeTempo = state.getPositionInBeats();
    state.setTempo(180);
    CHECK(state.getPositionInBeats() == beforeTempo);
    CHECK(std::abs(state.getPosition() - beforeTempo / 3) < tolerance);
    block = render();
    CHECK(block.startBeats == beforeTempo && std::abs(block.endBeats - beforeTempo - 0.03) < tolerance);
    const auto beforeRate = block.endBeats;
    block = render(960, 96000);
    CHECK(block.startBeats == beforeRate && std::abs(block.endBeats - beforeRate - 0.03) < tolerance);
    CHECK(block.discontinuity);
    state.stop();
    block = render();
    CHECK(!block.playing && block.discontinuity && block.startBeats == block.endBeats);
    state.pollRenderPosition();
    const auto stopped = state.getPositionInBeats();
    CHECK(render().endBeats == stopped);
    // Coalescing cannot lose cleanup, and publishing unrelated fields cannot replay a seek.
    state.stop(); state.setPlaying(true);
    for (int i = 0; i < 10000; ++i) state.setTimeSignature(i % 7 + 1, 8);
    block = render();
    CHECK(block.playing && block.discontinuity && block.startBeats == stopped);
    state.setMetronomeEnabled(true); state.setLoopRegion(0, 1); state.setLoopEnabled(true);
    block = render();
    CHECK(block.startBeats == 0 && block.endBeats < 1 && block.discontinuity);
    state.reset();
    block = render();
    CHECK(block.startBeats == 0 && block.playing && block.discontinuity);
    state.setPositionInBeats(0); state.setPositionInBeats(0); // Same-position seeks still acknowledge.
    CHECK(render().discontinuity);
    state.setRecording(true); CHECK(!state.isRecording());
    for (double bad : {-1.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN(), 1.0e100}) {
        state.setPositionInBeats(bad); CHECK(state.getPositionInBeats() == 0);
        state.setPosition(bad); CHECK(state.getPositionInBeats() == 0);
    }
    state.setTempo(std::numeric_limits<double>::quiet_NaN()); CHECK(state.getTempo() == 180);
    state.setTempo(0); CHECK(state.getTempo() == 20);
    state.setTempo(1000); CHECK(state.getTempo() == 300);
    state.setTimeSignature(0, 3);
    CHECK(state.getTimeSignature().numerator == 1 && state.getTimeSignature().denominator == 4);
    CHECK(TransportState::formatBarsBeatsTicks(4, {4, 4}) == "2:1:000");
    CHECK(TransportState::formatBarsBeatsTicks(3, {6, 8}) == "2:1:000");
    CHECK(TransportState::formatBarsBeatsTicks(0.75, {6, 8}) == "1:2:480");
    CHECK(TransportState::formatBarsBeatsTicks(3.5, {7, 8}) == "2:1:000");
    CHECK(TransportState::formatBarsBeatsTicks(1, {2, 2}) == "1:1:480");
    CHECK(TransportState::formatBarsBeatsTicks(0.25, {4, 16}) == "1:2:000");
    CHECK(listener.positions > 0);
    state.removeListener(&listener);

    // Exercise both SPSC directions concurrently; only main publishes UI commands/polls.
    TransportState concurrent;
    TransportClock concurrentClock;
    concurrent.setPlaying(true);
    std::atomic<bool> finished{false};
    std::thread audio([&] {
        for (int i = 0; i < 100000; ++i) {
            auto timing = concurrentClock.beginBlock(concurrent, 7, 44100);
            concurrentClock.endBlock(concurrent, timing);
        }
        finished.store(true);
    });
    while (!finished.load()) {
        concurrent.setTimeSignature(7, 8);
        concurrent.pollRenderPosition();
        CHECK(concurrent.getPositionInBeats() >= 0);
    }
    audio.join();
    concurrent.pollRenderPosition();
    CHECK(std::abs(concurrent.getPositionInBeats() - 700000.0 * 2 / 44100) < tolerance);
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static void transportEngineTests() {
    Probe probe;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState state;
    auto* channel = channels.addChannel();
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.setActiveChannel(0);
    AudioEngine engine;
    engine.setProcessor(&mixer);
    AudioEngineTestAccess::prepare(engine, 48000, 512);
    juce::AudioBuffer<float> buffer(2, 512);
    auto render = [&](int samples) {
        rendering = true;
        AudioEngineTestAccess::renderSamples(engine, buffer, samples);
        rendering = false;
        state.pollRenderPosition();
    };
    state.setPlaying(true);
    render(512);
    double end = state.getPositionInBeats();
    CHECK(std::abs(end - 512.0 / 24000) < 1e-10);
    auto revision = state.acquireRenderPosition().revision;
    { AudioQuiescence::Edit edit; render(512); }
    CHECK(state.getPositionInBeats() == end);
    render(0); render(-1);
    CHECK(state.getPositionInBeats() == end);
    render(128);
    CHECK(state.acquireRenderPosition().revision != revision);
    CHECK(std::abs(state.getPositionInBeats() - end - 128.0 / 24000) < 1e-10);
    AudioEngineTestAccess::prepare(engine, 96000, 256);
    end = state.getPositionInBeats();
    render(512); // Oversized: silence, freeze, cleanup at next admitted block.
    CHECK(state.getPositionInBeats() == end);
    render(256);
    CHECK(std::abs(state.getPositionInBeats() - end - 256.0 / 48000) < 1e-10);
    AudioEngineTestAccess::prepare(engine, 0, 256);
    end = state.getPositionInBeats(); render(128);
    CHECK(state.getPositionInBeats() == end);
    AudioEngineTestAccess::prepare(engine, 96000, 256);
    state.stop(); render(256); CHECK(state.getPositionInBeats() == end);
    state.setPositionInBeats(16); render(256); CHECK(state.getPositionInBeats() == 16);
    state.setPlaying(true); render(128);
    CHECK(std::abs(state.getPositionInBeats() - 16 - 128.0 / 48000) < 1e-10);
    engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOn(1, 60, 0.5f));
    render(128); CHECK(probe.sounding == 1);
    const auto offs = probe.noteOffs;
    state.setPositionInBeats(24);
    render(128);
    CHECK(probe.sounding == 0 && probe.noteOffs == offs + 1 && probe.lastOffSample == 0);
    state.stop(); render(128);
    engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOn(1, 61, 0.5f));
    render(128); CHECK(probe.sounding == 1); // Live audition remains available stopped.
    state.setPositionInBeats(24); render(128); CHECK(probe.sounding == 0);
    end = state.getPositionInBeats();
    mixer.releaseResources(); render(128); CHECK(state.getPositionInBeats() == end);
    mixer.prepareToPlay(96000, 256);
    state.setPlaying(true); render(128);
    CHECK(std::abs(state.getPositionInBeats() - end - 128.0 / 48000) < 1e-10);
    engine.clearProcessor();
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static void arrangementPlaybackTests() {
    // Exact absolute event times must be invariant under block partitioning. Source
    // origin/loop flag are deliberately irrelevant; placement tails are silent.
    for (double rate : {48000.0, 44100.0}) for (double tempo : {120.0, 137.0})
    for (int size : {7, 127, 512, 4096}) {
        Probe first, second;
        ChannelList channels;
        TrackList tracks;
        ClipPool clips;
        TransportState state;
        auto* a = channels.addChannel("A");
        auto* b = channels.addChannel("B");
        a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
        b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
        auto source = std::make_unique<MidiClip>(9, 4);
        source->setLoopEnabled(true);
        source->addNote(Note(62, 3.5, 1)); // Truncated at source end.
        source->addNote(Note(60, 0, 1));
        source->addNote(Note(60, 1, 0.5)); // Touching, not overlapping.
        source->addNote(Note(60, 0.5, 0.75)); // Union [0, 1.5), so next attack joins too.
        source->addNote(Note(64, 2, 0.5));
        source->addNote(Note(65, 2, 0.5));
        source->addNote(Note(66, 4, 1));
        source->addNote(Note(67, 0, 1, 0));
        Note muted(68, 0, 1); muted.setMuted(true); source->addNote(muted);
        Note ch2(60, 2, 0.25); ch2.setChannel(2); source->addNote(ch2);
        const auto id = clips.addClip(std::move(source));
        auto* x = tracks.addTrack();
        x->addClipInstance(std::make_unique<ClipInstance>(id, a->getId(), 0, 6));
        x->addClipInstance(std::make_unique<ClipInstance>(id, b->getId(), 0, 2.25));
        tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, a->getId(), 6, 4));
        ChannelMixer mixer(channels, tracks, clips, state);
        mixer.setActiveChannel(1);
        mixer.prepareToPlay(rate, size);
        state.setTempo(tempo); state.setPlaying(true);
        juce::AudioBuffer<float> buffer(2, size);
        juce::MidiBuffer midi; midi.ensureSize(32768);
        const double spb = rate * 60 / tempo;
        const int total = static_cast<int>(std::ceil(10 * spb)) + 1;
        int blocks = 0;
        for (int time = 0; time < total;) {
            const int n = std::min(size, total - time);
            float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
            juce::AudioBuffer<float> block(pointers, 2, n);
            rendering = true; mixer.processBlock(block, midi); rendering = false;
            CHECK(renderAllocations == 0 && renderDeletions == 0);
            CHECK(first.lastInputCount <= 2048 && second.lastInputCount <= 2048);
            time += n; ++blocks;
        }
        CHECK(first.blocks == blocks && second.blocks == blocks);
        CHECK(first.sounding == 0 && second.sounding == 0);
        const auto verify = [&](const Probe& p, std::vector<std::tuple<double, bool, int, int>> expected) {
            std::sort(expected.begin(), expected.end());
            CHECK(p.events.size() == expected.size());
            for (size_t i = 0; i < expected.size(); ++i) {
                const auto [beat, on, channel, pitch] = expected[i];
                const auto sample = static_cast<long long>(std::floor(beat * spb + 1e-7));
                CHECK(p.events[i].time == sample && p.events[i].offset == sample % size);
                CHECK(p.events[i].on == on && p.events[i].channel == channel && p.events[i].pitch == pitch);
            }
        };
        std::vector<std::tuple<double, bool, int, int>> expected;
        for (double start : {0.0, 6.0}) {
            expected.insert(expected.end(), {{start, true, 1, 60}, {start + 1.5, false, 1, 60},
                {start + 2, true, 1, 64}, {start + 2.5, false, 1, 64},
                {start + 2, true, 1, 65}, {start + 2.5, false, 1, 65},
                {start + 2, true, 2, 60}, {start + 2.25, false, 2, 60},
                {start + 3.5, true, 1, 62}, {start + 4, false, 1, 62}});
        }
        verify(first, expected);
        verify(second, {{0, true, 1, 60}, {1.5, false, 1, 60}, {2, true, 1, 64},
            {2.25, false, 1, 64}, {2, true, 1, 65}, {2.25, false, 1, 65},
            {2, true, 2, 60}, {2.25, false, 2, 60}});
        CHECK(mixer.getOverflowCount() == 0);
    }
}

static void arrangementLifecycleTests() {
    Probe first, second;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState state;
    auto* a = channels.addChannel("A");
    auto* b = channels.addChannel("B");
    a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
    b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
    auto source = std::make_unique<MidiClip>(0, 4);
    auto* notes = source.get();
    notes->addNote(Note(60, 0, 1));
    notes->addNote(Note(60, 1, 1));
    notes->addNote(Note(62, 2, 1));
    const auto id = clips.addClip(std::move(source));
    auto* track = tracks.addTrack();
    track->addClipInstance(std::make_unique<ClipInstance>(id, a->getId(), 0, 4));
    auto* placement = track->getClipInstance(0);
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.prepareToPlay(48000, 24000);
    mixer.setActiveChannel(0);
    juce::AudioBuffer<float> buffer(2, 24000);
    juce::MidiBuffer midi; midi.ensureSize(32768);
    auto render = [&](int n = 24000) {
        float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n);
        rendering = true; mixer.processBlock(block, midi); rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
        CHECK(first.lastInputCount <= 2048 && second.lastInputCount <= 2048);
    };
    auto dispatch = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil(20); };
    auto restart = [&] { state.setPositionInBeats(0); state.setPlaying(true); };
    restart(); render();
    CHECK(first.events.size() == 1 && first.events[0].time == 0 && first.events[0].offset == 0);
    render();
    CHECK(first.events.size() == 3 && !first.events[1].on && first.events[2].on);
    CHECK(first.events[1].time == 24000 && first.events[2].time == 24000);
    render(); render(); CHECK(first.sounding == 0);

    // Live preempts an arrangement voice at its actual offset. Its late release
    // cannot kill the next arrangement attack; missed attacks are never chased.
    restart(); render(12000);
    const auto preempt = first.events.size();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 17);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 101);
    render(12000);
    CHECK(first.events.size() == preempt + 3);
    CHECK(!first.events[preempt].on && first.events[preempt].offset == 17);
    CHECK(first.events[preempt + 1].on && first.events[preempt + 1].offset == 17);
    CHECK(!first.events[preempt + 2].on && first.events[preempt + 2].offset == 101);
    CHECK(first.sounding == 0);
    render(12000); CHECK(first.sounding == 1); // Next arrangement attack at b1.
    const auto stale = first.events.size();
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 5); render(12000);
    CHECK(first.events.size() == stale && first.sounding == 1);

    state.stop(); render(1);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render(1);
    CHECK(first.sounding == 1); // Stopped audition.
    state.setPlaying(true); // b2: independent pitch 62.
    render(1); CHECK(first.sounding == 2);
    mixer.setActiveChannel(1); render(1);
    CHECK(first.sounding == 1 && first.held[62] == 1); // Only live pitch was cleaned.

    restart(); render(12000);
    const auto seekOffs = first.noteOffs;
    state.setPositionInBeats(0.75); render(6000);
    CHECK(first.sounding == 0 && first.noteOffs == seekOffs + 1 && first.lastOffSample == 0);
    const auto attacks = first.notes;
    render(1); CHECK(first.notes == attacks + 1); // Exact b1 attack after seek through b0 note.
    state.setPositionInBeats(1.5); render(12001);
    CHECK(first.held[60] == 0 && first.held[62] == 1); // No orphan b2 off for missed b1 attack.

    // Publication is real AsyncUpdater delivery, not direct test injection.
    restart(); render(12000); placement->setMuted(true); dispatch(); render(1);
    CHECK(first.sounding == 0 && first.lastOffSample == 0);
    placement->setMuted(false); dispatch(); render(1); CHECK(first.sounding == 0);
    restart(); render(12000); notes->setMuted(true); dispatch(); render(1); CHECK(first.sounding == 0);
    notes->setMuted(false); dispatch(); restart(); render(12000);
    Note edited(65, 1, 1); notes->updateNote(&notes->getNotes()[0], edited); dispatch(); render(12000);
    CHECK(first.sounding == 0); // Replacement cleans, no chase of old note.
    render(1); CHECK(first.held[60] == 1 && first.held[65] == 1);
    placement->setChannelId(b->getId()); dispatch(); render(1);
    CHECK(first.sounding == 0 && second.sounding == 0);
    restart(); render(24000); render(1); CHECK(second.sounding == 2);
    channels.moveChannel(1, 0); dispatch(); render(1); // Stable ID remains B, conservative revision cleanup.
    restart(); render(24000); render(1); CHECK(second.sounding == 2 && first.sounding == 0);
    b->setMuted(true); dispatch(); render(1); CHECK(second.sounding == 0);
    b->setMuted(false); dispatch(); restart(); render(24000); render(1); CHECK(second.sounding == 2);
    track->removeClipInstance(0); dispatch(); render(1); CHECK(second.sounding == 0);
    track->addClipInstance(std::make_unique<ClipInstance>(id, b->getId(), 0, 4)); dispatch();
    restart(); render(24000); render(1); CHECK(second.sounding == 2);
    clips.removeClip(id); dispatch(); render(1); CHECK(second.sounding == 0);
    channels.removeChannel(0); dispatch(); render(1);
    CHECK(second.destroyed == 1 && !second.destroyedOnAudio && first.sounding == 0);
}

static void arrangementCapacityTests() {
    Probe probe, replacement, other;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState state;
    auto* channel = channels.addChannel();
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
    auto* independent = channels.addChannel();
    independent->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(other)));
    auto source = std::make_unique<MidiClip>(0, 8);
    auto* notes = source.get();
    notes->addNote(Note(60, 0, 1)); notes->addNote(Note(60, 1, 1));
    const auto id = clips.addClip(std::move(source));
    tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 0, 8));
    auto independentSource = std::make_unique<MidiClip>(0, 8);
    independentSource->addNote(Note(70, 0, 8));
    const auto independentId = clips.addClip(std::move(independentSource));
    tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(independentId, independent->getId(), 0, 8));
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.setActiveChannel(0); mixer.prepareToPlay(48000, 24000);
    juce::AudioBuffer<float> buffer(2, 24000);
    juce::MidiBuffer midi; midi.ensureSize(32768);
    auto render = [&](int n = 24000) {
        float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n);
        rendering = true; mixer.processBlock(block, midi); rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0 && probe.lastInputCount <= 2048);
    };
    auto dispatch = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil(20); };
    auto restart = [&] { state.setPositionInBeats(0); state.setPlaying(true); };
    render(1); // Initial cleanup while stopped.
    const auto beforeTap = probe.notes;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    render(1); CHECK(probe.notes == beforeTap && probe.sounding == 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0); render(1);
    const auto held = probe.notes;
    state.setPlaying(true);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 23); render();
    CHECK(probe.notes == held && probe.sounding == 0 && probe.lastOffSample == 23);
    render(); CHECK(probe.notes == held + 1 && probe.sounding == 1);
    render(1); CHECK(probe.sounding == 0);

    // Quantized sub-sample silence and both sides of an exact 64-sample edge.
    notes->clearNotes();
    notes->addNote(Note(61, 63.25 / 24000, 0.25 / 24000));
    notes->addNote(Note(62, 63.75 / 24000, 0.5 / 24000));
    notes->addNote(Note(63, 64.0 / 24000, 64.0 / 24000));
    dispatch(); restart(); probe.events.clear(); render(64);
    CHECK(probe.events.size() == 1 && probe.events[0].pitch == 62 && probe.events[0].offset == 63);
    render(64);
    CHECK(probe.events.size() == 3 && !probe.events[1].on && probe.events[1].offset == 0);
    CHECK(probe.events[2].pitch == 63 && probe.events[2].on && probe.events[2].offset == 0);
    render(1); CHECK(probe.events.size() == 4 && probe.events[3].offset == 0 && probe.sounding == 0);

    notes->clearNotes();
    notes->addNote(Note(60, 0.25, 1.5, 120)); // Later attack inserted first.
    notes->addNote(Note(60, 0, 1, 30));
    dispatch(); restart(); probe.events.clear(); render(); render();
    CHECK(probe.events.size() == 2 && probe.events[0].on && probe.events[0].velocity == 30);
    CHECK(!probe.events[1].on && probe.events[1].offset == 18000 && probe.sounding == 0);

    // Tempo changes preserve a held lifetime and recompute its precise end; a
    // sample-rate reprepare cleans without chasing, then schedules future attacks.
    notes->clearNotes(); notes->addNote(Note(60, 0, 1)); notes->addNote(Note(62, 1, 1));
    dispatch(); restart(); render(12000);
    state.setTempo(60); render(23999); CHECK(probe.held[60] == 1);
    render(2); CHECK(probe.lastOffSample == 1 && probe.held[62] == 1 && probe.held[60] == 0);
    mixer.prepareToPlay(96000, 24000); render(1);
    CHECK(probe.sounding == 0 && probe.lastOffSample == 0);
    state.setTempo(120); mixer.prepareToPlay(48000, 24000);

    notes->clearNotes();
    for (unsigned i = 0; i < Channel::maxLiveEvents - 1; ++i) {
        Note note(i % 128, 0, 1); note.setChannel(i / 128 + 1); notes->addNote(note);
    }
    dispatch(); restart();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), 0);
    const auto accepted = probe.notes; render();
    CHECK(probe.notes == accepted + 975 && probe.sounding == 975);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), 23999); render();
    CHECK(probe.sounding == 0 && probe.lastInputCount == 976 && probe.lastOffSample == 0);
    restart(); render(); CHECK(probe.sounding == 975);
    auto rejected = probe.notes;
    auto overflows = mixer.getOverflowCount();
    // 975 arrangement releases + two live messages exceeds the shared budget.
    midi.addEvent(juce::MidiMessage::noteOn(16, 127, 0.5f), 0);
    midi.addEvent(juce::MidiMessage::noteOff(16, 127), 23999); render();
    CHECK(probe.notes == rejected && probe.sounding == 0 && probe.lastOffSample == 0);
    CHECK(mixer.getOverflowCount() == overflows + 1);
    CHECK(other.sounding == 1); // Destination-local rejection cannot silence another instrument.

    for (unsigned i = 975; i < 977; ++i) {
        Note note(i % 128, 0, 1); note.setChannel(i / 128 + 1); notes->addNote(note);
    }
    dispatch(); restart(); rejected = probe.notes; overflows = mixer.getOverflowCount(); render();
    CHECK(probe.notes == rejected && probe.sounding == 0 && mixer.getOverflowCount() == overflows + 1);
    render(); CHECK(probe.sounding == 0); // Rejected attacks never produce orphan releases.

    // Fewer than 976 events in each block can still exceed outstanding capacity.
    notes->clearNotes();
    for (unsigned i = 0; i < 1025; ++i) {
        Note note(i % 128, i < 976 ? 0 : 1, 4); note.setChannel(i / 128 + 1); notes->addNote(note);
    }
    dispatch(); restart(); render(); CHECK(probe.sounding == 976);
    rejected = probe.notes; overflows = mixer.getOverflowCount(); render();
    CHECK(probe.sounding == 0 && probe.notes == rejected && mixer.getOverflowCount() == overflows + 1);

    notes->clearNotes(); notes->addNote(Note(60, 0, 4)); dispatch(); restart(); render(1);
    CHECK(probe.held[60] == 1);
    for (size_t i = 0; i < ArrangementSnapshot::maxNotes; ++i) notes->addNote(Note(60, 0, 4));
    dispatch(); CHECK(mixer.arrangementOverflowed()); render(1);
    CHECK(probe.sounding == 0 && probe.lastOffSample == 0);

    // Arrangement cleanup uses the existing sustained-voice reset latch. While
    // pending, no attack is recorded; after service, only a future attack resumes.
    notes->clearNotes(); notes->addNote(Note(60, 0, 1)); notes->addNote(Note(62, 2, 1));
    dispatch(); CHECK(!mixer.arrangementOverflowed()); restart();
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0); render(12000);
    state.setPositionInBeats(1); render(1);
    CHECK(channel->isVoiceResetPending() && probe.lastOffSample == 0);
    const auto blocked = probe.blocks; render(1); CHECK(probe.blocks == blocked);
    ChannelTestAccess::resetVoices(*channel);
    CHECK(probe.sounding == 0 && !channel->isVoiceResetPending() && probe.resetQuiescent && !probe.resetOnAudio);
    render(23998); CHECK(probe.sounding == 0);
    render(1); CHECK(probe.held[62] == 1 && !channel->isVoiceResetPending());
    state.stop(); render(1); CHECK(channel->isVoiceResetPending());
    ChannelTestAccess::resetVoices(*channel); CHECK(probe.sounding == 0);
    restart(); render(1); CHECK(probe.held[60] == 1);
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(replacement)));
    dispatch(); render(1);
    CHECK(probe.destroyed == 1 && !probe.destroyedOnAudio && replacement.notes == 0);
    state.setPositionInBeats(2); render(1); CHECK(replacement.held[62] == 1);
    channels.removeChannel(0); dispatch(); render(1);
    CHECK(replacement.destroyed == 1 && !replacement.destroyedOnAudio);
}

static void arrangementBoundaryOwnershipTests() {
    constexpr double rate = 48000, tempo = 137;
    const double spb = rate * 60 / tempo;
    for (double seek : {0.0, 2165.993971306134}) {
        const double boundary = seek + 127 * (tempo / 60) / rate;
        const double epsilon = 1e-7 + 8 * std::numeric_limits<double>::epsilon() * seek * spb;
        const double threshold = seek + (127 - epsilon) / spb;
        const double reported = seek == 0 ? threshold : 2166.00001262557;
        for (double end : {reported, boundary, std::nextafter(boundary, 0.0),
                           std::nextafter(boundary, std::numeric_limits<double>::infinity()), threshold,
                           std::nextafter(threshold, 0.0),
                           std::nextafter(threshold, std::numeric_limits<double>::infinity())})
        for (int size : {1, 7, 64, 127}) for (double nextTempo : {20.0, 137.0, 300.0}) {
            Probe probe;
            ChannelList channels;
            TrackList tracks;
            ClipPool clips;
            TransportState state;
            auto* channel = channels.addChannel();
            channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
            auto source = std::make_unique<MidiClip>(0, end + 1);
            source->addNote(Note(60, seek, end - seek));
            // The release and touching attack must share ownership and off-first order.
            source->addNote(Note(61, end, 100 / spb));
            const auto id = clips.addClip(std::move(source));
            tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 0, end + 1));
            ChannelMixer mixer(channels, tracks, clips, state);
            mixer.prepareToPlay(rate, size);
            state.setTempo(tempo); state.setPositionInBeats(seek); state.setPlaying(true);
            juce::AudioBuffer<float> buffer(2, size);
            juce::MidiBuffer midi; midi.ensureSize(32768);
            for (int time = 0; time < 1024;) {
                if (time == 127) state.setTempo(nextTempo);
                const int n = std::min(size, (time < 127 ? 127 : 1024) - time);
                float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
                juce::AudioBuffer<float> block(pointers, 2, n);
                rendering = true; mixer.processBlock(block, midi); rendering = false;
                CHECK(renderAllocations == 0 && renderDeletions == 0);
                time += n;
            }
            CHECK(probe.events.size() == 4 && probe.sounding == 0 && mixer.getOverflowCount() == 0);
            CHECK(probe.events[0].on && probe.events[0].pitch == 60 && probe.events[0].time == 0);
            CHECK(!probe.events[1].on && probe.events[1].pitch == 60);
            CHECK(probe.events[2].on && probe.events[2].pitch == 61);
            CHECK(probe.events[1].time == probe.events[2].time);
            CHECK(probe.events[1].time >= 126 && probe.events[1].time <= 127);
            if (end >= boundary || (size == 127 && seek != 0 && end == reported))
                CHECK(probe.events[1].time == 127);
            CHECK(!probe.events[3].on && probe.events[3].pitch == 61);
            const double remaining = (end + 100 / spb - boundary) * rate * 60 / nextTempo;
            CHECK(std::abs(probe.events[3].time - (127 + remaining)) < 1.000001);
        }
    }
}

static void arrangementMergedCapacityTests() {
    for (bool scheduledRelease : {false, true}) {
        Probe first, second;
        ChannelList channels;
        TrackList tracks;
        ClipPool clips;
        TransportState state;
        auto* a = channels.addChannel();
        auto* b = channels.addChannel();
        a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
        b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
        auto source = std::make_unique<MidiClip>(0, 16);
        // Fill 1024 delivered arrangement keys over two blocks, under the event budget.
        for (unsigned i = 0; i < 1024; ++i) {
            const double start = i < 512 ? 0 : 1;
            Note note(i % 128, start, i == 0 && scheduledRelease ? 2 : 8);
            note.setChannel(i / 128 + 1); source->addNote(note);
        }
        const auto id = clips.addClip(std::move(source));
        auto* track = tracks.addTrack();
        track->addClipInstance(std::make_unique<ClipInstance>(id, a->getId(), 0, 16));
        auto other = std::make_unique<MidiClip>(0, 16);
        other->addNote(Note(60, 0, 12));
        const auto otherId = clips.addClip(std::move(other));
        track->addClipInstance(std::make_unique<ClipInstance>(otherId, b->getId(), 0, 16));
        ChannelMixer mixer(channels, tracks, clips, state);
        mixer.prepareToPlay(48000, 24000); mixer.setActiveChannel(0); state.setPlaying(true);
        juce::AudioBuffer<float> buffer(2, 24000);
        juce::MidiBuffer midi; midi.ensureSize(32768);
        auto render = [&] {
            rendering = true; mixer.processBlock(buffer, midi); rendering = false;
            CHECK(renderAllocations == 0 && renderDeletions == 0);
            CHECK(first.lastInputCount <= 2048 && second.lastInputCount <= 2048);
        };
        render(); render(); CHECK(first.sounding == 1024 && second.sounding == 1);
        const auto previous = first.events.size();
        const int offset = scheduledRelease ? 0 : 37;
        const int midiChannel = scheduledRelease ? 16 : 1;
        midi.addEvent(juce::MidiMessage::noteOn(midiChannel, 0, 0.5f), offset);
        render();
        CHECK(mixer.getOverflowCount() == 0 && first.sounding == 1024);
        CHECK(first.events.size() == previous + 2);
        CHECK(!first.events[previous].on && first.events[previous].channel == 1 && first.events[previous].pitch == 0);
        CHECK(first.events[previous].offset == offset && first.events[previous].time == 48000 + offset);
        CHECK(first.events[previous + 1].on && first.events[previous + 1].channel == midiChannel);
        CHECK(first.events[previous + 1].offset == offset && first.events[previous + 1].time == 48000 + offset);
        CHECK(second.sounding == 1 && second.events.size() == 1 && second.noteOffs == 0);
        midi.addEvent(juce::MidiMessage::noteOff(midiChannel, 0), 19); render();
        CHECK(first.sounding == 1023 && first.lastOffSample == 19 && mixer.getOverflowCount() == 0);
        for (int i = 0; i < 10; ++i) render();
        CHECK(first.sounding == 0 && first.notes == 1025 && first.noteOffs == 1025);
        CHECK(second.events.size() == 2 && second.events[1].time == 288000 && !second.events[1].on);
        CHECK(second.sounding == 0 && mixer.getOverflowCount() == 0);
    }
}

static void mixerSignalTests() {
    Probe first, second, third;
    first.constantOutput = second.constantOutput = true;
    first.rightOutput = 0.5f;
    second.leftOutput = 0.125f; second.rightOutput = 0.25f;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState state;
    auto* a = channels.addChannel("A");
    auto* b = channels.addChannel("B");
    auto* c = channels.addChannel("C");
    c->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(third)));
    c->setVolume(0); // A third distinct state catches short-circuit indexing in anySolo.
    a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
    b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
    auto& master = channels.getMasterBus();
    ChannelMixer mixer(channels, tracks, clips, state);
    juce::AudioBuffer<float> buffer(2, 127);
    juce::MidiBuffer midi; midi.ensureSize(32768);
    auto near = [](float a, float b) { return std::abs(a - b) < 1.0e-5f; };
    auto render = [&](float left, float right, float al, float ar, float bl, float br, int outputs = 2) {
        // Clear meter history so each assertion measures this actual signal, not a release envelope.
        mixer.prepareToPlay(48000, 127);
        float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, outputs, 127);
        const auto calls = first.blocks;
        rendering = true; mixer.processBlock(block, midi); rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
        CHECK(first.blocks == calls + 1 && second.blocks == first.blocks);
        for (int i = 0; i < 127; ++i) {
            CHECK(near(block.getSample(0, i), left));
            if (outputs == 2) CHECK(near(block.getSample(1, i), right));
        }
        CHECK(near(a->getLeftLevel(), al) && near(a->getRightLevel(), ar));
        CHECK(near(b->getLeftLevel(), bl) && near(b->getRightLevel(), br));
        CHECK(near(master.meter.getLeft(), std::abs(left)) && near(master.meter.getRight(), std::abs(right)));
    };
    CHECK(a->getVolume() == 1 && a->getPan() == 0 && !a->isMuted() && !a->isSolo());
    CHECK(master.getGain() == 1 && !master.isMuted());
    render(0.375f, 0.75f, 0.25f, 0.5f, 0.125f, 0.25f);
    a->setVolume(2); b->setVolume(0.5f);
    a->setPan(0.5f); b->setPan(-0.5f);
    render(0.3125f, 1.0625f, 0.25f, 1, 0.0625f, 0.0625f);
    master.setGain(0.5f);
    render(0.15625f, 0.53125f, 0.25f, 1, 0.0625f, 0.0625f);
    master.setMuted(true);
    render(0, 0, 0.25f, 1, 0.0625f, 0.0625f); // Master meter is not channel zero.
    master.setMuted(false); master.setGain(2);
    a->setVolume(1); b->setVolume(1); a->setPan(-1); b->setPan(1);
    render(0.5f, 0.5f, 0.25f, 0, 0, 0.25f);
    a->setSolo(true);
    render(0.5f, 0, 0.25f, 0, 0, 0);
    b->setSolo(true);
    render(0.5f, 0.5f, 0.25f, 0, 0, 0.25f);
    a->setMuted(true);
    render(0, 0.5f, 0, 0, 0, 0.25f); // Mute wins over solo.
    b->setSolo(false);
    render(0, 0, 0, 0, 0, 0); // Even a muted solo participates in anySolo.
    a->setSolo(false); a->setMuted(false); master.setGain(1); b->setVolume(0);
    for (float pan : {-1.0f, -0.00001f, 0.0f, 0.00001f, 1.0f}) {
        a->setPan(pan);
        const float left = 0.25f * (1 - std::max(0.0f, pan));
        const float right = 0.5f * (1 + std::min(0.0f, pan));
        render(left, right, left, right, 0, 0);
        render(0.25f, 0.25f, 0.25f, 0.25f, 0, 0, 1); // Mono ignores balance.
    }
    a->setVolume(-1); a->setPan(9); master.setGain(9);
    CHECK(a->getVolume() == 0 && a->getPan() == 1 && master.getGain() == 2);
    a->setVolume(std::numeric_limits<float>::quiet_NaN());
    a->setPan(std::numeric_limits<float>::infinity());
    master.setGain(std::numeric_limits<float>::quiet_NaN());
    CHECK(a->getVolume() == 0 && a->getPan() == 1 && master.getGain() == 2);
    a->setVolume(1); a->setPan(0); b->setVolume(1); b->setPan(0); master.setGain(1);
    second.leftOutput = -0.25f; second.rightOutput = -0.5f;
    render(0, 0, 0.25f, 0.5f, 0.25f, 0.5f); // Cancellation: never sum channel meter magnitudes.
    channels.clearChannels();
    mixer.prepareToPlay(48000, 127);
    rendering = true; mixer.processBlock(buffer, midi); rendering = false;
    CHECK(buffer.getMagnitude(0, 127) == 0 && master.meter.getLeft() == 0 && master.meter.getRight() == 0);
    CHECK(renderAllocations == 0 && renderDeletions == 0);
    mixer.releaseResources();
}

static void mixerMeterTests() {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        float reference = 0;
        for (int partition : {1, 7, 127, 512, 4096}) {
            StereoMeter meter; meter.prepare(rate);
            juce::AudioBuffer<float> buffer(2, 4096); buffer.clear();
            buffer.setSample(0, 0, 1); buffer.setSample(1, 0, 0.5f);
            float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
            juce::AudioBuffer<float> attack(pointers, 2, 1); meter.update(attack);
            CHECK(meter.getLeft() == 1 && meter.getRight() == 0.5f);
            buffer.clear();
            int remaining = static_cast<int>(rate * 0.5);
            while (remaining > 0) {
                const int n = std::min(partition, remaining);
                juce::AudioBuffer<float> block(pointers, 2, n);
                rendering = true; meter.update(block); rendering = false;
                remaining -= n;
            }
            CHECK(std::abs(meter.getLeft() - 0.01f) < 0.00002f);
            CHECK(std::abs(meter.getRight() - 0.005f) < 0.00001f);
            if (partition == 1) reference = meter.getLeft();
            CHECK(meter.getLeft() == reference);
            meter.clear(); CHECK(meter.getLeft() == 0 && meter.getRight() == 0);
        }
    }
    CHECK(renderAllocations == 0 && renderDeletions == 0);
    StereoMeter source; source.prepare(48000);
    juce::AudioBuffer<float> buffer(1, 1); buffer.setSample(0, 0, 1);
    source.update(buffer);
    LevelMeter display;
    display.poll(source);
    CHECK(LevelMeterTestAccess::tick(display, 0.01) == 1);
    display.poll(source); // No new audio publication: old peak must not pin the UI.
    CHECK(std::abs(LevelMeterTestAccess::tick(display, 0.5) - 0.01f) < 0.0001f);
    source.update(buffer); display.poll(source);
    CHECK(LevelMeterTestAccess::tick(display, 0.01) == 1);
}

static void mixerSuppressionTests() {
    Probe first, second;
    ChannelList channels;
    TrackList tracks;
    ClipPool clips;
    TransportState state;
    auto* a = channels.addChannel("A");
    auto* b = channels.addChannel("B");
    a->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(first)));
    b->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(second)));
    auto source = std::make_unique<MidiClip>(0, 8);
    source->addNote(Note(60, 0, 4)); source->addNote(Note(62, 0.5, 1));
    source->addNote(Note(64, 1, 1)); source->addNote(Note(65, 2, 1));
    const auto id = clips.addClip(std::move(source));
    auto* track = tracks.addTrack();
    for (auto* c : {a, b}) track->addClipInstance(std::make_unique<ClipInstance>(id, c->getId(), 0, 8));
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.setActiveChannel(0); mixer.prepareToPlay(48000, 24000);
    juce::AudioBuffer<float> buffer(2, 24000);
    juce::MidiBuffer midi; midi.ensureSize(32768);
    auto render = [&](int n) {
        float* pointers[]{buffer.getWritePointer(0), buffer.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n);
        rendering = true; mixer.processBlock(block, midi); rendering = false;
        CHECK(renderAllocations == 0 && renderDeletions == 0);
        CHECK(first.lastInputCount <= 2048 && second.lastInputCount <= 2048);
    };
    auto dispatch = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil(20); };
    state.setPlaying(true); render(6000);
    CHECK(first.sounding == 1 && second.sounding == 1);
    a->setVolume(0.5f); a->setPan(0.2f); a->setName("renamed"); a->setColour(juce::Colours::red);
    dispatch(); render(1);
    CHECK(first.noteOffs == 0 && second.noteOffs == 0); // No control/cosmetic snapshot panic.
    b->setSolo(true); dispatch(); render(5999);
    CHECK(first.sounding == 0 && first.noteOffs == 1 && first.lastOffSample == 0);
    CHECK(second.sounding == 1 && second.noteOffs == 0);
    const auto attacks = first.notes;
    midi.addEvent(juce::MidiMessage::noteOn(1, 70, 0.5f), 0);
    render(6000); // Consume suppressed arrangement b0.5 and live attacks.
    CHECK(first.notes == attacks && second.sounding == 2);
    b->setSolo(false); dispatch(); render(6000);
    CHECK(first.notes == attacks && first.sounding == 0); // No chase on unmute/unsolo.
    render(1); CHECK(first.held[64] == 1 && second.held[64] == 1);
    midi.addEvent(juce::MidiMessage::noteOff(1, 64), 0); render(1);
    CHECK(first.held[64] == 1); // Stale live release cannot kill the new arrangement token.
    auto& master = channels.getMasterBus();
    master.setMuted(true); render(1);
    CHECK(first.held[64] == 1 && buffer.getMagnitude(0, 1) == 0);
    master.setMuted(false); render(1);
    CHECK(first.held[64] == 1 && buffer.getMagnitude(0, 1) > 0); // Master is output-only mute.
    a->setMuted(true); render(1); CHECK(first.sounding == 0);
    a->setMuted(false); render(23995); // End exactly at b2.
    CHECK(first.sounding == 0);
    render(1); CHECK(first.held[65] == 1 && first.sounding == 1);

    state.stop(); render(1);
    // Maximum outstanding ledger still fits explicit suppression cleanup, no CC reliance.
    for (unsigned i = 0; i < Channel::maxLiveEvents; ++i)
        midi.addEvent(juce::MidiMessage::noteOn(1, 70, 0.5f), 0);
    render(1);
    for (unsigned i = Channel::maxLiveEvents; i < Channel::maxDeliveredNotes; ++i)
        midi.addEvent(juce::MidiMessage::noteOn(1, 70, 0.5f), 0);
    render(1); CHECK(first.sounding == 1024);
    a->setMuted(true); render(1);
    CHECK(first.sounding == 0 && first.lastInputCount == 1072 && mixer.getOverflowCount() == 0);
    a->setMuted(false); render(1);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 70, 0.5f), 0); render(1);
    b->setSolo(true); render(1);
    CHECK(a->isVoiceResetPending() && buffer.getMagnitude(0, 1) == 0);
    CHECK(first.held[70] == 0); // Explicit release precedes off-audio reset even if CCs are ignored.
    const auto calls = first.blocks; render(1); CHECK(first.blocks == calls);
    ChannelTestAccess::resetVoices(*a);
    CHECK(first.sounding == 0 && !first.resetOnAudio && first.resetQuiescent);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0); render(1);
    CHECK(first.lastInputCount == 1 && first.sounding == 0); // Pedal-up is delivered even while solo-suppressed.
    b->setSolo(false); render(1);
    CHECK(first.sounding == 0 && !a->isVoiceResetPending());
    midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.5f), 0); render(1);
    CHECK(first.held[72] == 1);
    state.stop(); render(1); ChannelTestAccess::resetVoices(*a);
    CHECK(first.sounding == 0 && mixer.getOverflowCount() == 0);
}

static void mixerBindingTests() {
    Project project;
    auto& channels = project.getChannelList();
    auto* a = channels.addChannel("Alpha");
    auto* b = channels.addChannel("Beta");
    const auto aid = a->getId(), bid = b->getId();
    a->setVolume(1.75f); a->setPan(-0.4f); a->setMuted(true); a->setSolo(true);
    a->setColour(juce::Colours::red); project.setActiveChannel(1);
    project.getMasterBus().setGain(1.5f); project.getMasterBus().setMuted(true);
    {
        MixerPanel panel(project); // No peer/window, device, or application is created.
        panel.setBounds(0, 0, 160, 100);
        auto* viewport = dynamic_cast<juce::Viewport*>(panel.getContentComponent());
        CHECK(viewport && viewport->getViewedComponent()->getWidth() > viewport->getWidth());
        CHECK(viewport->getViewedComponent()->getHeight() > viewport->getHeight());
        panel.setCollapsed(true, false); CHECK(!viewport->isVisible());
        panel.setCollapsed(false, false); CHECK(viewport->isVisible());
        CHECK(panel.getNumChannels() == 2);
        auto* strip = panel.getChannelStrip(0);
        CHECK(strip->getChannelId() == aid && strip->getTrackName() == "Alpha");
        CHECK(strip->getVolume() == 1.75f && strip->getPan() == -0.4f && strip->isMuted() && strip->isSolo());
        CHECK(strip->getTrackColour() == juce::Colours::red && panel.getChannelStrip(1)->isSelected());
        CHECK(panel.getMasterStrip()->getVolume() == 1.5f && panel.getMasterStrip()->isMuted());
        int feedback = 0;
        auto gainAction = strip->onVolumeChanged;
        strip->onVolumeChanged = [&](float v) { ++feedback; gainAction(v); };
        a->setVolume(0.8f); a->sendSynchronousChangeMessage();
        CHECK(feedback == 0 && strip->getVolume() == 0.8f);
        // Store actions, not pointers, to exercise late actions after rebuild/removal.
        const auto panAction = strip->onPanChanged;
        const auto muteAction = strip->onMuteToggled;
        const auto soloAction = strip->onSoloToggled;
        const auto selectAction = strip->onStripSelected;
        channels.moveChannel(0, 1);
        CHECK(panel.getChannelStrip(0)->getChannelId() == bid && panel.getChannelStrip(1)->getChannelId() == aid);
        CHECK(panel.getChannelStrip(1)->getVolume() == 0.8f && panel.getChannelStrip(0)->isSelected());
        gainAction(1.25f); panAction(0.6f); muteAction(false); soloAction(false); selectAction();
        CHECK(a->getVolume() == 1.25f && a->getPan() == 0.6f && !a->isMuted() && !a->isSolo());
        CHECK(b->getVolume() == 1 && project.getActiveChannelId() == aid);
        CHECK(panel.getChannelStrip(1)->isSelected());
        panel.getMasterStrip()->onVolumeChanged(0.3f); panel.getMasterStrip()->onMuteToggled(false);
        CHECK(project.getMasterBus().getGain() == 0.3f && !project.getMasterBus().isMuted());
        channels.removeChannel(1);
        gainAction(2); panAction(-1); muteAction(true); soloAction(true); selectAction();
        CHECK(panel.getNumChannels() == 1 && b->getVolume() == 1 && !b->isMuted() && !b->isSolo());
        CHECK(project.getActiveChannelId() == InvalidChannelId);
        channels.clearChannels(); CHECK(panel.getNumChannels() == 0 && panel.getChannelStrip(0) == nullptr);
        for (int i = 0; i < 20; ++i) CHECK(channels.addChannel());
        CHECK(panel.getNumChannels() == 20);
        // Actual strip background mouse path must invoke audition selection.
        auto* selected = panel.getChannelStrip(19);
        const juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(), {5, 5},
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1, 0, 0, 0, 0,
            selected, selected, juce::Time::getCurrentTime(), {5, 5}, juce::Time::getCurrentTime(), 1, false);
        selected->mouseDown(click);
        CHECK(project.getActiveChannelId() == selected->getChannelId());

        // Simulate the pop-out's external content parent without creating a native
        // window. Structural rebuilds must not apply the hidden panel's bounds.
        juce::Component externalOwner;
        externalOwner.addAndMakeVisible(viewport);
        const juce::Rectangle<int> externalBounds(0, 0, 760, 480);
        viewport->setBounds(externalBounds);
        const auto checkExternalLayout = [&] {
            CHECK(viewport->getParentComponent() == &externalOwner);
            CHECK(viewport->getBounds() == externalBounds);
            CHECK(panel.getMasterStrip()->getHeight() ==
                  externalBounds.getHeight() - viewport->getScrollBarThickness());
            CHECK(viewport->getViewedComponent()->getWidth() == (panel.getNumChannels() + 1) * 73);
        };
        CHECK(channels.addChannel("External addition"));
        checkExternalLayout();
        channels.removeChannel(channels.getNumChannels() - 1);
        checkExternalLayout();
        channels.moveChannel(0, 1);
        checkExternalLayout();
        panel.setBounds(0, 0, 220, 140);
        checkExternalLayout();

        // Exercise the real dock-return hook: reparent first, then use dock bounds.
        panel.onDisplayModeChanged(DisplayMode::Flex, DisplayMode::PopOut);
        CHECK(viewport->getParentComponent() == &panel);
        CHECK(viewport->getBounds() == juce::Rectangle<int>(0, panel.getTitleBarHeight(),
              panel.getWidth(), panel.getHeight() - panel.getTitleBarHeight()));
        panel.setSize(300, 240);
        CHECK(viewport->getWidth() == 300 && viewport->getHeight() == 240 - panel.getTitleBarHeight());
    }
    channels.getChannel(0)->setName("after teardown");
    channels.clearChannels();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
}

static void loopTimestampTests() {
    for (double rate : {44100.0, 48000.0}) for (double tempo : {120.0, 137.0})
    for (int size : {1, 7, 127, 512, 4096}) for (double length : {1.0 / 64, 16.0})
    for (double start : {3.125, 2165.993971306134}) {
        if (size == 1 && length == 16) continue;
        Probe probe;
        ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
        auto* channel = channels.addChannel("Loop");
        channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
        auto source = std::make_unique<MidiClip>(0, start + 40);
        source->addNote(Note(60, start, 32));
        source->addNote(Note(61, start + length, 1)); // Exclusive loop end never attacks.
        const auto id = clips.addClip(std::move(source));
        tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 0, start + 40));
        ChannelMixer mixer(channels, tracks, clips, state);
        mixer.prepareToPlay(rate, size); mixer.setActiveChannel(0);
        state.setTempo(tempo); state.setLoopRegion(start, start + length);
        state.setLoopEnabled(true); state.setPlaying(true);
        juce::AudioBuffer<float> storage(2, size);
        juce::MidiBuffer midi; midi.ensureSize(32768);
        midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.5f), 0);
        const double period = length * rate * 60 / tempo;
        const int total = static_cast<int>(std::ceil(period * 4.5));
        int blocks = 0;
        for (int time = 0; time < total;) {
            const int n = std::min(size, total - time);
            float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
            juce::AudioBuffer<float> block(pointers, 2, n);
            rendering = true; mixer.processBlock(block, midi); rendering = false;
            CHECK(probe.lastInputCount <= 2048);
            CHECK(!probe.invalidInputOffset);
            time += n; ++blocks;
        }
        CHECK(mixer.getOverflowCount() == 0 && probe.blocks == blocks);
        CHECK(probe.sounding == 2 && probe.held[72] == 1);
        std::vector<Probe::Event> notes;
        for (const auto& event : probe.events) if (event.pitch != 72) notes.push_back(event);
        CHECK(notes.size() == 9);
        for (size_t i = 0; i < notes.size(); ++i) {
            const auto pass = (i + 1) / 2;
            const auto expected = static_cast<long long>(std::floor(pass * period + 1e-7));
            CHECK(notes[i].time == expected);
            CHECK(notes[i].on == (i % 2 == 0) && notes[i].pitch == 60);
            CHECK(notes[i].offset >= 0 && notes[i].offset < size);
        }
        state.pollRenderPosition();
        const double expected = start + std::fmod(total * tempo / 60.0 / rate, length);
        CHECK(std::abs(state.getPositionInBeats() - expected) < 1e-10);
        state.stop();
        rendering = true; mixer.processBlock(storage, midi); rendering = false;
        CHECK(probe.sounding == 0);
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    }
}

static void deferredWrapOffsetTests() {
    const double reportedEnd = 0.021333333332;
    const double exactEnd = 512.0 / 24000;
    for (double end : {reportedEnd, std::nextafter(reportedEnd, 0.0),
            std::nextafter(reportedEnd, 1.0), exactEnd,
            std::nextafter(exactEnd, 0.0), std::nextafter(exactEnd, 1.0)})
    for (double tempo : {20.0, 120.0, 300.0}) {
        Probe probe;
        ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
        auto* channel = channels.addChannel("Deferred wrap");
        channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
        auto source = std::make_unique<MidiClip>(0, 1);
        source->addNote(Note(60, 0, 1));
        const double eventBeat = 100.0 / 144000;
        source->addNote(Note(62, eventBeat, 2.0 / 144000));
        const auto id = clips.addClip(std::move(source));
        tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 0, 1));
        ChannelMixer mixer(channels, tracks, clips, state);
        mixer.prepareToPlay(48000, 512);
        state.setLoopRegion(0, end); state.setLoopEnabled(true);
        state.setMetronomeEnabled(true); state.setPlaying(true);
        juce::AudioBuffer<float> buffer(2, 512); juce::MidiBuffer midi; midi.ensureSize(32768);
        const auto render = [&] {
            rendering = true; mixer.processBlock(buffer, midi); rendering = false;
            CHECK(!probe.invalidInputOffset); // All plugin inputs, including controllers/cleanup.
            CHECK(renderAllocations == 0 && renderDeletions == 0);
        };
        render();
        CHECK(probe.held[60] == 1 && buffer.getSample(0, 0) == 0.5f);
        const auto firstCount = probe.events.size();
        state.setTempo(tempo); render();
        CHECK(probe.blocks == 2 && probe.events.size() >= firstCount + 2);
        const auto& off = probe.events[firstCount];
        const auto& on = probe.events[firstCount + 1];
        CHECK(!off.on && on.on && off.pitch == 60 && on.pitch == 60);
        CHECK(off.time == 512 && on.time == 512 && off.offset == 0 && on.offset == 0);
        CHECK(buffer.getSample(0, 0) == 0.5f); // Click retrigger aligns with wrap off/on.
        for (const auto& event : probe.events) CHECK(event.offset >= 0 && event.offset < 512);
        if (end == reportedEnd && tempo == 20) {
            // Keep the negative fractional span origin: clamping the origin itself
            // would incorrectly move this later attack from sample 99 to sample 100.
            CHECK(probe.events[firstCount + 2].pitch == 62 && probe.events[firstCount + 2].on);
            CHECK(probe.events[firstCount + 2].offset == 99);
        }
        state.stop(); render(); CHECK(probe.sounding == 0 && mixer.getOverflowCount() == 0);
    }
}

static void loopControlCapacityTests() {
    TransportState state;
    for (const auto bounds : {std::pair<double, double>{-1, 4}, {0, 0}, {1, 0},
            {0, 1e-8}, {0, 1e10}, {0, std::numeric_limits<double>::infinity()},
            {std::numeric_limits<double>::quiet_NaN(), 4}}) {
        state.setLoopRegion(bounds.first, bounds.second);
        CHECK(state.getLoopRegion().startBeats == 0 && state.getLoopRegion().endBeats == 4);
    }
    Probe probe;
    ChannelList channels; TrackList tracks; ClipPool clips;
    auto* channel = channels.addChannel("Control");
    channel->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(probe)));
    auto source = std::make_unique<MidiClip>(0, 16);
    source->addNote(Note(60, 0, 16)); source->addNote(Note(61, 4, 8));
    const auto id = clips.addClip(std::move(source));
    tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 0, 16));
    ChannelMixer mixer(channels, tracks, clips, state);
    mixer.prepareToPlay(48000, 4096); mixer.setActiveChannel(0);
    juce::AudioBuffer<float> storage(2, 4096); juce::MidiBuffer midi; midi.ensureSize(32768);
    const auto render = [&](int n = 4096) {
        float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n);
        rendering = true; mixer.processBlock(block, midi); rendering = false;
        state.pollRenderPosition();
        CHECK(probe.lastInputCount <= 2048);
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    };
    state.setPlaying(true); render(); CHECK(probe.held[60] == 1);
    state.setLoopRegion(4, 5); state.setLoopEnabled(true); render();
    CHECK(probe.held[60] == 0 && probe.held[61] == 1 && probe.lastOffSample == 0);
    CHECK(state.getPositionInBeats() > 4 && state.getPositionInBeats() < 5);
    state.setLoopEnabled(false); render(); CHECK(probe.sounding == 0); // Disable is seek-like, no chase.
    state.setPositionInBeats(0); render(); CHECK(probe.held[60] == 1);
    state.setLoopRegion(0, 1.0 / 64); state.setLoopEnabled(true);
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 0);
    render(); CHECK(probe.held[60] == 1); // Live priority survives all ordinary wraps.
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 101); render();
    CHECK(probe.held[60] == 1); // Only a later loop attack resumes, not immediate chase.
    state.stop(); render(); CHECK(probe.sounding == 0);

    // More than 128 spans rejects traversal, but keeps exact elapsed position and live audition.
    mixer.prepareToPlay(48000, 96000); storage.setSize(2, 96000);
    state.setPlaying(true); state.setPositionInBeats(0);
    const auto overflow = mixer.getOverflowCount();
    render(96000);
    CHECK(mixer.getOverflowCount() > overflow && probe.sounding == 0);
    CHECK(std::abs(state.getPositionInBeats()) < 1e-10);
    render(64); CHECK(probe.held[60] == 1); // Recovery resets consumed indices safely.

    // Dense passes exhaust normal capacity atomically, not the 1072 cleanup reserve.
    state.stop(); render(64);
    auto* clip = dynamic_cast<MidiClip*>(clips.getClip(id));
    for (int key = 1; key < 100; ++key) clip->addNote(Note(key, 0, 16));
    juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    state.setPositionInBeats(0); state.setPlaying(true);
    const auto notesBefore = probe.notes;
    render(4096);
    CHECK(probe.notes == notesBefore && probe.sounding == 0 && probe.lastInputCount <= 1072);
}

static void metronomeTests() {
    for (double rate : {44100.0, 48000.0}) for (double tempo : {120.0, 137.0})
    for (TimeSignature meter : {TimeSignature{4, 4}, {3, 4}, {6, 8}})
    for (int size : {7, 127, 4096}) {
        ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
        ChannelMixer mixer(channels, tracks, clips, state);
        mixer.prepareToPlay(rate, size);
        state.setTempo(tempo); state.setTimeSignature(meter.numerator, meter.denominator);
        state.setMetronomeEnabled(true); state.setPlaying(true);
        channels.getMasterBus().setGain(0.5f);
        juce::AudioBuffer<float> storage(2, size); juce::MidiBuffer midi; midi.ensureSize(32768);
        const double period = rate * 60 / tempo * 4 / meter.denominator;
        const int total = static_cast<int>(std::ceil(period * (meter.numerator + 1))) + 1000;
        int tick = 0;
        for (int time = 0; time < total;) {
            const int n = std::min(size, total - time);
            float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
            juce::AudioBuffer<float> block(pointers, 2, n);
            rendering = true; mixer.processBlock(block, midi); rendering = false;
            for (int i = 0; i < n; ++i) {
                const int sample = time + i;
                while (sample >= static_cast<int>(std::floor((tick + 1) * period + 1e-7))) ++tick;
                const int age = sample - static_cast<int>(std::floor(tick * period + 1e-7));
                const bool accent = tick % meter.numerator == 0;
                const double expected = age < std::ceil(rate * 0.02) ?
                    0.5 * (accent ? 0.25 : 0.15) * std::exp(-7.0 * age / (rate * 0.02)) *
                    std::cos(juce::MathConstants<double>::twoPi * (accent ? 1760 : 1320) * age / rate) : 0;
                CHECK(std::abs(block.getSample(0, i) - expected) < 1e-6);
                CHECK(block.getSample(0, i) == block.getSample(1, i));
            }
            time += n;
        }
        CHECK(channels.getMasterBus().meter.getLeft() > 0);
        state.setPositionInBeats(0);
        channels.getMasterBus().setMuted(true);
        rendering = true; mixer.processBlock(storage, midi); rendering = false;
        CHECK(storage.getMagnitude(0, size) == 0);
        channels.getMasterBus().setMuted(false);
        state.setMetronomeEnabled(false);
        rendering = true; mixer.processBlock(storage, midi); rendering = false;
        CHECK(storage.getMagnitude(0, size) == 0);
        state.setMetronomeEnabled(true); state.stop();
        rendering = true; mixer.processBlock(storage, midi); rendering = false;
        CHECK(storage.getMagnitude(0, size) == 0);
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    }
}

static void clickMasterAndOverflowTests() {
    ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
    auto* silent = channels.addChannel("Muted solo"); silent->setSolo(true); silent->setMuted(true);
    ChannelMixer mixer(channels, tracks, clips, state); mixer.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> storage(2, 512); juce::MidiBuffer midi; midi.ensureSize(32768);
    const auto render = [&](int n) {
        float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n);
        rendering = true; mixer.processBlock(block, midi); rendering = false;
    };
    state.setMetronomeEnabled(true); state.setPlaying(true); channels.getMasterBus().setGain(0.5f);
    render(1);
    CHECK(storage.getSample(0, 0) == 0.125f && channels.getMasterBus().meter.getLeft() == 0.125f);
    CHECK(silent->getMeter().getLeft() == 0);
    channels.getMasterBus().setMuted(true); render(1); CHECK(storage.getSample(0, 0) == 0);
    channels.getMasterBus().setMuted(false); render(1);
    const double expected = 0.125 * std::exp(-14.0 / 960) * std::cos(juce::MathConstants<double>::twoPi * 1760 * 2 / 48000);
    CHECK(std::abs(storage.getSample(0, 0) - expected) < 1e-6); // Master gate doesn't restart click phase.
    mixer.prepareToPlay(1, 512); state.setTempo(300); state.setTimeSignature(4, 16); state.setPositionInBeats(0);
    const auto before = mixer.getOverflowCount(); render(512);
    CHECK(mixer.getOverflowCount() == before + 1 && storage.getMagnitude(0, 512) == 0);
    state.pollRenderPosition(); CHECK(state.getPositionInBeats() == 2560);
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static void loopUiTests() {
    TransportState state;
    TransportComponent ui(state); ui.setSize(1000, 104);
    auto* start = dynamic_cast<juce::TextEditor*>(ui.findChildWithID("loopStart"));
    auto* end = dynamic_cast<juce::TextEditor*>(ui.findChildWithID("loopEnd"));
    auto* apply = dynamic_cast<juce::TextButton*>(ui.findChildWithID("applyLoop"));
    auto* loop = dynamic_cast<TransportButton*>(ui.findChildWithID("loopToggle"));
    auto* click = dynamic_cast<TransportButton*>(ui.findChildWithID("metronomeToggle"));
    auto* validation = dynamic_cast<juce::Label*>(ui.findChildWithID("loopValidation"));
    CHECK(start && end && apply && loop && click && validation);
    for (const auto invalid : {"", "abc", "1foo", "nan", "inf", "-1"}) {
        start->setText(invalid); apply->onClick();
        CHECK(state.getLoopRegion().startBeats == 0 && validation->getText().startsWith("Invalid"));
    }
    start->setText("2.5"); end->setText("6.5"); start->onReturnKey();
    CHECK(state.getLoopRegion().startBeats == 2.5 && state.getLoopRegion().endBeats == 6.5);
    loop->onClick(); click->onClick();
    CHECK(state.isLoopEnabled() && state.isMetronomeEnabled() && loop->isActive() && click->isActive());
    CHECK(!ui.findChildWithID("record")->isEnabled());
    state.setLoopRegion(1, 3); CHECK(start->getText().getDoubleValue() == 1 && end->getText().getDoubleValue() == 3);
    CHECK(ui.getLocalBounds().contains(start->getBounds()) && ui.getLocalBounds().contains(end->getBounds()));
    ui.setSize(600, 104);
    for (auto* child : ui.getChildren()) if (child->isVisible()) CHECK(ui.getLocalBounds().contains(child->getBounds()));
    CHECK(validation->getWidth() == 580 && validation->getHeight() == 20);
    Project project;
    TimelinePanel timeline(project); timeline.setSize(800, 400);
    TimelineContent* content = nullptr;
    juce::ScrollBar* horizontal = nullptr;
    for (auto* child : timeline.getChildren()) {
        if (auto* value = dynamic_cast<TimelineContent*>(child)) content = value;
        if (auto* value = dynamic_cast<juce::ScrollBar*>(child)) if (!value->isVertical()) horizontal = value;
    }
    CHECK(content && horizontal);
    project.getTransportState().setLoopRegion(100, 104);
    CHECK(content->getTotalWidth() == 108 * content->getPixelsPerBeat());
    CHECK(horizontal->getMaximumRangeLimit() == content->getTotalWidth());
    TimeRuler* ruler = nullptr;
    for (auto* child : content->getChildren()) if (auto* value = dynamic_cast<TimeRuler*>(child)) ruler = value;
    CHECK(ruler);
    project.getTransportState().setLoopEnabled(true);
    ruler->setSize(400, 24); ruler->setScrollOffset(100 * 50);
    juce::Image image(juce::Image::RGB, 400, 24, true);
    juce::Graphics graphics(image); ruler->paint(graphics);
    CHECK(image.getPixelAt(25, 1) == juce::Colour(0xff66aaff));
    CHECK(image.getPixelAt(225, 1) == juce::Colour(0xff2a2a2a));
}

static void loopPedalAndLedgerTests() {
    for (int size : {512, 1024}) {
        Probe a, b;
        ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
        auto* first = channels.addChannel("Pedal"); auto* second = channels.addChannel("Independent");
        first->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(a)));
        second->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(b)));
        auto source = std::make_unique<MidiClip>(0, 4); source->addNote(Note(60, 0, 4));
        const auto id = clips.addClip(std::move(source));
        auto* track = tracks.addTrack();
        track->addClipInstance(std::make_unique<ClipInstance>(id, first->getId(), 0, 4));
        track->addClipInstance(std::make_unique<ClipInstance>(id, second->getId(), 0, 4));
        ChannelMixer mixer(channels, tracks, clips, state); mixer.prepareToPlay(48000, size); mixer.setActiveChannel(0);
        state.setLoopRegion(0, 1.0 / 32); state.setLoopEnabled(true); state.setPlaying(true);
        juce::AudioBuffer<float> buffer(2, size); juce::MidiBuffer midi; midi.ensureSize(32768);
        const auto render = [&] { rendering = true; mixer.processBlock(buffer, midi); rendering = false; };
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.5f), 0);
        render();
        if (size == 512) { CHECK(a.sounding == 2); render(); }
        CHECK(first->isVoiceResetPending() && b.sounding == 1 && b.resets == 0);
        CHECK(buffer.getSample(0, 0) == 0.25f); // Only independent destination is audible.
        CHECK(a.lastInputCount <= 1072 && mixer.getOverflowCount() == 0);
        const auto calls = a.blocks; render(); CHECK(a.blocks == calls);
        ChannelTestAccess::resetVoices(*first);
        CHECK(a.resets == 1 && !a.resetOnAudio && a.resetQuiescent && a.sounding == 0);
        CHECK(renderAllocations == 0 && renderDeletions == 0);
    }
    // A live-only pedal owner must not be reset merely because another destination wraps.
    Probe dense, live;
    ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
    auto* first = channels.addChannel("1024 notes"); auto* second = channels.addChannel("Live pedal");
    first->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(dense)));
    second->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(live)));
    auto source = std::make_unique<MidiClip>(0, 4);
    for (int key = 0; key < 1024; ++key) {
        Note note(key % 128, key < 512 ? 0 : 0.05, 3); note.setChannel(key / 128 + 1); source->addNote(note);
    }
    const auto id = clips.addClip(std::move(source));
    auto* track = tracks.addTrack();
    track->addClipInstance(std::make_unique<ClipInstance>(id, first->getId(), 0, 4));
    // Future events give the live-only destination a scheduler range, but no delivered arrangement history.
    track->addClipInstance(std::make_unique<ClipInstance>(id, second->getId(), 8, 4));
    ChannelMixer mixer(channels, tracks, clips, state); mixer.prepareToPlay(48000, 1024); mixer.setActiveChannel(1);
    state.setLoopRegion(0, 0.125); state.setLoopEnabled(true); state.setPlaying(true);
    juce::AudioBuffer<float> buffer(2, 1024); juce::MidiBuffer midi; midi.ensureSize(32768);
    const auto render = [&] { rendering = true; mixer.processBlock(buffer, midi); rendering = false; };
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.5f), 0);
    render(); CHECK(dense.sounding == 512 && live.sounding == 1);
    render(); CHECK(dense.sounding == 1024);
    const auto notes = dense.notes;
    render();
    CHECK(dense.sounding == 0 && dense.notes == notes && dense.lastInputCount == 1072);
    CHECK(mixer.getOverflowCount() == 1 && live.sounding == 1 && live.noteOffs == 0 && live.resets == 0);
    CHECK(!second->isVoiceResetPending());
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

static void loopClickAndClockTests() {
    // Musical time remains partition-independent over one hour, including fractional wraps.
    for (int size : {127, 4096}) {
        TransportState state; TransportClock clock;
        state.setTempo(137); state.setLoopRegion(3.125, 19.125); state.setLoopEnabled(true); state.setPlaying(true);
        const int total = 44100 * 3600;
        for (int time = 0; time < total;) {
            const int n = std::min(size, total - time);
            rendering = true;
            const auto block = clock.beginBlock(state, n, 44100); clock.endBlock(state, block);
            rendering = false;
            CHECK(!block.spanOverflow && block.endBeats >= 3.125 && block.endBeats < 19.125);
            time += n;
        }
        state.pollRenderPosition();
        CHECK(std::abs(state.getPositionInBeats() - (3.125 + std::fmod(137.0 * 60, 16.0))) < 1e-10);
    }
    for (int size : {1, 7, 127, 4096}) {
        ChannelList channels; TrackList tracks; ClipPool clips; TransportState state;
        ChannelMixer mixer(channels, tracks, clips, state); mixer.prepareToPlay(44100, size);
        state.setTempo(137); state.setTimeSignature(6, 8);
        state.setLoopRegion(0, 1.0 / 64); state.setLoopEnabled(true);
        state.setMetronomeEnabled(true); state.setPlaying(true);
        juce::AudioBuffer<float> storage(2, size); juce::MidiBuffer midi; midi.ensureSize(32768);
        const double period = 44100.0 * 60 / 137 / 64;
        int pass = 0;
        const int total = 10000;
        for (int time = 0; time < total;) {
            const int n = std::min(size, total - time);
            float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
            juce::AudioBuffer<float> block(pointers, 2, n);
            rendering = true; mixer.processBlock(block, midi); rendering = false;
            for (int i = 0; i < n; ++i) {
                const int sample = time + i;
                while (sample >= static_cast<int>(std::floor((pass + 1) * period + 1e-7))) ++pass;
                const int age = sample - static_cast<int>(std::floor(pass * period + 1e-7));
                const double expected = 0.25 * std::exp(-7.0 * age / (44100 * 0.02)) *
                    std::cos(juce::MathConstants<double>::twoPi * 1760 * age / 44100);
                CHECK(std::abs(block.getSample(0, i) - expected) < 1e-6);
            }
            time += n;
        }
    }
    // Exact end ownership, seek/edit, tempo and denominator changes on captured boundaries.
    TransportState state; TransportClock clock; Metronome click;
    juce::AudioBuffer<float> storage(2, 24000);
    state.setLoopRegion(0, 1); state.setLoopEnabled(true); state.setMetronomeEnabled(true); state.setPlaying(true);
    const auto render = [&](int n) {
        float* pointers[]{storage.getWritePointer(0), storage.getWritePointer(1)};
        juce::AudioBuffer<float> block(pointers, 2, n); block.clear();
        rendering = true;
        const auto timing = clock.beginBlock(state, n, 48000);
        click.render(block, timing); clock.endBlock(state, timing);
        rendering = false;
        return timing;
    };
    auto timing = render(24000);
    CHECK(timing.spanCount == 1 && !timing.spans[0].wrap && timing.endBeats == 0);
    CHECK(storage.getSample(0, 0) == 0.25f && storage.getSample(0, 23999) == 0);
    timing = render(1); CHECK(timing.spans[0].wrap && storage.getSample(0, 0) == 0.25f);
    state.setLoopEnabled(false); state.setPositionInBeats(0); state.setTempo(60); state.setTimeSignature(6, 8);
    timing = render(24000); CHECK(timing.discontinuity && timing.endBeats == 0.5);
    render(1); CHECK(storage.getSample(0, 0) == 0.15f); // Eighth-note beat, not dotted-quarter.
    state.setPositionInBeats(3); render(1); CHECK(storage.getSample(0, 0) == 0.25f);
    state.setPositionInBeats(2); state.setTimeSignature(3, 4); render(1); CHECK(storage.getSample(0, 0) == 0.15f);
    state.setPositionInBeats(3); render(1); CHECK(storage.getSample(0, 0) == 0.25f);
    state.setMetronomeEnabled(false); render(1); CHECK(storage.getSample(0, 0) == 0);
    state.setMetronomeEnabled(true); render(1); CHECK(storage.getSample(0, 0) == 0); // No off-grid chase.
    state.setPositionInBeats(0); state.setTempo(120); state.setTimeSignature(4, 4);
    render(1); state.setTempo(60); render(24000);
    const double tail = 0.25 * std::exp(-7.0 / 960) * std::cos(juce::MathConstants<double>::twoPi * 1760 / 48000);
    CHECK(std::abs(storage.getSample(0, 0) - tail) < 1e-6); // Tempo preserves the existing oscillator tail.
    render(23998); render(1); CHECK(storage.getSample(0, 0) == 0.15f); // New tempo owns the next grid tick.
    state.setTempo(300); state.setTimeSignature(6, 8); render(4800);
    CHECK(storage.getSample(0, 0) == 0 && storage.getSample(0, 4799) == 0.15f);
    state.setLoopRegion(0, 1.0 / 64); state.setLoopEnabled(true);
    timing = clock.beginBlock(state, 4, 1); // Below one sample per loop: bounded rejection, valid final clock.
    CHECK(timing.spanOverflow && timing.spanCount <= TransportClock::Block::maxSpans);
    clock.endBlock(state, timing);
    CHECK(renderAllocations == 0 && renderDeletions == 0);
}

int main() {
    try {
        // Project's real destructor saves settings. Never touch the user's home.
        const juce::File home(VIBEDAW_TEST_HOME);
        CHECK(home.createDirectory().wasOk());
        CHECK(setenv("HOME", home.getFullPathName().toRawUTF8(), 1) == 0);
        juce::ScopedJuceInitialiser_GUI juceInitialiser; // Framework only; no app/window/device.
        boundaryTests();
        modelTests();
        renderTests();
        reviewRegressionTests();
        transportClockTests();
        transportEngineTests();
        arrangementPlaybackTests();
        arrangementLifecycleTests();
        arrangementCapacityTests();
        arrangementBoundaryOwnershipTests();
        arrangementMergedCapacityTests();
        mixerSignalTests();
        mixerMeterTests();
        mixerSuppressionTests();
        mixerBindingTests();
        loopTimestampTests();
        deferredWrapOffsetTests();
        loopControlCapacityTests();
        metronomeTests();
        clickMasterAndOverflowTests();
        loopUiTests();
        loopPedalAndLedgerTests();
        loopClickAndClockTests();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        std::cout << "T03/T06/T01/T02/T04 and T05 loop/metronome tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
