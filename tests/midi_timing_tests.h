#ifndef VIBEDAW_MIDI_TIMING_TESTS_H
#define VIBEDAW_MIDI_TIMING_TESTS_H

// Include after AudioEngineTestAccess, CHECK, and the allocation guards.
#include <array>
#include <limits>

namespace midi_timing_tests {
using namespace vibedaw;
#define MIDI_CHECK(x) CHECK(x)

class Capture : public juce::AudioProcessor {
public:
    struct Event { int offset = 0, size = 0; std::array<unsigned char, 3> bytes{}; };
    std::array<Event, 2048> events{};
    int count = 0, blocks = 0;
    bool invalid = false;
    const juce::String getName() const override { return "MIDI timing capture"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override {
        ++blocks;
        count = 0;
        for (const auto event : midi) {
            if (count == static_cast<int>(events.size()) || event.numBytes > 3 ||
                event.samplePosition < 0 || event.samplePosition >= buffer.getNumSamples()) {
                invalid = true;
                continue;
            }
            auto& captured = events[static_cast<size_t>(count++)];
            captured.offset = event.samplePosition;
            captured.size = event.numBytes;
            std::copy_n(event.data, event.numBytes, captured.bytes.begin());
        }
    }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};
}

inline void midiTimingTests() {
    midi_timing_tests::Capture capture;
    AudioEngine engine;
    AudioEngineTestAccess::resetClock();
    AudioEngineTestAccess::attachClock(engine);
    AudioEngineTestAccess::prepare(engine, 48000, 480);
    engine.setProcessor(&capture);
    juce::AudioBuffer<float> buffer(2, 481);
    auto& now = AudioEngineTestAccess::nowMs;
    const auto render = [&](double time, int size = 480) {
        const auto allocations = renderAllocations, deletions = renderDeletions;
        rendering = true;
        AudioEngineTestAccess::renderAt(engine, buffer, size, time);
        rendering = false;
        MIDI_CHECK(renderAllocations == allocations && renderDeletions == deletions && !capture.invalid);
    };
    const auto cleanup = [&] {
        MIDI_CHECK(capture.count == 48);
        for (int i = 0; i < 48; ++i) {
            const auto& e = capture.events[static_cast<size_t>(i)];
            MIDI_CHECK(e.offset == 0 && e.size == 3 && e.bytes[0] == 0xb0 + i / 3);
            MIDI_CHECK(e.bytes[1] == (i % 3 == 0 ? 64 : i % 3 == 1 ? 120 : 123));
            MIDI_CHECK(e.bytes[2] == 0);
        }
    };
    render(1000); cleanup();

    // Missing timestamps use the very same clock as the callback. Feedback
    // updates both keyboard edges but must not enqueue duplicate input.
    juce::MidiKeyboardState keyboard;
    keyboard.addListener(&engine);
    now = 1002.5;
    const auto on = juce::MidiMessage::noteOn(1, 60, 0.5f);
    engine.handleIncomingMidiMessage(nullptr, on);
    engine.updateKeyboardFeedback(keyboard, on);
    MIDI_CHECK(keyboard.isNoteOn(1, 60));
    now = 1007.5;
    const auto off = juce::MidiMessage::noteOff(1, 60);
    engine.handleIncomingMidiMessage(nullptr, off);
    engine.updateKeyboardFeedback(keyboard, off);
    MIDI_CHECK(!keyboard.isNoteOn(1, 60));
    render(1010);
    MIDI_CHECK(capture.count == 2 && capture.events[0].offset == 120 && capture.events[1].offset == 360);
    MIDI_CHECK(capture.events[0].bytes[0] == 0x90 && capture.events[1].bytes[0] == 0x80);
    render(1020); MIDI_CHECK(capture.count == 0);

    now = 1022.5; keyboard.noteOn(2, 61, 0.5f);
    now = 1027.5; keyboard.noteOff(2, 61, 0.5f);
    render(1030);
    MIDI_CHECK(capture.count == 2 && capture.events[0].offset == 120 && capture.events[1].offset == 360);
    MIDI_CHECK(capture.events[0].bytes[0] == 0x91 && capture.events[1].bytes[0] == 0x81);
    keyboard.removeListener(&engine);

    // Device timestamps are seconds: preserve a timestamp earlier than delivery.
    // Regressing timestamps cannot move a release before its attack; ties stay FIFO.
    now = 1040;
    auto stamped = on.withTimeStamp(1.03125);
    engine.handleIncomingMidiMessage(nullptr, stamped);
    engine.handleIncomingMidiMessage(nullptr, off.withTimeStamp(1.030));
    engine.handleIncomingMidiMessage(nullptr, on.withTimeStamp(1.035));
    render(1040);
    MIDI_CHECK(capture.count == 3 && capture.events[0].offset == 60);
    MIDI_CHECK(capture.events[1].offset == 60 && capture.events[1].bytes[0] == 0x80);
    MIDI_CHECK(capture.events[2].offset == 240);

    // Negative/nonfinite/overflowing times fall back to ingress, and future times
    // clamp there rather than poisoning the FIFO or deferring events indefinitely.
    const std::array<double, 6> invalidTimes{{-1, 0, std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), std::numeric_limits<double>::max(), 1e9}};
    now = 1045;
    for (const auto time : invalidTimes) engine.handleIncomingMidiMessage(nullptr, on.withTimeStamp(time));
    render(1050);
    MIDI_CHECK(capture.count == 6);
    for (int i = 0; i < capture.count; ++i) MIDI_CHECK(capture.events[static_cast<size_t>(i)].offset == 240);

    // Old, exact-start and exact-end arrivals clamp safely; callback jitter does
    // not stretch timing. A freshly queued immediate event belongs at the end.
    now = 1051; engine.handleIncomingMidiMessage(nullptr, on);
    now = 1060; engine.handleIncomingMidiMessage(nullptr, on);
    now = 1070; engine.handleIncomingMidiMessage(nullptr, on);
    render(1070);
    MIDI_CHECK(capture.count == 3 && capture.events[0].offset == 0);
    MIDI_CHECK(capture.events[1].offset == 0 && capture.events[2].offset == 479);

    // Actual callback length, not prepared capacity, defines the window.
    now = 1077.5; engine.handleIncomingMidiMessage(nullptr, on);
    render(1080, 240);
    MIDI_CHECK(capture.count == 1 && capture.events[0].offset == 120);
    now = 1081; engine.handleIncomingMidiMessage(nullptr, off);
    render(1081, 1);
    MIDI_CHECK(capture.count == 1 && capture.events[0].offset == 0);

    // All 2048 slots can be drained without allocation, and a full queue still
    // rejects the extra event and replaces the entire batch with cleanup CCs.
    now = 1085;
    for (int i = 0; i < 2048; ++i) engine.handleIncomingMidiMessage(nullptr, on);
    render(1090); MIDI_CHECK(capture.count == 2048);
    for (int i = 0; i < capture.count; ++i) MIDI_CHECK(capture.events[static_cast<size_t>(i)].offset == 240);
    for (int i = 0; i < 2049; ++i) engine.handleIncomingMidiMessage(nullptr, on);
    MIDI_CHECK(engine.getMidiOverflowCount() == 1);
    render(1100); cleanup();
    render(1110); MIDI_CHECK(capture.count == 0);
    const unsigned char payload[]{1, 2};
    engine.handleIncomingMidiMessage(nullptr, juce::MidiMessage::createSysExMessage(payload, 2));
    engine.handleIncomingMidiMessage(nullptr, on);
    render(1120); cleanup();
    MIDI_CHECK(engine.getMidiOverflowCount() == 2);

    // Preserving edits retain input (stale arrivals clamp to zero). Destructive
    // rejection, including a nested edit, keeps the existing cleanup barrier.
    now = 1125; engine.handleIncomingMidiMessage(nullptr, on);
    auto blocks = capture.blocks;
    {
        AudioQuiescence::Edit edit(AudioQuiescence::Interruption::PreserveVoices);
        render(1130);
        MIDI_CHECK(capture.blocks == blocks && buffer.getMagnitude(0, 480) == 0);
    }
    render(1150);
    MIDI_CHECK(capture.count == 1 && capture.events[0].offset == 0);
    engine.handleIncomingMidiMessage(nullptr, on);
    {
        AudioQuiescence::Edit edit(AudioQuiescence::Interruption::PreserveVoices);
        AudioQuiescence::Edit destructive;
        render(1160);
    }
    render(1170); cleanup();
    engine.requestPanic(); engine.handleIncomingMidiMessage(nullptr, on);
    render(1180); cleanup();

    // Invalid/oversized blocks and restart/rate changes never leak queued attacks
    // through the cleanup barrier; subsequent input uses the new sample rate.
    double time = 1180;
    for (const int size : {-1, 0, 481}) {
        engine.handleIncomingMidiMessage(nullptr, on);
        render(time + 10, size);
        render(time + 20); cleanup();
        time += 20;
    }
    AudioEngineTestAccess::stop(engine);
    engine.handleIncomingMidiMessage(nullptr, on);
    AudioEngineTestAccess::prepare(engine, 96000, 480);
    render(1250); cleanup();
    now = 1252.5; engine.handleIncomingMidiMessage(nullptr, on);
    render(1255);
    MIDI_CHECK(capture.count == 1 && capture.events[0].offset == 240);
    AudioEngineTestAccess::prepare(engine, 44100, 441);
    render(1260, 441); cleanup();
    now = 1265; engine.handleIncomingMidiMessage(nullptr, on);
    render(1270, 441);
    MIDI_CHECK(capture.count == 1 && capture.events[0].offset == 220); // Floor fractional sample.
    render(1280, 441); MIDI_CHECK(capture.count == 0);
    engine.clearProcessor();
}
#undef MIDI_CHECK
#endif
