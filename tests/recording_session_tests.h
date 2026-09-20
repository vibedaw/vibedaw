#pragma once

#include "core/MidiRecorder.h"

// Include after Probe, OfflineInstrument, CHECK, and the allocation guards.
namespace recording_session_test {
struct Instrument : OfflineInstrument {
    struct Expression { int sample = 0, status = 0, data1 = 0, data2 = 0; };
    explicit Instrument(Probe& probe) : OfflineInstrument(probe) {}
    std::array<Expression, 4096> expressions{};
    unsigned count = 0;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
        for (const auto e : midi)
            if (e.numBytes == 3 && ((e.data[0] & 0xf0) == 0xb0 || (e.data[0] & 0xf0) == 0xe0))
                if (count < expressions.size()) expressions[count++] = {e.samplePosition, e.data[0], e.data[1], e.data[2]};
        OfflineInstrument::processBlock(buffer, midi);
    }
    bool saw(int status, int data1, int data2, int sample) const {
        for (unsigned i = 0; i < count; ++i) {
            const auto& e = expressions[i];
            if (e.status == status && e.data1 == data1 && e.data2 == data2 && e.sample == sample) return true;
        }
        return false;
    }
};

// Unlike OfflineInstrument's deliberately hostile cleanup behavior, this fake
// implements ordinary MIDI pedal release. Keep parameter state across reset so
// stop tests must actually deliver expression cleanup, not just reset voices.
struct CompliantInstrument : Instrument {
    explicit CompliantInstrument(Probe& probe) : Instrument(probe) { bend.fill(8192); }
    std::array<unsigned, 2048> keys{}, voices{};
    std::array<bool, 16> sustain{}, sostenuto{};
    std::array<int, 16> bend{}, modulation{};
    unsigned sounding = 0;
    void reset() override {
        Instrument::reset(); keys.fill(0); voices.fill(0); sounding = 0;
    }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
        for (const auto e : midi) {
            if (e.numBytes != 3) continue;
            const int ch = e.data[0] & 15, kind = e.data[0] & 0xf0, key = ch * 128 + e.data[1];
            if (kind == 0x90 && e.data[2]) { ++keys[key]; ++voices[key]; }
            else if (kind == 0x80 || kind == 0x90) {
                if (keys[key]) --keys[key];
                if (!sustain[ch] && !sostenuto[ch]) voices[key] = keys[key];
            } else if (kind == 0xe0) bend[ch] = e.data[1] + 128 * e.data[2];
            else if (kind == 0xb0) {
                if (e.data[1] == 1) modulation[ch] = e.data[2];
                if (e.data[1] == 64) sustain[ch] = e.data[2] >= 64;
                if (e.data[1] == 66) sostenuto[ch] = e.data[2] >= 64;
                for (int pitch = 0; pitch < 128; ++pitch) {
                    const int k = ch * 128 + pitch;
                    if (e.data[1] == 120 || e.data[1] == 123) keys[k] = voices[k] = 0;
                    else if (!sustain[ch] && !sostenuto[ch]) voices[k] = keys[k];
                }
            }
        }
        Instrument::processBlock(buffer, midi);
        sounding = 0;
        for (const auto count : voices) sounding += count;
        buffer.clear();
        if (sounding)
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), 0.25f, buffer.getNumSamples());
    }
};

struct Session {
    Probe probe;
    Project project;
    MidiRecorder recorder{project};
    ChannelMixer mixer{project.getChannelList(), project.getTrackList(), project.getClipPool(),
                       project.getTransportState(), &recorder};
    Channel* channel = nullptr;
    Instrument* instrument = nullptr;
    juce::AudioBuffer<float> audio{2, 240};
    juce::MidiBuffer input;
    juce::String error;
    explicit Session(bool compliant = false) {
        channel = project.getChannelList().addChannel("Recorder test");
        std::unique_ptr<Instrument> plugin;
        if (compliant) plugin = std::make_unique<CompliantInstrument>(probe);
        else plugin = std::make_unique<Instrument>(probe);
        instrument = plugin.get();
        channel->setPlugin(std::make_unique<PluginHost>(std::move(plugin)));
        mixer.prepareToPlay(480, 240); // Exactly 240 samples per quarter-note beat.
        mixer.setActiveChannel(project.getChannelList().indexOfChannel(channel));
        input.ensureSize(32768);
    }
    ~Session() { recorder.endSession(); mixer.releaseResources(); }
    MidiRecorder::Options options(MidiRecorder::Mode mode, ClipId id = InvalidClipId, double length = 1) {
        return {id, channel->getId(), mode, true, 0, length};
    }
    void render() {
        CHECK(AudioQuiescence::instance().enter());
        const auto allocations = renderAllocations, deletions = renderDeletions;
        rendering = true;
        mixer.processBlock(audio, input);
        rendering = false;
        AudioQuiescence::instance().leave();
        CHECK(renderAllocations == allocations && renderDeletions == deletions);
        CHECK(!probe.invalidInputOffset);
    }
    void note(int pitch, int on = 30, int off = 90, int channelNumber = 1) {
        input.addEvent(juce::MidiMessage::noteOn(channelNumber, pitch, static_cast<juce::uint8>(99)), on);
        input.addEvent(juce::MidiMessage::noteOff(channelNumber, pitch), off);
    }
    MidiClip* target() { return dynamic_cast<MidiClip*>(project.getClipPool().getClip(recorder.getTargetClipId())); }
};
} // namespace recording_session_test

inline void recorderSessionTests() {
    using namespace recording_session_test;
    {
        Session s;
        AudioEngine engine;
        AudioEngineTestAccess::prepare(engine, 480, 240);
        engine.setProcessor(&s.mixer);
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Continuous), true, s.error));
        const auto render = [&] {
            const auto allocations = renderAllocations, deletions = renderDeletions;
            rendering = true; AudioEngineTestAccess::render(engine, s.audio); rendering = false;
            CHECK(renderAllocations == allocations && renderDeletions == deletions);
        };
        render(); // Startup's 48 synthetic cleanup messages are not a performance.
        CHECK(s.recorder.isRecording() && !s.recorder.hasPendingContent());
        AudioEngineTestAccess::nowMs += 125;
        engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)));
        render();
        for (int i = 0; i < 2049; ++i)
            engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::controllerEvent(1, 1, 42));
        render();
        CHECK(!s.recorder.isRecording() && s.recorder.getStatus().containsIgnoreCase("input-loss"));
        s.recorder.stop();
        CHECK(s.target() && s.target()->getNumNotes() == 1);
        CHECK(s.target()->getNotes()[0].getEndTime() == 2);
        CHECK(s.target()->getExpressionEvents().empty()); // No synthetic CC64 captured.
        engine.clearProcessor();
    }
    {
        Session s;
        AudioEngine engine;
        AudioEngineTestAccess::prepare(engine, 480, 240); engine.setProcessor(&s.mixer);
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Continuous), true, s.error));
        AudioEngineTestAccess::render(engine, s.audio);
        { AudioQuiescence::Edit edit(AudioQuiescence::Interruption::PreserveVoices);
          AudioEngineTestAccess::render(engine, s.audio); }
        AudioEngineTestAccess::render(engine, s.audio);
        CHECK(!s.recorder.isRecording() && s.recorder.getStatus().containsIgnoreCase("interruption"));
        s.recorder.endSession();
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Continuous), true, s.error));
        AudioEngineTestAccess::render(engine, s.audio);
        AudioEngineTestAccess::prepare(engine, 960, 240);
        CHECK(!s.recorder.isRecording() && s.recorder.getStatus().containsIgnoreCase("interruption"));
        engine.clearProcessor();
    }
    {
        Session s;
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Continuous), true, s.error));
        s.note(60); s.render();
        std::atomic<bool> running{true}, started{false};
        std::atomic<unsigned> rejected{0};
        std::thread renderAdmission([&] {
            started = true;
            while (running.load()) {
                if (AudioQuiescence::instance().enter()) AudioQuiescence::instance().leave();
                else ++rejected;
            }
        });
        while (!started.load()) std::this_thread::yield();
        for (int i = 0; i < 1000; ++i) {
            s.recorder.poll();
            (void) s.recorder.isSessionActive(); (void) s.recorder.isRecording();
            (void) s.recorder.hasPendingContent(); (void) s.recorder.getStatus();
            (void) s.recorder.canUndoLastRecording();
        }
        running = false; renderAdmission.join();
        CHECK(rejected.load() == 0); // Neither ordinary poll nor status getters gate render.
        CHECK(s.recorder.getTargetClipId() == InvalidClipId && s.recorder.hasPendingContent());
        CHECK(s.recorder.setRecording(false, s.error));
        CHECK(s.target() && s.target()->getNumNotes() == 1);
    }
    {
        Session s(true);
        auto* plugin = dynamic_cast<CompliantInstrument*>(s.instrument);
        CHECK(plugin);
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Takes, InvalidClipId, 0.5), true, s.error));
        s.input.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
        s.input.addEvent(juce::MidiMessage::pitchWheel(1, 12000), 0);
        s.input.addEvent(juce::MidiMessage::controllerEvent(1, 1, 70), 0);
        s.note(60, 30, 180); // Physically held through the mid-block wrap.
        s.render(); // Mid-block wrap at sample 120, without a message-thread reset.
        CHECK(s.recorder.isRecording() && !s.channel->isVoiceResetPending());
        CHECK(s.probe.resets == 0 && plugin->bend[0] == 12000 && plugin->sustain[0]);
        CHECK(s.instrument->saw(0xb0, 64, 0, 120) && s.instrument->saw(0xb0, 64, 127, 120));
        CHECK(s.instrument->saw(0xe0, 12000 & 127, 12000 >> 7, 120));
        CHECK(!s.instrument->saw(0xe0, 0, 64, 120));
        CHECK(s.audio.getMagnitude(0, s.audio.getNumSamples()) > 0); // Take replay is audible, not a discarded block.
        s.recorder.stop();
        CHECK(s.recorder.getTakeCount() == 2);
        CHECK(s.recorder.selectTake(1, s.error));
        bool pedal = false, bend = false, modulation = false;
        for (const auto& event : s.target()->getExpressionEvents()) {
            pedal = pedal || (event.beat == 0 && event.status == 0xb0 && event.data1 == 64 && event.data2 == 127);
            bend = bend || (event.beat == 0 && event.status == 0xe0 && event.data1 + 128 * event.data2 == 12000);
            modulation = modulation || (event.beat == 0 && event.status == 0xb0 && event.data1 == 1 && event.data2 == 70);
        }
        CHECK(pedal && bend && modulation);
        s.render(); // Deliver stop cleanup before servicing the conservative stopped reset.
        CHECK(plugin->bend[0] == 8192 && plugin->modulation[0] == 0 && !plugin->sustain[0]);
        ChannelTestAccess::resetVoices(*s.channel);
        s.recorder.endSession(); s.note(67); s.render();
        CHECK(plugin->bend[0] == 8192 && plugin->modulation[0] == 0);
    }
    {
        Session s;
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Takes), true, s.error));
        s.note(60); s.render(); s.note(62); s.render(); s.recorder.stop();
        CHECK(s.recorder.selectTake(0, s.error));
        const auto selected = s.recorder.getTargetClipId();
        s.recorder.getPlaybackTransport().setPlaying(true);
        s.probe.events.clear();
        for (int i = 0; i < 4; ++i) { s.render(); s.recorder.poll(); }
        int attacks = 0;
        for (const auto& event : s.probe.events) if (event.on) { ++attacks; CHECK(event.pitch == 60); }
        CHECK(attacks == 4 && s.recorder.getTargetClipId() == selected);
    }
    {
        Session s(true);
        auto* plugin = dynamic_cast<CompliantInstrument*>(s.instrument);
        s.input.addEvent(juce::MidiMessage::pitchWheel(2, 11000), 0);
        s.input.addEvent(juce::MidiMessage::controllerEvent(2, 1, 90), 0);
        s.render(); // Live expression already held before starting the session.
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Takes, InvalidClipId, 4), true, s.error));
        s.note(65, 30, 90, 2); s.render(); s.recorder.stop();
        CHECK(s.recorder.getTakeCount() == 1);
        bool seed = false;
        for (const auto& event : s.target()->getExpressionEvents())
            seed = seed || (event.beat == 0 && event.status == 0xe1 && event.data1 + 128 * event.data2 == 11000);
        CHECK(seed);
        s.render(); // A normal (non-pedal) stop needs no plugin reset to clear bend/mod.
        CHECK(plugin && plugin->bend[1] == 8192 && plugin->modulation[1] == 0);
        CHECK(!s.channel->isVoiceResetPending());
        s.recorder.endSession(); s.note(67, 30, 90, 2); s.render();
        CHECK(plugin->bend[1] == 8192 && plugin->modulation[1] == 0);
        s.input.addEvent(juce::MidiMessage::pitchWheel(1, 13000), 0);
        s.input.addEvent(juce::MidiMessage::controllerEvent(1, 1, 42), 0);
        s.render();
        auto* other = s.project.getChannelList().addChannel("Expression destination");
        Probe otherProbe;
        other->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(otherProbe)));
        s.mixer.setActiveChannel(s.project.getChannelList().indexOfChannel(other));
        s.render();
        CHECK(plugin->bend[0] == 8192 && plugin->modulation[0] == 0);
        other->setPlugin(nullptr);
    }
    {
        Session s;
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub), true, s.error));
        s.channel->requestVoiceReset();
        s.note(60); s.render();
        CHECK(!s.recorder.isRecording() && s.recorder.getStatus().containsIgnoreCase("delivery"));
        ChannelTestAccess::resetVoices(*s.channel);
        s.recorder.stop();
        CHECK(s.target() && s.target()->getNumNotes() == 1);
    }
    {
        Session s;
        auto source = std::make_unique<MidiClip>(0, 4);
        source->addNote(Note(60, 0, 1)); source->addNote(Note(60, 0.5, 1.5));
        const auto id = s.project.getClipPool().addClip(std::move(source));
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub, id, 4), false, s.error));
        s.render(); s.render();
        CHECK(s.probe.notes == 1 && s.probe.noteOffs == 0 && s.probe.sounding == 1);
        s.render();
        CHECK(s.probe.noteOffs == 1 && s.probe.events.back().time == 480 && s.probe.sounding == 0);
    }
    {
        Session s;
        auto source = std::make_unique<MidiClip>(0, 4);
        CHECK(source->replaceContent(std::vector<Note>(60000, Note(60, 0, 1)), {}));
        const auto id = s.project.getClipPool().addClip(std::move(source));
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub, id, 4), false, s.error));
        s.render();
        CHECK(s.recorder.getStatus().containsIgnoreCase("work-budget"));
        CHECK(s.target()->getNumNotes() == 60000); // Bounded fault leaves the source intact.
    }
    {
        Session s;
        auto source = std::make_unique<MidiClip>(0, 4);
        source->addNote(Note(48, 0, 0.5));
        const auto id = s.project.getClipPool().addClip(std::move(source));
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub, id, 4), false, s.error));
        s.render(); s.recorder.endSession();
        CHECK(!s.recorder.canUndoLastRecording());
        CHECK(!s.recorder.undoLastRecording(s.error));
        CHECK(s.target()->getNumNotes() == 1);
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub, id, 4), true, s.error));
        s.render(); s.recorder.endSession(); // Armed silence is not a content transaction.
        CHECK(!s.recorder.canUndoLastRecording());
        CHECK(!s.recorder.undoLastRecording(s.error));
        CHECK(s.target()->getNumNotes() == 1);
    }
    {
        Session s;
        auto source = std::make_unique<MidiClip>(0, 4);
        source->addNote(Note(48, 0, 0.5));
        const auto id = s.project.getClipPool().addClip(std::move(source));
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub, id, 4), false, s.error));
        s.target()->addNote(Note(55, 2, 0.5)); // Manual edit during play, before the first arm.
        s.recorder.poll();
        CHECK(s.recorder.setRecording(true, s.error));
        s.note(60); s.render();
        CHECK(s.recorder.setRecording(false, s.error));
        CHECK(s.recorder.canUndoLastRecording());
        auto committed = s.target()->clone();
        s.target()->addNote(Note(77, 3, 0.5));
        s.recorder.poll(); // Playback import must not silently advance the undo guard.
        s.recorder.endSession();
        CHECK(s.recorder.canUndoLastRecording());
        CHECK(!s.recorder.undoLastRecording(s.error));
        CHECK(s.target()->getNumNotes() == 4 && s.target()->getNotes().back().getPitch() == 77);
        // A failed undo retains its baseline and guard; resolving the conflict permits retry.
        auto* expected = dynamic_cast<MidiClip*>(committed.get());
        CHECK(s.target()->replaceContent(expected->getNotes(), expected->getExpressionEvents()));
        CHECK(s.recorder.undoLastRecording(s.error));
        CHECK(s.target()->getNumNotes() == 2 && s.target()->getNotes()[1].getPitch() == 55);
        CHECK(!s.recorder.canUndoLastRecording());
    }
    {
        Session s;
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub, InvalidClipId, 4), true, s.error));
        s.note(60); s.render();
        CHECK(s.recorder.setRecording(false, s.error));
        const auto id = s.recorder.getTargetClipId();
        s.target()->addNote(Note(69, 3, 0.25));
        CHECK(s.recorder.setRecording(true, s.error));
        s.note(72); s.render();
        s.recorder.endSession();
        CHECK(s.recorder.canUndoLastRecording());
        CHECK(s.recorder.undoLastRecording(s.error));
        CHECK(s.recorder.getTargetClipId() == id);
        CHECK(s.target()->getNumNotes() == 2); // Undo only the second arm interval.
        CHECK(s.target()->getNotes()[0].getPitch() == 60 && s.target()->getNotes()[1].getPitch() == 69);
    }
    {
        Session s;
        auto source = std::make_unique<MidiClip>(0, 4);
        const auto id = s.project.getClipPool().addClip(std::move(source));
        auto* track = s.project.getTrackList().addTrack("Selected lane");
        track->addClipInstance(std::make_unique<ClipInstance>(id, s.channel->getId(), 0, 4));
        track->addClipInstance(std::make_unique<ClipInstance>(id, s.channel->getId(), 0, 2));
        auto* selected = track->getClipInstance(0);
        const auto trackId = track->getId(), placementId = selected->getId();
        auto options = s.options(MidiRecorder::Mode::Continuous, id);
        options.clipFocused = false; options.startBeat = 4;
        CHECK(!s.recorder.start(options, true, s.error)); // No source/start guessing.
        options.trackId = trackId;
        CHECK(!s.recorder.start(options, true, s.error));
        options.placementId = "missing";
        CHECK(!s.recorder.start(options, true, s.error));
        options.placementId = placementId;
        const auto wrongSource = s.project.getClipPool().addClip(std::make_unique<MidiClip>(0, 4));
        options.clipId = wrongSource;
        CHECK(!s.recorder.start(options, true, s.error));
        options.clipId = id;
        const auto correctChannel = options.channelId;
        auto* other = s.project.getChannelList().addChannel("Wrong routing");
        Probe otherProbe;
        other->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(otherProbe)));
        options.channelId = other->getId();
        CHECK(!s.recorder.start(options, true, s.error));
        options.channelId = correctChannel;
        CHECK(s.recorder.start(options, true, s.error));
        CHECK(s.recorder.getPlaybackTransport().getPositionInBeats() == 4);
        s.note(64); s.render(); s.recorder.endSession();
        CHECK(s.project.getTrackList().getNumTracks() == 1 && track->getNumClipInstances() == 2);
        CHECK(s.target()->getNotes()[0].getStartTime() == 4.125);
        CHECK(selected->getDuration() == 5 && track->getClipInstance(1)->getDuration() == 2);
        CHECK(s.recorder.undoLastRecording(s.error));
        CHECK(s.target()->getNumNotes() == 0 && s.target()->getDuration() == 4);
        CHECK(selected->getDuration() == 4 && track->getClipInstance(1)->getDuration() == 2);
        other->setPlugin(nullptr);
    }
    {
        Session s;
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub), true, s.error));
        s.note(60); s.render(); s.recorder.endSession();
        CHECK(s.recorder.canUndoLastRecording());
        const auto id = s.recorder.getTargetClipId();
        auto replacement = s.target()->clone();
        s.project.getClipPool().removeClip(id);
        CHECK(s.project.getClipPool().restoreClip(id, std::move(replacement)) == id);
        CHECK(!s.recorder.canUndoLastRecording()); // Loading reused IDs cannot resurrect stale undo.
        CHECK(!s.recorder.undoLastRecording(s.error));
        CHECK(s.target()->getNumNotes() == 1);
    }
    {
        Session s;
        auto source = std::make_unique<MidiClip>(0, 4);
        const auto id = s.project.getClipPool().addClip(std::move(source));
        auto* track = s.project.getTrackList().addTrack("Takes lane");
        track->addClipInstance(std::make_unique<ClipInstance>(id, s.channel->getId(), 0, 4));
        auto* placed = track->getClipInstance(0);
        auto options = s.options(MidiRecorder::Mode::Takes, id, 1);
        options.clipFocused = false; options.startBeat = 1;
        options.trackId = track->getId(); options.placementId = placed->getId();
        s.project.getTransportState().setLoopRegion(1, 2);
        s.project.getTransportState().setLoopEnabled(true);
        CHECK(s.recorder.start(options, true, s.error));
        s.note(60); s.render();
        CHECK(s.recorder.setRecording(false, s.error));
        const auto first = s.recorder.getTargetClipId();
        CHECK(first != id && placed->getClipId() == first);
        CHECK(s.recorder.setRecording(true, s.error));
        s.note(62); s.render(); s.recorder.endSession();
        const auto second = s.recorder.getTargetClipId();
        CHECK(first != second && placed->getClipId() == second);
        CHECK(s.recorder.undoLastRecording(s.error));
        CHECK(s.project.getClipPool().getClip(first) && !s.project.getClipPool().getClip(second));
        CHECK(placed->getClipId() == first && s.recorder.getTargetClipId() == first);
        CHECK(s.recorder.getTakeCount() == 1);
    }
    {
        Session s;
        auto source = std::make_unique<MidiClip>(0, 4);
        const std::vector<MidiExpressionEvent> original{
            {0, 0xb0, 64, 127}, {0.75, 0xb0, 64, 0}, {1.1, 0xb0, 1, 31},
            {1.3, 0xe0, 22, 77}, {1.4, 0xb1, 1, 75}, {1.5, 0xb0, 1, 55},
            {2.5, 0xb0, 1, 77}, {2.75, 0xe0, 23, 78}};
        CHECK(source->setExpressionEvents(original));
        const auto id = s.project.getClipPool().addClip(std::move(source));
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub, id, 4), true, s.error));
        s.note(60); s.render();
        CHECK(s.recorder.setRecording(false, s.error));
        CHECK(s.target()->getExpressionEvents().size() == original.size());
        const auto has = [&](double beat, int status, int data1, int data2) {
            for (const auto& event : s.target()->getExpressionEvents())
                if (event.beat == beat && event.status == status && event.data1 == data1 && event.data2 == data2) return true;
            return false;
        };
        for (const auto& event : original) CHECK(has(event.beat, event.status, event.data1, event.data2));
        CHECK(s.recorder.setRecording(true, s.error));
        s.input.addEvent(juce::MidiMessage::controllerEvent(1, 1, 99), 60); // Source beat 1.25.
        s.render();
        CHECK(s.recorder.setRecording(false, s.error));
        CHECK(has(1.1, 0xb0, 1, 31)); // Before actual movement.
        CHECK(!has(1.5, 0xb0, 1, 55));
        CHECK(has(1.25, 0xb0, 1, 99) && has(2.0, 0xb0, 1, 55)); // Punch boundary restores the original lane.
        CHECK(has(1.4, 0xb1, 1, 75) && has(1.3, 0xe0, 22, 77)); // Channel and bend are separate lanes.
        CHECK(has(0, 0xb0, 64, 127) && has(0.75, 0xb0, 64, 0));
        CHECK(s.recorder.setRecording(true, s.error));
        s.input.addEvent(juce::MidiMessage::pitchWheel(1, 12345), 120);
        s.render(); s.recorder.stop();
        CHECK(has(2.5, 0xb0, 1, 77));
        CHECK(!has(2.75, 0xe0, 23, 78) && has(3, 0xe0, 23, 78));
    }
    {
        Session s;
        auto source = std::make_unique<MidiClip>(0, 4);
        CHECK(source->setExpressionEvents({{0, 0xb0, 64, 127}, {0.5, 0xb0, 1, 88}, {0.75, 0xb0, 64, 0}}));
        const auto id = s.project.getClipPool().addClip(std::move(source));
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Replace, id, 4), false, s.error));
        s.recorder.getPlaybackTransport().setPositionInBeats(0.25);
        CHECK(s.recorder.setRecording(true, s.error));
        s.input.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 60);
        s.render();
        CHECK(s.recorder.setRecording(false, s.error));
        bool boundary = false, untouched = false;
        for (const auto& event : s.target()->getExpressionEvents()) {
            boundary = boundary || (event.beat == 1.25 && event.data1 == 64 && event.data2 == 0);
            untouched = untouched || (event.beat == 0.5 && event.data1 == 1 && event.data2 == 88);
        }
        CHECK(boundary && untouched);
        s.instrument->count = 0;
        s.render();
        CHECK(s.instrument->saw(0xb0, 64, 0, 0)); // Boundary restoration is audible without a seek/UI roundtrip.
    }
    {
        Session s;
        auto source = std::make_unique<MidiClip>(0, 4);
        CHECK(source->setExpressionEvents({{0.1, 0xb0, 1, 10}, {1, 0xb0, 1, 80}}));
        const auto id = s.project.getClipPool().addClip(std::move(source));
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub, id, 4), true, s.error));
        s.input.addEvent(juce::MidiMessage::controllerEvent(1, 1, 20), 120);
        s.render();
        CHECK(s.recorder.setRecording(false, s.error));
        int boundaryValue = -1;
        for (const auto& event : s.target()->getExpressionEvents()) if (event.beat == 1) boundaryValue = event.data2;
        CHECK(boundaryValue == 80); // The untouched original event at punch-out wins.
        s.instrument->count = 0;
        s.render();
        CHECK(s.instrument->saw(0xb0, 1, 80, 0));
        CHECK(!s.instrument->saw(0xb0, 1, 10, 0));
    }
    {
        Session s;
        const int initial = s.project.getClipPool().getNumClips();
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Continuous), true, s.error));
        s.render(); s.recorder.stop();
        CHECK(s.project.getClipPool().getNumClips() == initial);
        CHECK(s.recorder.getTargetClipId() == InvalidClipId);
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Continuous), true, s.error));
        s.note(64, 30, 90, 7);
        s.input.addEvent(juce::MidiMessage::controllerEvent(7, 1, 83), 60);
        s.input.addEvent(juce::MidiMessage::pitchWheel(7, 12345), 120);
        s.render();
        CHECK(s.recorder.hasPendingContent());
        CHECK(s.recorder.getTargetClipId() == InvalidClipId);
        s.render(); // Silent traversed time still grows a nonempty continuous source.
        CHECK(s.recorder.setRecording(false, s.error));
        auto* clip = s.target();
        CHECK(clip && clip->getNumNotes() == 1 && clip->getDuration() == 2);
        CHECK(clip->getNotes()[0].getStartTime() == 0.125);
        CHECK(clip->getNotes()[0].getDuration() == 0.25);
        CHECK(clip->getNotes()[0].getChannel() == 7);
        CHECK(clip->getExpressionEvents().size() == 2);
        CHECK(clip->getExpressionEvents()[0].beat == 0.25);
        CHECK(clip->getExpressionEvents()[1].beat == 0.5);
        CHECK(!s.recorder.isRecording() && s.recorder.getPlaybackTransport().isPlaying());
        CHECK(!s.recorder.hasPendingContent());
        s.recorder.stop();
        CHECK(!s.recorder.getPlaybackTransport().isPlaying());
        CHECK(s.recorder.undoLastRecording(s.error));
        CHECK(s.project.getClipPool().getNumClips() == initial);
    }
    {
        Session s;
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub), true, s.error));
        s.note(60);
        s.input.addEvent(juce::MidiMessage::controllerEvent(1, 1, 44), 60);
        s.render();
        const auto liveAttacks = s.probe.notes;
        s.instrument->count = 0;
        s.render(); // No poll or message dispatch between passes.
        CHECK(s.probe.notes == liveAttacks + 1);
        CHECK(s.instrument->saw(0xb0, 1, 44, 0)); // Physical ownership is restored at the wrap, not chased later.
        CHECK(s.recorder.getTargetClipId() == InvalidClipId);
        s.note(67, 120, 180);
        s.render();
        s.recorder.stop();
        CHECK(s.target() && s.target()->getNumNotes() == 2);
        CHECK(s.project.getTrackList().getNumTracks() == 0); // No fake clip placement.
    }
    {
        Session s;
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub), true, s.error));
        s.input.addEvent(juce::MidiMessage::noteOn(2, 72, static_cast<juce::uint8>(100)), 180);
        s.render();
        s.input.addEvent(juce::MidiMessage::noteOff(2, 72), 60);
        s.render();
        s.recorder.stop();
        CHECK(s.target() && s.target()->getNumNotes() == 2);
        const auto& notes = s.target()->getNotes();
        CHECK(notes[0].getStartTime() == 0.75 && notes[0].getEndTime() == 1.0);
        CHECK(notes[1].getStartTime() == 0.0 && notes[1].getEndTime() == 0.25);
    }
    {
        Session s;
        auto source = std::make_unique<MidiClip>(0, 4);
        source->setName("Original");
        source->addNote(Note(60, 0, 2));
        source->addNote(Note(67, 3, 0.5));
        CHECK(source->addExpressionEvent({0.5, 0xb0, 1, 77}));
        CHECK(source->addExpressionEvent({2.0, 0xb0, 1, 88}));
        const auto id = s.project.getClipPool().addClip(std::move(source));
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Replace, id, 4), true, s.error));
        s.render(); // Silence replaces only [0, 1), not the entire source.
        s.recorder.stop();
        CHECK(s.recorder.getTargetClipId() == id);
        CHECK(s.target()->getNumNotes() == 2);
        CHECK(s.target()->getNotes()[0].getStartTime() == 1);
        CHECK(s.target()->getNotes()[0].getEndTime() == 2);
        CHECK(s.target()->getNotes()[1].getStartTime() == 3);
        CHECK(s.target()->getExpressionEvents().size() == 2); // No controller movement: lanes stay intact.
        CHECK(s.project.getClipPool().getNumClips() == 2); // Recoverable original.
        CHECK(s.recorder.undoLastRecording(s.error));
        CHECK(s.target()->getNotes()[0].getStartTime() == 0);
        CHECK(s.target()->getExpressionEvents().size() == 2);
    }
    {
        Session s;
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub, InvalidClipId, 0.25), true, s.error));
        s.note(73, 45, 75);
        s.render(); // Four spans in one callback; the captured first pass is already replayed.
        CHECK(s.probe.notes > 1);
        s.recorder.stop();
        CHECK(s.target() && s.target()->getNumNotes() == 2);
        CHECK(s.target()->getNotes()[0].getStartTime() == 0.1875);
        CHECK(s.target()->getNotes()[0].getEndTime() == 0.25);
        CHECK(s.target()->getNotes()[1].getStartTime() == 0);
        CHECK(s.target()->getNotes()[1].getEndTime() == 0.0625);
    }
    {
        Session s;
        auto original = std::make_unique<MidiClip>(0, 1);
        original->addNote(Note(48, 0, 0.5));
        const auto id = s.project.getClipPool().addClip(std::move(original));
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Takes, id), true, s.error));
        s.note(60); s.render();
        s.note(62); s.render();
        s.recorder.stop();
        CHECK(s.recorder.getTakeCount() == 2);
        CHECK(s.target()->getNotes()[0].getPitch() == 62);
        CHECK(s.recorder.selectTake(0, s.error));
        s.recorder.poll();
        CHECK(s.target()->getNotes()[0].getPitch() == 60);
        auto* untouched = dynamic_cast<MidiClip*>(s.project.getClipPool().getClip(id));
        CHECK(untouched && untouched->getNumNotes() == 1 && untouched->getNotes()[0].getPitch() == 48);
        CHECK(s.recorder.getTargetClipId() != id);
        CHECK(!s.recorder.selectTake(20, s.error));
    }
    {
        Session s;
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub), true, s.error));
        s.note(60); s.render();
        CHECK(s.recorder.setRecording(false, s.error));
        CHECK(s.target()->replaceContent({Note(69, 0.125, 0.25)}, {}));
        s.recorder.poll();
        const auto before = s.probe.notes;
        s.render();
        CHECK(s.probe.notes == before + 1);
        CHECK(s.probe.events.back().pitch == 69);
        CHECK(s.recorder.setRecording(true, s.error));
        s.note(72, 120, 180); s.render(); s.recorder.stop();
        CHECK(s.target()->getNumNotes() == 2);
        CHECK(s.target()->getNotes()[0].getPitch() == 69);
    }
    {
        Session s;
        s.project.getTransportState().setPositionInBeats(8);
        s.project.getTransportState().setPlaying(true);
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub), true, s.error));
        CHECK(!s.project.getTransportState().isPlaying());
        CHECK(s.project.getTransportState().getPositionInBeats() == 8);
        auto* other = s.project.getChannelList().addChannel("Other");
        Probe otherProbe;
        other->setPlugin(std::make_unique<PluginHost>(std::make_unique<OfflineInstrument>(otherProbe)));
        s.mixer.setActiveChannel(s.project.getChannelList().indexOfChannel(other));
        s.note(60); s.render();
        CHECK(s.probe.notes == 1 && otherProbe.notes == 0); // Pinned target.
        s.recorder.endSession();
        s.render();
        CHECK(s.project.getTransportState().getPositionInBeats() == 8);
        other->setPlugin(nullptr); // Probe must outlive its plugin.
        auto options = s.options(MidiRecorder::Mode::Continuous);
        options.clipFocused = false; options.startBeat = 8;
        CHECK(s.recorder.start(options, true, s.error));
        s.note(65); s.render(); s.render(); s.recorder.stop();
        CHECK(s.target()->getDuration() == 2);
        CHECK(s.project.getTrackList().getNumTracks() == 1);
        const auto* placement = s.project.getTrackList().getTrack(0)->getClipInstance(0);
        CHECK(placement && placement->getStartTime() == 8 && placement->getDuration() == 2);
    }
    {
        Session s;
        auto clip = std::make_unique<MidiClip>(0, 400);
        std::vector<MidiExpressionEvent> events(65535, {100.0, 0xb0, 1, 12});
        CHECK(clip->setExpressionEvents(std::move(events)));
        const auto id = s.project.getClipPool().addClip(std::move(clip));
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Continuous, id), true, s.error));
        s.note(60, 24, 48); // Last available workspace slot.
        s.note(62, 72, 96); // Explicit fault, never overwrite the captured prefix.
        s.render();
        CHECK(!s.recorder.isRecording());
        CHECK(s.recorder.getStatus().containsIgnoreCase("fault"));
        s.recorder.stop();
        CHECK(s.target()->getNumNotes() == 1 && s.target()->getNotes()[0].getPitch() == 60);
        CHECK(s.target()->getExpressionEvents().size() == 65535);
    }
    {
        Session s;
        auto backing = std::make_unique<MidiClip>(0, 1);
        backing->addNote(Note(48, 0, 0.5));
        const auto id = s.project.getClipPool().addClip(std::move(backing));
        s.project.getTrackList().addTrack("Backing")->addClipInstance(
            std::make_unique<ClipInstance>(id, s.channel->getId(), 8, 1));
        auto options = s.options(MidiRecorder::Mode::Overdub);
        options.clipFocused = false; options.startBeat = 8;
        s.project.getTransportState().setLoopRegion(8, 9);
        s.project.getTransportState().setLoopEnabled(true);
        CHECK(s.recorder.start(options, true, s.error));
        s.note(60); s.render();
        CHECK(s.probe.notes == 2); // Backing and live input, processed at one destination.
        s.recorder.stop();
        CHECK(s.target() && s.target()->getNumNotes() == 1);
        CHECK(s.target()->getNotes()[0].getPitch() == 60); // Never record accompaniment.
        CHECK(s.target()->getNotes()[0].getStartTime() == 0.125);
        CHECK(s.project.getTrackList().getNumTracks() == 2);
    }
    {
        Session s;
        CHECK(s.recorder.start(s.options(MidiRecorder::Mode::Overdub), true, s.error));
        s.note(60); s.render(); CHECK(s.recorder.setRecording(false, s.error));
        const auto id = s.recorder.getTargetClipId();
        CHECK(id != InvalidClipId);
        s.project.getClipPool().removeClip(id);
        s.recorder.poll();
        CHECK(!s.recorder.isSessionActive());
    }
    {
        // Persisted controller-only arrangement, including start/seek/wrap state.
        Probe probe;
        ChannelList channels;
        TrackList tracks;
        ClipPool clips;
        TransportState transport;
        auto* channel = channels.addChannel("Expression");
        auto plugin = std::make_unique<Instrument>(probe);
        auto* instrument = plugin.get();
        channel->setPlugin(std::make_unique<PluginHost>(std::move(plugin)));
        auto source = std::make_unique<MidiClip>(0, 2);
        CHECK(source->addExpressionEvent({0.25, 0xb3, 1, 44}));
        CHECK(source->addExpressionEvent({0.25, 0xb3, 1, 55}));
        CHECK(source->addExpressionEvent({0.5, 0xe3, 23, 78}));
        const auto id = clips.addClip(std::move(source));
        tracks.addTrack()->addClipInstance(std::make_unique<ClipInstance>(id, channel->getId(), 0, 2));
        const auto snapshot = compileArrangement(tracks, clips, channels, 1);
        CHECK(snapshot.notes.empty() && snapshot.destinations.size() == 1 && snapshot.events.size() == 3);
        ChannelMixer mixer(channels, tracks, clips, transport);
        mixer.prepareToPlay(480, 240);
        juce::AudioBuffer<float> audio(2, 240);
        juce::MidiBuffer midi;
        midi.ensureSize(32768);
        const auto render = [&] {
            CHECK(AudioQuiescence::instance().enter());
            const auto before = renderAllocations;
            rendering = true; mixer.processBlock(audio, midi); rendering = false;
            AudioQuiescence::instance().leave();
            CHECK(renderAllocations == before && !probe.invalidInputOffset);
        };
        transport.setPlaying(true);
        render();
        CHECK(instrument->saw(0xb3, 1, 44, 60) && instrument->saw(0xb3, 1, 55, 60));
        CHECK(instrument->saw(0xe3, 23, 78, 120));
        instrument->count = 0;
        transport.setPositionInBeats(0.75);
        render();
        CHECK(instrument->saw(0xb3, 1, 55, 0) && instrument->saw(0xe3, 23, 78, 0));
        instrument->count = 0;
        transport.setPositionInBeats(0);
        transport.setLoopRegion(0, 1); transport.setLoopEnabled(true);
        render(); render();
        CHECK(instrument->saw(0xb3, 1, 0, 0) && instrument->saw(0xe3, 0, 64, 0));
        mixer.releaseResources();
    }
}
