#pragma once

#include <juce_core/juce_core.h>
#include "Clip.h"
#include "ClipInstance.h"
#include <vector>
#include <functional>

namespace vibedaw {

class ClipPool : private Clip::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void clipAdded(ClipId clipId, Clip* clip) = 0;
        virtual void clipRemoved(ClipId clipId) = 0;
        virtual void clipChanged(ClipId clipId, Clip* clip) = 0;
        virtual void clipWillBeRemoved(ClipId) {}
    };
    
    ClipPool();
    ~ClipPool();
    
    ClipId addClip(std::unique_ptr<Clip> clip);
    void removeClip(ClipId clipId);
    void clearClips();
    
    Clip* getClip(ClipId clipId) const;
    const std::vector<std::pair<ClipId, std::unique_ptr<Clip>>>& getClips() const { return clips_; }
    
    int getNumClips() const { return static_cast<int>(clips_.size()); }
    
    void addListener(Listener* listener);
    void removeListener(Listener* listener);
    
private:
    void clipChanged() override;
    std::vector<std::pair<ClipId, std::unique_ptr<Clip>>> clips_;
    ClipId nextId_ = 0;
    juce::ListenerList<Listener> listeners_;
    
    void notifyClipAdded(ClipId id, Clip* clip);
    void notifyClipRemoved(ClipId id);
    void notifyClipChanged(ClipId id, Clip* clip);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClipPool)
};

} // namespace vibedaw
