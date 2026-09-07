#include "ChannelMixer.h"
#include "project/Channel.h"
#include "utils/Logger.h"

namespace vibedaw {

ChannelMixer::ChannelMixer(ChannelList& list)
    : channelList(list)
{
    channelList.addListener(this);
    LOG_INFO("ChannelMixer: Created");
}

ChannelMixer::~ChannelMixer() {
    channelList.removeListener(this);
    LOG_INFO("ChannelMixer: Destroyed");
}

void ChannelMixer::setActiveChannel(int index) {
    activeChannelIndex.store(index);
    LOG_INFO("ChannelMixer: Active channel set to " + juce::String(index));
}

void ChannelMixer::prepareToPlay(double sampleRate, int blockSize) {
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    isPrepared = true;
    
    LOG_INFO("ChannelMixer: Preparing - SR: " + juce::String(sampleRate) + " BS: " + juce::String(blockSize));
    
    for (int i = 0; i < channelList.getNumChannels(); ++i) {
        if (auto* channel = channelList.getChannel(i)) {
            channel->prepareToPlay(sampleRate, blockSize);
        }
    }
}

void ChannelMixer::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    buffer.clear();
    
    int activeIndex = activeChannelIndex.load();
    
    int numMidiEvents = midiMessages.getNumEvents();
    if (numMidiEvents > 0) {
        LOG_INFO("ChannelMixer: Received " + juce::String(numMidiEvents) + " MIDI events, active channel: " + juce::String(activeIndex));
    }
    
    juce::AudioBuffer<float> channelBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    
    for (int i = 0; i < channelList.getNumChannels(); ++i) {
        auto* channel = channelList.getChannel(i);
        if (!channel || channel->isMuted()) {
            continue;
        }
        
        channelBuffer.clear();
        
        juce::MidiBuffer channelMidi;
        if (i == activeIndex) {
            channelMidi = midiMessages;
        }
        
        channel->processBlock(channelBuffer, channelMidi);
        
        float maxLevel = 0.0f;
        for (int ch = 0; ch < channelBuffer.getNumChannels(); ++ch) {
            maxLevel = std::max(maxLevel, channelBuffer.getMagnitude(ch, 0, channelBuffer.getNumSamples()));
        }
        if (maxLevel > 0.001f) {
            LOG_INFO("ChannelMixer: Channel " + juce::String(i) + " producing audio, max level: " + juce::String(maxLevel, 4));
        }
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            buffer.addFrom(ch, 0, channelBuffer, ch % channelBuffer.getNumChannels(), 0, buffer.getNumSamples());
        }
    }
    
    midiMessages.clear();
}

void ChannelMixer::releaseResources() {
    isPrepared = false;
    LOG_INFO("ChannelMixer: Releasing resources");
    
    for (int i = 0; i < channelList.getNumChannels(); ++i) {
        if (auto* channel = channelList.getChannel(i)) {
            channel->releaseResources();
        }
    }
}

void ChannelMixer::channelAdded(Channel* channel) {
    if (isPrepared && channel) {
        LOG_INFO("ChannelMixer: Preparing newly added channel");
        channel->prepareToPlay(currentSampleRate, currentBlockSize);
    }
}

void ChannelMixer::channelRemoved(int index) {
    juce::ignoreUnused(index);
}

void ChannelMixer::channelChanged(Channel* channel) {
    juce::ignoreUnused(channel);
}

void ChannelMixer::channelListChanged() {
}

} // namespace vibedaw
