#include "Logger.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace vibedaw {

static juce::String getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time), "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count();
    return juce::String(ss.str());
}

void Logger::log(Level level, const juce::String& message) {
    juce::String levelStr;
    juce::String colourCode;
    
    switch (level) {
        case Info:
            levelStr = "INFO";
            colourCode = "\033[32m";
            break;
        case Warn:
            levelStr = "WARN";
            colourCode = "\033[33m";
            break;
        case Error:
            levelStr = "ERROR";
            colourCode = "\033[31m";
            break;
    }
    
    juce::String output = "[" + getTimestamp() + "] [" + levelStr + "] " + message;
    
    std::cout << colourCode.toStdString() << output.toStdString() << "\033[0m" << std::endl;
}

} // namespace vibedaw
