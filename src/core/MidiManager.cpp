#include "MidiManager.h"
#include "utils/Logger.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace vibedaw {

MidiManager::MidiManager() {
    LOG_INFO("MidiManager: Constructing");
}

MidiManager::~MidiManager() {
    disconnect();
    LOG_INFO("MidiManager: Destroyed");
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
    
    midiInput = juce::MidiInput::openDevice(deviceIndex, this);
    
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
    listeners.call([&message](MidiListener& l) {
        l.handleMidiMessage(message, 0);
    });
}

void MidiManager::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) {
    if (message.isNoteOn()) {
        LOG_INFO("MidiManager: Note On - " + juce::MidiMessage::getMidiNoteName(message.getNoteNumber(), true, true, 4)
                 + " vel: " + juce::String(message.getVelocity()));
    } else if (message.isNoteOff()) {
        LOG_INFO("MidiManager: Note Off - " + juce::MidiMessage::getMidiNoteName(message.getNoteNumber(), true, true, 4));
    } else if (message.isController()) {
        LOG_INFO("MidiManager: CC " + juce::String(message.getControllerNumber())
                 + " val: " + juce::String(message.getControllerValue()));
    } else {
        LOG_INFO("MidiManager: MIDI message - " + juce::String::toHexString(message.getRawData(), message.getRawDataSize()));
    }
    
    listeners.call([&message](MidiListener& l) {
        l.handleMidiMessage(message, 0);
    });
}

} // namespace vibedaw
