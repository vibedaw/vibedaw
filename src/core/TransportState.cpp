#include "TransportState.h"
#include "utils/Logger.h"
#include <cmath>
#include <juce_events/juce_events.h>

namespace vibedaw {

TransportState::TransportState() {
    LOG_INFO("TransportState: Created");
}

TransportState::~TransportState() {
    LOG_INFO("TransportState: Destroyed");
}

void TransportState::addListener(TransportListener* listener) {
    listeners_.add(listener);
}

void TransportState::removeListener(TransportListener* listener) {
    listeners_.remove(listener);
}

void TransportState::setPlaying(bool playing) {
    if (!playing) { ++stopGeneration; panicRequested.store(true); }
    if (playing_ != playing) {
        playing_ = playing;
        publishControl();
        listeners_.call([&](TransportListener& l) { l.transportPlayingChanged(playing_); });
        LOG_INFO("TransportState: Playing = " + juce::String(playing_ ? "true" : "false"));
    }
    publishControl();
}

void TransportState::setRecording(bool recording) {
    recording = false; // Recording is unavailable until the recording task.
    if (recording_ != recording) {
        recording_ = recording;
        publishControl();
        listeners_.call([&](TransportListener& l) { l.transportRecordingChanged(recording_); });
        LOG_INFO("TransportState: Recording = " + juce::String(recording_ ? "true" : "false"));
    }
}

void TransportState::togglePlay() {
    setPlaying(!playing_);
}

void TransportState::toggleRecord() {
    setRecording(!recording_);
}

void TransportState::stop() {
    setPlaying(false);
    setRecording(false);
}

void TransportState::setPosition(double positionInSeconds) {
    if (!std::isfinite(positionInSeconds) || positionInSeconds < 0.0) return;
    setPositionInBeats(positionInSeconds * (tempo_ / 60.0));
}

void TransportState::setPositionInBeats(double beats) {
    if (!std::isfinite(beats) || beats < 0.0 || beats > maxPositionBeats) return;
    positionInBeats_ = seekPositionBeats_ = beats;
    ++seekGeneration; // Even seeking to the displayed position is a discontinuity.
    publishControl();
    listeners_.call([&](TransportListener& l) { l.transportPositionChanged(getPosition()); });
}

double TransportState::getPositionInBeats() const {
    return positionInBeats_;
}

void TransportState::setTempo(double tempo) {
    if (!std::isfinite(tempo)) return;
    if (tempo < 20.0) tempo = 20.0;
    if (tempo > 300.0) tempo = 300.0;
    
    if (!juce::approximatelyEqual(tempo_, tempo)) {
        tempo_ = tempo;
        publishControl();
        listeners_.call([&](TransportListener& l) { l.transportTempoChanged(tempo_); });
        LOG_INFO("TransportState: Tempo = " + juce::String(tempo_, 1) + " BPM");
    }
}

void TransportState::setTimeSignature(int numerator, int denominator) {
    if (numerator < 1) numerator = 1;
    if (numerator > 32) numerator = 32;
    if (denominator != 2 && denominator != 4 && denominator != 8 && denominator != 16) {
        denominator = 4;
    }
    
    if (timeSignature_.numerator != numerator || timeSignature_.denominator != denominator) {
        timeSignature_ = TimeSignature(numerator, denominator);
        publishControl();
        listeners_.call([&](TransportListener& l) { 
            l.transportTimeSignatureChanged(timeSignature_.numerator, timeSignature_.denominator); 
        });
    }
}

void TransportState::setLoopEnabled(bool enabled) {
    if (loop_.enabled != enabled) {
        loop_.enabled = enabled;
        publishControl();
        listeners_.call([&](TransportListener& l) { 
            l.transportLoopChanged(loop_.enabled, loop_.startBeats, loop_.endBeats); 
        });
    }
}

void TransportState::setLoopRegion(double startBeats, double endBeats) {
    if (!std::isfinite(startBeats) || !std::isfinite(endBeats) || startBeats < 0.0 || endBeats <= startBeats) return;
    loop_.startBeats = startBeats;
    loop_.endBeats = endBeats;
    publishControl();
    listeners_.call([&](TransportListener& l) { 
        l.transportLoopChanged(loop_.enabled, loop_.startBeats, loop_.endBeats); 
    });
}

void TransportState::setMetronomeEnabled(bool enabled) {
    if (metronomeEnabled_ != enabled) {
        metronomeEnabled_ = enabled;
        publishControl();
        listeners_.call([&](TransportListener& l) { l.transportMetronomeChanged(metronomeEnabled_); });
    }
}

void TransportState::tapTempo() {
    auto now = juce::Time::currentTimeMillis();
    
    if (lastTapTime_ > 0 && (now - lastTapTime_) < 2000) {
        double intervalMs = static_cast<double>(now - lastTapTime_);
        double tappedTempo = 60000.0 / intervalMs;
        
        tapTempoHistory_.add(tappedTempo);
        
        if (tapTempoHistory_.size() > 8) {
            tapTempoHistory_.remove(0);
        }
        
        if (tapTempoHistory_.size() >= 2) {
            double sum = 0.0;
            for (auto t : tapTempoHistory_) {
                sum += t;
            }
            double avgTempo = sum / tapTempoHistory_.size();
            setTempo(avgTempo);
        }
    } else {
        tapTempoHistory_.clear();
    }
    
    lastTapTime_ = now;
}

void TransportState::reset() {
    setPositionInBeats(0.0);
}

void TransportState::publishControl() {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    control.publish({playing_, recording_, metronomeEnabled_, seekPositionBeats_, tempo_,
                     timeSignature_, loop_, stopGeneration, seekGeneration});
}

void TransportState::pollRenderPosition() {
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    const auto& value = acquireRenderPosition();
    if (value.seekGeneration != seekGeneration || value.beats == positionInBeats_) return;
    positionInBeats_ = value.beats;
    listeners_.call([&](TransportListener& l) { l.transportPositionChanged(getPosition()); });
}

juce::String TransportState::formatBarsBeatsTicks(double beats, TimeSignature meter) {
    const double units = beats * meter.denominator / 4.0;
    const auto whole = static_cast<juce::int64>(std::floor(units));
    return juce::String(whole / meter.numerator + 1) + ":" +
        juce::String(whole % meter.numerator + 1) + ":" +
        juce::String(static_cast<int>((units - whole) * 960.0)).paddedLeft('0', 3);
}

TransportClock::Block TransportClock::beginBlock(TransportState& transport, int samples, double rate, bool interrupted) noexcept {
    const auto& control = transport.acquireControl();
    const bool panic = transport.consumePanic();
    const bool seek = control.seekGeneration != lastSeek;
    const bool changed = interrupted || panic || seek || control.stopGeneration != lastStop || rate != lastRate;
    if (seek) { beats = control.positionBeats; correction = 0; }
    lastSeek = control.seekGeneration;
    lastStop = control.stopGeneration;
    lastRate = rate;
    if (changed) ++revision;
    Block block{beats, beats, rate, control.tempo, control.meter, samples,
                control.playing, changed, revision, lastSeek};
    if (control.playing && samples > 0 && std::isfinite(rate) && rate > 0) {
        // Compensated summation retains fractional beats across arbitrary block sizes.
        const double delta = samples * (control.tempo / 60.0) / rate - correction;
        block.endBeats = juce::jmin(TransportState::maxPositionBeats, beats + delta);
        correction = (block.endBeats - beats) - delta;
        if (block.endBeats == TransportState::maxPositionBeats) correction = 0;
    }
    return block;
}

void TransportClock::endBlock(TransportState& transport, const Block& block) noexcept {
    beats = block.endBeats;
    transport.publishRenderPosition({beats, block.tempo, block.sampleRate, block.meter,
                                     block.playing, block.revision, block.seekGeneration});
}

} // namespace vibedaw
