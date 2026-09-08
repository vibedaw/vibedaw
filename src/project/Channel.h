#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "core/ProcessorBase.h"
#include "ClipInstance.h"
#include <atomic>
#include <vector>
#include "core/AudioBoundary.h"
#include "core/MixerState.h"

namespace vibedaw {

class PluginHost;

class Channel : public ProcessorBase, public juce::ChangeBroadcaster, private juce::Timer {
public:
    enum class Type {
        Instrument,  // VST/AU plugin
        Sampler      // Audio sample player
    };
    
    Channel(const juce::String& name = "Channel", Type type = Type::Instrument, ChannelId id = InvalidChannelId);
    ~Channel() override;
    
    Type getType() const { return channelType; }
    ChannelId getId() const { return id_; }
    // Model setters are message-thread-only. Change notifications are asynchronous
    // and coalesced on that thread; audio processing and meter updates never notify.
    void setName(const juce::String& newName);
    
    void setPlugin(std::unique_ptr<PluginHost> pluginHost);
    PluginHost* getPlugin() const { return plugin.get(); }
    bool hasPlugin() const { return plugin != nullptr; }
    
    void setSampleFile(const juce::File& file);
    const juce::File& getSampleFile() const { return sampleFile; }
    bool hasSample() const { return sampleFile.exists(); }
    
    void setMixerTrackId(int id) { mixerTrackId = id; notifyChanged(); }
    int getMixerTrackId() const { return mixerTrackId; }
    
    void setVolume(float newVolume);
    float getVolume() const { return volume; }
    
    void setPan(float newPan);
    float getPan() const { return pan; }
    
    void setMuted(bool muted);
    bool isMuted() const { return muted; }
    void setSolo(bool value);
    bool isSolo() const { return solo; }
    
    void setColour(const juce::Colour& colour);
    juce::Colour getColour() const { return colour; }
    
    float getLeftLevel() const { return meter.getLeft(); }
    float getRightLevel() const { return meter.getRight(); }
    const StereoMeter& getMeter() const { return meter; }
    
    void prepareToPlay(double sampleRate, int blockSize) override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;
    void releaseResources() override;
    const juce::String getName() const override { return name; }
    
    bool isPrepared() const { return isPrepared_; }
    // The VST3 adapter counts every input message, including ignored controllers.
    static constexpr unsigned maxDeliveredNotes = 1024;
    static constexpr unsigned cleanupControllers = 48;
    // T02 shares this normal-input budget with arrangement events (including
    // collision releases); cleanup space is never borrowed for dense playback.
    static constexpr unsigned maxLiveEvents = 2048 - maxDeliveredNotes - cleanupControllers;
    bool canDeliverMidi(const juce::MidiBuffer&) const;
    void appendNoteCleanup(juce::MidiBuffer&);
    bool needsVoiceReset() const { return sustainSeen && voicesSinceReset; } // Render thread only.
    void requestVoiceReset() noexcept { resetPending.store(true); }
    bool isVoiceResetPending() const noexcept { return resetPending.load(); }

private:
    friend class ChannelMixer;
    friend struct ChannelTestAccess;
    void timerCallback() override { servicePendingVoiceReset(); }
    void servicePendingVoiceReset();
    std::array<unsigned short, 16 * 128> deliveredNotes{};
    std::array<unsigned, 16 * 128> arrangementVoices{}; // Render tokens, zero means no delivered attack.
    unsigned deliveredCount = 0;
    bool sustainSeen = false;
    bool voicesSinceReset = false;
    std::atomic<bool> resetPending{false};
    const ChannelId id_; // Assigned by ChannelList, never an index.
    void processWithControls(juce::AudioBuffer<float>&, juce::MidiBuffer&, float gain, float balance);
    void notifyChanged();
    
    juce::String name;
    Type channelType;
    
    std::unique_ptr<PluginHost> plugin;
    juce::File sampleFile;
    
    int mixerTrackId = -1;
    float volume = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool solo = false;
    bool wasSuppressed = false; // Render-owned, independent of arrangement tokens.
    bool arrangementSinceReset = false; // Includes released notes potentially held by pedals.
    struct Controls { float volume = 1.0f, pan = 0.0f; bool muted = false, solo = false; };
    LatestState<Controls> controls;
    juce::Colour colour{0xff6a6aff};
    
    StereoMeter meter;
    
    bool isPrepared_ = false;
    double preparedSampleRate{0.0};
    int preparedBlockSize{0};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Channel)
};

} // namespace vibedaw
