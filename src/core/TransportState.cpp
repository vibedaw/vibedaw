#include "TransportState.h"
#include "utils/Logger.h"

namespace vibedaw {

TransportState::TransportState() {
    updateSamplesPerBeat(44100.0);
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
    if (playing_ != playing) {
        playing_ = playing;
        listeners_.call([&](TransportListener& l) { l.transportPlayingChanged(playing_); });
        LOG_INFO("TransportState: Playing = " + juce::String(playing_ ? "true" : "false"));
    }
}

void TransportState::setRecording(bool recording) {
    if (recording_ != recording) {
        recording_ = recording;
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
    if (positionInSeconds_ != positionInSeconds) {
        positionInSeconds_ = positionInSeconds;
        listeners_.call([&](TransportListener& l) { l.transportPositionChanged(positionInSeconds_); });
    }
}

void TransportState::setPositionInBeats(double beats) {
    double secondsPerBeat = 60.0 / tempo_;
    setPosition(beats * secondsPerBeat);
}

double TransportState::getPositionInBeats() const {
    double secondsPerBeat = 60.0 / tempo_;
    return positionInSeconds_ / secondsPerBeat;
}

void TransportState::setTempo(double tempo) {
    if (tempo < 20.0) tempo = 20.0;
    if (tempo > 300.0) tempo = 300.0;
    
    if (!juce::approximatelyEqual(tempo_, tempo)) {
        tempo_ = tempo;
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
        listeners_.call([&](TransportListener& l) { 
            l.transportTimeSignatureChanged(timeSignature_.numerator, timeSignature_.denominator); 
        });
    }
}

void TransportState::setLoopEnabled(bool enabled) {
    if (loop_.enabled != enabled) {
        loop_.enabled = enabled;
        listeners_.call([&](TransportListener& l) { 
            l.transportLoopChanged(loop_.enabled, loop_.startBeats, loop_.endBeats); 
        });
    }
}

void TransportState::setLoopRegion(double startBeats, double endBeats) {
    loop_.startBeats = startBeats;
    loop_.endBeats = endBeats;
    listeners_.call([&](TransportListener& l) { 
        l.transportLoopChanged(loop_.enabled, loop_.startBeats, loop_.endBeats); 
    });
}

void TransportState::setMetronomeEnabled(bool enabled) {
    if (metronomeEnabled_ != enabled) {
        metronomeEnabled_ = enabled;
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

void TransportState::processBlock(int numSamples, double sampleRate) {
    if (!playing_) return;
    
    updateSamplesPerBeat(sampleRate);
    
    samplesSinceLastBeat_ += numSamples;
    
    if (samplesSinceLastBeat_ >= samplesPerBeat_) {
        samplesSinceLastBeat_ -= samplesPerBeat_;
    }
    
    positionInSeconds_ += static_cast<double>(numSamples) / sampleRate;
    
    checkLoop();
}

void TransportState::reset() {
    positionInSeconds_ = 0.0;
    samplesSinceLastBeat_ = 0.0;
    listeners_.call([&](TransportListener& l) { l.transportPositionChanged(0.0); });
}

void TransportState::updateSamplesPerBeat(double sampleRate) {
    samplesPerBeat_ = (60.0 / tempo_) * sampleRate;
}

void TransportState::checkLoop() {
    if (!loop_.enabled) return;
    
    double currentBeat = getPositionInBeats();
    if (currentBeat >= loop_.endBeats) {
        setPositionInBeats(loop_.startBeats);
    }
}

} // namespace vibedaw
