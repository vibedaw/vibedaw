#include "ClipPool.h"
#include "utils/Logger.h"
#include <cmath>
#include <limits>

namespace vibedaw {

ClipPool::ClipPool() {
    LOG_INFO("ClipPool: Created");
}

ClipPool::~ClipPool() {
    clearClips();
    LOG_INFO("ClipPool: Destroyed");
}

ClipId ClipPool::addClip(std::unique_ptr<Clip> clip) {
    if (nextId_ == std::numeric_limits<ClipId>::max()) return InvalidClipId;
    if (!clip || !std::isfinite(clip->getStartTime()) || clip->getStartTime() < 0.0 ||
        !std::isfinite(clip->getDuration()) || clip->getDuration() <= 0.0 || !std::isfinite(clip->getEndTime())) {
        return InvalidClipId;
    }
    
    ClipId id = nextId_++;
    clips_.emplace_back(id, std::move(clip));
    
    Clip* ptr = clips_.back().second.get();
    ptr->addListener(this);
    notifyClipAdded(id, ptr);
    LOG_INFO("ClipPool: Added clip with ID " + juce::String(id));
    
    return id;
}

ClipId ClipPool::restoreClip(ClipId clipId, std::unique_ptr<Clip> clip) {
    if (clipId < 0 || clipId == std::numeric_limits<ClipId>::max()) return InvalidClipId;
    if (!clip || !std::isfinite(clip->getStartTime()) || clip->getStartTime() < 0.0 ||
        !std::isfinite(clip->getDuration()) || clip->getDuration() <= 0.0 || !std::isfinite(clip->getEndTime())) {
        return InvalidClipId;
    }
    if (getClip(clipId) != nullptr) return InvalidClipId;
    if (nextId_ <= clipId) nextId_ = static_cast<ClipId>(clipId + 1);

    clips_.emplace_back(clipId, std::move(clip));
    Clip* ptr = clips_.back().second.get();
    ptr->addListener(this);
    notifyClipAdded(clipId, ptr);
    LOG_INFO("ClipPool: Restored clip with ID " + juce::String(clipId));
    return clipId;
}

void ClipPool::removeClip(ClipId clipId) {
    auto it = std::find_if(clips_.begin(), clips_.end(),
        [clipId](const auto& pair) { return pair.first == clipId; });
    
    if (it != clips_.end()) {
        listeners_.call([clipId](Listener& l) { l.clipWillBeRemoved(clipId); });
        it->second->removeListener(this);
        clips_.erase(it);
        notifyClipRemoved(clipId);
        LOG_INFO("ClipPool: Removed clip with ID " + juce::String(clipId));
    }
}

void ClipPool::clearClips() {
    while (!clips_.empty()) removeClip(clips_.back().first);
    LOG_INFO("ClipPool: Cleared all clips");
}

void ClipPool::clipChanged() {
    // Small M1 pool: conservatively invalidate all sources on a source edit.
    for (const auto& entry : clips_) notifyClipChanged(entry.first, entry.second.get());
}

Clip* ClipPool::getClip(ClipId clipId) const {
    auto it = std::find_if(clips_.begin(), clips_.end(),
        [clipId](const auto& pair) { return pair.first == clipId; });
    
    if (it != clips_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void ClipPool::addListener(Listener* listener) {
    listeners_.add(listener);
}

void ClipPool::removeListener(Listener* listener) {
    listeners_.remove(listener);
}

void ClipPool::notifyClipAdded(ClipId id, Clip* clip) {
    listeners_.call([id, clip](Listener& l) { l.clipAdded(id, clip); });
}

void ClipPool::notifyClipRemoved(ClipId id) {
    listeners_.call([id](Listener& l) { l.clipRemoved(id); });
}

void ClipPool::notifyClipChanged(ClipId id, Clip* clip) {
    listeners_.call([id, clip](Listener& l) { l.clipChanged(id, clip); });
}

} // namespace vibedaw
