#include "Channel.h"
#include "plugins/PluginHost.h"
#include "utils/Logger.h"
#include <cmath>

namespace vibedaw {

Channel::Channel(const juce::String& channelName, Type type)
    : name(channelName)
    , channelType(type)
{
    LOG_INFO("Channel: Created '" + name + "'");
}

Channel::~Channel() {
    releaseResources();
    LOG_INFO("Channel: Destroyed '" + name + "'");
}

void Channel::setPlugin(std::unique_ptr<PluginHost> pluginHost) {
    plugin = std::move(pluginHost);
    if (plugin) {
        channelType = Type::Instrument;
        LOG_INFO("Channel: Plugin set on '" + name + "': " + plugin->getName());
        if (isPrepared_) {
            LOG_INFO("Channel: Preparing newly set plugin");
            plugin->prepareToPlay(preparedSampleRate, preparedBlockSize);
        }
    }
}

void Channel::setSampleFile(const juce::File& file) {
    sampleFile = file;
    if (file.exists()) {
        channelType = Type::Sampler;
        LOG_INFO("Channel: Sample set on '" + name + "': " + file.getFileName());
    }
}

void Channel::setVolume(float newVolume) {
    volume = juce::jlimit(0.0f, 2.0f, newVolume);
}

void Channel::setPan(float newPan) {
    pan = juce::jlimit(-1.0f, 1.0f, newPan);
}

void Channel::setMuted(bool m) {
    muted = m;
}

void Channel::setColour(const juce::Colour& c) {
    colour = c;
}

void Channel::prepareToPlay(double sampleRate, int blockSize) {
    LOG_INFO("Channel: Preparing '" + name + "' - SR: " + juce::String(sampleRate) + " BS: " + juce::String(blockSize));
    
    preparedSampleRate = sampleRate;
    preparedBlockSize = blockSize;
    isPrepared_ = true;
    
    if (plugin) {
        plugin->prepareToPlay(sampleRate, blockSize);
    }
    
    levelDecay = 1.0f - std::pow(0.01f, 1.0f / (sampleRate * 0.5f));
}

void Channel::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    if (muted) {
        audio.clear();
        leftLevel = 0.0f;
        rightLevel = 0.0f;
        return;
    }
    
    int numMidi = midi.getNumEvents();
    if (numMidi > 0 && plugin) {
        LOG_INFO("Channel '" + name + "': Processing " + juce::String(numMidi) + " MIDI events through plugin");
    }
    
    if (plugin) {
        plugin->processBlock(audio, midi);
    }
    
    if (pan != 0.0f && audio.getNumChannels() >= 2) {
        float leftGain = std::cos((pan + 1.0f) * 0.5f * juce::MathConstants<float>::halfPi);
        float rightGain = std::cos((1.0f - (pan + 1.0f) * 0.5f) * juce::MathConstants<float>::halfPi);
        
        audio.applyGain(0, 0, audio.getNumSamples(), leftGain * volume);
        audio.applyGain(1, 0, audio.getNumSamples(), rightGain * volume);
    } else {
        for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
            audio.applyGain(channel, 0, audio.getNumSamples(), volume);
        }
    }
    
    updateLevels(audio);
}

void Channel::updateLevels(const juce::AudioBuffer<float>& audio) {
    if (audio.getNumChannels() >= 2) {
        float leftSum = 0.0f;
        float rightSum = 0.0f;
        int numSamples = audio.getNumSamples();
        
        const float* leftChannel = audio.getReadPointer(0);
        const float* rightChannel = audio.getReadPointer(1);
        
        for (int i = 0; i < numSamples; ++i) {
            leftSum += leftChannel[i] * leftChannel[i];
            rightSum += rightChannel[i] * rightChannel[i];
        }
        
        float leftRms = std::sqrt(leftSum / numSamples);
        float rightRms = std::sqrt(rightSum / numSamples);
        
        float currentLeft = leftLevel.load();
        float currentRight = rightLevel.load();
        
        if (leftRms > currentLeft) {
            leftLevel = leftRms;
        } else {
            leftLevel = currentLeft * levelDecay;
        }
        
        if (rightRms > currentRight) {
            rightLevel = rightRms;
        } else {
            rightLevel = currentRight * levelDecay;
        }
    } else if (audio.getNumChannels() == 1) {
        float sum = 0.0f;
        int numSamples = audio.getNumSamples();
        const float* channel = audio.getReadPointer(0);
        
        for (int i = 0; i < numSamples; ++i) {
            sum += channel[i] * channel[i];
        }
        
        float rms = std::sqrt(sum / numSamples);
        float current = leftLevel.load();
        
        if (rms > current) {
            leftLevel = rms;
            rightLevel = rms;
        } else {
            leftLevel = current * levelDecay;
            rightLevel = current * levelDecay;
        }
    }
}

void Channel::releaseResources() {
    isPrepared_ = false;
    if (plugin) {
        plugin->releaseResources();
    }
    LOG_INFO("Channel: Released resources for '" + name + "'");
}

} // namespace vibedaw