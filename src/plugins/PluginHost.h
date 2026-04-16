#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/ProcessorBase.h"

namespace vibedaw {

class PluginHost : public ProcessorBase {
public:
    PluginHost();
    ~PluginHost() override;
    
    bool loadPlugin(const juce::String& pluginPath);
    bool isLoaded() const { return pluginInstance != nullptr; }
    
    void prepareToPlay(double sampleRate, int blockSize) override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;
    void releaseResources() override;
    const juce::String getName() const override;
    
    juce::AudioProcessor* getProcessor() const { return pluginInstance.get(); }
    juce::AudioPluginInstance* getPluginInstance() const { return pluginInstance.get(); }
    
    bool hasEditor() const;
    std::unique_ptr<juce::AudioProcessorEditor> createEditor();
    
    juce::String getPluginName() const;
    juce::String getPluginPath() const { return currentPluginPath; }
    
private:
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::String currentPluginPath;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHost)
};

} // namespace vibedaw
