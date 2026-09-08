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
    };
    
    ChannelList();
    static constexpr int maxChannels = 128;
    ~ChannelList();
    
    Channel* addChannel(const juce::String& name = {}, Channel::Type type = Channel::Type::Instrument);
    void removeChannel(int index);
    void clearChannels();
    
    int getNumChannels() const { return static_cast<int>(channels.size()); }
    Channel* getChannel(int index) const;
    Channel* getChannelById(ChannelId id) const;
    const std::vector<std::unique_ptr<Channel>>& getChannels() const { return channels; }
    MasterBus& getMasterBus() { return masterBus; }
    
    void moveChannel(int fromIndex, int toIndex);
    
    int indexOfChannel(const Channel* channel) const;
    
    void addListener(Listener* listener);
    void removeListener(Listener* listener);
    
private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    ChannelId nextId_ = 0;
    MasterBus masterBus;
    std::vector<std::unique_ptr<Channel>> channels;
    juce::ListenerList<Listener> listeners;
    
    void notifyChannelAdded(Channel* channel);
    void notifyChannelRemoved(int index);
    void notifyChannelChanged(Channel* channel);
    void notifyChannelListChanged();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelList)
};

} // namespace vibedaw
