#pragma once

#include "TransportState.h"
#include "ArrangementSnapshot.h"
#include "project/ClipInstance.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

namespace vibedaw {

class Project;
class ChannelMixer;

// One message-thread controller shared by song and clip-window entry points.
// Its render implementation is consumed only by ChannelMixer under AudioQuiescence.
class MidiRecorder : private juce::Timer {
public:
    enum class Mode { Continuous, Takes, Replace, Overdub };
    struct Options {
        ClipId clipId = InvalidClipId;
        ChannelId channelId = InvalidChannelId;
        Mode mode = Mode::Overdub;
        bool clipFocused = true;
        double startBeat = 0.0;
        double loopLength = 4.0;
        // Required together for an existing song source. startBeat is the cursor,
        // while the explicitly selected placement supplies the source origin.
        juce::String trackId, placementId;
    };

    explicit MidiRecorder(Project&);
    ~MidiRecorder() override;
    // Message-thread API. The UI must confirm edits to an existing shared source
    // before calling start; this service never opens a dialog. Content is bounded
    // to 65536 workspace entries (including the original) and 65536 loop passes.
    // Repeated live attacks close the preceding same-channel/pitch capture.
    // CC/bend input punches only that channel/lane from its first received event;
    // untouched lanes and original state beyond punch-out are retained.
    // While armed, notes/takes stay in the audio workspace: model preview and
    // publication are deferred until disarm/stop, never rebuilt by the 30 Hz timer.
    bool start(const Options&, bool record, juce::String& error);
    bool setRecording(bool record, juce::String& error);
    void stop();
    void endSession();
    bool undoLastRecording(juce::String& error);
    // Last committed arm-to-disarm transaction, retained after endSession. Undo
    // validates sources/placement against its own last commit, not playback imports.
    bool canUndoLastRecording() const;
    bool selectTake(unsigned index, juce::String& error);
    bool isSessionActive() const;
    bool isRecording() const;
    bool isClipFocused() const;
    ClipId getTargetClipId() const;
    ChannelId getChannelId() const;
    Mode getMode() const;
    unsigned getTakeCount() const;
    bool hasPendingContent() const;
    juce::String getStatus() const;
    TransportState& getPlaybackTransport();
    void poll();

private:
    friend class ChannelMixer;
    struct Impl;
    std::unique_ptr<Impl> impl;
    struct RenderEvent {
        int sample = 0, size = 3;
        unsigned token = 0;
        unsigned char data[3]{};
        bool physical = false;
    };
    // Accessed only inside the mixer callback, or under an off-audio Edit.
    bool renderActive() const noexcept;
    bool renderHasStarted() const noexcept;
    unsigned renderGeneration() const noexcept;
    ChannelId renderChannel() const noexcept;
    TransportState& renderTransport() noexcept;
    const ArrangementSnapshot& renderArrangement() const noexcept;
    void render(const TransportClock::Block&, const juce::MidiBuffer&, bool inputValid,
                bool syntheticCleanup = false, bool interrupted = false) noexcept;
    const RenderEvent* renderEvents() const noexcept;
    unsigned renderEventCount() const noexcept;
    bool renderOverflowed() const noexcept;
    void renderDeliveryFailed() noexcept;
    void renderInterrupted() noexcept;
    void primeLiveExpression(const std::array<int, 16 * 129>&) noexcept;
    void timerCallback() override { poll(); }
};

} // namespace vibedaw
