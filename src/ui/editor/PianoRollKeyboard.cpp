#include "PianoRollKeyboard.h"
#include "ui/Theme.h"

namespace vibedaw {

PianoRollKeyboard::PianoRollKeyboard()
    : heldNotes_(128, false)
{
    setOpaque(false);
}

void PianoRollKeyboard::setLowestNote(int lowest) {
    geometry_.lowestNote = juce::jlimit(0, 127, lowest);
    geometry_.numKeys = juce::jlimit(1, 128 - geometry_.lowestNote, geometry_.numKeys);
    repaint();
}

void PianoRollKeyboard::setNumKeys(int numKeys) {
    geometry_.numKeys = juce::jlimit(1, 128 - geometry_.lowestNote, numKeys);
    repaint();
}

void PianoRollKeyboard::setKeyHeight(int height) {
    geometry_.keyHeight = juce::jmax(4, height);
    repaint();
}

int PianoRollKeyboard::getKeyForY(int y) const {
    return geometry_.pitchFromY(y, scrollOffset_);
}

int PianoRollKeyboard::getYForKey(int noteNumber) const {
    return geometry_.yFromPitch(noteNumber, scrollOffset_);
}

void PianoRollKeyboard::setHeldNote(int pitch, bool held) {
    if (pitch >= 0 && pitch < 128) {
        heldNotes_[pitch] = held;
        repaint();
    }
}

void PianoRollKeyboard::clearHeldNotes() {
    std::fill(heldNotes_.begin(), heldNotes_.end(), false);
    repaint();
}

bool PianoRollKeyboard::isBlackKey(int noteNumber) const {
    int noteInOctave = noteNumber % 12;
    return noteInOctave == 1 || noteInOctave == 3 || 
           noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10;
}

juce::String PianoRollKeyboard::getNoteName(int noteNumber) const {
    static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = noteNumber / 12 - 1;
    int noteInOctave = noteNumber % 12;
    return juce::String(noteNames[noteInOctave]) + juce::String(octave);
}

void PianoRollKeyboard::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    int width = bounds.getWidth();
    g.fillAll(theme::deepWell);
    
    for (int i = 0; i < geometry_.numKeys; ++i) {
        int noteNumber = geometry_.lowestNote + i;
        if (noteNumber > 127) break;
        
        int y = getYForKey(noteNumber);
        if (y + geometry_.keyHeight <= 0 || y >= getHeight()) continue;
        
        bool isBlack = isBlackKey(noteNumber);
        bool isHeld = heldNotes_[noteNumber];
        
        juce::Colour keyColour;
        if (isHeld) {
            keyColour = isBlack ? theme::accent.darker(0.35f) : theme::accent;
        } else {
            keyColour = isBlack ? theme::controlHover : juce::Colour(0xffe3e8e6);
        }

        const auto edgeColour = isHeld ? keyColour.darker(0.2f)
                                      : isBlack ? theme::deepWell : juce::Colour(0xffaab8c2);
        g.setGradientFill(juce::ColourGradient(keyColour, 0.0f, static_cast<float>(y),
                                             edgeColour, static_cast<float>(width),
                                             static_cast<float>(y + geometry_.keyHeight), false));
        g.fillRect(0, y, width, geometry_.keyHeight);

        g.setColour(theme::deepWell.withAlpha(isBlack ? 0.8f : 0.4f));
        g.drawHorizontalLine(y + geometry_.keyHeight - 1, 0.0f, static_cast<float>(width));
        g.setColour(theme::white.withAlpha(isBlack ? 0.08f : 0.4f));
        g.drawHorizontalLine(y, 1.0f, static_cast<float>(width - 1));
        if (isHeld) {
            g.setColour(theme::accent.brighter(0.3f));
            g.fillRect(width - 3, y, 3, geometry_.keyHeight - 1);
        }
        
        if (noteNumber % 12 == 0) {
            g.setColour(theme::deepWell);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(getNoteName(noteNumber), 4, y + 2, width - 8, geometry_.keyHeight - 4,
                       juce::Justification::centredLeft, true);
        }
    }
    
    g.setColour(theme::border);
    g.drawVerticalLine(width - 1, 0.0f, static_cast<float>(getHeight()));
}

void PianoRollKeyboard::mouseDown(const juce::MouseEvent& e) {
    int note = getKeyForY(e.y);
    if (note >= 0 && note < 128) {
        lastHeldNote_ = note;
        heldNotes_[note] = true;
        if (listener_) {
            listener_->noteOn(note);
        }
        repaint();
    }
}

void PianoRollKeyboard::mouseUp(const juce::MouseEvent& e) {
    if (lastHeldNote_ >= 0 && lastHeldNote_ < 128) {
        heldNotes_[lastHeldNote_] = false;
        if (listener_) {
            listener_->noteOff(lastHeldNote_);
        }
        lastHeldNote_ = -1;
        repaint();
    }
}

void PianoRollKeyboard::mouseDrag(const juce::MouseEvent& e) {
    int note = getKeyForY(e.y);
    if (note >= 0 && note < 128 && note != lastHeldNote_) {
        if (lastHeldNote_ >= 0 && lastHeldNote_ < 128) {
            heldNotes_[lastHeldNote_] = false;
            if (listener_) {
                listener_->noteOff(lastHeldNote_);
            }
        }
        lastHeldNote_ = note;
        heldNotes_[note] = true;
        if (listener_) {
            listener_->noteOn(note);
        }
        repaint();
    }
}

} // namespace vibedaw
