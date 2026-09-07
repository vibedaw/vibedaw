#pragma once

#include <juce_core/juce_core.h>

namespace vibedaw {

class Note {
public:
    Note();
    Note(int pitch, double startTime, double duration, int velocity = 100);
    ~Note() = default;
    
    int getPitch() const { return pitch_; }
    void setPitch(int pitch);
    
    double getStartTime() const { return startTime_; }
    void setStartTime(double time);
    
    double getDuration() const { return duration_; }
    void setDuration(double duration);
    
    double getEndTime() const { return startTime_ + duration_; }
    
    int getVelocity() const { return velocity_; }
    void setVelocity(int velocity);
    
    bool isSelected() const { return selected_; }
    void setSelected(bool selected) { selected_ = selected; }
    
    bool isMuted() const { return muted_; }
    void setMuted(bool muted) { muted_ = muted; }
    
    int getChannel() const { return channel_; }
    void setChannel(int channel);
    
    bool overlaps(const Note& other) const;
    bool containsTime(double time) const;
    bool containsPitch(int pitch) const;
    
    static bool compareByTime(const Note& a, const Note& b);
    static bool compareByPitch(const Note& a, const Note& b);
    
    static constexpr int minPitch = 0;
    static constexpr int maxPitch = 127;
    static constexpr int minVelocity = 0;
    static constexpr int maxVelocity = 127;
    
Note(const Note&) = default;
    Note& operator=(const Note&) = default;
    Note(Note&&) = default;
    Note& operator=(Note&&) = default;
    
private:
    int pitch_ = 60;
    double startTime_ = 0.0;
    double duration_ = 1.0;
    int velocity_ = 100;
    int channel_ = 1;
    bool selected_ = false;
    bool muted_ = false;
};

} // namespace vibedaw