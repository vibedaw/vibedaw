#include "Track.h"
#include "ClipInstance.h"
#include "utils/Logger.h"
#include <cmath>

namespace vibedaw {

Track::Track(const juce::String& trackName)
    : name(trackName)
{
    LOG_INFO("Track: Created '" + name + "'");
}

Track::~Track() {
    releaseResources();
    LOG_INFO("Track: Destroyed '" + name + "'");
}

void Track::setColour(const juce::Colour& c) {
    colour = c;
}

void Track::prepareToPlay(double sampleRate, int blockSize) {
    LOG_INFO("Track: Preparing '" + name + "' - SR: " + juce::String(sampleRate) + " BS: " + juce::String(blockSize));
    levelDecay = 1.0f - std::pow(0.01f, 1.0f / (sampleRate * 0.5f));
}

void Track::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
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
    LOG_INFO("Track: Released resources for '" + name + "'");
}

void Track::addClipInstance(std::unique_ptr<ClipInstance> instance) {
    if (instance && instance->isValid()) {
        instance->onChange_ = [this] { sendChangeMessage(); };
        clipInstances.push_back(std::move(instance));
        sendChangeMessage();
    }
}

void Track::removeClipInstance(int index) {
    if (index >= 0 && index < static_cast<int>(clipInstances.size())) {
        clipInstances.erase(clipInstances.begin() + index);
        sendChangeMessage();
    }
}

void Track::clearClipInstances() {
    clipInstances.clear();
    sendChangeMessage();
}

ClipInstance* Track::getClipInstance(int index) const {
    if (index >= 0 && index < static_cast<int>(clipInstances.size())) {
        return clipInstances[index].get();
    }
    return nullptr;
}

} // namespace vibedaw
