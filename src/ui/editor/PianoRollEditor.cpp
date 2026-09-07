#include "PianoRollEditor.h"
#include "project/Clip.h"
#include "core/MidiManager.h"

namespace vibedaw {

PianoRollEditor::PianoRollEditor(MidiManager* midiManager)
    : midiManager_(midiManager)
{
    timeRuler_ = std::make_unique<TimeRulerComponent>();
    keyboard_ = std::make_unique<PianoRollKeyboard>();
    noteGrid_ = std::make_unique<NoteGridComponent>();
    viewport_ = std::make_unique<juce::Viewport>();
    
    keyboard_->setListener(this);
    noteGrid_->setListener(this);
    
    viewport_->setViewedComponent(noteGrid_.get(), false);
    viewport_->setScrollBarsShown(true, true);
    
    addAndMakeVisible(timeRuler_.get());
    addAndMakeVisible(keyboard_.get());
    addAndMakeVisible(viewport_.get());
}

PianoRollEditor::~PianoRollEditor() = default;

void PianoRollEditor::setMidiClip(MidiClip* clip, ClipId clipId) {
    midiClip_ = clip;
    clipId_ = clipId;
    noteGrid_->setMidiClip(clip);
    repaint();
}

void PianoRollEditor::setGridResolution(GridResolution resolution) {
    noteGrid_->setGridResolution(resolution);
    repaint();
}

GridResolution PianoRollEditor::getGridResolution() const {
    return noteGrid_->getGridResolution();
}

void PianoRollEditor::setZoomLevel(double zoom) {
    zoomLevel_ = juce::jmax(0.25, juce::jmin(4.0, zoom));
    pixelsPerBeat_ = static_cast<int>(80 * zoomLevel_);
    timeRuler_->setPixelsPerBeat(pixelsPerBeat_);
    noteGrid_->setPixelsPerBeat(pixelsPerBeat_);
    updateLayout();
}

void PianoRollEditor::setVerticalScroll(int offset) {
    keyboard_->setLowestNote(offset / 12);
    noteGrid_->setScrollOffset(offset);
}

int PianoRollEditor::getVerticalScroll() const {
    return noteGrid_->getScrollOffset();
}

void PianoRollEditor::setHorizontalScroll(double beats) {
    noteGrid_->setTimeOffset(beats);
    timeRuler_->setTimeOffset(beats);
}

double PianoRollEditor::getHorizontalScroll() const {
    return noteGrid_->getTimeOffset();
}

void PianoRollEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void PianoRollEditor::resized() {
    updateLayout();
}

void PianoRollEditor::noteOn(int pitch) {
    keyboard_->setHeldNote(pitch, true);
    
    if (midiManager_) {
        midiManager_->sendMidiMessage(juce::MidiMessage::noteOn(1, pitch, 0.8f));
    }
}

void PianoRollEditor::noteOff(int pitch) {
    keyboard_->setHeldNote(pitch, false);
    
    if (midiManager_) {
        midiManager_->sendMidiMessage(juce::MidiMessage::noteOff(1, pitch));
    }
}

void PianoRollEditor::noteAdded(const Note& note) {
    juce::ignoreUnused(note);
    if (listener_) {
        listener_->clipModified(clipId_);
    }
}

void PianoRollEditor::noteRemoved(const Note& note) {
    juce::ignoreUnused(note);
    if (listener_) {
        listener_->clipModified(clipId_);
    }
}

void PianoRollEditor::noteChanged(const Note& note) {
    juce::ignoreUnused(note);
    if (listener_) {
        listener_->clipModified(clipId_);
    }
}

void PianoRollEditor::updateLayout() {
    auto bounds = getLocalBounds();
    
    int timeRulerHeight = timeRuler_->defaultHeight;
    int kbWidth = keyboardWidth;
    
    timeRuler_->setBounds(kbWidth, 0, bounds.getWidth() - kbWidth, timeRulerHeight);
    keyboard_->setBounds(0, timeRulerHeight, kbWidth, bounds.getHeight() - timeRulerHeight);
    viewport_->setBounds(kbWidth, timeRulerHeight, 
                          bounds.getWidth() - kbWidth, bounds.getHeight() - timeRulerHeight);
    
    int gridWidth = static_cast<int>(8.0 * pixelsPerBeat_);
    noteGrid_->setSize(gridWidth, bounds.getHeight() * 4);
    noteGrid_->setKeyHeight(keyboard_->getKeyHeight());
}

void PianoRollEditor::syncScrollBetweenKeyboardAndGrid() {
}

} // namespace vibedaw