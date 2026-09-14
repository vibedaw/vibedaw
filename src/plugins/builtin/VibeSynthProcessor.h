#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace vibedaw {

// Built-in polyphonic instrument: two oscillators, ADSR, state-variable filter.
// It is an ordinary AudioPluginInstance riding the PluginHost injection seam,
// so hosts without any VSTs can still make sound. Bounded audio contract:
// no allocation, logging, or locking in processBlock.
class VibeSynthProcessor : public juce::AudioPluginInstance {
public:
    VibeSynthProcessor();
    ~VibeSynthProcessor() override;

    static constexpr int voiceCount = 16;

    // Raw parameter atomics for the render path; pointers stay valid for the
    // processor's lifetime (APVTS owns the parameters).
    struct RawParameters {
        std::atomic<float>* osc1Wave = nullptr;
        std::atomic<float>* osc1Octave = nullptr;
        std::atomic<float>* osc1Detune = nullptr;
        std::atomic<float>* osc2Wave = nullptr;
        std::atomic<float>* osc2Octave = nullptr;
        std::atomic<float>* osc2Detune = nullptr;
        std::atomic<float>* oscMix = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* cutoff = nullptr;
        std::atomic<float>* resonance = nullptr;
        std::atomic<float>* filterType = nullptr;
        std::atomic<float>* gain = nullptr;
    };
    const RawParameters& rawParameters() const { return raw; }
    juce::AudioProcessorValueTreeState& getParameterState() { return parameters; }

    void fillInPluginDescription(juce::PluginDescription& description) const override;

    const juce::String getName() const override { return "VibeSynth"; }
    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;
    void reset() override;
    double getTailLengthSeconds() const override { return 5.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { juce::ignoreUnused(index); }
    const juce::String getProgramName(int index) override { juce::ignoreUnused(index); return "Default"; }
    void changeProgramName(int index, const juce::String& newName) override { juce::ignoreUnused(index, newName); }
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState parameters;
    RawParameters raw;
    juce::Synthesiser synth;
    double sampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VibeSynthProcessor)
};

} // namespace vibedaw
