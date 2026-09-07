#include "ClipInstance.h"

namespace vibedaw {

ClipInstance::ClipInstance() = default;

ClipInstance::ClipInstance(ClipId clipId, ChannelId channelId, double startTime, double duration)
    : clipId_(clipId)
    , channelId_(channelId)
    , startTime_(startTime)
    , duration_(duration)
{
}

bool ClipInstance::containsTime(double time) const {
    return time >= startTime_ && time < getEndTime();
}

bool ClipInstance::overlapsRange(double start, double end) const {
    return startTime_ < end && getEndTime() > start;
}

} // namespace vibedaw