#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ProcessorBase.h"
#include "AudioBoundary.h"

namespace vibedaw {
class ChannelMixer;

class AudioEngine : private juce::AudioIODeviceCallback,
                    public juce::MidiInputCallback, public juce::MidiKeyboardStateListener {
public:
    AudioEngine();
    ~AudioEngine();
    
    void initialise();
    void shutdown();
    
    void setProcessor(juce::AudioProcessor* processor);
    void clearProcessor();
    
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    const juce::AudioDeviceManager& getDeviceManager() const { return deviceManager; }

    // Message-thread read of the audio callback load (0..1; near -1 before a
    // device is open). Display-only: UI status bar, never a safety decision.
    double getCpuUsage() const { return deviceManager.getCpuUsage(); }
    
    double getCurrentSampleRate() const;
    int getCurrentBufferSize() const;
    
    juce::String getCurrentDeviceType() const;
    juce::String getCurrentOutputDeviceName() const;
    
    // Input timestamps are JUCE counter-based seconds, not beats or wall-clock time.
    // Missing/invalid timestamps use ingress time; future timestamps clamp to now.
    // Each callback schedules the preceding block-duration window into its own
    // [0, samples) range (older -> 0, newer -> last sample). At steady cadence this
    // adds one block of input latency, preserving timing instead of quantizing to 0.
    // Monitoring and capture share these offsets; this is not device-latency compensation.
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    unsigned getMidiOverflowCount() const { return midiOverflow.load(); }
    unsigned getSkippedBlockCount() const { return skippedBlocks.load(); }
    void requestPanic() noexcept { inputLoss.store(true); panic.store(true); }
    // Message-thread visual feedback must not be re-enqueued as fresh live input.
    void updateKeyboardFeedback(juce::MidiKeyboardState&, const juce::MidiMessage&);
    
private:
    friend struct AudioEngineTestAccess;
    void prepare(double rate, int capacity);
    juce::AudioDeviceManager deviceManager;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
                                        const juce::AudioIODeviceCallbackContext&) override;
    struct MidiEvent {
        unsigned char bytes[3]{};
        int size = 0;
        double arrivalTimeMs = 0;
    };
    BoundedQueue<MidiEvent, 2048> midiQueue;
    std::mutex midiProducers;
    double lastMidiTimeMs = 0; // Producer-mutex protected; preserve FIFO on timestamp regressions.
    // Test-only injection via AudioEngineTestAccess, before producers/render start.
    // Must be a nonblocking monotonic millisecond clock, shared by ingress and render.
    double (*midiClock)() = &juce::Time::getMillisecondCounterHiRes;
    std::atomic<bool> panic{false};
    std::atomic<bool> inputLoss{false}, interrupted{false};
    bool hasRendered = false;
    ChannelMixer* inputStatusReceiver = nullptr; // Optional, cached off audio; generic processors still work.
    std::atomic<unsigned> midiOverflow{0}, skippedBlocks{0};
    juce::AudioProcessor* processor = nullptr;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer midiScratch;
    double sampleRate = 44100.0;
    int blockCapacity = 0;
    bool updatingKeyboardFeedback = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

} // namespace vibedaw
