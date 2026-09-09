#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace vibedaw {

class Track;

class TrackHeader : public juce::Component {
public:
    TrackHeader(Track* track, int index);
    ~TrackHeader() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    
    void setTrackName(const juce::String& name);
    void setTrackColour(const juce::Colour& colour);
    void setMuted(bool muted);
    void setSolo(bool solo);
    void setSelected(bool selected);

    int getTrackIndex() const { return trackIndex; }
    Track* getTrack() const { return track; }
    const juce::String& getTrackName() const { return trackName; }

    juce::PopupMenu createContextMenu() const;

    std::function<void()> onSelected;
    std::function<void(bool)> onMuteToggled;
    std::function<void(bool)> onSoloToggled;
    std::function<void()> onRenameRequested;
    std::function<void()> onRemoveRequested;
    
    static constexpr int defaultHeight = 64;
    
private:
    class ToggleButton;
    
    void updateButtonColours();
    
    Track* track = nullptr;
    int trackIndex = 0;
    juce::String trackName;
    juce::Colour trackColour{0xffaaaaaa};
    bool muted = false;
    bool solo = false;
    bool selected = false;
    
    std::unique_ptr<ToggleButton> muteButton;
    std::unique_ptr<ToggleButton> soloButton;
    
    static constexpr int buttonSize = 20;
    static constexpr int colourStripWidth = 4;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackHeader)
};

} // namespace vibedaw
