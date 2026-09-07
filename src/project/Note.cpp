#include "Note.h"

namespace vibedaw {

Note::Note()
    : pitch_(60)
    , startTime_(0.0)
    , duration_(1.0)
    , velocity_(100)
    , channel_(1)
{
}

Note::Note(int pitch, double startTime, double duration, int velocity)
    : pitch_(juce::jlimit(minPitch, maxPitch, pitch))
    , startTime_(startTime)
    , duration_(juce::jmax(0.0, duration))
    , velocity_(juce::jlimit(minVelocity, maxVelocity, velocity))
    , channel_(1)
{
}

void Note::setPitch(int pitch) {
    pitch_ = juce::jlimit(minPitch, maxPitch, pitch);
}

void Note::setStartTime(double time) {
    startTime_ = time;
}

void Note::setDuration(double duration) {
    duration_ = juce::jmax(0.0, duration);
}

void Note::setVelocity(int velocity) {
    velocity_ = juce::jlimit(minVelocity, maxVelocity, velocity);
}

void Note::setChannel(int channel) {
    channel_ = juce::jlimit(1, 16, channel);
}

bool Note::overlaps(const Note& other) const {
    if (pitch_ != other.pitch_) return false;
    return startTime_ < other.getEndTime() && getEndTime() > other.startTime_;
}

bool Note::containsTime(double time) const {
    return time >= startTime_ && time < getEndTime();
}

bool Note::containsPitch(int pitch) const {
    return pitch == pitch_;
}

bool Note::compareByTime(const Note& a, const Note& b) {
    return a.startTime_ < b.startTime_;
}

bool Note::compareByPitch(const Note& a, const Note& b) {
    return a.pitch_ < b.pitch_;
}

} // namespace vibedaw