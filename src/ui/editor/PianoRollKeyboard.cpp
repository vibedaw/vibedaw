#include "PianoRollKeyboard.h"

namespace vibedaw {

PianoRollKeyboard::PianoRollKeyboard()
    : heldNotes_(128, false)
{
    setOpaque(false);
}

void PianoRollKeyboard::setLowestNote(int lowest) {
    lowestNote_ = juce::jlimit(0, 127, lowest);
    repaint();
}

void PianoRollKeyboard::setNumKeys(int numKeys) {
    numKeys_ = juce::jlimit(1, 128, numKeys);
    repaint();
}

void PianoRollKeyboard::setKeyHeight(int height) {
    keyHeight_ = juce::jmax(4, height);
    repaint();
}

int PianoRollKeyboard::getKeyForY(int y) const {
    int noteIndex = (getHeight() - y) / keyHeight_;
    return lowestNote_ + noteIndex;
}

int PianoRollKeyboard::getYForKey(int noteNumber) const {
    int noteIndex = noteNumber - lowestNote_;
    return getHeight() - (noteIndex + 1) * keyHeight_;
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
    
    for (int i = 0; i < numKeys_; ++i) {
        int noteNumber = lowestNote_ + i;
        if (noteNumber > 127) break;
        
        int y = getYForKey(noteNumber);
        
        bool isBlack = isBlackKey(noteNumber);
        bool isHeld = heldNotes_[noteNumber];
        
        juce::Colour keyColour;
        if (isHeld) {
            keyColour = isBlack ? juce::Colour(0xff4466aa) : juce::Colour(0xff6688cc);
        } else {
            keyColour = isBlack ? juce::Colour(0xff2a2a2a) : juce::Colour(0xff3a3a3a);
        }
        
        g.setColour(keyColour);
        g.fillRect(0, y, width, keyHeight_);
        
        if (!isBlack) {
            g.setColour(juce::Colour(0xff505050));
            g.drawHorizontalLine(y, 0.0f, static_cast<float>(width));
        }
        
        if (noteNumber % 12 == 0) {
            g.setColour(juce::Colours::white);
            g.setFont(10.0f);
            g.drawText(getNoteName(noteNumber), 4, y + 2, width - 8, keyHeight_ - 4,
                       juce::Justification::centredLeft, true);
        }
    }
    
    g.setColour(juce::Colour(0xff606060));
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