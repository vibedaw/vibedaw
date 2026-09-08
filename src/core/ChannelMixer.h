#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "ProcessorBase.h"
#include "project/ChannelList.h"
#include <atomic>
#include "ArrangementSnapshot.h"
#include "TransportState.h"
#include "Metronome.h"

namespace vibedaw {

class Channel;

class ChannelMixer : public juce::AudioProcessor, public ChannelList::Listener {
public:
    ChannelMixer(ChannelList&, TrackList&, ClipPool&, TransportState&);
    ~ChannelMixer() override;
    
    void setActiveChannel(int index);
    ChannelId getActiveChannelId() const { return activeChannelId.load(); }
    
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
    unsigned getOverflowCount() const { return overflowCount.load(); }
    bool arrangementOverflowed() const { return arrangement.hasOverflow(); }

private:
    ChannelList& channelList;
    ArrangementPublisher arrangement;
    TransportState& transport;
    TransportClock clock;
    Metronome metronome;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer channelMidi;
    struct ScheduledEvent {
        // size == -1 is an internal arrangement-only wrap barrier, never plugin MIDI.
        int sample = 0, size = 0;
        unsigned token = 0;
        unsigned char data[3]{};
        unsigned order = 0;
        int previousAttack = -1;
    };
    std::array<ScheduledEvent, Channel::maxLiveEvents> scheduled{};
    std::array<ScheduledEvent, Channel::maxLiveEvents> merged{};
    // Snapshot-relative indices only; reset on revision/discontinuity, never borrowed pointers.
    std::array<size_t, ChannelList::maxChannels> nextArrangementEvent{};
    ChannelId previousActive = InvalidChannelId;
    unsigned lastRevision = 0;
    std::atomic<unsigned> overflowCount{0};
    std::atomic<ChannelId> activeChannelId{InvalidChannelId};
    double currentSampleRate{44100.0};
    int currentBlockSize{512};
    bool isPrepared{false};
    bool clockInterrupted = true; // Lifecycle writes require quiescence, like preparation.
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelMixer)
};

} // namespace vibedaw
