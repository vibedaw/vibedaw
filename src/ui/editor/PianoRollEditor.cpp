#include "PianoRollEditor.h"
#include "PianoRollGeometry.h"
#include "ui/Theme.h"
#include "project/Clip.h"
#include "core/MidiManager.h"
#include <cmath>

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
    viewport_->onViewChanged = [this] { onViewChanged(); };
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
    if (transport_) transport_->removeListener(this);
    viewport_->onViewChanged = nullptr;
    viewport_->setViewedComponent(nullptr, false);
}

void PianoRollEditor::setMidiClip(MidiClip* clip, ClipId clipId) {
    midiClip_ = clip;
    clipId_ = clipId;
    noteGrid_->setMidiClip(clip);
    updateContentExtent();
    centerOnNotes();
    updatePlayhead();
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
    zoom = juce::jlimit(0.25, 4.0, zoom);
    if (zoom == zoomLevel_) return;
    const double anchorBeat = getHorizontalScroll();
    zoomLevel_ = zoom;
    pixelsPerBeat_ = static_cast<int>(80 * zoomLevel_);
    timeRuler_->setPixelsPerBeat(pixelsPerBeat_);
    noteGrid_->setPixelsPerBeat(pixelsPerBeat_);
    {
        ++programmaticViewChanges_;
        updateLayout();
        viewport_->setViewPosition(static_cast<int>(std::lround(anchorBeat * pixelsPerBeat_)),
                                   viewport_->getViewPositionY());
        --programmaticViewChanges_;
    }
    // Zoom is a deliberate navigation gesture: suspend follow like a manual scroll.
    followSuspended_ = true;
    repaint();
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

void PianoRollEditor::setTransport(TransportState* transport) {
    if (transport_ == transport) return;
    if (transport_) transport_->removeListener(this);
    transport_ = transport;
    if (transport_) transport_->addListener(this);
    updatePlayhead();
    repaint();
}

void PianoRollEditor::setLocalBeatProvider(std::function<bool(double, double&)> provider) {
    localBeatProvider_ = std::move(provider);
    updatePlayhead();
    repaint();
}

void PianoRollEditor::setFollowEnabled(bool enabled) {
    followEnabled_ = enabled;
    if (!enabled) return;
    followSuspended_ = false;
    if (transport_ && transport_->isPlaying()) {
        updatePlayhead();
        applyFollowScroll();
    }
}

void PianoRollEditor::paint(juce::Graphics& g) {
    g.fillAll(theme::windowBackground);
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

void PianoRollEditor::gridContentChanged() {
    updateContentExtent();
}

void PianoRollEditor::transportPlayingChanged(bool isPlaying) {
    // Starting playback clears a manual-edit suspension so following resumes.
    if (isPlaying && followEnabled_) followSuspended_ = false;
    repaint();
}

void PianoRollEditor::transportPositionChanged(double positionInSeconds) {
    juce::ignoreUnused(positionInSeconds);
    updatePlayhead();
    if (followEnabled_ && !followSuspended_ && transport_ && transport_->isPlaying())
        applyFollowScroll();
    repaint();
}

void PianoRollEditor::updateLayout() {
    ++programmaticViewChanges_;
    auto bounds = getLocalBounds();
    
    int timeRulerHeight = timeRuler_->defaultHeight;
    int kbWidth = keyboardWidth;
    
    timeRuler_->setBounds(kbWidth, 0, bounds.getWidth() - kbWidth, timeRulerHeight);
    keyboard_->setBounds(0, timeRulerHeight, kbWidth, bounds.getHeight() - timeRulerHeight);
    viewport_->setBounds(kbWidth, timeRulerHeight, 
                          bounds.getWidth() - kbWidth, bounds.getHeight() - timeRulerHeight);
    
    noteGrid_->setKeyHeight(keyboard_->getKeyHeight());
    updateContentExtent();
    const bool firstLayout = !hasLaidOut_;
    hasLaidOut_ = true;
    if (firstLayout) {
        centerOnNotes();
    }
    syncScrollBetweenKeyboardAndGrid();
    --programmaticViewChanges_;
}

void PianoRollEditor::updateContentExtent() {
    double lastNoteEnd = 0.0;
    if (midiClip_) {
        for (const auto& note : midiClip_->getNotes())
            lastNoteEnd = juce::jmax(lastNoteEnd, note.getEndTime());
    }
    const double clipLength = midiClip_ ? midiClip_->getDuration() : minBeats;
    const double beats = pianoRollContentBeats(clipLength, lastNoteEnd, minBeats, contentMarginBeats);
    const int width = juce::jmax(1, static_cast<int>(std::lround(beats * pixelsPerBeat_)));
    noteGrid_->setSize(width, noteGrid_->getGeometry().gridHeight());
}

void PianoRollEditor::centerOnNotes() {
    if (!viewport_) return;
    const int visibleHeight = viewport_->getMaximumVisibleHeight();
    if (visibleHeight <= 0 || noteGrid_->getHeight() <= 0) return;

    int pitch = 60;
    if (midiClip_ && !midiClip_->getNotes().empty()) {
        long sum = 0;
        for (const auto& note : midiClip_->getNotes()) sum += note.getPitch();
        pitch = static_cast<int>(sum / static_cast<long>(midiClip_->getNotes().size()));
    }
    const int y = noteGrid_->getGeometry().yFromPitch(pitch, 0);
    const int maxY = juce::jmax(0, noteGrid_->getHeight() - visibleHeight);
    const int target = juce::jlimit(0, maxY, y - visibleHeight / 2);

    ++programmaticViewChanges_;
    viewport_->setViewPosition(viewport_->getViewPositionX(), target);
    --programmaticViewChanges_;
}

void PianoRollEditor::updatePlayhead() {
    playheadVisible_ = false;
    playheadBeat_ = -1.0;
    if (transport_ && localBeatProvider_) {
        double local = 0.0;
        if (localBeatProvider_(transport_->getPositionInBeats(), local) && std::isfinite(local)) {
            playheadVisible_ = true;
            playheadBeat_ = local;
        }
    }
    const double visible = playheadVisible_ ? playheadBeat_ : -1.0;
    timeRuler_->setPlayhead(visible);
    noteGrid_->setPlayheadBeats(visible);
}

void PianoRollEditor::applyFollowScroll() {
    if (!playheadVisible_) return;
    const double playheadX = PianoRollGeometry::xFromBeat(playheadBeat_, pixelsPerBeat_);
    const double target = followedScrollX(playheadX, viewport_->getViewPositionX(),
                                          viewport_->getMaximumVisibleWidth(),
                                          noteGrid_->getWidth());
    if (target < 0.0) return;
    applyingFollowScroll_ = true;
    viewport_->setViewPosition(static_cast<int>(std::lround(target)), viewport_->getViewPositionY());
    applyingFollowScroll_ = false;
}

void PianoRollEditor::syncScrollBetweenKeyboardAndGrid() {
    keyboard_->setScrollOffset(viewport_->getViewPositionY());
    timeRuler_->setTimeOffset(getHorizontalScroll());
}

void PianoRollEditor::onViewChanged() {
    syncScrollBetweenKeyboardAndGrid();
    // A viewport move that we did not drive is a manual scroll: pause following
    // until the Follow toggle (or playback restart) resumes it.
    if (programmaticViewChanges_ == 0 && !applyingFollowScroll_)
        followSuspended_ = true;
}

} // namespace vibedaw
