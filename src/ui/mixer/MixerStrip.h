#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LevelMeter.h"
#include "project/ClipInstance.h"
#include <functional>

namespace vibedaw {

class MixerStrip : public juce::Component {
public:
    MixerStrip(ChannelId channelId);
    ~MixerStrip() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    
    void setTrackName(const juce::String& name);
    juce::String getTrackName() const { return trackName; }
    
    void setTrackColour(const juce::Colour& colour);
    juce::Colour getTrackColour() const { return trackColour; }
    
    void setVolume(float volume);
    float getVolume() const { return volume; }
    
    void setPan(float pan);
    float getPan() const { return pan; }
    
    void setMuted(bool muted);
    bool isMuted() const { return muted; }
    
    void setSolo(bool solo);
    bool isSolo() const { return solo; }
    
    void pollMeter(const StereoMeter& source);
    
    void setSelected(bool selected);
    bool isSelected() const { return selected; }
    
    ChannelId getChannelId() const { return channelId; }
    
    std::function<void(float)> onVolumeChanged;
    std::function<void(float)> onPanChanged;
    std::function<void(bool)> onMuteToggled;
    std::function<void(bool)> onSoloToggled;
    std::function<void()> onStripSelected;
    
private:
    class FaderComponent;
    class PanKnobComponent;
    class SmallButton;
    
    void updateComponentPositions();
    
    const ChannelId channelId;
    juce::String trackName;
    juce::Colour trackColour{0xff888888};
    float volume = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool solo = false;
    bool selected = false;
    
    std::unique_ptr<LevelMeter> meter;
    std::unique_ptr<FaderComponent> fader;
    std::unique_ptr<PanKnobComponent> panKnob;
    std::unique_ptr<SmallButton> muteButton;
    std::unique_ptr<SmallButton> soloButton;
    std::unique_ptr<SmallButton> fxButton;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerStrip)
};

} // namespace vibedaw
