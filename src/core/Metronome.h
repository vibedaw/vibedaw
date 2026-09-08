#pragma once

#include "TransportState.h"
#include <cmath>
#include <limits>

namespace vibedaw {

// One bounded oscillator, retriggered at musical beats. No plugin or device dependency.
class Metronome {
public:
    bool render(juce::AudioBuffer<float>& audio, const TransportClock::Block& block) noexcept {
        const bool enabled = block.playing && block.metronome && !block.spanOverflow;
        if (!enabled) { running = false; remaining = 0; return true; }
        const bool restart = !running || block.discontinuity || denominator != block.meter.denominator ||
                             numerator != block.meter.numerator;
        if (restart) { remaining = 0; nextTick = static_cast<int64_t>(std::ceil(block.startBeats * block.meter.denominator / 4.0)); }
        running = true;
        denominator = block.meter.denominator;
        numerator = block.meter.numerator;
        if (rate != block.sampleRate) {
            rate = block.sampleRate;
            decay = std::exp(-7.0 / (rate * 0.02));
        }
        const double spacing = 4.0 / denominator;
        const double samplesPerBeat = rate * 60.0 / block.tempo;
        const double epsilon = 1.0e-7 + 8 * std::numeric_limits<double>::epsilon() *
                               std::abs(block.startBeats) * samplesPerBeat;
        unsigned count = 0;
        for (unsigned s = 0; s < block.spanCount; ++s) {
            const auto& span = block.spans[s];
            if (span.wrap) nextTick = static_cast<int64_t>(std::ceil(span.start / spacing));
            while (nextTick * spacing < span.end) {
                const double offset = std::max(0.0, std::floor(span.sampleOffset +
                    (nextTick * spacing - span.start) * samplesPerBeat + epsilon));
                if (offset >= block.numSamples) break;
                if (count == clicks.size()) { remaining = 0; running = false; return false; }
                clicks[count++] = {static_cast<int>(offset), nextTick % numerator == 0};
                ++nextTick;
            }
        }
        unsigned next = 0;
        for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
            while (next < count && clicks[next].sample == sample) {
                const bool accent = clicks[next++].accent;
                phase = 0;
                increment = juce::MathConstants<double>::twoPi * (accent ? 1760.0 : 1320.0) / rate;
                amplitude = accent ? 0.25 : 0.15;
                remaining = static_cast<int>(std::min(std::ceil(rate * 0.02),
                    static_cast<double>(std::numeric_limits<int>::max())));
            }
            if (remaining == 0) continue;
            const float value = static_cast<float>(std::cos(phase) * amplitude);
            for (int ch = 0; ch < audio.getNumChannels(); ++ch) audio.addSample(ch, sample, value);
            phase = std::fmod(phase + increment, juce::MathConstants<double>::twoPi);
            amplitude *= decay;
            --remaining;
        }
        return true;
    }
private:
    struct Click { int sample; bool accent; };
    std::array<Click, 1024> clicks{};
    int64_t nextTick = 0;
    int numerator = 4, denominator = 4, remaining = 0;
    double rate = 0, phase = 0, increment = 0, amplitude = 0, decay = 0;
    bool running = false;
};

} // namespace vibedaw
