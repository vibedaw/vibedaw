#pragma once

#include "ui/DawWindow.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace vibedaw {

class PluginHost;

class PluginWindow : public DawWindow {
public:
    ~PluginWindow() override;
    
    void closeButtonPressed() override;
    
private:
    friend class PluginHost;
    friend struct PluginWindowTestAccess;
    PluginWindow(PluginHost* pluginHost, const juce::String& title, bool addToDesktop = true);
    PluginHost* pluginHost = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindow)
};

} // namespace vibedaw
