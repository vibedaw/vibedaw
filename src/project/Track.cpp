#include "Track.h"
#include "Clip.h"
#include "plugins/PluginHost.h"
#include "utils/Logger.h"
#include <cmath>

namespace vibedaw {

Track::Track(const juce::String& trackName, Type type)
    : name(trackName)
    , trackType(type)
{
    LOG_INFO("Track: Created '" + name + "'");
}

Track::~Track() {
    releaseResources();
    LOG_INFO("Track: Destroyed '" + name + "'");
}

void Track::setPlugin(std::unique_ptr<PluginHost> pluginHost) {
    plugin = std::move(pluginHost);
    if (plugin) {
        LOG_INFO("Track: Plugin set on '" + name + "': " + plugin->getName());
    } else {
        LOG_INFO("Track: Plugin cleared on '" + name + "'");
    }
}

void Track::setVolume(float newVolume) {
    volume = juce::jlimit(0.0f, 2.0f, newVolume);
}

void Track::setPan(float newPan) {
    pan = juce::jlimit(-1.0f, 1.0f, newPan);
}

void Track::setMuted(bool m) {
    muted = m;
}

void Track::setSolo(bool s) {
    solo = s;
}

void Track::setColour(const juce::Colour& c) {
    colour = c;
}

void Track::setSend(int index, int destinationTrack, float level, bool enabled) {
    if (index >= 0 && index < maxSends) {
        sends[index].destinationIndex = destinationTrack;
        sends[index].level = juce::jlimit(0.0f, 1.0f, level);
        sends[index].enabled = enabled;
    }
}

SendDestination Track::getSend(int index) const {
    if (index >= 0 && index < maxSends) {
        return sends[index];
    }
    return {};
}

void Track::prepareToPlay(double sampleRate, int blockSize) {
    LOG_INFO("Track: Preparing '" + name + "' - SR: " + juce::String(sampleRate) + " BS: " + juce::String(blockSize));
    
    if (plugin) {
        plugin->prepareToPlay(sampleRate, blockSize);
    }
    
    levelDecay = 1.0f - std::pow(0.01f, 1.0f / (sampleRate * 0.5f));
}

void Track::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    if (muted) {
        audio.clear();
        leftLevel = 0.0f;
        rightLevel = 0.0f;
        return;
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

void Track::updateLevels(const juce::AudioBuffer<float>& audio) {
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
        
        float newPeak = std::max(leftRms, rightRms);
        float currentPeak = peakLevel.load();
        if (newPeak > currentPeak) {
            peakLevel = newPeak;
        } else {
            peakLevel = currentPeak * levelDecay;
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

void Track::releaseResources() {
    if (plugin) {
        plugin->releaseResources();
    }
    LOG_INFO("Track: Released resources for '" + name + "'");
}

void Track::addClip(std::unique_ptr<Clip> clip) {
    if (clip) {
        clips.push_back(std::move(clip));
    }
}

void Track::removeClip(int index) {
    if (index >= 0 && index < static_cast<int>(clips.size())) {
        clips.erase(clips.begin() + index);
    }
}

void Track::clearClips() {
    clips.clear();
}

Clip* Track::getClip(int index) const {
    if (index >= 0 && index < static_cast<int>(clips.size())) {
        return clips[index].get();
    }
    return nullptr;
}

} // namespace vibedaw
