#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "core/TransportState.h"

namespace vibedaw {

class TimeDisplay : public juce::Component {
public:
    TimeDisplay();
    ~TimeDisplay() override = default;
    
    void paint(juce::Graphics& g) override;
    void setPosition(double positionInBeats, double tempo);
    void setTimeSignature(int numerator, int denominator);
    void setDisplayMode(int mode);
    
private:
    double position_ = 0.0;
    double tempo_ = 120.0;
    int timeSigNumerator_ = 4;
    int timeSigDenominator_ = 4;
    int displayMode_ = 0;
    
    juce::String formatBarsBeatsTicks();
    juce::String formatTimeCode();
};

class TransportButton : public juce::Component {
public:
    enum class Type {
        ReturnToStart,
        Rewind,
        Stop,
        Play,
        Record,
        FastForward,
        Loop,
        Metronome
    };
    
    TransportButton(Type type);
    ~TransportButton() override = default;
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    
    void setActive(bool active);
    bool isActive() const { return active_; }
    
    std::function<void()> onClick;
    
private:
    Type type_;
    bool active_ = false;
    bool pressed_ = false;
    
    juce::Path createIcon();
};

class TempoControl : public juce::Component {
public:
    TempoControl();
    ~TempoControl() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    
    void setTempo(double tempo);
    
    std::function<void(double)> onTempoChanged;
    std::function<void()> onTempoTapped;
    
private:
    double tempo_ = 120.0;
    bool editing_ = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TempoControl)
};

class TimeSignatureControl : public juce::Component {
public:
    TimeSignatureControl();
    ~TimeSignatureControl() override = default;
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    
    void setTimeSignature(int numerator, int denominator);
    
    std::function<void(int, int)> onTimeSignatureChanged;
    
private:
    int numerator_ = 4;
    int denominator_ = 4;
    bool showingMenu_ = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimeSignatureControl)
};

class TransportComponent : public juce::Component, public TransportListener {
public:
    TransportComponent(TransportState& state);
    ~TransportComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void transportPlayingChanged(bool isPlaying) override;
    void transportRecordingChanged(bool isRecording) override;
    void transportPositionChanged(double positionInSeconds) override;
    void transportTempoChanged(double tempo) override;
    void transportTimeSignatureChanged(int numerator, int denominator) override;
    void transportLoopChanged(bool enabled, double start, double end) override;
    void transportMetronomeChanged(bool enabled) override;
    
private:
    TransportState& transportState_;
    
    std::unique_ptr<TimeDisplay> timeDisplay_;
    std::unique_ptr<TransportButton> returnToStartBtn_;
    std::unique_ptr<TransportButton> rewindBtn_;
    std::unique_ptr<TransportButton> stopBtn_;
    std::unique_ptr<TransportButton> playBtn_;
    std::unique_ptr<TransportButton> recordBtn_;
    std::unique_ptr<TransportButton> fastForwardBtn_;
    std::unique_ptr<TempoControl> tempoControl_;
    std::unique_ptr<TimeSignatureControl> timeSigControl_;
    std::unique_ptr<TransportButton> loopBtn_;
    std::unique_ptr<TransportButton> metronomeBtn_;
    
    void setupButtons();
    void updateButtonStates();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportComponent)
};

} // namespace vibedaw
