#pragma once

#include <juce_core/juce_core.h>
#include <cstdint>
#include <cmath>
#include <functional>

namespace vibedaw {

using ClipId = int;
using ChannelId = int;
using TrackId = int;

constexpr ClipId InvalidClipId = -1;
constexpr ChannelId InvalidChannelId = -1;
constexpr TrackId InvalidTrackId = -1;

class ClipInstance {
public:
    // Quarter-note beats. Source beat zero maps to startTime. Play once, truncate
    // at min(source length, placement length); a longer placement ends in silence.
    ClipInstance();
    // An explicit id restores a saved instance identity across sessions (T07/T12).
    ClipInstance(ClipId clipId, ChannelId channelId, double startTime, double duration = 1.0,
                 const juce::String& id = {});
    ~ClipInstance() = default;
    const juce::String& getId() const { return id_; }
    
    ClipId getClipId() const { return clipId_; }
    void setClipId(ClipId id) { clipId_ = id; changed(); }
    
    ChannelId getChannelId() const { return channelId_; }
    void setChannelId(ChannelId id) { channelId_ = id; changed(); }
    
    double getStartTime() const { return startTime_; }
    void setStartTime(double time) {
        if (std::isfinite(time) && time >= 0.0 && std::isfinite(time + duration_)) {
            startTime_ = time; changed();
        }
    }
    
    double getDuration() const { return duration_; }
    void setDuration(double duration) {
        if (std::isfinite(duration) && duration > 0.0 && std::isfinite(startTime_ + duration)) {
            duration_ = duration; changed();
        }
    }
    
    double getEndTime() const { return startTime_ + duration_; }
    
    bool isValid() const {
        return clipId_ >= 0 && std::isfinite(startTime_) && startTime_ >= 0.0 &&
               std::isfinite(duration_) && duration_ > 0.0 && std::isfinite(getEndTime());
    }
    
    bool isSelected() const { return selected_; }
    void setSelected(bool selected) { selected_ = selected; changed(); }
    
    bool isMuted() const { return muted_; }
    void setMuted(bool muted) { muted_ = muted; changed(); }
    
    bool containsTime(double time) const;
    bool overlapsRange(double start, double end) const;
    
private:
    friend class Track;
    friend class TrackList;
    const juce::String id_ = juce::Uuid().toString();
    std::function<void()> onChange_;
    void changed() { if (onChange_) onChange_(); }
    ClipId clipId_ = InvalidClipId;
    ChannelId channelId_ = InvalidChannelId;
    double startTime_ = 0.0;
    double duration_ = 1.0;
    bool selected_ = false;
    bool muted_ = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipInstance)
};

} // namespace vibedaw
