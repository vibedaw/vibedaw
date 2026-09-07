#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "core/ProcessorBase.h"
#include "ClipInstance.h"
#include <atomic>
#include <vector>

namespace vibedaw {

class Track : public ProcessorBase {
public:
    Track(const juce::String& name = "Track");
    ~Track() override;
    
    void setName(const juce::String& newName) { name = newName; }
    const juce::String getName() const override { return name; }
    
    void setHeight(int h) { height = h; }
    int getHeight() const { return height; }
    
    void setColour(const juce::Colour& colour);
    juce::Colour getColour() const { return colour; }
    
    float getLeftLevel() const { return leftLevel.load(); }
    float getRightLevel() const { return rightLevel.load(); }
    
    void prepareToPlay(double sampleRate, int blockSize) override;
    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override;
    void releaseResources() override;
    
    void addClipInstance(std::unique_ptr<ClipInstance> instance);
    void removeClipInstance(int index);
    void clearClipInstances();
    
    int getNumClipInstances() const { return static_cast<int>(clipInstances.size()); }
    ClipInstance* getClipInstance(int index) const;
    const std::vector<std::unique_ptr<ClipInstance>>& getClipInstances() const { return clipInstances; }
    
private:
    void updateLevels(const juce::AudioBuffer<float>& audio);
    
    juce::String name;
    int height = 80;
    juce::Colour colour{0xffaaaaaa};
    
    std::atomic<float> leftLevel{0.0f};
    std::atomic<float> rightLevel{0.0f};
    float levelDecay = 0.0f;
    
    std::vector<std::unique_ptr<ClipInstance>> clipInstances;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Track)
};

} // namespace vibedaw
