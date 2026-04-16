#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace vibedaw {

struct Settings {
    juce::String pluginPath;
    juce::String audioDeviceType;
    juce::String audioOutputDevice;
    juce::String midiInputDevice;
    int sampleRate = 44100;
    int bufferSize = 512;
    
    Settings() = default;
    
    void loadFromFile(const juce::File& file);
    void saveToFile(const juce::File& file);
    
    static juce::File getDefaultSettingsFile();
};

} // namespace vibedaw
