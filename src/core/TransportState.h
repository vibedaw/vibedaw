#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioBoundary.h"

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
    // Half-open [startBeats, endBeats), in quarter-note beats regardless of meter.
    // exists distinguishes "no loop" (cleared/never created: nothing on the
    // ruler, no grabbing) from set-but-disabled (dim band, grabbable).
    bool enabled = false;
    bool exists = false;
    double startBeats = 0.0;
    double endBeats = 4.0;
};

class TransportState {
public:
    struct RenderControl {
        bool playing = false, recording = false, metronome = false;
        double positionBeats = 0, tempo = 120;
        TimeSignature meter;
        LoopRegion loop;
        unsigned stopGeneration = 0, seekGeneration = 0;
    };
    // Only audio calls acquireControl. UI APIs/listeners below remain message-only.
    const RenderControl& acquireControl() noexcept { return control.acquire(); }
    bool consumePanic() noexcept { return panicRequested.exchange(false); }
    struct RenderPosition {
        double beats = 0, tempo = 120, sampleRate = 0;
        TimeSignature meter;
        bool playing = false;
        unsigned revision = 0, seekGeneration = 0;
    };
    // Audio publishes; only the message thread polls/notifies listeners.
    void publishRenderPosition(RenderPosition value) noexcept { positionFeedback.publish(value); }
    const RenderPosition& acquireRenderPosition() noexcept { return positionFeedback.acquire(); }
    void pollRenderPosition();
    static constexpr double maxPositionBeats = 1.0e9;
    static constexpr double minLoopBeats = 1.0 / 64.0;
    static bool validLoopRegion(double start, double end) noexcept;
    static juce::String formatBarsBeatsTicks(double beats, TimeSignature meter);
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
    double getPosition() const { return positionInBeats_ * 60.0 / tempo_; }
    double getPositionInBeats() const;
    
    void setTempo(double tempo);
    double getTempo() const { return tempo_; }
    
    void setTimeSignature(int numerator, int denominator);
    TimeSignature getTimeSignature() const { return timeSignature_; };
    
    void setLoopEnabled(bool enabled);
    void setLoopRegion(double startBeats, double endBeats);
    // Removes the loop entirely: nothing on the ruler, nothing grabbable.
    void clearLoop();
    bool isLoopEnabled() const { return loop_.enabled; }
    bool isLoopRegionSet() const { return loop_.exists; }
    LoopRegion getLoopRegion() const { return loop_; }
    
    void setMetronomeEnabled(bool enabled);
    bool isMetronomeEnabled() const { return metronomeEnabled_; }
    
    void tapTempo();
    
    void reset();
    
private:
    void publishControl();
    LatestState<RenderControl> control;
    LatestState<RenderPosition> positionFeedback;
    std::atomic<bool> panicRequested{false};
    unsigned stopGeneration = 0, seekGeneration = 0;
    juce::ListenerList<TransportListener> listeners_;
    
    bool playing_ = false;
    bool recording_ = false;
    double positionInBeats_ = 0.0;
    double seekPositionBeats_ = 0.0;
    double tempo_ = 120.0;
    TimeSignature timeSignature_;
    LoopRegion loop_;
    bool metronomeEnabled_ = false;
    
    
    juce::int64 lastTapTime_ = 0;
    juce::Array<double> tapTempoHistory_;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportState)
};

// Owned by the engine's mixer, accessed only by the admitted render thread.
// beginBlock -> cleanup/schedule/process using this immutable value -> endBlock.
class TransportClock {
public:
    struct Block {
        double startBeats = 0, endBeats = 0, sampleRate = 0, tempo = 120;
        TimeSignature meter;
        int numSamples = 0;
        bool playing = false, discontinuity = false;
        unsigned revision = 0, seekGeneration = 0;
        struct Span {
            double start = 0, end = 0, sampleOffset = 0;
            bool wrap = false;
        };
        static constexpr unsigned maxSpans = 128;
        std::array<Span, maxSpans> spans{};
        unsigned spanCount = 0;
        bool spanOverflow = false, metronome = false;
    };
    Block beginBlock(TransportState& transport, int samples, double rate, bool interrupted = false) noexcept;
    void endBlock(TransportState& transport, const Block& block) noexcept;
private:
    double beats = 0, correction = 0, lastRate = 0;
    unsigned lastSeek = 0, lastStop = 0, revision = 0;
    LoopRegion lastLoop;
    bool pendingWrap = false;
};

} // namespace vibedaw
