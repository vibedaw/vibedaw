#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ProcessorBase.h"
#include "AudioBoundary.h"

namespace vibedaw {

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
    
    double getCurrentSampleRate() const;
    int getCurrentBufferSize() const;
    
    juce::String getCurrentDeviceType() const;
    juce::String getCurrentOutputDeviceName() const;
    
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    void handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) override;
    unsigned getMidiOverflowCount() const { return midiOverflow.load(); }
    unsigned getSkippedBlockCount() const { return skippedBlocks.load(); }
    void requestPanic() noexcept { panic.store(true); }
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
    struct MidiEvent { unsigned char bytes[3]{}; int size = 0; };
    BoundedQueue<MidiEvent, 2048> midiQueue;
    std::mutex midiProducers;
    std::atomic<bool> panic{false};
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
