#include "AudioEngine.h"
#include "Constants.h"
#include "utils/Logger.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace vibedaw {

AudioEngine::AudioEngine() {
    LOG_INFO("AudioEngine: Constructing");
}

AudioEngine::~AudioEngine() {
    shutdown();
    LOG_INFO("AudioEngine: Destroyed");
}

void AudioEngine::initialise() {
    LOG_INFO("AudioEngine: Initialising audio device manager");
    
    auto& deviceTypes = deviceManager.getAvailableDeviceTypes();
    juce::StringArray typeNames;
    for (auto* type : deviceTypes) {
        typeNames.add(type->getTypeName());
    }
    LOG_INFO("AudioEngine: Available device types: " + typeNames.joinIntoString(", "));
    
    juce::String error = deviceManager.initialise(
        0,
        2,
        nullptr,
        true,
        juce::String(),
        nullptr
    );
    
    if (error.isNotEmpty()) {
        LOG_ERROR("AudioEngine: Failed to initialise audio device: " + error);
        return;
    }
    
    auto* currentDevice = deviceManager.getCurrentAudioDevice();
    if (currentDevice != nullptr) {
        LOG_INFO("AudioEngine: Audio device opened: " + currentDevice->getName());
        LOG_INFO("AudioEngine: Sample rate: " + juce::String(getCurrentSampleRate()));
        LOG_INFO("AudioEngine: Buffer size: " + juce::String(getCurrentBufferSize()));
    }
    
    deviceManager.addAudioCallback(&processorPlayer);
    deviceManager.addMidiInputDeviceCallback({}, &processorPlayer.getMidiMessageCollector());
    LOG_INFO("AudioEngine: Initialised successfully");
}

void AudioEngine::shutdown() {
    LOG_INFO("AudioEngine: Shutting down");
    deviceManager.removeAudioCallback(&processorPlayer);
    deviceManager.closeAudioDevice();
    LOG_INFO("AudioEngine: Shutdown complete");
}

void AudioEngine::setProcessor(juce::AudioProcessor* processor) {
    LOG_INFO("AudioEngine: Setting processor: " + (processor ? processor->getName() : "nullptr"));
    processorPlayer.setProcessor(processor);
}

void AudioEngine::clearProcessor() {
    LOG_INFO("AudioEngine: Clearing processor");
    processorPlayer.setProcessor(nullptr);
}

double AudioEngine::getCurrentSampleRate() const {
    auto* device = deviceManager.getCurrentAudioDevice();
    return device ? device->getCurrentSampleRate() : 0.0;
}

int AudioEngine::getCurrentBufferSize() const {
    auto* device = deviceManager.getCurrentAudioDevice();
    return device ? device->getCurrentBufferSizeSamples() : 0;
}

juce::String AudioEngine::getCurrentDeviceType() const {
    auto* device = deviceManager.getCurrentAudioDevice();
    return device ? device->getTypeName() : juce::String();
}

juce::String AudioEngine::getCurrentOutputDeviceName() const {
    auto* device = deviceManager.getCurrentAudioDevice();
    return device ? device->getName() : juce::String();
}

} // namespace vibedaw
