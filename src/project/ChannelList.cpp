#include "ChannelList.h"
#include "utils/Logger.h"
#include <limits>
#include "core/AudioBoundary.h"

namespace vibedaw {

ChannelList::ChannelList() {
    addMixerChannel();
    LOG_INFO("ChannelList: Created");
}

ChannelList::~ChannelList() {
    clearChannels();
    clearMixerChannels();
    LOG_INFO("ChannelList: Destroyed");
}

Channel* ChannelList::addChannel(const juce::String& name, Channel::Type type) {
    AudioQuiescence::Edit edit;
    if (getNumChannels() >= maxChannels) return nullptr;
    if (nextId_ == std::numeric_limits<ChannelId>::max()) return nullptr;
    int index = static_cast<int>(channels.size());
    juce::String channelName = name.isNotEmpty() ? name : "Channel " + juce::String(index + 1);
    
    auto channel = std::make_unique<Channel>(channelName, type, nextId_++);
    auto* ptr = channel.get();
    channels.push_back(std::move(channel));
    ptr->addChangeListener(this);
    
    notifyChannelAdded(ptr);
    LOG_INFO("ChannelList: Added channel '" + channelName + "' at index " + juce::String(index));
    
    return ptr;
}

Channel* ChannelList::restoreChannel(ChannelId id, const juce::String& name, Channel::Type type) {
    AudioQuiescence::Edit edit;
    if (id < 0 || id == std::numeric_limits<ChannelId>::max()) return nullptr;
    if (getNumChannels() >= maxChannels) return nullptr;
    if (getChannelById(id) != nullptr) return nullptr;
    if (nextId_ <= id) nextId_ = static_cast<ChannelId>(id + 1);
    int index = static_cast<int>(channels.size());
    juce::String channelName = name.isNotEmpty() ? name : "Channel " + juce::String(index + 1);

    auto channel = std::make_unique<Channel>(channelName, type, id);
    auto* ptr = channel.get();
    channels.push_back(std::move(channel));
    ptr->addChangeListener(this);

    notifyChannelAdded(ptr);
    LOG_INFO("ChannelList: Restored channel '" + channelName + "' with ID " + juce::String(id));

    return ptr;
}

void ChannelList::removeChannel(int index) {
    AudioQuiescence::Edit edit;
    if (index >= 0 && index < static_cast<int>(channels.size())) {
        LOG_INFO("ChannelList: Removing channel at index " + juce::String(index));
        channels[index]->removeChangeListener(this);
        channels.erase(channels.begin() + index);
        notifyChannelRemoved(index);
    }
}

void ChannelList::clearChannels() {
    AudioQuiescence::Edit edit;
    for (const auto& channel : channels) channel->removeChangeListener(this);
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

Channel* ChannelList::getChannelById(ChannelId id) const {
    for (const auto& channel : channels)
        if (channel->getId() == id) return channel.get();
    return nullptr;
}

MixerChannel* ChannelList::getMixerChannelById(MixerChannelId id) const {
    for (const auto& channel : mixerChannels)
        if (channel->getId() == id) return channel.get();
    return nullptr;
}

MixerChannel* ChannelList::addMixerChannel(const juce::String& name) {
    AudioQuiescence::Edit edit(AudioQuiescence::Interruption::PreserveVoices);
    return restoreMixerChannel(nextMixerId_, name.isNotEmpty() ? name : "Mixer " + juce::String(getNumMixerChannels() + 1));
}

MixerChannel* ChannelList::restoreMixerChannel(MixerChannelId id, const juce::String& name) {
    AudioQuiescence::Edit edit(AudioQuiescence::Interruption::PreserveVoices);
    if (id < 0 || id == std::numeric_limits<MixerChannelId>::max() ||
        getNumMixerChannels() >= maxMixerChannels || getMixerChannelById(id)) return nullptr;
    if (nextMixerId_ <= id) nextMixerId_ = id + 1;
    auto channel = std::make_unique<MixerChannel>(id, name);
    auto* ptr = channel.get();
    mixerChannels.push_back(std::move(channel));
    ptr->addChangeListener(this);
    listeners.call([](Listener& l) { l.mixerChannelsChanged(); });
    return ptr;
}

void ChannelList::removeMixerChannel(MixerChannelId id) {
    AudioQuiescence::Edit edit(AudioQuiescence::Interruption::PreserveVoices);
    for (auto it = mixerChannels.begin(); it != mixerChannels.end(); ++it) {
        if ((*it)->getId() != id) continue;
        for (const auto& channel : channels)
            if (channel->getMixerTrackId() == id) channel->setMixerTrackId(MasterDestination);
        (*it)->removeChangeListener(this);
        mixerChannels.erase(it);
        listeners.call([](Listener& l) { l.mixerChannelsChanged(); });
        return;
    }
}

void ChannelList::clearMixerChannels() {
    AudioQuiescence::Edit edit(AudioQuiescence::Interruption::PreserveVoices);
    for (const auto& channel : channels)
        if (channel->getMixerTrackId() != MasterDestination) channel->setMixerTrackId(MasterDestination);
    for (const auto& channel : mixerChannels) channel->removeChangeListener(this);
    mixerChannels.clear();
    listeners.call([](Listener& l) { l.mixerChannelsChanged(); });
}

bool ChannelList::setChannelMixerDestination(ChannelId channelId, int destination) {
    AudioQuiescence::Edit edit(AudioQuiescence::Interruption::PreserveVoices);
    auto* channel = getChannelById(channelId);
    if (!channel || (destination != MasterDestination && !getMixerChannelById(destination))) return false;
    channel->setMixerTrackId(destination);
    return true;
}

void ChannelList::moveChannel(int fromIndex, int toIndex) {
    AudioQuiescence::Edit edit;
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

void ChannelList::changeListenerCallback(juce::ChangeBroadcaster* source) {
    for (const auto& channel : channels) {
        if (source == channel.get()) {
            notifyChannelChanged(channel.get());
            return;
        }
    }
    for (const auto& channel : mixerChannels) {
        if (source == channel.get()) {
            listeners.call([ptr = channel.get()](Listener& l) { l.mixerChannelChanged(ptr); });
            return;
        }
    }
}

void ChannelList::notifyChannelChanged(Channel* channel) {
    listeners.call([channel](Listener& l) { l.channelChanged(channel); });
}

void ChannelList::notifyChannelListChanged() {
    listeners.call([](Listener& l) { l.channelListChanged(); });
}

} // namespace vibedaw
