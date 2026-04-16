#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace vibedaw {

struct SidebarSettings {
    juce::String name;
    int width = 250;
    bool expanded = true;
    int order = 0;
    
    SidebarSettings() = default;
    SidebarSettings(const juce::String& n, int w = 250, bool e = true, int o = 0)
        : name(n), width(w), expanded(e), order(o) {}
};

struct Settings {
    juce::String pluginPath;
    juce::String audioDeviceType;
    juce::String audioOutputDevice;
    juce::String midiInputDevice;
    int sampleRate = 44100;
    int bufferSize = 512;
    
    juce::Array<SidebarSettings> leftSidebars;
    juce::Array<SidebarSettings> rightSidebars;
    
    Settings() = default;
    
    void loadFromFile(const juce::File& file);
    void saveToFile(const juce::File& file);
    
    static juce::File getDefaultSettingsFile();
};

} // namespace vibedaw
