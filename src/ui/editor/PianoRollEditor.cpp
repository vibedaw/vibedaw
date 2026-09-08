#include "PianoRollEditor.h"
#include "project/Clip.h"
#include "core/MidiManager.h"

namespace vibedaw {

class PianoRollEditor::GridViewport : public juce::Viewport {
public:
    std::function<void()> onViewChanged;
    void visibleAreaChanged(const juce::Rectangle<int>&) override {
        if (onViewChanged) onViewChanged();
    }
};

PianoRollEditor::PianoRollEditor(MidiManager* midiManager)
    : midiManager_(midiManager)
{
    timeRuler_ = std::make_unique<TimeRulerComponent>();
    keyboard_ = std::make_unique<PianoRollKeyboard>();
    noteGrid_ = std::make_unique<NoteGridComponent>();
    viewport_ = std::make_unique<GridViewport>();
    viewport_->onViewChanged = [this] { syncScrollBetweenKeyboardAndGrid(); };
    keyboard_->setLowestNote(0);
    noteGrid_->setLowestNote(0);
    
    keyboard_->setListener(this);
    noteGrid_->setListener(this);
    
    viewport_->setViewedComponent(noteGrid_.get(), false);
    viewport_->setScrollBarsShown(true, true);
    
    addAndMakeVisible(timeRuler_.get());
    addAndMakeVisible(keyboard_.get());
    addAndMakeVisible(viewport_.get());
}

PianoRollEditor::~PianoRollEditor() {
    viewport_->onViewChanged = nullptr;
    viewport_->setViewedComponent(nullptr, false);
}

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
    viewport_->setViewPosition(viewport_->getViewPositionX(), offset);
}

int PianoRollEditor::getVerticalScroll() const {
    return viewport_->getViewPositionY();
}

void PianoRollEditor::setHorizontalScroll(double beats) {
    viewport_->setViewPosition(static_cast<int>(beats * pixelsPerBeat_), viewport_->getViewPositionY());
}

double PianoRollEditor::getHorizontalScroll() const {
    return static_cast<double>(viewport_->getViewPositionX()) / pixelsPerBeat_;
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
    noteGrid_->setKeyHeight(keyboard_->getKeyHeight());
    const bool firstLayout = noteGrid_->getHeight() == 0;
    noteGrid_->setSize(gridWidth, 128 * keyboard_->getKeyHeight());
    if (firstLayout) viewport_->setViewPosition(0, 48 * keyboard_->getKeyHeight());
    syncScrollBetweenKeyboardAndGrid();
}

void PianoRollEditor::syncScrollBetweenKeyboardAndGrid() {
    keyboard_->setScrollOffset(viewport_->getViewPositionY());
    timeRuler_->setTimeOffset(getHorizontalScroll());
}

} // namespace vibedaw
