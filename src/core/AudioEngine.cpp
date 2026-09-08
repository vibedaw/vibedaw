#include "AudioEngine.h"
#include "Constants.h"
#include "utils/Logger.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <cmath>

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
    
    deviceManager.addAudioCallback(this);
    LOG_INFO("AudioEngine: Initialised successfully");
}

void AudioEngine::shutdown() {
    LOG_INFO("AudioEngine: Shutting down");
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
    LOG_INFO("AudioEngine: Shutdown complete");
}

void AudioEngine::setProcessor(juce::AudioProcessor* processor) {
    LOG_INFO("AudioEngine: Setting processor: " + (processor ? processor->getName() : "nullptr"));
    AudioQuiescence::Edit edit;
    if (this->processor) this->processor->releaseResources();
    this->processor = processor;
    if (processor && blockCapacity > 0) processor->prepareToPlay(sampleRate, blockCapacity);
}

void AudioEngine::clearProcessor() {
    LOG_INFO("AudioEngine: Clearing processor");
    setProcessor(nullptr);
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    prepare(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::prepare(double rate, int capacity) {
    AudioQuiescence::Edit edit;
    if (!std::isfinite(rate) || rate <= 0 || capacity <= 0) {
        if (processor) processor->releaseResources();
        blockCapacity = 0;
        panic.store(true);
        return;
    }
    sampleRate = rate;
    blockCapacity = juce::jmax(1, capacity);
    scratch.setSize(2, blockCapacity);
    midiScratch.ensureSize(32768);
    if (processor) processor->prepareToPlay(sampleRate, blockCapacity);
    panic.store(true);
}

void AudioEngine::audioDeviceStopped() {
    AudioQuiescence::Edit edit;
    if (processor) processor->releaseResources();
    blockCapacity = 0;
}

void AudioEngine::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) {
    std::lock_guard<std::mutex> lock(midiProducers);
    MidiEvent event;
    event.size = message.getRawDataSize();
    if (event.size < 1 || event.size > 3) {
        ++midiOverflow;
        panic.store(true);
        return;
    }
    std::copy_n(message.getRawData(), event.size, event.bytes);
    if (!midiQueue.push(event)) { ++midiOverflow; panic.store(true); }
}

void AudioEngine::handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity) {
    if (updatingKeyboardFeedback) return;
    handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOn(channel, note, velocity));
}

void AudioEngine::handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity) {
    if (updatingKeyboardFeedback) return;
    handleIncomingMidiMessage(nullptr, juce::MidiMessage::noteOff(channel, note, velocity));
}

void AudioEngine::updateKeyboardFeedback(juce::MidiKeyboardState& state, const juce::MidiMessage& message) {
    updatingKeyboardFeedback = true;
    state.processNextMidiEvent(message);
    updatingKeyboardFeedback = false;
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const*, int,
        float* const* outputs, int numOutputs, int samples,
        const juce::AudioIODeviceCallbackContext&) {
    if (samples <= 0) { ++skippedBlocks; panic.store(true); return; }
    for (int ch = 0; ch < numOutputs; ++ch)
        if (outputs[ch]) juce::FloatVectorOperations::clear(outputs[ch], samples);
    auto& gate = AudioQuiescence::instance();
    if (!gate.enter()) { ++skippedBlocks; panic.store(true); return; }
    if (!processor || samples > blockCapacity) {
        ++skippedBlocks;
        panic.store(true);
        gate.leave();
        return;
    }
    midiScratch.clear();
    const bool cleanup = panic.exchange(false);
    MidiEvent event;
    for (unsigned i = 0; i < 2048 && midiQueue.pop(event); ++i)
        if (!cleanup) midiScratch.addEvent(event.bytes, event.size, 0);
    if (cleanup) {
        for (int ch = 1; ch <= 16; ++ch) {
            midiScratch.addEvent(juce::MidiMessage::controllerEvent(ch, 64, 0), 0);
            midiScratch.addEvent(juce::MidiMessage::allSoundOff(ch), 0);
            midiScratch.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
        }
    }
    float* pointers[]{scratch.getWritePointer(0), scratch.getWritePointer(1)};
    juce::AudioBuffer<float> block(pointers, 2, samples);
    block.clear();
    processor->processBlock(block, midiScratch);
    for (int ch = 0; ch < juce::jmin(2, numOutputs); ++ch)
        if (outputs[ch]) juce::FloatVectorOperations::copy(outputs[ch], block.getReadPointer(ch), samples);
    gate.leave();
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
