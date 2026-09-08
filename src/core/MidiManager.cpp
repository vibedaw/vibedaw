#include "MidiManager.h"
#include "AudioEngine.h"
#include "utils/Logger.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace vibedaw {

MidiManager::MidiManager() {
    LOG_INFO("MidiManager: Constructing");
}

MidiManager::~MidiManager() {
    stopTimer();
    disconnect();
    LOG_INFO("MidiManager: Destroyed");
}

void MidiManager::setAudioDestination(AudioEngine& engine, juce::MidiKeyboardState& state) {
    jassert(!midiInput);
    audio = &engine;
    keyboard = &state;
    startTimerHz(60);
}

juce::StringArray MidiManager::getAvailableDevices() const {
    auto devices = juce::MidiInput::getAvailableDevices();
    juce::StringArray result;
    for (const auto& device : devices) {
        result.add(device.name);
    }
    return result;
}

bool MidiManager::connectToDevice(int deviceIndex) {
    auto devices = juce::MidiInput::getAvailableDevices();
    if (deviceIndex < 0 || deviceIndex >= devices.size()) {
        LOG_ERROR("MidiManager: Invalid device index: " + juce::String(deviceIndex));
        return false;
    }
    return connectToDevice(devices[deviceIndex].name);
}

bool MidiManager::connectToDevice(const juce::String& deviceName) {
    disconnect();
    
    auto devices = juce::MidiInput::getAvailableDevices();
    int deviceIndex = -1;
    
    for (int i = 0; i < devices.size(); ++i) {
        if (devices[i].name == deviceName) {
            deviceIndex = i;
            break;
        }
    }
    
    if (deviceIndex < 0) {
        LOG_WARN("MidiManager: Device not found: " + deviceName);
        return false;
    }
    
    midiInput = juce::MidiInput::openDevice(devices[deviceIndex].identifier, this);
    
    if (midiInput == nullptr) {
        LOG_ERROR("MidiManager: Failed to open device: " + deviceName);
        return false;
    }
    
    midiInput->start();
    LOG_INFO("MidiManager: Connected to: " + deviceName);
    return true;
}

void MidiManager::disconnect() {
    if (midiInput != nullptr) {
        midiInput->stop();
        midiInput.reset();
        for (int ch = 1; ch <= 16; ++ch)
            sendMidiMessage(juce::MidiMessage::allSoundOff(ch));
        LOG_INFO("MidiManager: Disconnected");
    }
}

juce::String MidiManager::getCurrentDeviceName() const {
    if (midiInput == nullptr) {
        return juce::String();
    }
    return midiInput->getName();
}

void MidiManager::addListener(MidiListener* listener) {
    listeners.add(listener);
}

void MidiManager::removeListener(MidiListener* listener) {
    listeners.remove(listener);
}

void MidiManager::sendMidiMessage(const juce::MidiMessage& message) {
    handleIncomingMidiMessage(nullptr, message);
}

void MidiManager::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) {
    if (audio) audio->handleIncomingMidiMessage(source, message);
    std::lock_guard<std::mutex> lock(producers);
    Feedback event;
    event.size = message.getRawDataSize();
    if (event.size < 1 || event.size > 3) return;
    std::copy_n(message.getRawData(), event.size, event.bytes);
    if (!feedback.push(event)) feedbackOverflow.store(true);
}

void MidiManager::timerCallback() {
    const bool overflow = feedbackOverflow.exchange(false);
    Feedback event;
    for (unsigned i = 0; i < 2048 && feedback.pop(event); ++i) {
        if (overflow) continue;
        juce::MidiMessage message(event.bytes, event.size);
        if (audio && keyboard) audio->updateKeyboardFeedback(*keyboard, message);
        listeners.call([&](MidiListener& l) { l.handleMidiMessage(message, 0); });
    }
    if (overflow && audio && keyboard) {
        audio->requestPanic();
        for (int ch = 1; ch <= 16; ++ch)
            audio->updateKeyboardFeedback(*keyboard, juce::MidiMessage::allNotesOff(ch));
    }
}

} // namespace vibedaw
