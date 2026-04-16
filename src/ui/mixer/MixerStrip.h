#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LevelMeter.h"
#include <functional>
#include <array>

namespace vibedaw {

class MixerStrip : public juce::Component {
public:
    MixerStrip(int stripIndex);
    ~MixerStrip() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
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
    
    void setLevels(float left, float right);
    
    void setSendLevel(int sendIndex, float level);
    float getSendLevel(int sendIndex) const;
    
    void setSelected(bool selected);
    bool isSelected() const { return selected; }
    
    int getStripIndex() const { return stripIndex; }
    
    std::function<void(float)> onVolumeChanged;
    std::function<void(float)> onPanChanged;
    std::function<void(bool)> onMuteToggled;
    std::function<void(bool)> onSoloToggled;
    std::function<void(int, float)> onSendLevelChanged;
    std::function<void()> onStripSelected;
    std::function<void()> onFxButtonClicked;
    
    enum class StripType {
        Channel,
        Send,
        Master
    };
    
    void setStripType(StripType type);
    StripType getStripType() const { return stripType; }
    
    void setShowSends(bool show) { showSends = show; }
    
private:
    class FaderComponent;
    class PanKnobComponent;
    class SmallButton;
    
    void updateComponentPositions();
    
    int stripIndex;
    juce::String trackName;
    juce::Colour trackColour{0xff888888};
    float volume = 0.8f;
    float pan = 0.0f;
    bool muted = false;
    bool solo = false;
    bool selected = false;
    bool showSends = true;
    StripType stripType = StripType::Channel;
    
    std::unique_ptr<LevelMeter> meter;
    std::unique_ptr<FaderComponent> fader;
    std::unique_ptr<PanKnobComponent> panKnob;
    std::unique_ptr<SmallButton> muteButton;
    std::unique_ptr<SmallButton> soloButton;
    std::unique_ptr<SmallButton> fxButton;
    
    std::array<float, 4> sendLevels{{0.0f, 0.0f, 0.0f, 0.0f}};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerStrip)
};

} // namespace vibedaw
