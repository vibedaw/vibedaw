#pragma once

#include "AudioBoundary.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>

namespace vibedaw {

using MixerChannelId = int;
constexpr MixerChannelId MasterDestination = -1;

// Sample-peak envelope, instantaneous attack and -40 dB / 0.5 s release.
// The recurrence is sample-based, so splitting a buffer cannot change the result.
class StereoMeter {
public:
    void prepare(double rate) { decay = static_cast<float>(std::pow(0.01, 1.0 / (rate * 0.5))); clear(); }
    void clear() { left.store(0); right.store(0); ++revision; }
    void update(const juce::AudioBuffer<float>& buffer) {
        float levels[]{left.load(), right.load()};
        for (int ch = 0; ch < 2; ++ch) {
            const auto* samples = buffer.getNumChannels() ? buffer.getReadPointer(juce::jmin(ch, buffer.getNumChannels() - 1)) : nullptr;
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                const float sample = samples && std::isfinite(samples[i]) ? std::abs(samples[i]) : 0.0f;
                levels[ch] = juce::jmax(sample, levels[ch] * decay);
                if (levels[ch] < 1.0e-6f) levels[ch] = 0;
            }
        }
        left.store(levels[0]); right.store(levels[1]);
        ++revision;
    }
    float getLeft() const { return left.load(); }
    float getRight() const { return right.load(); }
    unsigned getRevision() const { return revision.load(); }
private:
    std::atomic<float> left{0}, right{0};
    std::atomic<unsigned> revision{0};
    float decay = 0;
};

// Independent destination, always feeding Master. Model access is message-thread
// only; acquireControls and meter updates belong to the sole render consumer.
class MixerChannel : public juce::ChangeBroadcaster {
public:
    MixerChannel(MixerChannelId id, const juce::String& name) : id_(id), name_(name) {}
    MixerChannelId getId() const { return id_; }
    const juce::String& getName() const { return name_; }
    void setName(const juce::String& value) {
        if (name_ == value) return;
        name_ = value;
        sendChangeMessage();
    }

    struct Controls { float volume = 1, pan = 0; bool muted = false, solo = false; };
    float getVolume() const { return state_.volume; }
    void setVolume(float value) {
        if (!std::isfinite(value)) return;
        value = juce::jlimit(0.0f, 2.0f, value);
        if (state_.volume == value) return;
        state_.volume = value;
        publishControls();
    }
    float getPan() const { return state_.pan; }
    void setPan(float value) {
        if (!std::isfinite(value)) return;
        value = juce::jlimit(-1.0f, 1.0f, value);
        if (state_.pan == value) return;
        state_.pan = value;
        publishControls();
    }
    bool isMuted() const { return state_.muted; }
    void setMuted(bool value) {
        if (state_.muted == value) return;
        state_.muted = value;
        publishControls();
    }
    bool isSolo() const { return state_.solo; }
    void setSolo(bool value) {
        if (state_.solo == value) return;
        state_.solo = value;
        publishControls();
    }
    juce::Colour getColour() const { return colour_; }
    void setColour(const juce::Colour& value) {
        if (colour_ == value) return;
        colour_ = value;
        sendChangeMessage();
    }
    Controls acquireControls() { return controls_.acquire(); } // Render only.
    StereoMeter meter;
    const StereoMeter& getMeter() const { return meter; }

private:
    void publishControls() { controls_.publish(state_); sendChangeMessage(); }
    const MixerChannelId id_;
    juce::String name_;
    juce::Colour colour_{0xff6a6aff};
    Controls state_;
    LatestState<Controls> controls_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerChannel)
};

// Master belongs to the instrument graph, not an arrangement Track or a fake channel.
// Setters/getters: message thread. process: sole admitted render consumer.
class MasterBus {
public:
    struct Controls { float gain = 1; bool muted = false; };
    void setGain(float value) {
        if (!std::isfinite(value)) return;
        state.gain = juce::jlimit(0.0f, 2.0f, value); controls.publish(state);
    }
    void setMuted(bool value) { state.muted = value; controls.publish(state); }
    float getGain() const { return state.gain; } // Message thread only.
    bool isMuted() const { return state.muted; }
    void process(juce::AudioBuffer<float>& buffer) {
        const auto value = controls.acquire();
        if (value.muted || value.gain == 0) buffer.clear();
        else buffer.applyGain(value.gain);
        meter.update(buffer);
    }
    StereoMeter meter;
private:
    Controls state;
    LatestState<Controls> controls;
};

} // namespace vibedaw
