#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Channel.h"
#include "core/MixerState.h"
#include <vector>

namespace vibedaw {

// Collection mutations/subscriptions are message-thread-only. channelChanged is
// forwarded from Channel's coalesced message-thread callback, never from audio.
class ChannelList : private juce::ChangeListener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void channelAdded(Channel* channel) = 0;
        virtual void channelRemoved(int index) = 0;
        virtual void channelChanged(Channel* channel) = 0;
        virtual void channelListChanged() = 0;
        virtual void mixerChannelsChanged() {}
        virtual void mixerChannelChanged(MixerChannel*) {}
    };
    
    ChannelList();
    static constexpr int maxChannels = 128;
    static constexpr int maxMixerChannels = 128;
    ~ChannelList();
    
    Channel* addChannel(const juce::String& name = {}, Channel::Type type = Channel::Type::Instrument);
    // Project-load restore: creates a channel with an exact saved ID (T07).
    // Validates id >= 0, uniqueness and capacity; advances the ID counter past
    // the restored ID. Message thread; caller holds any required quiescence.
    Channel* restoreChannel(ChannelId id, const juce::String& name, Channel::Type type);
    void removeChannel(int index);
    void clearChannels();
    
    int getNumChannels() const { return static_cast<int>(channels.size()); }
    Channel* getChannel(int index) const;
    Channel* getChannelById(ChannelId id) const;
    const std::vector<std::unique_ptr<Channel>>& getChannels() const { return channels; }
    MasterBus& getMasterBus() { return masterBus; }
    const MasterBus& getMasterBus() const { return masterBus; }

    int getNumMixerChannels() const { return static_cast<int>(mixerChannels.size()); }
    const std::vector<std::unique_ptr<MixerChannel>>& getMixerChannels() const { return mixerChannels; }
    MixerChannel* getMixerChannelById(MixerChannelId id) const;
    MixerChannel* addMixerChannel(const juce::String& name = {});
    // Exact-ID restore only; ordinary additions never reuse removed IDs.
    MixerChannel* restoreMixerChannel(MixerChannelId id, const juce::String& name);
    void removeMixerChannel(MixerChannelId id);
    void clearMixerChannels();
    bool setChannelMixerDestination(ChannelId channelId, int destination);
    
    void moveChannel(int fromIndex, int toIndex);
    
    int indexOfChannel(const Channel* channel) const;
    
    void addListener(Listener* listener);
    void removeListener(Listener* listener);
    
private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    ChannelId nextId_ = 0;
    MixerChannelId nextMixerId_ = 0;
    MasterBus masterBus;
    std::vector<std::unique_ptr<Channel>> channels;
    std::vector<std::unique_ptr<MixerChannel>> mixerChannels;
    juce::ListenerList<Listener> listeners;
    
    void notifyChannelAdded(Channel* channel);
    void notifyChannelRemoved(int index);
    void notifyChannelChanged(Channel* channel);
    void notifyChannelListChanged();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelList)
};

} // namespace vibedaw
