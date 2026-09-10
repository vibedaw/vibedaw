#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PianoRollGeometry.h"

namespace vibedaw {

// Piano keyboard shown beside the grid. Pitch rows come from the shared
// PianoRollGeometry so its keys line up with the grid at any viewport scroll.
class PianoRollKeyboard : public juce::Component {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void noteOn(int pitch) = 0;
        virtual void noteOff(int pitch) = 0;
    };
    
    PianoRollKeyboard();
    ~PianoRollKeyboard() override = default;
    
    void setListener(Listener* listener) { listener_ = listener; }
    
    void setLowestNote(int lowest);
    int getLowestNote() const { return geometry_.lowestNote; }
    
    void setNumKeys(int numKeys);
    int getNumKeys() const { return geometry_.numKeys; }
    
    int getKeyHeight() const { return geometry_.keyHeight; }
    void setKeyHeight(int height);
    void setScrollOffset(int pixels) { scrollOffset_ = pixels; repaint(); }
    
    int getKeyForY(int y) const;
    int getYForKey(int noteNumber) const;
    
    void setHeldNote(int pitch, bool held);
    void clearHeldNotes();
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    
    static constexpr int defaultKeyWidth = 60;
    static constexpr int defaultKeyHeight = 12;
    
private:
    Listener* listener_ = nullptr;
    PianoRollGeometry geometry_;
    int scrollOffset_ = 0;
    int keyWidth_ = defaultKeyWidth;
    
    std::vector<bool> heldNotes_;
    int lastHeldNote_ = -1;
    
    bool isBlackKey(int noteNumber) const;
    juce::String getNoteName(int noteNumber) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollKeyboard)
};

} // namespace vibedaw
