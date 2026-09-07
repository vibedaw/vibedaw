#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "core/ProcessorBase.h"
#include <atomic>
#include <vector>

namespace vibedaw {

class PluginHost;

class Channel : public ProcessorBase {
public:
    enum class Type {
        Instrument,  // VST/AU plugin
        Sampler      // Audio sample player
    };
    
    Channel(const juce::String& name = "Channel", Type type = Type::Instrument);
    ~Channel() override;
    
    Type getType() const { return channelType; }
    
    void setPlugin(std::unique_ptr<PluginHost> pluginHost);
    PluginHost* getPlugin() const { return plugin.get(); }
    bool hasPlugin() const { return plugin != nullptr; }
    
    void setSampleFile(const juce::File& file);
    const juce::File& getSampleFile() const { return sampleFile; }
    bool hasSample() const { return sampleFile.exists(); }
    
    void setMixerTrackId(int id) { mixerTrackId = id; }
    int getMixerTrackId() const { return mixerTrackId; }
    
    void setVolume(float newVolume);
    float getVolume() const { return volume; }
    
    void setPan(float newPan);
    float getPan() const { return pan; }
    
    void setMuted(bool muted);
    bool isMuted() const { return muted; }
    
    void setColour(const juce::Colour& colour);
    juce::Colour getColour() const { return colour; }
    
    float getLeftLevel() const { return leftLevel.load(); }
    float getRightLevel() const { return rightLevel.load(); }
    
    void prepareToPlay(double sampleRate, int blockSize) override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;
    void releaseResources() override;
    const juce::String getName() const override { return name; }
    
    bool isPrepared() const { return isPrepared_; }

private:
    void updateLevels(const juce::AudioBuffer<float>& audio);
    
    juce::String name;
    Type channelType;
    
    std::unique_ptr<PluginHost> plugin;
    juce::File sampleFile;
    
    int mixerTrackId = -1;
    float volume = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    juce::Colour colour{0xff6a6aff};
    
    std::atomic<float> leftLevel{0.0f};
    std::atomic<float> rightLevel{0.0f};
    float levelDecay = 0.0f;
    
    bool isPrepared_ = false;
    double preparedSampleRate{0.0};
    int preparedBlockSize{0};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Channel)
};

} // namespace vibedaw