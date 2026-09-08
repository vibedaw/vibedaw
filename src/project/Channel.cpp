#include "Channel.h"
#include "plugins/PluginHost.h"
#include "utils/Logger.h"
#include <cmath>

namespace vibedaw {

Channel::Channel(const juce::String& channelName, Type type, ChannelId id)
    : id_(id)
    , name(channelName)
    , channelType(type)
{
    LOG_INFO("Channel: Created '" + name + "'");
}

Channel::~Channel() {
    stopTimer();
    AudioQuiescence::Edit edit;
    releaseResources();
    LOG_INFO("Channel: Destroyed '" + name + "'");
}

void Channel::notifyChanged() {
    jassert(juce::MessageManager::getInstanceWithoutCreating() != nullptr &&
            juce::MessageManager::getInstanceWithoutCreating()->isThisTheMessageThread());
    sendChangeMessage();
}

void Channel::setName(const juce::String& newName) {
    name = newName;
    notifyChanged();
}

void Channel::setPlugin(std::unique_ptr<PluginHost> pluginHost) {
    AudioQuiescence::Edit edit;
    deliveredNotes.fill(0);
    arrangementVoices.fill(0);
    deliveredCount = 0;
    sustainSeen = false;
    voicesSinceReset = false;
    arrangementSinceReset = false;
    resetPending.store(false);
    startTimerHz(60);
    plugin = std::move(pluginHost);
    if (plugin) {
        channelType = Type::Instrument;
        LOG_INFO("Channel: Plugin set on '" + name + "': " + plugin->getName());
        if (isPrepared_) {
            LOG_INFO("Channel: Preparing newly set plugin");
            plugin->prepareToPlay(preparedSampleRate, preparedBlockSize);
        }
    }
    notifyChanged();
}

void Channel::setSampleFile(const juce::File& file) {
    sampleFile = file;
    if (file.exists()) {
        channelType = Type::Sampler;
        LOG_INFO("Channel: Sample set on '" + name + "': " + file.getFileName());
    }
    notifyChanged();
}

void Channel::setVolume(float newVolume) {
    if (!std::isfinite(newVolume)) return;
    volume = juce::jlimit(0.0f, 2.0f, newVolume);
    controls.publish({volume, pan, muted, solo});
    notifyChanged();
}

void Channel::setPan(float newPan) {
    if (!std::isfinite(newPan)) return;
    pan = juce::jlimit(-1.0f, 1.0f, newPan);
    controls.publish({volume, pan, muted, solo});
    notifyChanged();
}

void Channel::setMuted(bool m) {
    muted = m;
    controls.publish({volume, pan, muted, solo});
    notifyChanged();
}

void Channel::setSolo(bool value) {
    solo = value;
    controls.publish({volume, pan, muted, solo});
    notifyChanged();
}

void Channel::setColour(const juce::Colour& c) {
    colour = c;
    notifyChanged();
}

void Channel::prepareToPlay(double sampleRate, int blockSize) {
    AudioQuiescence::Edit edit;
    LOG_INFO("Channel: Preparing '" + name + "' - SR: " + juce::String(sampleRate) + " BS: " + juce::String(blockSize));
    
    preparedSampleRate = sampleRate;
    preparedBlockSize = blockSize;
    isPrepared_ = false;
    
    if (plugin) {
        plugin->prepareToPlay(sampleRate, blockSize);
    }
    isPrepared_ = true;
    // Resolve deferred cleanup after preparation, before reopening admission.
    servicePendingVoiceReset();
    
    meter.prepare(sampleRate);
}

void Channel::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    const auto state = controls.acquire();
    processWithControls(audio, midi, state.muted ? 0.0f : state.volume, state.pan);
    meter.update(audio);
}

void Channel::processWithControls(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi,
                                  float volume, float pan) {
    if (audio.getNumSamples() == 0) return;
    if (resetPending.load()) { audio.clear(); return; }
    
    if (plugin) {
        // Record before processing: adapters may consume/clear the MIDI buffer.
        for (const auto event : midi) {
            if (event.numBytes != 3) continue;
            const auto status = event.data[0] & 0xf0;
            const auto index = (event.data[0] & 15) * 128 + (event.data[1] & 127);
            if (status == 0x90 && event.data[2] != 0) {
                ++deliveredNotes[index]; ++deliveredCount;
                voicesSinceReset = true;
            } else if ((status == 0x80 || status == 0x90) && deliveredNotes[index] != 0) {
                --deliveredNotes[index]; --deliveredCount;
            } else if (status == 0xb0 && (event.data[1] == 64 || event.data[1] == 66) && event.data[2] >= 64) {
                sustainSeen = true;
            }
        }
        plugin->processBlock(audio, midi);
    }
    
    if (volume == 0) { audio.clear(); return; }
    if (audio.getNumChannels() >= 2) {
        // Linear stereo balance: unity centre preserves the baseline loudness.
        const float leftGain = 1.0f - juce::jmax(0.0f, pan);
        const float rightGain = 1.0f + juce::jmin(0.0f, pan);
        
        audio.applyGain(0, 0, audio.getNumSamples(), leftGain * volume);
        audio.applyGain(1, 0, audio.getNumSamples(), rightGain * volume);
    } else {
        for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
            audio.applyGain(channel, 0, audio.getNumSamples(), volume);
        }
    }
    
}

bool Channel::canDeliverMidi(const juce::MidiBuffer& midi) const {
    auto notes = deliveredNotes;
    auto count = deliveredCount;
    for (const auto event : midi) {
        if (event.numBytes != 3) continue;
        const auto status = event.data[0] & 0xf0;
        const auto index = (event.data[0] & 15) * 128 + (event.data[1] & 127);
        if (status == 0x90 && event.data[2] != 0) {
            if (++count > maxDeliveredNotes) return false;
            ++notes[index];
        } else if ((status == 0x80 || status == 0x90) && notes[index] != 0) {
            --notes[index]; --count;
        }
    }
    return true;
}

void Channel::appendNoteCleanup(juce::MidiBuffer& midi) {
    for (unsigned i = 0; i < deliveredNotes.size(); ++i)
        for (unsigned n = 0; n < deliveredNotes[i]; ++n)
            midi.addEvent(juce::MidiMessage::noteOff(static_cast<int>(i / 128 + 1), static_cast<int>(i % 128)), 0);
    // processBlock records these actual releases before handing them to the adapter.
}

void Channel::servicePendingVoiceReset() {
    if (!resetPending.load()) return;
    AudioQuiescence::Edit edit;
    if (!isPrepared_) return; // VST3 reset reactivates the instance; wait for preparation.
    if (plugin) plugin->resetVoices();
    deliveredNotes.fill(0);
    arrangementVoices.fill(0);
    deliveredCount = 0;
    voicesSinceReset = false;
    arrangementSinceReset = false;
    // A mapped pedal parameter can survive reset. Keep this conservative latch
    // until plugin replacement, but redundant panic without fresh notes needs no reset.
    resetPending.store(false);
}

void Channel::releaseResources() {
    AudioQuiescence::Edit edit;
    isPrepared_ = false;
    meter.clear();
    if (plugin) {
        plugin->releaseResources();
    }
    LOG_INFO("Channel: Released resources for '" + name + "'");
}

} // namespace vibedaw
