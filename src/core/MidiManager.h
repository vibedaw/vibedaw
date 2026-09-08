#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "AudioBoundary.h"

namespace vibedaw {
class AudioEngine;

class MidiListener {
public:
    virtual ~MidiListener() = default;
    virtual void handleMidiMessage(const juce::MidiMessage& message, int sampleOffset) = 0;
};

class MidiManager : private juce::MidiInputCallback, private juce::Timer {
public:
    MidiManager();
    ~MidiManager();
    // Configure once before connecting MIDI; both objects outlive this manager.
    void setAudioDestination(AudioEngine& engine, juce::MidiKeyboardState& keyboard);
    
    juce::StringArray getAvailableDevices() const;
    bool connectToDevice(int deviceIndex);
    bool connectToDevice(const juce::String& deviceName);
    void disconnect();
    
    bool isConnected() const { return midiInput != nullptr; }
    juce::String getCurrentDeviceName() const;
    
    void addListener(MidiListener* listener);
    void removeListener(MidiListener* listener);
    
    void sendMidiMessage(const juce::MidiMessage& message);
    
private:
    friend struct MidiManagerTestAccess;
    void timerCallback() override;
    AudioEngine* audio = nullptr;
    juce::MidiKeyboardState* keyboard = nullptr;
    struct Feedback { unsigned char bytes[3]{}; int size = 0; };
    BoundedQueue<Feedback, 2048> feedback;
    std::mutex producers;
    std::atomic<bool> feedbackOverflow{false};
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    
    std::unique_ptr<juce::MidiInput> midiInput;
    juce::ListenerList<MidiListener> listeners;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiManager)
};

} // namespace vibedaw
