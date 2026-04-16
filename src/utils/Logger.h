#pragma once

#include <juce_core/juce_core.h>

namespace vibedaw::Logger {
    enum Level {
        Info,
        Warn,
        Error
    };
    
    void log(Level level, const juce::String& message);
}

#define LOG_INFO(msg)  vibedaw::Logger::log(vibedaw::Logger::Info, msg)
#define LOG_WARN(msg)  vibedaw::Logger::log(vibedaw::Logger::Warn, msg)
#define LOG_ERROR(msg) vibedaw::Logger::log(vibedaw::Logger::Error, msg)
