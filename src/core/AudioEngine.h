#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ProcessorBase.h"

namespace vibedaw {

class AudioEngine {
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
    
    juce::MidiMessageCollector& getMidiMessageCollector() { return processorPlayer.getMidiMessageCollector(); }
    
private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer processorPlayer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

} // namespace vibedaw
