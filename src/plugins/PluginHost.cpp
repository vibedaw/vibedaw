#include "PluginHost.h"
#include "PluginWindow.h"
#include "core/AudioBoundary.h"
#include "utils/Logger.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace vibedaw {

PluginHost::PluginHost() {
    formatManager.addDefaultFormats();
    LOG_INFO("PluginHost: Constructed with " + juce::String(formatManager.getNumFormats()) + " plugin formats");
}

PluginHost::PluginHost(std::unique_ptr<juce::AudioPluginInstance> instance)
    : pluginInstance(std::move(instance)) {
    if (pluginInstance) {
        pluginInstance->fillInPluginDescription(pluginDescription_);
        hasDescription_ = true;
    }
}

PluginHost::~PluginHost() {
    JUCE_ASSERT_MESSAGE_THREAD
    AudioQuiescence::Edit edit;
    closeWindows();
    releaseResources();
    pluginInstance.reset();
    LOG_INFO("PluginHost: Destroyed");
}

bool PluginHost::loadPlugin(const juce::String& pluginPath) {
    JUCE_ASSERT_MESSAGE_THREAD
    AudioQuiescence::Edit edit;
    closeWindows();
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
    pluginDescription_ = description;
    hasDescription_ = true;

    juce::String error;
    pluginInstance = formatManager.createPluginInstance(description, currentSampleRate, currentBlockSize, error);
    
    if (pluginInstance == nullptr) {
        LOG_ERROR("PluginHost: Failed to create plugin instance: " + error);
        return false;
    }
    
    LOG_INFO("PluginHost: Successfully loaded plugin: " + pluginInstance->getName());
    LOG_INFO("PluginHost: Plugin has " + juce::String(pluginInstance->getTotalNumInputChannels()) + " inputs, " 
             + juce::String(pluginInstance->getTotalNumOutputChannels()) + " outputs");
    if (pluginInstance->getTotalNumInputChannels() > 2 || pluginInstance->getTotalNumOutputChannels() > 2) {
        LOG_ERROR("PluginHost: Only mono/stereo plugins are supported by the bounded audio path");
        pluginInstance.reset();
        return false;
    }
    
    return true;
}

bool PluginHost::createFromDescription(const juce::PluginDescription& description) {
    JUCE_ASSERT_MESSAGE_THREAD
    AudioQuiescence::Edit edit;
    closeWindows();
    LOG_INFO("PluginHost: Creating plugin from description: " + description.name);

    releaseResources();
    pluginInstance.reset();
    pluginDescription_ = description;
    hasDescription_ = true;
    currentPluginPath = description.fileOrIdentifier;

    juce::String error;
    pluginInstance = formatManager.createPluginInstance(description, currentSampleRate, currentBlockSize, error);
    if (pluginInstance == nullptr) {
        LOG_ERROR("PluginHost: Failed to create plugin instance from description: " + error);
        return false;
    }
    if (pluginInstance->getTotalNumInputChannels() > 2 || pluginInstance->getTotalNumOutputChannels() > 2) {
        LOG_ERROR("PluginHost: Only mono/stereo plugins are supported by the bounded audio path");
        pluginInstance.reset();
        return false;
    }
    LOG_INFO("PluginHost: Successfully created plugin: " + pluginInstance->getName());
    return true;
}

void PluginHost::prepareToPlay(double sampleRate, int blockSize) {
    AudioQuiescence::Edit edit;
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
    AudioQuiescence::Edit edit;
    if (pluginInstance != nullptr) {
        pluginInstance->releaseResources();
        LOG_INFO("PluginHost: Released plugin resources");
    }
}

void PluginHost::resetVoices() {
    JUCE_ASSERT_MESSAGE_THREAD
    AudioQuiescence::Edit edit;
    if (pluginInstance) pluginInstance->reset();
}

void PluginHost::registerWindow(PluginWindow* window) { windows.push_back(window); }
void PluginHost::unregisterWindow(PluginWindow* window) {
    windows.erase(std::remove(windows.begin(), windows.end(), window), windows.end());
}
void PluginHost::closeWindows() {
    while (!windows.empty()) delete windows.back();
}

const juce::String PluginHost::getName() const {
    return getPluginName();
}

bool PluginHost::hasEditor() const {
    JUCE_ASSERT_MESSAGE_THREAD
    AudioQuiescence::Edit edit;
    return pluginInstance && pluginInstance->hasEditor();
}

std::unique_ptr<juce::AudioProcessorEditor> PluginHost::createEditor() {
    JUCE_ASSERT_MESSAGE_THREAD
    AudioQuiescence::Edit edit;
    if (!hasEditor() || pluginInstance->getActiveEditor() != nullptr) return nullptr;
    return std::unique_ptr<juce::AudioProcessorEditor>(pluginInstance->createEditorIfNeeded());
}

void PluginHost::openEditorWindow() {
    JUCE_ASSERT_MESSAGE_THREAD
    AudioQuiescence::Edit edit;
    if (!windows.empty()) {
        windows.front()->setVisible(true);
        windows.front()->toFront(true);
    } else if (hasEditor()) {
        new PluginWindow(this, getPluginName());
    }
}

juce::String PluginHost::getPluginName() const {
    if (pluginInstance == nullptr) {
        return "<no plugin>";
    }
    return pluginInstance->getName();
}

} // namespace vibedaw
