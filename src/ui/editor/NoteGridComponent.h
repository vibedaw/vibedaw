#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "project/Clip.h"
#include "PianoRollGeometry.h"
#include <vector>
#include <functional>

namespace vibedaw {

class MidiClip;
class Note;

enum class GridResolution {
    Whole = 1,
    Half = 2,
    Quarter = 4,
    Eighth = 8,
    Sixteenth = 16,
    ThirtySecond = 32,
    QuarterTriplet = 6,
    EighthTriplet = 12,
    SixteenthTriplet = 24
};

// Editable note grid rendered at grid-local coordinates. It is the viewed
// component of the piano-roll viewport, so it never applies scroll offsets
// itself: the viewport translates it and the keyboard/ruler consume the
// viewport's offsets. All pitch/time transforms come from PianoRollGeometry.
class NoteGridComponent : public juce::Component,
                          private Clip::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void noteAdded(const Note& note) = 0;
        virtual void noteRemoved(const Note& note) = 0;
        virtual void noteChanged(const Note& note) = 0;
        // Content bounds may have changed; the owner can grow the extent.
        virtual void gridContentChanged() {}
    };

    NoteGridComponent();
    ~NoteGridComponent() override;

    void setListener(Listener* listener) { listener_ = listener; }

    void setMidiClip(MidiClip* clip);
    MidiClip* getMidiClip() const { return midiClip_; }

    void setGridResolution(GridResolution resolution);
    GridResolution getGridResolution() const { return gridResolution_; }

    void setPixelsPerBeat(int pixels);
    int getPixelsPerBeat() const { return static_cast<int>(geometry_.pixelsPerBeat); }

    void setKeyHeight(int height);
    int getKeyHeight() const { return geometry_.keyHeight; }

    void setLowestNote(int lowest);
    int getLowestNote() const { return geometry_.lowestNote; }

    const PianoRollGeometry& getGeometry() const { return geometry_; }

    // Vertical playhead line in source-local quarter-note beats; negative hides.
    void setPlayheadBeats(double beats) { playheadBeats_ = beats; repaint(); }
    double getPlayheadBeats() const { return playheadBeats_; }

    void selectNote(const Note* note);
    void clearSelection();
    std::vector<const Note*> getSelectedNotes();

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

    static constexpr int defaultPixelsPerBeat = 80;
    static constexpr int defaultKeyHeight = 12;

private:
    void clipChanged() override;
    void notesInvalidated() override;
    MidiClip* midiClip_ = nullptr;
    Listener* listener_ = nullptr;

    GridResolution gridResolution_ = GridResolution::Sixteenth;
    PianoRollGeometry geometry_;
    double playheadBeats_ = -1.0;

    struct DragState {
        enum class Mode { None, Create, Move, ResizeStart, ResizeEnd };
        Mode mode = Mode::None;
        const Note* note = nullptr;
        int startX = 0;
        int startY = 0;
        double originalStart = 0.0;
        double originalDuration = 1.0;
        int originalPitch = 60;
        bool isExisting = false;
    } dragState_;

    std::vector<const Note*> selectedNotes_;
    const Note* hoveredNote_ = nullptr;

    double snapToGrid(double time) const;
    int pitchFromY(int y) const;
    int yFromPitch(int pitch) const;
    double timeFromX(int x) const;
    int xFromTime(double time) const;

    const Note* findNoteAt(int x, int y);
    void drawNote(juce::Graphics& g, const Note& note, bool isSelected, bool isHovered);
    void drawGridLines(juce::Graphics& g);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteGridComponent)
};

} // namespace vibedaw
