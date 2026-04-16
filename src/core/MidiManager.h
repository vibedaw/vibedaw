#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace vibedaw {

class MidiListener {
public:
    virtual ~MidiListener() = default;
    virtual void handleMidiMessage(const juce::MidiMessage& message, int sampleOffset) = 0;
};

class MidiManager : private juce::MidiInputCallback {
public:
    MidiManager();
    ~MidiManager();
    
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
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    
    std::unique_ptr<juce::MidiInput> midiInput;
    juce::ListenerList<MidiListener> listeners;
    juce::MidiBuffer incomingMidi;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiManager)
};

} // namespace vibedaw
