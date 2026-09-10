#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PianoRollKeyboard.h"
#include "NoteGridComponent.h"
#include "TimeRulerComponent.h"
#include "project/ClipInstance.h"
#include "core/TransportState.h"
#include <functional>

namespace vibedaw {

class MidiClip;
class MidiManager;

// Piano roll: an unscrolled keyboard and ruler beside a viewport-owned note
// grid. The viewport is the single scroll source; the grid stays grid-local
// and the keyboard/ruler consume the viewport's pixel offsets through the
// shared PianoRollGeometry. When a transport and local-beat provider are set,
// a playhead is drawn and optional following can scroll the viewport to keep
// it visible (read-only: it never changes the transport position).
class PianoRollEditor : public juce::Component,
                        public PianoRollKeyboard::Listener,
                        public NoteGridComponent::Listener,
                        public TransportListener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void clipModified(ClipId clipId) = 0;
    };

    PianoRollEditor(MidiManager* midiManager = nullptr);
    ~PianoRollEditor() override;

    void setListener(Listener* listener) { listener_ = listener; }

    void setMidiClip(MidiClip* clip, ClipId clipId);
    MidiClip* getMidiClip() const { return midiClip_; }
    ClipId getClipId() const { return clipId_; }

    void setGridResolution(GridResolution resolution);
    GridResolution getGridResolution() const;

    void setZoomLevel(double zoom);
    double getZoomLevel() const { return zoomLevel_; }

    void setVerticalScroll(int offset);
    int getVerticalScroll() const;

    void setHorizontalScroll(double beats);
    double getHorizontalScroll() const;

    // Transport + arrangement->source-local beat provider for the playhead.
    // The provider returns false when no placement covers the arrangement beat.
    void setTransport(TransportState* transport);
    void setLocalBeatProvider(std::function<bool(double, double&)> provider);

    void setFollowEnabled(bool enabled);
    bool isFollowEnabled() const { return followEnabled_; }
    bool isFollowSuspended() const { return followSuspended_; }

    double getPlayheadBeat() const { return playheadBeat_; }
    bool isPlayheadVisible() const { return playheadVisible_; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void noteOn(int pitch) override;
    void noteOff(int pitch) override;

    void noteAdded(const Note& note) override;
    void noteRemoved(const Note& note) override;
    void noteChanged(const Note& note) override;
    void gridContentChanged() override;

    void transportPlayingChanged(bool isPlaying) override;
    void transportPositionChanged(double positionInSeconds) override;

    static constexpr int keyboardWidth = 60;
    static constexpr int timeRulerHeight = 24;
    static constexpr double minBeats = 8.0;
    static constexpr double contentMarginBeats = 4.0;

private:
    MidiManager* midiManager_ = nullptr;
    Listener* listener_ = nullptr;

    MidiClip* midiClip_ = nullptr;
    ClipId clipId_;

    std::unique_ptr<TimeRulerComponent> timeRuler_;
    std::unique_ptr<PianoRollKeyboard> keyboard_;
    std::unique_ptr<NoteGridComponent> noteGrid_;
    class GridViewport;
    std::unique_ptr<GridViewport> viewport_;

    double zoomLevel_ = 1.0;
    int pixelsPerBeat_ = 80;

    TransportState* transport_ = nullptr;
    std::function<bool(double, double&)> localBeatProvider_;
    bool followEnabled_ = false;
    bool followSuspended_ = false;
    bool applyingFollowScroll_ = false;
    bool hasLaidOut_ = false;
    int programmaticViewChanges_ = 0;
    double playheadBeat_ = -1.0;
    bool playheadVisible_ = false;

    void updateLayout();
    void updateContentExtent();
    void centerOnNotes();
    void updatePlayhead();
    void applyFollowScroll();
    void syncScrollBetweenKeyboardAndGrid();
    void onViewChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollEditor)
};

} // namespace vibedaw
