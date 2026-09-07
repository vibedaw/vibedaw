#include "ClipPool.h"
#include "utils/Logger.h"

namespace vibedaw {

ClipPool::ClipPool() {
    LOG_INFO("ClipPool: Created");
}

ClipPool::~ClipPool() {
    clearClips();
    LOG_INFO("ClipPool: Destroyed");
}

ClipId ClipPool::addClip(std::unique_ptr<Clip> clip) {
    if (!clip) {
        return InvalidClipId;
    }
    
    ClipId id = nextId_++;
    clips_.emplace_back(id, std::move(clip));
    
    Clip* ptr = clips_.back().second.get();
    notifyClipAdded(id, ptr);
    LOG_INFO("ClipPool: Added clip with ID " + juce::String(id));
    
    return id;
}

void ClipPool::removeClip(ClipId clipId) {
    auto it = std::find_if(clips_.begin(), clips_.end(),
        [clipId](const auto& pair) { return pair.first == clipId; });
    
    if (it != clips_.end()) {
        clips_.erase(it);
        notifyClipRemoved(clipId);
        LOG_INFO("ClipPool: Removed clip with ID " + juce::String(clipId));
    }
}

void ClipPool::clearClips() {
    clips_.clear();
    nextId_ = 0;
    LOG_INFO("ClipPool: Cleared all clips");
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