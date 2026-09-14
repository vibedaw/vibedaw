#include "VibeSynthProcessor.h"
#include "InternalPluginFormat.h"
#include "VibeSynthEditor.h"
#include <cmath>

namespace vibedaw {
namespace {

class VibeSynthSound : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

// One bounded voice: two naive-band-limited-enough oscillators, TPT state
// variable filter, and an ADSR. All state is fixed-size; parameter changes are
// read as relaxed atomics per render call.
class VibeSynthVoice : public juce::SynthesiserVoice {
public:
    explicit VibeSynthVoice(const VibeSynthProcessor::RawParameters& rawParameters)
        : parameters(rawParameters) {}

    bool canPlaySound(juce::SynthesiserSound* sound) override {
        return dynamic_cast<VibeSynthSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int /*pitchWheel*/) override {
        fundamental = 440.0 * std::pow(2.0, (midiNoteNumber - 69) / 12.0);
        velocityLevel = juce::jlimit(0.0f, 1.0f, velocity);
        phase1 = 0.0;
        phase2 = 0.0;
        integrator1 = 0.0;
        integrator2 = 0.0;
        if (sampleRate > 0.0) envelope.setSampleRate(sampleRate);
        applyEnvelopeParameters();
        envelope.noteOn();
    }

    void stopNote(float, bool allowTailOff) override {
        if (allowTailOff) {
            envelope.noteOff();
        } else {
            envelope.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void setCurrentPlaybackSampleRate(double newRate) override {
        sampleRate = newRate;
        // Synthesiser::addVoice pushes rate 0 at construction; only the ADSR
        // rejects a non-positive rate.
        if (newRate > 0.0) envelope.setSampleRate(newRate);
    }

    void renderNextBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override {
        if (!SynthesiserVoice::isVoiceActive() || sampleRate <= 0.0 || numSamples <= 0) return;

        const int wave1 = juce::roundToInt(parameters.osc1Wave->load(std::memory_order_relaxed));
        const int wave2 = juce::roundToInt(parameters.osc2Wave->load(std::memory_order_relaxed));
        const double octave1 = juce::roundToInt(parameters.osc1Octave->load(std::memory_order_relaxed));
        const double octave2 = juce::roundToInt(parameters.osc2Octave->load(std::memory_order_relaxed));
        const double detune1 = parameters.osc1Detune->load(std::memory_order_relaxed);
        const double detune2 = parameters.osc2Detune->load(std::memory_order_relaxed);
        const double oscMix = juce::jlimit(0.0f, 1.0f, parameters.oscMix->load(std::memory_order_relaxed));
        const float outGain = juce::jlimit(0.0f, 1.0f, parameters.gain->load(std::memory_order_relaxed));
        applyEnvelopeParameters();

        const double nyquist = sampleRate * 0.5;
        const double frequency1 = juce::jlimit(1.0, nyquist,
            fundamental * std::pow(2.0, octave1 + detune1 / 1200.0));
        const double frequency2 = juce::jlimit(1.0, nyquist,
            fundamental * std::pow(2.0, octave2 + detune2 / 1200.0));
        const double increment1 = juce::MathConstants<double>::twoPi * frequency1 / sampleRate;
        const double increment2 = juce::MathConstants<double>::twoPi * frequency2 / sampleRate;

        // TPT (zero-delay-feedback) state variable filter; stable across the
        // whole cutoff range unlike the naive Chamberlin form.
        const double cutoff = juce::jlimit(20.0f, 18000.0f, parameters.cutoff->load(std::memory_order_relaxed));
        const double g = std::tan(juce::MathConstants<double>::pi *
                                  juce::jmin(cutoff, nyquist * 0.9) / sampleRate);
        const double k = 1.0 / (0.5 + juce::jlimit(0.0f, 1.0f, parameters.resonance->load(std::memory_order_relaxed)) * 9.5);
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;
        const bool highPass = juce::roundToInt(parameters.filterType->load(std::memory_order_relaxed)) == 1;

        const int numChannels = buffer.getNumChannels();
        float* const* channels = buffer.getArrayOfWritePointers();
        for (int i = 0; i < numSamples; ++i) {
            if (!envelope.isActive()) break;
            const double envelopeValue = envelope.getNextSample();
            const double source = (renderWave(wave1, phase1) * (1.0 - oscMix) +
                                   renderWave(wave2, phase2) * oscMix) * velocityLevel;
            const double v3 = source - integrator2;
            const double v1 = a1 * integrator1 + a2 * v3;
            const double v2 = integrator2 + a2 * integrator1 + a3 * v3;
            integrator1 = 2.0 * v1 - integrator1;
            integrator2 = 2.0 * v2 - integrator2;
            const double filtered = highPass ? source - k * v1 - v2 : v2;
            const double value = juce::jlimit(-2.0, 2.0, filtered * envelopeValue * outGain);
            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][startSample + i] += static_cast<float>(value);
            phase1 = advance(phase1, increment1);
            phase2 = advance(phase2, increment2);
        }
        if (!envelope.isActive()) clearCurrentNote();
    }

private:
    static double renderWave(int wave, double phase) {
        const double frac = phase * (1.0 / juce::MathConstants<double>::twoPi);
        switch (wave) {
            case 0: return std::sin(phase);
            case 1: return 2.0 * frac - 1.0;
            case 2: return frac < 0.5 ? 1.0 : -1.0;
            default: return 4.0 * std::abs(frac - 0.5) - 1.0;
        }
    }

    static double advance(double phase, double increment) {
        phase += increment;
        while (phase >= juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
        return phase;
    }

    void applyEnvelopeParameters() {
        juce::ADSR::Parameters shape;
        shape.attack = juce::jlimit(0.001f, 4.0f, parameters.attack->load(std::memory_order_relaxed));
        shape.decay = juce::jlimit(0.01f, 3.0f, parameters.decay->load(std::memory_order_relaxed));
        shape.sustain = juce::jlimit(0.0f, 1.0f, parameters.sustain->load(std::memory_order_relaxed));
        shape.release = juce::jlimit(0.01f, 5.0f, parameters.release->load(std::memory_order_relaxed));
        envelope.setParameters(shape);
    }

    const VibeSynthProcessor::RawParameters& parameters;
    juce::ADSR envelope;
    double sampleRate = 0.0;
    double fundamental = 440.0;
    float velocityLevel = 0.0f;
    double phase1 = 0.0, phase2 = 0.0;
    double integrator1 = 0.0, integrator2 = 0.0;
};

} // namespace

VibeSynthProcessor::VibeSynthProcessor()
    : AudioPluginInstance(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "state", createLayout())
{
    raw.osc1Wave = parameters.getRawParameterValue("osc1Wave");
    raw.osc1Octave = parameters.getRawParameterValue("osc1Octave");
    raw.osc1Detune = parameters.getRawParameterValue("osc1Detune");
    raw.osc2Wave = parameters.getRawParameterValue("osc2Wave");
    raw.osc2Octave = parameters.getRawParameterValue("osc2Octave");
    raw.osc2Detune = parameters.getRawParameterValue("osc2Detune");
    raw.oscMix = parameters.getRawParameterValue("oscMix");
    raw.attack = parameters.getRawParameterValue("attack");
    raw.decay = parameters.getRawParameterValue("decay");
    raw.sustain = parameters.getRawParameterValue("sustain");
    raw.release = parameters.getRawParameterValue("release");
    raw.cutoff = parameters.getRawParameterValue("cutoff");
    raw.resonance = parameters.getRawParameterValue("resonance");
    raw.filterType = parameters.getRawParameterValue("filterType");
    raw.gain = parameters.getRawParameterValue("gain");

    for (int i = 0; i < voiceCount; ++i) synth.addVoice(new VibeSynthVoice(raw));
    synth.addSound(new VibeSynthSound());
    synth.setNoteStealingEnabled(true);
}

VibeSynthProcessor::~VibeSynthProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout VibeSynthProcessor::createLayout() {
    using FloatParam = juce::AudioParameterFloat;
    using IntParam = juce::AudioParameterInt;
    using ChoiceParam = juce::AudioParameterChoice;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const juce::StringArray waves{"Sine", "Saw", "Square", "Triangle"};
    const juce::StringArray filterModes{"Low-pass", "High-pass"};

    layout.add(std::make_unique<ChoiceParam>("osc1Wave", "Osc 1 Wave", waves, 1));
    layout.add(std::make_unique<IntParam>("osc1Octave", "Osc 1 Octave", -2, 2, 0));
    layout.add(std::make_unique<FloatParam>("osc1Detune", "Osc 1 Detune",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<ChoiceParam>("osc2Wave", "Osc 2 Wave", waves, 2));
    layout.add(std::make_unique<IntParam>("osc2Octave", "Osc 2 Octave", -2, 2, 0));
    layout.add(std::make_unique<FloatParam>("osc2Detune", "Osc 2 Detune",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 7.0f));
    layout.add(std::make_unique<FloatParam>("oscMix", "Osc Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    layout.add(std::make_unique<FloatParam>("attack", "Attack",
        juce::NormalisableRange<float>(0.001f, 4.0f, 0.0f, 0.4f), 0.01f));
    layout.add(std::make_unique<FloatParam>("decay", "Decay",
        juce::NormalisableRange<float>(0.01f, 3.0f, 0.0f, 0.4f), 0.15f));
    layout.add(std::make_unique<FloatParam>("sustain", "Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    layout.add(std::make_unique<FloatParam>("release", "Release",
        juce::NormalisableRange<float>(0.01f, 5.0f, 0.0f, 0.4f), 0.3f));

    layout.add(std::make_unique<FloatParam>("cutoff", "Cutoff",
        juce::NormalisableRange<float>(40.0f, 16000.0f, 0.0f, 0.25f), 9000.0f));
    layout.add(std::make_unique<FloatParam>("resonance", "Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));
    layout.add(std::make_unique<ChoiceParam>("filterType", "Filter Mode", filterModes, 0));

    layout.add(std::make_unique<FloatParam>("gain", "Gain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.75f));
    return layout;
}

void VibeSynthProcessor::fillInPluginDescription(juce::PluginDescription& description) const {
    description = InternalPluginFormat::makeDescription();
}

void VibeSynthProcessor::prepareToPlay(double newSampleRate, int /*maximumExpectedSamplesPerBlock*/) {
    sampleRate = newSampleRate;
    synth.setCurrentPlaybackSampleRate(newSampleRate);
}

void VibeSynthProcessor::releaseResources() {
    sampleRate = 0.0;
}

void VibeSynthProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    // Replace, like a VST3 bridge: the incoming scratch buffer is not input audio.
    for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        audio.clear(ch, 0, audio.getNumSamples());
    synth.renderNextBlock(audio, midi, 0, audio.getNumSamples());
}

void VibeSynthProcessor::reset() {
    synth.allNotesOff(0, false);
}

juce::AudioProcessorEditor* VibeSynthProcessor::createEditor() {
    return new VibeSynthEditor(*this);
}

void VibeSynthProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, destData);
}

void VibeSynthProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.getType() == parameters.state.getType()) parameters.replaceState(state);
    }
}

} // namespace vibedaw
