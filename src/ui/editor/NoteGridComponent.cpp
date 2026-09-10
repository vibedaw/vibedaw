#include "NoteGridComponent.h"
#include "ui/Theme.h"
#include "project/Clip.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace vibedaw {

NoteGridComponent::NoteGridComponent() {
    setOpaque(false);
    setWantsKeyboardFocus(true);
}

NoteGridComponent::~NoteGridComponent() {
    if (midiClip_) midiClip_->removeListener(this);
}

void NoteGridComponent::clipChanged() {
    repaint();
    if (listener_) listener_->gridContentChanged();
}

void NoteGridComponent::notesInvalidated() {
    selectedNotes_.clear();
    hoveredNote_ = nullptr;
    dragState_ = {};
    repaint();
    if (listener_) listener_->gridContentChanged();
}

void NoteGridComponent::setMidiClip(MidiClip* clip) {
    if (midiClip_) midiClip_->removeListener(this);
    notesInvalidated();
    midiClip_ = clip;
    if (midiClip_) midiClip_->addListener(this);
    repaint();
    if (listener_) listener_->gridContentChanged();
}

void NoteGridComponent::setGridResolution(GridResolution resolution) {
    gridResolution_ = resolution;
    repaint();
}

void NoteGridComponent::setPixelsPerBeat(int pixels) {
    geometry_.pixelsPerBeat = juce::jmax(10.0, static_cast<double>(pixels));
    repaint();
}

void NoteGridComponent::setKeyHeight(int height) {
    geometry_.keyHeight = juce::jmax(4, height);
    repaint();
}

void NoteGridComponent::setLowestNote(int lowest) {
    geometry_.lowestNote = juce::jlimit(0, 127, lowest);
    geometry_.numKeys = 128 - geometry_.lowestNote;
    repaint();
}

void NoteGridComponent::selectNote(const Note* note) {
    if (note == nullptr) return;
    auto it = std::find(selectedNotes_.begin(), selectedNotes_.end(), note);
    if (it == selectedNotes_.end()) {
        selectedNotes_.push_back(note);
    }
    repaint();
}

void NoteGridComponent::clearSelection() {
    selectedNotes_.clear();
    repaint();
}

std::vector<const Note*> NoteGridComponent::getSelectedNotes() {
    return selectedNotes_;
}

double NoteGridComponent::snapToGrid(double time) const {
    double gridSize = 4.0 / static_cast<double>(gridResolution_);
    return juce::jmax(0.0, std::round(time / gridSize) * gridSize);
}

int NoteGridComponent::pitchFromY(int y) const {
    return geometry_.pitchFromY(y, 0);
}

int NoteGridComponent::yFromPitch(int pitch) const {
    return geometry_.yFromPitch(pitch, 0);
}

double NoteGridComponent::timeFromX(int x) const {
    return PianoRollGeometry::beatFromX(static_cast<double>(x), geometry_.pixelsPerBeat);
}

int NoteGridComponent::xFromTime(double time) const {
    return static_cast<int>(juce::jlimit(static_cast<double>(std::numeric_limits<int>::min()),
                                       static_cast<double>(std::numeric_limits<int>::max()),
                                       PianoRollGeometry::xFromBeat(time, geometry_.pixelsPerBeat)));
}

const Note* NoteGridComponent::findNoteAt(int x, int y) {
    if (!midiClip_) return nullptr;
    
    int pitch = pitchFromY(y);
    if (pitch < 0 || pitch > 127) return nullptr;
    double time = timeFromX(x);
    
    return midiClip_->findNoteAt(time, pitch);
}

void NoteGridComponent::drawNote(juce::Graphics& g, const Note& note, bool isSelected, bool isHovered) {
    const double ppb = geometry_.pixelsPerBeat;
    const double left = PianoRollGeometry::xFromBeat(note.getStartTime(), ppb);
    const double right = PianoRollGeometry::xFromBeat(note.getEndTime(), ppb);
    if (right < 0.0 || left >= getWidth()) return;
    int x = static_cast<int>(juce::jmax(0.0, left));
    int y = yFromPitch(note.getPitch());
    int width = juce::jmax(3, static_cast<int>(juce::jmin(static_cast<double>(getWidth()), right)) - x);
    
    juce::Colour noteColour;
    if (isSelected) {
        noteColour = theme::noteSelected;
    } else if (isHovered) {
        noteColour = theme::noteHover;
    } else {
        noteColour = theme::noteDefault;
    }
    
    g.setColour(noteColour);
    g.fillRect(x + 1, y + 1, width - 2, geometry_.keyHeight - 2);
    
    g.setColour(noteColour.darker(0.3f));
    g.drawRect(x, y, width, geometry_.keyHeight, 1);
}

void NoteGridComponent::drawGridLines(juce::Graphics& g) {
    double gridSize = 4.0 / static_cast<double>(gridResolution_);
    
    g.setColour(theme::controlActive);
    
    for (int y = 0; y < getHeight(); y += geometry_.keyHeight) {
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));
    }
    
    for (double time = 0.0; time < timeFromX(getWidth()); time += gridSize) {
        int x = xFromTime(time);
        bool isBeat = std::fmod(time, 1.0) < 0.001;
        
        if (isBeat) {
            g.setColour(theme::separator);
        } else {
            g.setColour(theme::gridSub);
        }
        g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));
    }
    
    for (int beat = 0; xFromTime(beat) < getWidth(); ++beat) {
        bool isMeasure = beat % 4 == 0;
        if (isMeasure) {
            g.setColour(theme::gridMeasure);
            g.drawVerticalLine(xFromTime(beat), 0.0f, static_cast<float>(getHeight()));
        }
    }
}

void NoteGridComponent::paint(juce::Graphics& g) {
    g.fillAll(theme::raised);
    
    drawGridLines(g);
    
    if (midiClip_) {
        auto& notes = midiClip_->getNotes();
        for (const auto& note : notes) {
            bool isSelected = std::find(selectedNotes_.begin(), selectedNotes_.end(), &note) != selectedNotes_.end();
            bool isHovered = &note == hoveredNote_;
            drawNote(g, note, isSelected, isHovered);
        }
    }

    if (playheadBeats_ >= 0.0) {
        const int x = xFromTime(playheadBeats_);
        g.setColour(theme::accent);
        g.fillRect(static_cast<float>(x), 0.0f, 2.0f, static_cast<float>(getHeight()));
    }
}

void NoteGridComponent::mouseDown(const juce::MouseEvent& e) {
    if (!midiClip_) return;

    const Note* existingNote = findNoteAt(e.x, e.y);

    if (e.mods.isRightButtonDown()) {
        if (existingNote) {
            Note removed = *existingNote;
            midiClip_->removeNote(existingNote);
            if (listener_) {
                listener_->noteRemoved(removed);
            }
        }
        repaint();
        return;
    }

    if (isShowing()) grabKeyboardFocus();

    if (e.mods.isShiftDown() && existingNote) {
        selectNote(existingNote);
        return;
    }
    
    if (existingNote) {
        int noteEndX = xFromTime(existingNote->getEndTime());
        int edgeThreshold = 8;
        
        if (e.x < noteEndX && e.x > noteEndX - edgeThreshold) {
            dragState_.mode = DragState::Mode::ResizeEnd;
            dragState_.note = existingNote;
            dragState_.originalDuration = existingNote->getDuration();
            dragState_.originalStart = existingNote->getStartTime();
        } else if (e.x > xFromTime(existingNote->getStartTime()) &&
                   e.x < xFromTime(existingNote->getStartTime()) + edgeThreshold) {
            dragState_.mode = DragState::Mode::ResizeStart;
            dragState_.note = existingNote;
            dragState_.originalStart = existingNote->getStartTime();
            dragState_.originalDuration = existingNote->getDuration();
        } else {
            dragState_.mode = DragState::Mode::Move;
            dragState_.note = existingNote;
            dragState_.originalStart = existingNote->getStartTime();
            dragState_.originalPitch = existingNote->getPitch();
        }
        dragState_.startX = e.x;
        dragState_.startY = e.y;
        clearSelection();
        selectNote(existingNote);
    } else {
        int pitch = pitchFromY(e.y);
        if (pitch < 0 || pitch > 127) return;
        double time = snapToGrid(timeFromX(e.x));
        
        dragState_.note = midiClip_->addNote(Note(pitch, time, 4.0 / static_cast<double>(gridResolution_)));
        if (!dragState_.note) return;
        dragState_.mode = DragState::Mode::Create;
        dragState_.startX = e.x;
        dragState_.originalStart = time;
        
        clearSelection();
        selectNote(dragState_.note);
        
        if (listener_) {
            listener_->noteAdded(*dragState_.note);
        }
    }
    
    repaint();
}

void NoteGridComponent::mouseDrag(const juce::MouseEvent& e) {
    if (!midiClip_ || !dragState_.note) return;
    
    int deltaY = e.y - dragState_.startY;
    int deltaX = e.x - dragState_.startX;
    Note edited = *dragState_.note;
    
    switch (dragState_.mode) {
        case DragState::Mode::Create:
        case DragState::Mode::ResizeEnd: {
            double endTime = snapToGrid(timeFromX(e.x));
            double startTime = dragState_.note->getStartTime();
            double duration = juce::jmax(4.0 / static_cast<double>(gridResolution_), endTime - startTime);
            edited.setDuration(duration);
            break;
        }
        case DragState::Mode::ResizeStart: {
            double newStart = snapToGrid(timeFromX(e.x));
            double endTime = dragState_.note->getStartTime() + dragState_.note->getDuration();
            if (newStart < endTime - 0.01) {
                edited.setStartTime(newStart);
                edited.setDuration(endTime - newStart);
            }
            break;
        }
        case DragState::Mode::Move: {
            double newStart = snapToGrid(dragState_.originalStart +
                                         deltaX / geometry_.pixelsPerBeat);
            int newPitch = juce::jlimit(0, 127, dragState_.originalPitch - deltaY / geometry_.keyHeight);
            edited.setStartTime(newStart);
            edited.setPitch(newPitch);
            break;
        }
        default:
            break;
    }
    midiClip_->updateNote(dragState_.note, edited);
    repaint();
}

void NoteGridComponent::mouseUp(const juce::MouseEvent& e) {
    juce::ignoreUnused(e);
    if (dragState_.mode != DragState::Mode::None && dragState_.note && listener_) {
        listener_->noteChanged(*dragState_.note);
    }
    dragState_.mode = DragState::Mode::None;
    dragState_.note = nullptr;
}

void NoteGridComponent::mouseMove(const juce::MouseEvent& e) {
    const Note* note = findNoteAt(e.x, e.y);
    if (note != hoveredNote_) {
        hoveredNote_ = note;
        repaint();
    }
}

void NoteGridComponent::mouseDoubleClick(const juce::MouseEvent& e) {
    const Note* note = findNoteAt(e.x, e.y);
    if (note && midiClip_) {
        Note removed = *note;
        midiClip_->removeNote(note);
        if (listener_) {
            listener_->noteRemoved(removed);
        }
        repaint();
    }
}

bool NoteGridComponent::keyPressed(const juce::KeyPress& key) {
    if (!midiClip_ || selectedNotes_.empty()) return false;
    if (!key.isKeyCode(juce::KeyPress::deleteKey) && !key.isKeyCode(juce::KeyPress::backspaceKey)) return false;
    // removeNote invalidates borrowed pointers and clears the grid selection,
    // so delete from value snapshots, re-resolving each live note.
    std::vector<Note> snapshot;
    snapshot.reserve(selectedNotes_.size());
    for (const auto* note : selectedNotes_) snapshot.push_back(*note);
    bool removed = false;
    for (const auto& value : snapshot) {
        for (const auto& note : midiClip_->getNotes()) {
            if (note.getStartTime() == value.getStartTime() && note.getPitch() == value.getPitch()) {
                Note removedNote = note;
                midiClip_->removeNote(&note);
                if (listener_) listener_->noteRemoved(removedNote);
                removed = true;
                break;
            }
        }
    }
    repaint();
    return removed;
}

} // namespace vibedaw
