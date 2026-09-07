#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

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
    int getLowestNote() const { return lowestNote_; }
    
    void setNumKeys(int numKeys);
    int getNumKeys() const { return numKeys_; }
    
    int getKeyHeight() const { return keyHeight_; }
    void setKeyHeight(int height);
    
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
    int lowestNote_ = 36;
    int numKeys_ = 128;
    int keyHeight_ = defaultKeyHeight;
    int keyWidth_ = defaultKeyWidth;
    
    std::vector<bool> heldNotes_;
    int lastHeldNote_ = -1;
    
    bool isBlackKey(int noteNumber) const;
    juce::String getNoteName(int noteNumber) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollKeyboard)
};

} // namespace vibedaw