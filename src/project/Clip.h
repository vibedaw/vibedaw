#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Note.h"
#include <vector>
#include <memory>

namespace vibedaw {

class Clip {
public:
    // MIDI times are quarter-note beats; AudioClip times remain seconds.
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void clipChanged() = 0;
        virtual void notesInvalidated() {}
    };
    void addListener(Listener* listener) { listeners_.add(listener); }
    void removeListener(Listener* listener) { listeners_.remove(listener); }
    enum class Type {
        Audio,
        Midi,
        Pattern
    };
    
    Clip(Type type, double startTime, double duration);
    virtual ~Clip() = default;
    
    Type getType() const { return clipType; }
    
    double getStartTime() const { return startTime; }
    void setStartTime(double time);
    
    double getDuration() const { return duration; }
    void setDuration(double dur);
    
    double getEndTime() const { return startTime + duration; }
    
    const juce::String& getName() const { return name; }
    void setName(const juce::String& n);
    
    const juce::Colour& getColour() const { return colour; }
    void setColour(const juce::Colour& c);
    
    bool isSelected() const { return selected; }
    void setSelected(bool sel);
    
    bool isMuted() const { return muted; }
    void setMuted(bool m);
    
    bool overlaps(double time) const;
    bool contains(double time) const;
    
    virtual std::unique_ptr<Clip> clone() const = 0;
    
protected:
    void notifyChanged() { listeners_.call([](Listener& l) { l.clipChanged(); }); }
    void invalidateNotes() { listeners_.call([](Listener& l) { l.notesInvalidated(); }); }
    juce::ListenerList<Listener> listeners_;
    Type clipType;
    double startTime = 0.0;
    double duration = 1.0;
    juce::String name;
    juce::Colour colour{0xff6a6aff};
    bool selected = false;
    bool muted = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Clip)
};

class AudioClip : public Clip {
public:
    AudioClip(double startTime = 0.0, double duration = 1.0);
    ~AudioClip() override = default;
    
    void setAudioFile(const juce::File& file);
    const juce::File& getAudioFile() const { return audioFile; }
    
    void setWaveform(const juce::AudioBuffer<float>& buffer);
    const juce::AudioBuffer<float>& getWaveform() const { return waveform; }
    
    bool hasWaveform() const { return waveform.getNumSamples() > 0; }
    
    std::unique_ptr<Clip> clone() const override;
    
private:
    juce::File audioFile;
    juce::AudioBuffer<float> waveform;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioClip)
};

// Clip-local quarter-note beat and exact three-byte MIDI message. The status
// includes the channel nibble: CC 0xB0..0xBF or pitch bend 0xE0..0xEF only.
struct MidiExpressionEvent {
    double beat = 0.0;
    int status = 0xb0;
    int data1 = 0;
    int data2 = 0;

    int getChannel() const { return (status & 0x0f) + 1; }
    bool isValid() const;
    static bool compareByBeat(const MidiExpressionEvent& a, const MidiExpressionEvent& b) {
        return a.beat < b.beat;
    }
};

class MidiClip : public Clip {
public:
    // Notes use local beat zero, not Clip::startTime. Only ClipInstance positions
    // a pooled source in the arrangement; this source's duration is its length.
    MidiClip(double startTime = 0.0, double duration = 1.0);
    ~MidiClip() override = default;
    
    // Reserved for future clip-local repetition, not transport looping. M1 plays once.
    void setLoopEnabled(bool loop) { loopEnabled = loop; notifyChanged(); }
    bool isLoopEnabled() const { return loopEnabled; }
    
    const std::vector<Note>& getNotes() const { return notes_; }
    // Insertion order, not time-sorted. Borrowed pointers expire at notesInvalidated.
    
    const Note* addNote(const Note& note);
    void updateNote(const Note* note, const Note& replacement);
    void removeNote(int index);
    void removeNote(const Note* note);
    void clearNotes();
    int getNumNotes() const { return static_cast<int>(notes_.size()); }
    const Note* findNoteAt(double time, int pitch) const;

    static constexpr size_t maxNotes = 65536;
    static constexpr size_t maxExpressionEvents = 65536;
    const std::vector<MidiExpressionEvent>& getExpressionEvents() const { return expressionEvents_; }
    // Message-thread mutations; false means invalid input/capacity and no change
    // or notification. Events are sorted by beat, preserving input order at ties.
    // Borrowed expression elements expire on the next expression mutation.
    bool addExpressionEvent(const MidiExpressionEvent& event);
    bool setExpressionEvents(std::vector<MidiExpressionEvent> events);
    void clearExpressionEvents();
    // Recorder commit: validate both collections before replacing either, then
    // invalidate note pointers and emit one clipChanged notification. No audio
    // thread synchronization is implied; source duration is left unchanged.
    bool replaceContent(std::vector<Note> notes, std::vector<MidiExpressionEvent> events);
    
    std::unique_ptr<Clip> clone() const override;
    
private:
    bool loopEnabled = false;
    std::vector<Note> notes_;
    std::vector<MidiExpressionEvent> expressionEvents_;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiClip)
};

class PatternClip : public Clip {
public:
    PatternClip(double startTime = 0.0, double duration = 1.0);
    ~PatternClip() override = default;
    
    void setPatternLength(double length);
    double getPatternLength() const { return patternLength; }
    
    void setLoopCount(int count);
    int getLoopCount() const { return loopCount; }
    
    std::unique_ptr<Clip> clone() const override;
    
private:
    double patternLength = 1.0;
    int loopCount = 1;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatternClip)
};

} // namespace vibedaw
