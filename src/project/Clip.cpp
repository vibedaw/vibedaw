#include "Clip.h"
#include "Note.h"
#include <algorithm>

namespace vibedaw {

Clip::Clip(Type type, double start, double dur)
    : clipType(type)
    , startTime(start)
    , duration(dur)
{
}

void Clip::setStartTime(double time) {
    startTime = time;
}

void Clip::setDuration(double dur) {
    duration = dur;
}

void Clip::setName(const juce::String& n) {
    name = n;
}

void Clip::setColour(const juce::Colour& c) {
    colour = c;
}

void Clip::setSelected(bool sel) {
    selected = sel;
}

void Clip::setMuted(bool m) {
    muted = m;
}

bool Clip::overlaps(double time) const {
    return time >= startTime && time < getEndTime();
}

bool Clip::contains(double time) const {
    return time >= startTime && time <= getEndTime();
}

AudioClip::AudioClip(double startTime, double duration)
    : Clip(Type::Audio, startTime, duration)
{
    colour = juce::Colour(0xff4a90d9);
    name = "Audio";
}

void AudioClip::setAudioFile(const juce::File& file) {
    audioFile = file;
    name = file.getFileNameWithoutExtension();
}

void AudioClip::setWaveform(const juce::AudioBuffer<float>& buffer) {
    waveform = buffer;
}

std::unique_ptr<Clip> AudioClip::clone() const {
    auto cloned = std::make_unique<AudioClip>(startTime, duration);
    cloned->name = name;
    cloned->colour = colour;
    cloned->selected = selected;
    cloned->muted = muted;
    cloned->audioFile = audioFile;
    cloned->waveform = waveform;
    return cloned;
}

MidiClip::MidiClip(double startTime, double duration)
    : Clip(Type::Midi, startTime, duration)
{
    colour = juce::Colour(0xff6ad94a);
    name = "MIDI";
}

void MidiClip::addNote(const Note& note) {
    notes_.push_back(note);
    std::sort(notes_.begin(), notes_.end(), Note::compareByTime);
}

void MidiClip::removeNote(int index) {
    if (index >= 0 && index < static_cast<int>(notes_.size())) {
        notes_.erase(notes_.begin() + index);
    }
}

void MidiClip::removeNote(const Note* note) {
    if (note == nullptr) return;
    auto it = std::find_if(notes_.begin(), notes_.end(),
        [note](const Note& n) { return &n == note; });
    if (it != notes_.end()) {
        notes_.erase(it);
    }
}

void MidiClip::clearNotes() {
    notes_.clear();
}

Note* MidiClip::findNoteAt(double time, int pitch) {
    for (auto& note : notes_) {
        if (note.getPitch() == pitch && note.containsTime(time)) {
            return &note;
        }
    }
    return nullptr;
}

std::unique_ptr<Clip> MidiClip::clone() const {
    auto cloned = std::make_unique<MidiClip>(startTime, duration);
    cloned->name = name;
    cloned->colour = colour;
    cloned->selected = selected;
    cloned->muted = muted;
    cloned->loopEnabled = loopEnabled;
    cloned->notes_ = notes_;
    return cloned;
}

PatternClip::PatternClip(double startTime, double duration)
    : Clip(Type::Pattern, startTime, duration)
{
    colour = juce::Colour(0xffd94a6a);
    name = "Pattern";
}

void PatternClip::setPatternLength(double length) {
    patternLength = length;
}

void PatternClip::setLoopCount(int count) {
    loopCount = count;
}

std::unique_ptr<Clip> PatternClip::clone() const {
    auto cloned = std::make_unique<PatternClip>(startTime, duration);
    cloned->name = name;
    cloned->colour = colour;
    cloned->selected = selected;
    cloned->muted = muted;
    cloned->patternLength = patternLength;
    cloned->loopCount = loopCount;
    return cloned;
}

} // namespace vibedaw
