#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace vibedaw {

class ProcessorBase {
public:
    virtual ~ProcessorBase() = default;
    virtual void prepareToPlay(double sampleRate, int blockSize) = 0;
    virtual void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) = 0;
    virtual void releaseResources() = 0;
    virtual const juce::String getName() const = 0;
};

} // namespace vibedaw
