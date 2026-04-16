#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "core/ProcessorBase.h"
#include "Clip.h"
#include <atomic>
#include <vector>

namespace vibedaw {

class PluginHost;

struct SendDestination {
    int destinationIndex = -1;
    float level = 0.0f;
    bool enabled = false;
};

class Track : public ProcessorBase {
public:
    enum class Type { Insert, Send, Master };
    
    Track(const juce::String& name = "Track", Type type = Type::Insert);
    ~Track() override;
    
    void setPlugin(std::unique_ptr<PluginHost> pluginHost);
    PluginHost* getPlugin() const { return plugin.get(); }
    bool hasPlugin() const { return plugin != nullptr; }
    
    void setVolume(float newVolume);
    float getVolume() const { return volume; }
    
    void setPan(float newPan);
    float getPan() const { return pan; }
    
    void setMuted(bool muted);
    bool isMuted() const { return muted; }
    
    void setSolo(bool solo);
    bool isSolo() const { return solo; }
    
    Type getType() const { return trackType; }
    
    void setColour(const juce::Colour& colour);
    juce::Colour getColour() const { return colour; }
    
    void setSend(int index, int destinationTrack, float level, bool enabled = true);
    SendDestination getSend(int index) const;
    int getMaxSends() const { return maxSends; }
    
    float getLeftLevel() const { return leftLevel.load(); }
    float getRightLevel() const { return rightLevel.load(); }
    float getPeakLevel() const { return peakLevel.load(); }
    
    void prepareToPlay(double sampleRate, int blockSize) override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;
    void releaseResources() override;
    const juce::String getName() const override { return name; }
    
    void addClip(std::unique_ptr<Clip> clip);
    void removeClip(int index);
    void clearClips();
    
    int getNumClips() const { return static_cast<int>(clips.size()); }
    Clip* getClip(int index) const;
    const std::vector<std::unique_ptr<Clip>>& getClips() const { return clips; }
    
private:
    void updateLevels(const juce::AudioBuffer<float>& audio);
    
    juce::String name;
    std::unique_ptr<PluginHost> plugin;
    float volume = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool solo = false;
    Type trackType = Type::Insert;
    juce::Colour colour{0xffaaaaaa};
    
    static constexpr int maxSends = 4;
    SendDestination sends[maxSends];
    
    std::atomic<float> leftLevel{0.0f};
    std::atomic<float> rightLevel{0.0f};
    std::atomic<float> peakLevel{0.0f};
    float levelDecay = 0.0f;
    
    std::vector<std::unique_ptr<Clip>> clips;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};

} // namespace vibedaw
