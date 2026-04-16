#include "PluginHost.h"
#include "utils/Logger.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace vibedaw {

PluginHost::PluginHost() {
    formatManager.addDefaultFormats();
    LOG_INFO("PluginHost: Constructed with " + juce::String(formatManager.getNumFormats()) + " plugin formats");
}

PluginHost::~PluginHost() {
    releaseResources();
    LOG_INFO("PluginHost: Destroyed");
}

bool PluginHost::loadPlugin(const juce::String& pluginPath) {
    LOG_INFO("PluginHost: Loading plugin from: " + pluginPath);
    
    releaseResources();
    pluginInstance.reset();
    currentPluginPath = pluginPath;
    
    juce::File pluginFile(pluginPath);
    if (!pluginFile.exists()) {
        LOG_ERROR("PluginHost: Plugin file does not exist: " + pluginPath);
        return false;
    }
    
    juce::PluginDescription description;
    bool found = false;
    
    for (int i = 0; i < formatManager.getNumFormats(); ++i) {
        auto* format = formatManager.getFormat(i);
        if (format == nullptr) continue;
        
        juce::OwnedArray<juce::PluginDescription> descriptions;
        if (format->fileMightContainThisPluginType(pluginPath)) {
            format->findAllTypesForFile(descriptions, pluginPath);
            
            if (descriptions.size() > 0) {
                description = *descriptions[0];
                found = true;
                LOG_INFO("PluginHost: Found plugin: " + description.name + " (" + format->getName() + ")");
                break;
            }
        }
    }
    
    if (!found) {
        LOG_ERROR("PluginHost: No valid plugin found in file");
        return false;
    }
    
    juce::String error;
    pluginInstance = formatManager.createPluginInstance(description, currentSampleRate, currentBlockSize, error);
    
    if (pluginInstance == nullptr) {
        LOG_ERROR("PluginHost: Failed to create plugin instance: " + error);
        return false;
    }
    
    LOG_INFO("PluginHost: Successfully loaded plugin: " + pluginInstance->getName());
    LOG_INFO("PluginHost: Plugin has " + juce::String(pluginInstance->getNumInputChannels()) + " inputs, " 
             + juce::String(pluginInstance->getNumOutputChannels()) + " outputs");
    
    return true;
}

void PluginHost::prepareToPlay(double sampleRate, int blockSize) {
    if (pluginInstance == nullptr) {
        LOG_WARN("PluginHost: Cannot prepare - no plugin loaded");
        return;
    }
    
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    
    LOG_INFO("PluginHost: Preparing plugin - SR: " + juce::String(sampleRate) + " BS: " + juce::String(blockSize));
    pluginInstance->prepareToPlay(sampleRate, blockSize);
    LOG_INFO("PluginHost: Plugin prepared");
}

void PluginHost::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    if (pluginInstance == nullptr) {
        audio.clear();
        return;
    }
    
    pluginInstance->processBlock(audio, midi);
}

void PluginHost::releaseResources() {
    if (pluginInstance != nullptr) {
        pluginInstance->releaseResources();
        LOG_INFO("PluginHost: Released plugin resources");
    }
}

const juce::String PluginHost::getName() const {
    return getPluginName();
}

bool PluginHost::hasEditor() const {
    if (pluginInstance == nullptr) return false;
    return pluginInstance->hasEditor();
}

std::unique_ptr<juce::AudioProcessorEditor> PluginHost::createEditor() {
    if (pluginInstance == nullptr) {
        LOG_ERROR("PluginHost: Cannot create editor - no plugin loaded");
        return nullptr;
    }
    
    auto* editor = pluginInstance->createEditor();
    if (editor == nullptr) {
        LOG_ERROR("PluginHost: Plugin returned null editor");
        return nullptr;
    }
    
    LOG_INFO("PluginHost: Created plugin editor");
    return std::unique_ptr<juce::AudioProcessorEditor>(editor);
}

juce::String PluginHost::getPluginName() const {
    if (pluginInstance == nullptr) {
        return "<no plugin>";
    }
    return pluginInstance->getName();
}

} // namespace vibedaw
