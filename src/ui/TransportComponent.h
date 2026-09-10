#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "core/TransportState.h"
#include "ui/Icons.h"
#include <functional>
#include <vector>

namespace vibedaw {

class TimeDisplay : public juce::Component {
public:
    TimeDisplay();
    ~TimeDisplay() override = default;
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
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
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    void setActive(bool active);
    bool isActive() const { return active_; }
    // Borderless buttons render icon-only inside the transport pill; the pill
    // (drawn by the parent) is their container surface.
    void setBorderless(bool borderless) { borderless_ = borderless; repaint(); }

    std::function<void()> onClick;
    // Right-click/menu-button press; the owner builds the context menu.
    std::function<void(const juce::MouseEvent&)> onContextMenu;

private:
    Type type_;
    bool active_ = false;
    bool pressed_ = false;
    bool hovered_ = false;
    bool borderless_ = false;
};

// Tempo entry: drag scrubs vertically (Shift = 0.1 fine steps), a tap opens an
// inline numeric editor in place, right-click shows Tap Tempo plus BPM presets.
// Every interaction is a live TransportState command; no quiescence required.
class TempoControl : public juce::Component,
                     private juce::TextEditor::Listener {
public:
    TempoControl();
    ~TempoControl() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    void setTempo(double tempo);

    // Preset list and menu model are static so offline tests can inspect them;
    // dispatch is public like TransportComponent::handleLoopMenuAction.
    static const std::vector<double>& presetTempos();
    static juce::PopupMenu buildMenu(double currentTempo);
    void handleMenuAction(int actionId);

    // Inline editing lifecycle; tests drive these directly (no focus needed).
    void beginEdit();
    void commitEdit();
    void cancelEdit();
    bool isEditing() const { return editing_; }
    bool isScrubbing() const { return scrubbing_; }
    bool isInvalidEntry() const;
    juce::String editingText() const { return editor_.getText(); }
    void setEditingText(const juce::String& text) {
        editor_.setText(text, juce::dontSendNotification);
    }

    std::function<void(double)> onTempoChanged;
    std::function<void()> onTempoTapped;
    // Set by tests so a right-click never opens a native popup menu.
    std::function<void()> showMenuOverride;

private:
    void textEditorReturnKeyPressed(juce::TextEditor&) override { commitEdit(); }
    void textEditorEscapeKeyPressed(juce::TextEditor&) override { cancelEdit(); }
    void textEditorFocusLost(juce::TextEditor&) override { commitEdit(); }

    void showContextMenu();
    static bool parseTempoText(const juce::String& text, double& value);

    static constexpr double minTempo = 20.0;
    static constexpr double maxTempo = 300.0;
    static constexpr double bpmPerPx = 0.1; // One BPM per ten pixels of travel.
    static constexpr int dragThreshold = 4;

    double tempo_ = 120.0;
    bool scrubbing_ = false;
    bool moved_ = false;
    int dragStartY_ = 0;
    double dragStartTempo_ = 120.0;
    bool editing_ = false;
    bool invalid_ = false;
    juce::TextEditor editor_;

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
    juce::Rectangle<int> buttonGroupBounds_;
    juce::Component::SafePointer<LoopEditorPopover> openPopover_;

    void setupButtons();
    void updateButtonStates();
    void showLoopMenu();
    void openLoopEditor();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportComponent)
};

} // namespace vibedaw
