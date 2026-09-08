#include "Clip.h"
#include "Note.h"
#include <algorithm>
#include <cmath>

namespace vibedaw {

Clip::Clip(Type type, double start, double dur)
    : clipType(type)
    , startTime(start)
    , duration(dur)
{
}

void Clip::setStartTime(double time) {
    if (!std::isfinite(time) || time < 0.0 || !std::isfinite(time + duration)) return;
    startTime = time;
    notifyChanged();
}

void Clip::setDuration(double dur) {
    if (!std::isfinite(dur) || dur <= 0.0 || !std::isfinite(startTime + dur)) return;
    duration = dur;
    notifyChanged();
}

void Clip::setName(const juce::String& n) {
    name = n;
    notifyChanged();
}

void Clip::setColour(const juce::Colour& c) {
    colour = c;
    notifyChanged();
}

void Clip::setSelected(bool sel) {
    selected = sel;
}

void Clip::setMuted(bool m) {
    muted = m;
    notifyChanged();
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

const Note* MidiClip::addNote(const Note& note) {
    if (!std::isfinite(note.getStartTime()) || note.getStartTime() < 0.0 ||
        !std::isfinite(note.getDuration()) || note.getDuration() <= 0.0 || !std::isfinite(note.getEndTime())) return nullptr;
    invalidateNotes();
    notes_.push_back(note);
    notifyChanged();
    return &notes_.back();
}

void MidiClip::updateNote(const Note* note, const Note& replacement) {
    if (!std::isfinite(replacement.getStartTime()) || replacement.getStartTime() < 0.0 ||
        !std::isfinite(replacement.getDuration()) || replacement.getDuration() <= 0.0 || !std::isfinite(replacement.getEndTime())) return;
    for (auto& current : notes_) {
        if (&current == note) {
            current = replacement;
            notifyChanged();
            return;
        }
    }
}

void MidiClip::removeNote(int index) {
    if (index >= 0 && index < static_cast<int>(notes_.size())) {
        invalidateNotes();
        notes_.erase(notes_.begin() + index);
        notifyChanged();
    }
}

void MidiClip::removeNote(const Note* note) {
    if (note == nullptr) return;
    auto it = std::find_if(notes_.begin(), notes_.end(),
        [note](const Note& n) { return &n == note; });
    if (it != notes_.end()) {
        invalidateNotes();
        notes_.erase(it);
        notifyChanged();
    }
}

void MidiClip::clearNotes() {
    invalidateNotes();
    notes_.clear();
    notifyChanged();
}

const Note* MidiClip::findNoteAt(double time, int pitch) const {
    for (const auto& note : notes_) {
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
