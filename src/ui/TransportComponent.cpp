#include "TransportComponent.h"
#include "utils/Logger.h"

namespace vibedaw {

TransportComponent::TransportComponent() {
    playButton.setButtonText("Play");
    playButton.onClick = [this]() {
        setPlaying(!playing);
        if (onPlayClicked) {
            onPlayClicked();
        }
    };
    addAndMakeVisible(playButton);
    
    stopButton.setButtonText("Stop");
    stopButton.onClick = [this]() {
        setPlaying(false);
        if (onStopClicked) {
            onStopClicked();
        }
    };
    addAndMakeVisible(stopButton);
    
    tempoLabel.setText("120 BPM", juce::dontSendNotification);
    tempoLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(tempoLabel);
    
    LOG_INFO("TransportComponent: Created");
}

TransportComponent::~TransportComponent() {
    LOG_INFO("TransportComponent: Destroyed");
}

void TransportComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff2a2a2a));
    
    g.setColour(juce::Colour(0xff4a4a4a));
    g.drawRect(getLocalBounds());
}

void TransportComponent::resized() {
    auto bounds = getLocalBounds().reduced(5);
    
    playButton.setBounds(bounds.removeFromLeft(60));
    stopButton.setBounds(bounds.removeFromLeft(60).withTrimmedLeft(5));
    tempoLabel.setBounds(bounds.removeFromLeft(80).withTrimmedLeft(10));
}

void TransportComponent::setPlaying(bool isPlaying) {
    playing = isPlaying;
    playButton.setButtonText(playing ? "Pause" : "Play");
    playButton.setColour(juce::TextButton::buttonColourId, 
                          playing ? juce::Colour(0xff4a8a4a) : juce::Colour(0xff3a3a3a));
}

void TransportComponent::setTempo(double newTempo) {
    tempo = newTempo;
    tempoLabel.setText(juce::String(tempo, 1) + " BPM", juce::dontSendNotification);
}

} // namespace vibedaw
