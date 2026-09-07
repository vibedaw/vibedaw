#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "project/Note.h"
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

class NoteGridComponent : public juce::Component,
                          public juce::ScrollBar::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void noteAdded(const Note& note) = 0;
        virtual void noteRemoved(const Note& note) = 0;
        virtual void noteChanged(const Note& note) = 0;
    };
    
    NoteGridComponent();
    ~NoteGridComponent() override;
    
    void setListener(Listener* listener) { listener_ = listener; }
    
    void setMidiClip(MidiClip* clip);
    MidiClip* getMidiClip() const { return midiClip_; }
    
    void setGridResolution(GridResolution resolution);
    GridResolution getGridResolution() const { return gridResolution_; }
    
    void setPixelsPerBeat(int pixels);
    int getPixelsPerBeat() const { return pixelsPerBeat_; }
    
    void setKeyHeight(int height);
    int getKeyHeight() const { return keyHeight_; }
    
    void setLowestNote(int lowest);
    int getLowestNote() const { return lowestNote_; }
    
    void setVisibleBeats(double beats);
    double getTimeOffset() const { return timeOffset_; }
    void setTimeOffset(double beats);
    
    void setScrollOffset(int offsetY);
    int getScrollOffset() const { return scrollOffsetY_; }
    
    void selectNote(Note* note);
    void clearSelection();
    std::vector<Note*> getSelectedNotes();
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;
    
    static constexpr int defaultPixelsPerBeat = 80;
    static constexpr int defaultKeyHeight = 12;
    
private:
    MidiClip* midiClip_ = nullptr;
    Listener* listener_ = nullptr;
    
    GridResolution gridResolution_ = GridResolution::Sixteenth;
    int pixelsPerBeat_ = defaultPixelsPerBeat;
    int keyHeight_ = defaultKeyHeight;
    int lowestNote_ = 36;
    
    int scrollOffsetY_ = 0;
    double timeOffset_ = 0.0;
    
    struct DragState {
        enum class Mode { None, Create, Move, ResizeStart, ResizeEnd };
        Mode mode = Mode::None;
        Note* note = nullptr;
        int startX = 0;
        int startY = 0;
        double originalStart = 0.0;
        double originalDuration = 1.0;
        int originalPitch = 60;
        bool isExisting = false;
    } dragState_;
    
    std::vector<Note*> selectedNotes_;
    Note* hoveredNote_ = nullptr;
    
    double snapToGrid(double time) const;
    int pitchFromY(int y) const;
    int yFromPitch(int pitch) const;
    double timeFromX(int x) const;
    int xFromTime(double time) const;
    
    Note* findNoteAt(int x, int y);
    void drawNote(juce::Graphics& g, const Note& note, bool isSelected, bool isHovered);
    void drawGridLines(juce::Graphics& g);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteGridComponent)
};

} // namespace vibedaw