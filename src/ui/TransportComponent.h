#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vibedaw {

class TransportComponent : public juce::Component {
public:
    TransportComponent();
    ~TransportComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setPlaying(bool isPlaying);
    void setTempo(double tempo);
    bool isPlaying() const { return playing; }
    
    std::function<void()> onPlayClicked;
    std::function<void()> onStopClicked;
    
private:
    bool playing = false;
    double tempo = 120.0;
    
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::Label tempoLabel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportComponent)
};

} // namespace vibedaw
