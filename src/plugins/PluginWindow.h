#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace vibedaw {

class PluginHost;

class PluginWindow : public juce::DocumentWindow {
public:
    ~PluginWindow() override;
    
    void closeButtonPressed() override;
    
private:
    friend class PluginHost;
    PluginWindow(PluginHost* pluginHost, const juce::String& title);
    PluginHost* pluginHost = nullptr;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginWindow)
};

} // namespace vibedaw
