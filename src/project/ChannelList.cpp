#include "ChannelList.h"
#include "utils/Logger.h"

namespace vibedaw {

ChannelList::ChannelList() {
    LOG_INFO("ChannelList: Created");
}

ChannelList::~ChannelList() {
    clearChannels();
    LOG_INFO("ChannelList: Destroyed");
}

Channel* ChannelList::addChannel(const juce::String& name, Channel::Type type) {
    int index = static_cast<int>(channels.size());
    juce::String channelName = name.isNotEmpty() ? name : "Channel " + juce::String(index + 1);
    
    auto channel = std::make_unique<Channel>(channelName, type);
    auto* ptr = channel.get();
    channels.push_back(std::move(channel));
    
    notifyChannelAdded(ptr);
    LOG_INFO("ChannelList: Added channel '" + channelName + "' at index " + juce::String(index));
    
    return ptr;
}

void ChannelList::removeChannel(int index) {
    if (index >= 0 && index < static_cast<int>(channels.size())) {
        LOG_INFO("ChannelList: Removing channel at index " + juce::String(index));
        channels.erase(channels.begin() + index);
        notifyChannelRemoved(index);
    }
}

void ChannelList::clearChannels() {
    channels.clear();
    notifyChannelListChanged();
    LOG_INFO("ChannelList: Cleared all channels");
}

Channel* ChannelList::getChannel(int index) const {
    if (index >= 0 && index < static_cast<int>(channels.size())) {
        return channels[index].get();
    }
    return nullptr;
}

void ChannelList::moveChannel(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= static_cast<int>(channels.size()) ||
        toIndex < 0 || toIndex >= static_cast<int>(channels.size())) {
        return;
    }
    
    if (fromIndex == toIndex) {
        return;
    }
    
    auto channel = std::move(channels[fromIndex]);
    channels.erase(channels.begin() + fromIndex);
    channels.insert(channels.begin() + toIndex, std::move(channel));
    
    notifyChannelListChanged();
    LOG_INFO("ChannelList: Moved channel from " + juce::String(fromIndex) + " to " + juce::String(toIndex));
}

int ChannelList::indexOfChannel(const Channel* channel) const {
    for (int i = 0; i < static_cast<int>(channels.size()); ++i) {
        if (channels[i].get() == channel) {
            return i;
        }
    }
    return -1;
}

void ChannelList::addListener(Listener* listener) {
    listeners.add(listener);
}

void ChannelList::removeListener(Listener* listener) {
    listeners.remove(listener);
}

void ChannelList::notifyChannelAdded(Channel* channel) {
    listeners.call([channel](Listener& l) { l.channelAdded(channel); });
}

void ChannelList::notifyChannelRemoved(int index) {
    listeners.call([index](Listener& l) { l.channelRemoved(index); });
}

void ChannelList::notifyChannelChanged(Channel* channel) {
    listeners.call([channel](Listener& l) { l.channelChanged(channel); });
}

void ChannelList::notifyChannelListChanged() {
    listeners.call([](Listener& l) { l.channelListChanged(); });
}

} // namespace vibedaw