#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace vibedaw {

class TransportListener {
public:
    virtual ~TransportListener() = default;
    virtual void transportPlayingChanged(bool isPlaying) {}
    virtual void transportRecordingChanged(bool isRecording) {}
    virtual void transportPositionChanged(double positionInSeconds) {}
    virtual void transportTempoChanged(double tempo) {}
    virtual void transportTimeSignatureChanged(int numerator, int denominator) {}
    virtual void transportLoopChanged(bool enabled, double start, double end) {}
    virtual void transportMetronomeChanged(bool enabled) {}
};

struct TimeSignature {
    int numerator = 4;
    int denominator = 4;
    
    TimeSignature() = default;
    TimeSignature(int num, int denom) : numerator(num), denominator(denom) {}
};

struct LoopRegion {
    bool enabled = false;
    double startBeats = 0.0;
    double endBeats = 4.0;
};

class TransportState {
public:
    TransportState();
    ~TransportState();
    
    void addListener(TransportListener* listener);
    void removeListener(TransportListener* listener);
    
    void setPlaying(bool playing);
    void setRecording(bool recording);
    void togglePlay();
    void toggleRecord();
    void stop();
    
    bool isPlaying() const { return playing_; }
    bool isRecording() const { return recording_; }
    
    void setPosition(double positionInSeconds);
    void setPositionInBeats(double beats);
    double getPosition() const { return positionInSeconds_; }
    double getPositionInBeats() const;
    
    void setTempo(double tempo);
    double getTempo() const { return tempo_; }
    
    void setTimeSignature(int numerator, int denominator);
    TimeSignature getTimeSignature() const { return timeSignature_; };
    
    void setLoopEnabled(bool enabled);
    void setLoopRegion(double startBeats, double endBeats);
    bool isLoopEnabled() const { return loop_.enabled; }
    LoopRegion getLoopRegion() const { return loop_; }
    
    void setMetronomeEnabled(bool enabled);
    bool isMetronomeEnabled() const { return metronomeEnabled_; }
    
    void tapTempo();
    
    void processBlock(int numSamples, double sampleRate);
    void reset();
    
private:
    juce::ListenerList<TransportListener> listeners_;
    
    bool playing_ = false;
    bool recording_ = false;
    double positionInSeconds_ = 0.0;
    double tempo_ = 120.0;
    TimeSignature timeSignature_;
    LoopRegion loop_;
    bool metronomeEnabled_ = false;
    
    double samplesSinceLastBeat_ = 0.0;
    double samplesPerBeat_ = 0.0;
    
    juce::int64 lastTapTime_ = 0;
    juce::Array<double> tapTempoHistory_;
    
    void updateSamplesPerBeat(double sampleRate);
    void checkLoop();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportState)
};

} // namespace vibedaw
