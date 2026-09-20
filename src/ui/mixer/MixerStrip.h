#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/Theme.h"
#include "LevelMeter.h"
#include <functional>

namespace vibedaw {

class MixerStrip : public juce::Component, public juce::SettableTooltipClient {
public:
    MixerStrip(int mixerChannelId);
    ~MixerStrip() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    juce::PopupMenu createContextMenu() const;
    
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
    
    int getChannelId() const { return mixerChannelId; }
    
    std::function<void(float)> onVolumeChanged;
    std::function<void(float)> onPanChanged;
    std::function<void(bool)> onMuteToggled;
    std::function<void(bool)> onSoloToggled;
    std::function<void()> onStripSelected;
    std::function<void()> onRenameRequested;
    
private:
    class FaderComponent;
    class PanKnobComponent;
    class SmallButton;
    
    void updateComponentPositions();
    
    const int mixerChannelId; // Stable mixer ID, not an instrument ChannelId or index.
    juce::String trackName;
    juce::Colour trackColour{theme::headerStripDefault};
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
