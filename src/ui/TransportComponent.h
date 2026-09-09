#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "core/TransportState.h"
#include "ui/Icons.h"

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
    // Right-click/menu-button press; the owner builds the context menu.
    std::function<void(const juce::MouseEvent&)> onContextMenu;

private:
    Type type_;
    bool active_ = false;
    bool pressed_ = false;
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

// Popover contents for "Edit Loop...": the relocated numeric loop bounds.
// Apply commits both fields atomically and enables looping; invalid input is
// retained with a red validation message, matching the removed bar row.
class LoopEditorPopover : public juce::Component {
public:
    explicit LoopEditorPopover(TransportState& state);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void refreshFromState();

private:
    void commit();
    TransportState& transportState_;
    juce::TextEditor start_, end_;
    juce::TextButton apply_{"Apply"};
    juce::Label validation_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopEditorPopover)
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

    // Right-click menu dispatch: 1 toggle, 2 edit popover, 3 clear.
    // Public so offline tests drive actions without opening native menus.
    void handleLoopMenuAction(int action);
    // Builds the popover without launching it (offline test seam for fields).
    std::unique_ptr<LoopEditorPopover> createLoopEditor();
    // Set by tests so "Edit Loop..." never opens a native CallOutBox.
    std::function<void()> openLoopEditorOverride;

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
    juce::Component::SafePointer<LoopEditorPopover> openPopover_;

    void setupButtons();
    void updateButtonStates();
    void showLoopMenu();
    void openLoopEditor();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportComponent)
};

} // namespace vibedaw
