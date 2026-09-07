#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ProcessorBase.h"
#include "project/ChannelList.h"
#include <atomic>

namespace vibedaw {

class Channel;

class ChannelMixer : public juce::AudioProcessor, public ChannelList::Listener {
public:
    ChannelMixer(ChannelList& channelList);
    ~ChannelMixer() override;
    
    void setActiveChannel(int index);
    int getActiveChannel() const { return activeChannelIndex.load(); }
    
    const juce::String getName() const override { return "ChannelMixer"; }
    
    void prepareToPlay(double sampleRate, int blockSize) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    void releaseResources() override;
    
    double getTailLengthSeconds() const override { return 0.0; }
    
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
    
    void channelAdded(Channel* channel) override;
    void channelRemoved(int index) override;
    void channelChanged(Channel* channel) override;
    void channelListChanged() override;

private:
    ChannelList& channelList;
    std::atomic<int> activeChannelIndex{-1};
    double currentSampleRate{44100.0};
    int currentBlockSize{512};
    bool isPrepared{false};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelMixer)
};

} // namespace vibedaw
