#include "NoteGridComponent.h"
#include "project/Clip.h"
#include <algorithm>
#include <cmath>

namespace vibedaw {

NoteGridComponent::NoteGridComponent() {
    setOpaque(false);
    setWantsKeyboardFocus(true);
}

NoteGridComponent::~NoteGridComponent() = default;

void NoteGridComponent::setMidiClip(MidiClip* clip) {
    midiClip_ = clip;
    repaint();
}

void NoteGridComponent::setGridResolution(GridResolution resolution) {
    gridResolution_ = resolution;
    repaint();
}

void NoteGridComponent::setPixelsPerBeat(int pixels) {
    pixelsPerBeat_ = juce::jmax(10, pixels);
    repaint();
}

void NoteGridComponent::setKeyHeight(int height) {
    keyHeight_ = juce::jmax(4, height);
    repaint();
}

void NoteGridComponent::setLowestNote(int lowest) {
    lowestNote_ = juce::jlimit(0, 127, lowest);
    repaint();
}

void NoteGridComponent::setVisibleBeats(double beats) {
    juce::ignoreUnused(beats);
}

void NoteGridComponent::setTimeOffset(double beats) {
    timeOffset_ = beats;
    repaint();
}

void NoteGridComponent::setScrollOffset(int offsetY) {
    scrollOffsetY_ = offsetY;
    repaint();
}

void NoteGridComponent::selectNote(Note* note) {
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

std::vector<Note*> NoteGridComponent::getSelectedNotes() {
    return selectedNotes_;
}

double NoteGridComponent::snapToGrid(double time) const {
    double gridSize = 1.0 / static_cast<double>(gridResolution_);
    return std::round(time / gridSize) * gridSize;
}

int NoteGridComponent::pitchFromY(int y) const {
    int adjustedY = y + scrollOffsetY_;
    int noteIndex = (getHeight() - adjustedY) / keyHeight_;
    return lowestNote_ + noteIndex;
}

int NoteGridComponent::yFromPitch(int pitch) const {
    int noteIndex = pitch - lowestNote_;
    return getHeight() - (noteIndex + 1) * keyHeight_ - scrollOffsetY_;
}

double NoteGridComponent::timeFromX(int x) const {
    return static_cast<double>(x) / pixelsPerBeat_ + timeOffset_;
}

int NoteGridComponent::xFromTime(double time) const {
    return static_cast<int>((time - timeOffset_) * pixelsPerBeat_);
}

Note* NoteGridComponent::findNoteAt(int x, int y) {
    if (!midiClip_) return nullptr;
    
    int pitch = pitchFromY(y);
    double time = timeFromX(x);
    
    return midiClip_->findNoteAt(time, pitch);
}

void NoteGridComponent::drawNote(juce::Graphics& g, const Note& note, bool isSelected, bool isHovered) {
    int x = xFromTime(note.getStartTime());
    int y = yFromPitch(note.getPitch());
    int width = static_cast<int>(note.getDuration() * pixelsPerBeat_);
    
    juce::Colour noteColour;
    if (isSelected) {
        noteColour = juce::Colour(0xffee9933);
    } else if (isHovered) {
        noteColour = juce::Colour(0xff88aa55);
    } else {
        noteColour = juce::Colour(0xff6ad94a);
    }
    
    g.setColour(noteColour);
    g.fillRect(x + 1, y + 1, width - 2, keyHeight_ - 2);
    
    g.setColour(noteColour.darker(0.3f));
    g.drawRect(x, y, width, keyHeight_, 1);
}

void NoteGridComponent::drawGridLines(juce::Graphics& g) {
    double gridSize = 1.0 / static_cast<double>(gridResolution_);
    int gridPixels = static_cast<int>(gridSize * pixelsPerBeat_);
    
    g.setColour(juce::Colour(0xff404040));
    
    for (int y = 0; y < getHeight(); y += keyHeight_) {
        g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));
    }
    
    for (int x = 0; x < getWidth(); x += gridPixels) {
        double time = timeFromX(x);
        bool isBeat = std::fmod(time, 1.0) < 0.001;
        
        if (isBeat) {
            g.setColour(juce::Colour(0xff505050));
        } else {
            g.setColour(juce::Colour(0xff383838));
        }
        g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));
    }
    
    for (int beat = 0; beat * pixelsPerBeat_ < getWidth(); ++beat) {
        bool isMeasure = beat % 4 == 0;
        if (isMeasure) {
            g.setColour(juce::Colour(0xff606060));
            g.drawVerticalLine(beat * pixelsPerBeat_, 0.0f, static_cast<float>(getHeight()));
        }
    }
}

void NoteGridComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff252525));
    
    drawGridLines(g);
    
    if (midiClip_) {
        auto& notes = midiClip_->getNotes();
        for (const auto& note : notes) {
            bool isSelected = std::find(selectedNotes_.begin(), selectedNotes_.end(), &note) != selectedNotes_.end();
            bool isHovered = &note == hoveredNote_;
            drawNote(g, note, isSelected, isHovered);
        }
    }
}

void NoteGridComponent::mouseDown(const juce::MouseEvent& e) {
    if (!midiClip_) return;
    
    Note* existingNote = findNoteAt(e.x, e.y);
    
    if (e.mods.isRightButtonDown()) {
        if (existingNote) {
            midiClip_->removeNote(existingNote);
            if (listener_) {
                listener_->noteRemoved(*existingNote);
            }
        }
        repaint();
        return;
    }
    
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
        } else if (e.x > existingNote->getStartTime() * pixelsPerBeat_ && 
                   e.x < existingNote->getStartTime() * pixelsPerBeat_ + edgeThreshold) {
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
        double time = snapToGrid(timeFromX(e.x));
        
        auto& notes = midiClip_->getNotes();
        notes.emplace_back(pitch, time, 1.0 / static_cast<double>(gridResolution_));
        dragState_.note = &notes.back();
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
    
    switch (dragState_.mode) {
        case DragState::Mode::Create:
        case DragState::Mode::ResizeEnd: {
            double endTime = snapToGrid(timeFromX(e.x));
            double startTime = dragState_.note->getStartTime();
            double duration = juce::jmax(1.0 / static_cast<double>(gridResolution_), endTime - startTime);
            dragState_.note->setDuration(duration);
            break;
        }
        case DragState::Mode::ResizeStart: {
            double newStart = snapToGrid(timeFromX(e.x));
            double endTime = dragState_.note->getStartTime() + dragState_.note->getDuration();
            if (newStart < endTime - 0.01) {
                dragState_.note->setStartTime(newStart);
                dragState_.note->setDuration(endTime - newStart);
            }
            break;
        }
        case DragState::Mode::Move: {
            double newStart = snapToGrid(dragState_.originalStart + deltaX / static_cast<double>(pixelsPerBeat_));
            int newPitch = juce::jlimit(0, 127, dragState_.originalPitch - deltaY / keyHeight_);
            dragState_.note->setStartTime(newStart);
            dragState_.note->setPitch(newPitch);
            break;
        }
        default:
            break;
    }
    
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
    Note* note = findNoteAt(e.x, e.y);
    if (note != hoveredNote_) {
        hoveredNote_ = note;
        repaint();
    }
}

void NoteGridComponent::mouseDoubleClick(const juce::MouseEvent& e) {
    Note* note = findNoteAt(e.x, e.y);
    if (note && midiClip_) {
        midiClip_->removeNote(note);
        if (listener_) {
            listener_->noteRemoved(*note);
        }
        selectedNotes_.erase(
            std::remove(selectedNotes_.begin(), selectedNotes_.end(), note),
            selectedNotes_.end());
        repaint();
    }
}

void NoteGridComponent::scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) {
    juce::ignoreUnused(scrollBar, newRangeStart);
}

} // namespace vibedaw