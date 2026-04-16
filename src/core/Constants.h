#pragma once

#include <vector>
#include <juce_core/juce_core.h>

namespace vibedaw::Constants {
    constexpr const char* APP_NAME = "VibeDAW";
    constexpr const char* APP_VERSION = "0.1.0";
    constexpr const char* SETTINGS_FILE = "settings.json";
    constexpr const char* CONFIG_DIR = ".config/vibedaw";
    constexpr const char* DEFAULT_VST3_PATH = "~/.vst3/u-he/TyrellN6.vst3";
    constexpr int DEFAULT_SAMPLE_RATE = 44100;
    constexpr int DEFAULT_BUFFER_SIZE = 512;
    constexpr double DEFAULT_TEMPO = 120.0;
    
    inline std::vector<juce::String> getDefaultVST3SearchPaths() {
        return {
            "~/.vst3",
            "/usr/lib/vst3",
            "/usr/local/lib/vst3"
        };
    }
}
