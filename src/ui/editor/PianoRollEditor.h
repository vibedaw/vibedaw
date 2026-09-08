#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PianoRollKeyboard.h"
#include "NoteGridComponent.h"
#include "TimeRulerComponent.h"
#include "project/ClipInstance.h"

namespace vibedaw {

class MidiClip;
class MidiManager;

class PianoRollEditor : public juce::Component,
                       public PianoRollKeyboard::Listener,
                       public NoteGridComponent::Listener {
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
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void noteOn(int pitch) override;
    void noteOff(int pitch) override;
    
    void noteAdded(const Note& note) override;
    void noteRemoved(const Note& note) override;
    void noteChanged(const Note& note) override;
    
    static constexpr int keyboardWidth = 60;
    static constexpr int timeRulerHeight = 24;
    
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
    
    void updateLayout();
    void syncScrollBetweenKeyboardAndGrid();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollEditor)
};

} // namespace vibedaw
