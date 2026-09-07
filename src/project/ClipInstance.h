#pragma once

#include <juce_core/juce_core.h>
#include <cstdint>

namespace vibedaw {

using ClipId = int;
using ChannelId = int;
using TrackId = int;

constexpr ClipId InvalidClipId = -1;
constexpr ChannelId InvalidChannelId = -1;
constexpr TrackId InvalidTrackId = -1;

class ClipInstance {
public:
    ClipInstance();
    ClipInstance(ClipId clipId, ChannelId channelId, double startTime, double duration = 1.0);
    ~ClipInstance() = default;
    
    ClipId getClipId() const { return clipId_; }
    void setClipId(ClipId id) { clipId_ = id; }
    
    ChannelId getChannelId() const { return channelId_; }
    void setChannelId(ChannelId id) { channelId_ = id; }
    
    double getStartTime() const { return startTime_; }
    void setStartTime(double time) { startTime_ = time; }
    
    double getDuration() const { return duration_; }
    void setDuration(double duration) { duration_ = duration; }
    
    double getEndTime() const { return startTime_ + duration_; }
    
    bool isValid() const { return clipId_ != InvalidClipId; }
    
    bool isSelected() const { return selected_; }
    void setSelected(bool selected) { selected_ = selected; }
    
    bool isMuted() const { return muted_; }
    void setMuted(bool muted) { muted_ = muted; }
    
    bool containsTime(double time) const;
    bool overlapsRange(double start, double end) const;
    
private:
    ClipId clipId_ = InvalidClipId;
    ChannelId channelId_ = InvalidChannelId;
    double startTime_ = 0.0;
    double duration_ = 1.0;
    bool selected_ = false;
    bool muted_ = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipInstance)
};

} // namespace vibedaw