#pragma once

#include "core/MidiRecorder.h"
#include "project/Project.h"
#include "ui/Theme.h"
#include "ui/Icons.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>

namespace vibedaw {

// Shared clip toolbar and transport/setup popover. All commands are message-thread
// service calls; this view never advances the recording clock.
class RecordingControls : public juce::Component, private juce::Timer {
public:
    RecordingControls(Project& project, bool clipFocused, ClipId clipId = InvalidClipId,
                      bool expanded = false)
        : project_(project), clipFocused_(clipFocused), expanded_(expanded),
          binding_(std::make_shared<Binding>()) {
        binding_->clipId = clipId;
        for (auto* button : {&setup_, &play_, &record_, &stop_, &undo_, &end_})
            addAndMakeVisible(button);
        addAndMakeVisible(takes_);
        addAndMakeVisible(status_);
        status_.setFont(juce::Font(11.0f));
        status_.setComponentID("recordingStatus");
        setup_.setComponentID("recordingSetup");
        play_.setComponentID("recordingPlay");
        record_.setComponentID("recordingRecord");
        stop_.setComponentID("recordingStop");
        undo_.setComponentID("undoRecording");
        end_.setComponentID("endRecordingSession");
        takes_.setComponentID("recordingTake");
        takes_.setTextWhenNothingSelected("Select take...");
        takes_.setTooltip("Choose a take. The current source is shown in the status below.");
        record_.setColour(juce::TextButton::buttonOnColourId, theme::dangerDim);
        play_.setColour(juce::TextButton::buttonOnColourId, theme::toggleActiveBackground);
        setup_.onClick = [this] { openSetup(); };
        play_.onClick = [this] { run(false); };
        record_.onClick = [this] { run(true); };
        stop_.onClick = [this] {
            if (!canControl()) return;
            const juce::Component::SafePointer<RecordingControls> safe(this);
            Command command(binding_, recorder());
            error_.clear();
            recorder().stop();
            if (!safe || !safe->ownerIsAlive()) return;
            changed();
        };
        undo_.onClick = [this] {
            if (!ownerIsAlive() || (recorder().isSessionActive() && !canControl())
                || !recorder().canUndoLastRecording()) return;
            const juce::Component::SafePointer<RecordingControls> safe(this);
            Command command(binding_, recorder());
            juce::String error;
            const bool success = recorder().undoLastRecording(error);
            if (!safe || !safe->ownerIsAlive()) return;
            error_ = !success && error.isEmpty() ? "Recording could not be undone." : error;
            changed();
        };
        takes_.onChange = [this] {
            if (!canControl() || takes_.getSelectedId() <= 0) return;
            const juce::Component::SafePointer<RecordingControls> safe(this);
            Command command(binding_, recorder());
            juce::String error;
            const bool success = recorder().selectTake(static_cast<unsigned>(takes_.getSelectedId() - 1), error);
            if (!safe || !safe->ownerIsAlive()) return;
            error_ = !success && error.isEmpty() ? "Take could not be selected." : error;
            // The service may advance to another take; do not imply a stale selection.
            takes_.setSelectedId(0, juce::dontSendNotification);
            changed();
        };
        end_.setTooltip("Stop and keep captured content, then unlock the next session's settings.");
        end_.onClick = [this] {
            if (!canControl()) return;
            const juce::Component::SafePointer<RecordingControls> safe(this);
            Command command(binding_, recorder());
            error_.clear();
            recorder().stop();
            if (!safe || !safe->ownerIsAlive()) return;
            recorder().poll();
            if (!safe || !safe->ownerIsAlive()) return;
            if (recorder().hasPendingContent()) {
                error_ = "Captured content is still pending. Resolve the recorder status before ending.";
            } else {
                recorder().endSession();
                if (!safe || !safe->ownerIsAlive()) return;
                if (auto owner = activeBinding_.lock())
                    if (recorder().getTargetClipId() != InvalidClipId)
                        owner->clipId = recorder().getTargetClipId();
                activeBinding_.reset();
            }
            changed();
        };

        if (expanded_) {
            for (auto* button : {&setup_, &play_, &stop_, &undo_}) button->setVisible(false);
            record_.setColour(juce::TextButton::buttonColourId, theme::danger);
            record_.setColour(juce::TextButton::buttonOnColourId, theme::dangerDim);
            record_.setColour(juce::TextButton::textColourOffId, theme::deepWell);
            record_.setColour(juce::TextButton::textColourOnId, theme::white);
            addAndMakeVisible(title_);
            title_.setComponentID("recordingTitle");
            title_.setText("Record MIDI", juce::dontSendNotification);
            title_.setFont(juce::Font(20.0f, juce::Font::bold));
            title_.setColour(juce::Label::textColourId, theme::textBright);
            for (auto* combo : {&instrument_, &source_, &sourcePolicy_}) addAndMakeVisible(combo);
            addAndMakeVisible(length_);
            addAndMakeVisible(modeHint_);
            modeHint_.setComponentID("recordingModeHint");
            modeHint_.setFont(juce::Font(12.0f));
            modeHint_.setColour(juce::Label::textColourId, theme::textSecondary);
            selectedMode_ = clipFocused_ ? MidiRecorder::Mode::Overdub : MidiRecorder::Mode::Continuous;
            for (size_t i = 0; i < modeButtons_.size(); ++i) {
                auto& button = modeButtons_[i];
                addAndMakeVisible(button);
                button.setComponentID("recordingMode" + button.getName());
                button.setRadioGroupId(2101);
                button.setClickingTogglesState(true);
                button.setTooltip(button.getName());
                button.onClick = [this, i] {
                    if (recorder().isSessionActive()) return;
                    selectedMode_ = static_cast<MidiRecorder::Mode>(i);
                    refresh();
                };
            }
            instrument_.setComponentID("recordingInstrument");
            source_.setComponentID("recordingSource");
            sourcePolicy_.setComponentID("recordingSourcePolicy");
            length_.setComponentID("recordingLoopLength");
            int id = 1;
            for (const auto& channel : project_.getChannelList().getChannels()) {
                channelIds_.push_back(channel->getId());
                instrument_.addItem(channel->getName(), id);
                if (channel->getId() == project_.getActiveChannelId())
                    instrument_.setSelectedId(id, juce::dontSendNotification);
                ++id;
            }
            instrument_.setTextWhenNothingSelected("Choose an instrument");
            source_.addItem(clipFocused_ ? "New MIDI clip" : "New MIDI clip / new track", 1);
            targets_.push_back({});
            if (clipFocused_) {
                for (const auto& entry : project_.getClipPool().getClips()) {
                    if (entry.second->getType() != Clip::Type::Midi) continue;
                    targets_.push_back({entry.first, {}, {}});
                    source_.addItem("Existing: " + entry.second->getName(), static_cast<int>(targets_.size()));
                }
            } else {
                for (const auto& track : project_.getTrackList().getTracks())
                    for (const auto& instance : track->getClipInstances()) {
                        if (!instance || !instance->isValid()) continue;
                        auto* clip = dynamic_cast<MidiClip*>(project_.getClipPool().getClip(instance->getClipId()));
                        auto* channel = project_.getChannelList().getChannelById(instance->getChannelId());
                        if (!clip || !channel) continue;
                        targets_.push_back({instance->getClipId(), track->getId(), instance->getId()});
                        source_.addItem(track->getName() + " / " + clip->getName() + " / " + channel->getName()
                            + " / start " + juce::String(instance->getStartTime(), 3) + " qn",
                            static_cast<int>(targets_.size()));
                    }
            }
            source_.setSelectedId(1, juce::dontSendNotification);
            if (clipFocused_)
                for (size_t i = 0; i < targets_.size(); ++i)
                    if (targets_[i].clipId == clipId) source_.setSelectedId(static_cast<int>(i + 1), juce::dontSendNotification);
            sourcePolicy_.addItem("Edit shared source", 1);
            if (clipFocused_) sourcePolicy_.addItem("Independent clone (no placements changed)", 2);
            sourcePolicy_.setSelectedId(1, juce::dontSendNotification);
            length_.setText("4", false);
            length_.setSelectAllWhenFocused(true);
            source_.onChange = [this] { refresh(); };
            sourcePolicy_.onChange = [this] { updateWarning(); };
            updateWarning();
            setSize(500, 444);
        } else {
            setSize(460, 62);
        }
        refresh();
        startTimerHz(15);
    }

    ~RecordingControls() override {
        stopTimer();
        dismissSetup();
    }

    // Ownership is a UI token, not a source-ID heuristic. A second window showing
    // the same source must not end the first window's session when it closes.
    bool ownsSession() const {
        return ownerIsAlive() && project_.getMidiRecorder().isSessionActive() && activeBinding_.lock() == binding_;
    }
    ClipId getBoundClipId() const { return binding_->clipId; }
    void setChangedAction(std::function<void()> action) { binding_->changed = std::move(action); }
    void setLifetimeOwner(juce::Component* owner) {
        lifetimeOwner_ = owner;
        hasLifetimeOwner_ = owner != nullptr;
    }

    void closeOwnedSession() {
        dismissSetup();
        if (binding_->commands != 0) {
            // Finish the outer command before stopping: source notifications can
            // destroy the editor halfway through a recorder commit.
            binding_->closeRequested = true;
            binding_->changed = {};
            if (activeBinding_.lock() == binding_) activeBinding_.reset();
            return;
        }
        if (!ownsSession()) return;
        const juce::Component::SafePointer<RecordingControls> safe(this);
        auto& r = recorder();
        // Reentrant source removal must not try to stop this session a second time.
        activeBinding_.reset();
        binding_->changed = {};
        r.stop();
        if (!safe) return;
        r.poll();
        if (!safe) return;
        // Leave recoverable pending content in the service, even if this view closes.
        if (!r.hasPendingContent()) r.endSession();
    }

    // Test seam: construct/inspect the setup without creating a native callout.
    std::unique_ptr<RecordingControls> createSetup() {
        auto view = std::make_unique<RecordingControls>(project_, clipFocused_, binding_->clipId, true);
        view->binding_ = binding_;
        view->setLifetimeOwner(lifetimeOwner_.getComponent());
        view->refresh();
        return view;
    }
    std::function<void()> openSetupOverride;

    void openSetup() {
        if (!ownerIsAlive()) return;
        if (openSetupOverride) { openSetupOverride(); return; }
        if (popover_) return;
        auto view = createSetup();
        popover_ = &juce::CallOutBox::launchAsynchronously(std::move(view), getScreenBounds(), nullptr);
    }

    void refresh() {
        if (!ownerIsAlive()) {
            stopTimer();
            setEnabled(false);
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
            return;
        }
        auto& r = recorder();
        const bool active = r.isSessionActive();
        const bool bound = activeBinding_.lock() == binding_;
        if (bound && r.getTargetClipId() != InvalidClipId) binding_->clipId = r.getTargetClipId();
        if (!active && bound) activeBinding_.reset();
        const bool recording = r.isRecording();
        const bool pending = r.hasPendingContent();
        const bool control = !clipFocused_ || (active && bound);
        play_.setEnabled(!active || control);
        record_.setEnabled(!active || control);
        stop_.setEnabled(active && control);
        undo_.setEnabled((!active || control) && !recording && !pending && r.canUndoLastRecording());
        end_.setEnabled(active && control && !recording);
        record_.setToggleState(active && recording, juce::dontSendNotification);
        play_.setToggleState(active && r.getPlaybackTransport().isPlaying(), juce::dontSendNotification);
        const auto count = active ? r.getTakeCount() : 0;
        if (takeCount_ != count) {
            takeCount_ = count;
            takes_.clear(juce::dontSendNotification);
            for (unsigned i = 0; i < count; ++i) takes_.addItem("Take " + juce::String(i + 1), static_cast<int>(i + 1));
        }
        takes_.setEnabled(control && count > 0 && !recording && !pending);
        juce::String status = r.getStatus();
        if (binding_->error.isNotEmpty()) status += " | " + binding_->error;
        if (active) {
            auto* channel = project_.getChannelList().getChannelById(r.getChannelId());
            auto* clip = project_.getClipPool().getClip(r.getTargetClipId());
            status = (channel ? channel->getName() : "Missing instrument") + " / "
                + (clip ? clip->getName() : "New MIDI clip") + " / " + modeName(r.getMode()) + ": " + status;
            if (!control) status = "Session in another window. " + status;
            if (recording && clipFocused_) status += " | Note edits paused; disarm to edit.";
        }
        if (status.isEmpty()) status = "Choose setup to start playback or recording.";
        status_.setColour(juce::Label::textColourId, binding_->error.isEmpty() ? theme::textSecondary : theme::dangerText);
        status_.setText(status, juce::dontSendNotification);
        status_.setTooltip(status);
        if (expanded_) {
            const bool problem = binding_->error.isNotEmpty() || status.containsIgnoreCase("fault") || (active && !control);
            status_.setVisible(problem);
            if (binding_->error.isNotEmpty()) status_.setText(binding_->error, juce::dontSendNotification);
            record_.setTooltip(status);
            record_.setButtonText(recording ? "Disarm recording" : "Record");
        }
        if (expanded_) {
            if (!active && source_.getSelectedId() <= 0) source_.setSelectedId(1, juce::dontSendNotification);
            if (active) {
                for (size_t i = 0; i < channelIds_.size(); ++i)
                    if (channelIds_[i] == r.getChannelId())
                        instrument_.setSelectedId(static_cast<int>(i + 1), juce::dontSendNotification);
                if (clipFocused_) {
                    const auto target = r.getTargetClipId();
                    auto found = std::find_if(targets_.begin(), targets_.end(), [target](const Target& t) { return t.clipId == target; });
                    if (found == targets_.end()) {
                        if (auto* clip = project_.getClipPool().getClip(target)) {
                            targets_.push_back({target, {}, {}});
                            source_.addItem("Existing: " + clip->getName(), static_cast<int>(targets_.size()));
                            found = std::prev(targets_.end());
                        }
                    }
                    if (found != targets_.end())
                        source_.setSelectedId(static_cast<int>(std::distance(targets_.begin(), found) + 1), juce::dontSendNotification);
                } else if (!bound) {
                    source_.setText("Active session (target fixed)", juce::dontSendNotification);
                }
                sourcePolicy_.setSelectedId(1, juce::dontSendNotification);
                selectedMode_ = r.getMode();
                const auto loop = r.getPlaybackTransport().getLoopRegion();
                length_.setText(juce::String(loop.endBeats - loop.startBeats, 6), false);
            }
            instrument_.setEnabled(!active && (clipFocused_ || selectedTarget().placementId.isEmpty()));
            source_.setEnabled(!active);
            for (size_t i = 0; i < modeButtons_.size(); ++i) {
                modeButtons_[i].setEnabled(!active);
                modeButtons_[i].setToggleState(i == static_cast<size_t>(selectedMode_), juce::dontSendNotification);
            }
            sourcePolicy_.setEnabled(!active && clipFocused_ && selectedClip() != InvalidClipId);
            length_.setEnabled(!active && selectedMode_ != MidiRecorder::Mode::Continuous);
            updateWarning();
            const char* hint = "";
            switch (selectedMode_) {
                case MidiRecorder::Mode::Continuous: hint = "Record into one growing clip until you stop."; break;
                case MidiRecorder::Mode::Takes: hint = "Loop and keep each nonempty pass as a separate take."; break;
                case MidiRecorder::Mode::Replace: hint = "Loop and replace notes in the recorded region, including silence."; break;
                case MidiRecorder::Mode::Overdub: hint = "Loop and add notes. Previous passes play back as you record."; break;
            }
            modeHint_.setText(modeName(selectedMode_) + ": " + hint, juce::dontSendNotification);
        }
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(theme::raised);
        if (!expanded_) return;
        g.setColour(theme::textSecondary);
        g.setFont(juce::Font(12.0f));
        const char* captions[] = {"Instrument", clipFocused_ ? "Source" : "Placement / new", "Source editing"};
        for (int i = 0; i < 3; ++i)
            g.drawText(captions[i], 16, 60 + i * 34, 136, 26, juce::Justification::centredLeft);
        g.drawText("Recording mode", 16, 166, getWidth() - 32, 20, juce::Justification::centredLeft);
        g.drawText("Loop length (beats)", 16, length_.getY(), 136, length_.getHeight(), juce::Justification::centredLeft);
    }

    void resized() override {
        auto bounds = getLocalBounds().reduced(8);
        if (expanded_) {
            bounds = getLocalBounds().reduced(16);
            title_.setBounds(bounds.removeFromTop(28));
            bounds.removeFromTop(12);
            juce::Component* fields[] = {&instrument_, &source_, &sourcePolicy_};
            for (auto* field : fields) {
                auto row = bounds.removeFromTop(34);
                row.removeFromLeft(140);
                field->setBounds(row.reduced(0, 4));
            }
            bounds.removeFromTop(8);
            bounds.removeFromTop(20);
            auto modes = bounds.removeFromTop(44);
            for (size_t i = 0; i < modeButtons_.size(); ++i) {
                auto cell = modes.removeFromLeft(modes.getWidth() / static_cast<int>(modeButtons_.size() - i));
                modeButtons_[i].setBounds(cell.reduced(3, 0));
            }
            modeHint_.setBounds(bounds.removeFromTop(44));
            auto loop = bounds.removeFromTop(34);
            loop.removeFromLeft(140);
            length_.setBounds(loop.reduced(0, 4));
            record_.setBounds(bounds.removeFromBottom(42));
            bounds.removeFromBottom(8);
            auto secondary = bounds.removeFromBottom(28);
            takes_.setBounds(secondary.removeFromLeft(secondary.getWidth() / 2).withTrimmedRight(4));
            end_.setBounds(secondary.withTrimmedLeft(4));
            end_.setVisible(true);
            status_.setBounds(bounds);
            return;
        }
        auto row = bounds.removeFromTop(26);
        if (!expanded_) setup_.setBounds(row.removeFromLeft(60).reduced(2, 0));
        for (auto* button : {&play_, &record_, &stop_}) button->setBounds(row.removeFromLeft(58).reduced(2, 0));
        undo_.setBounds(row.removeFromLeft(66).reduced(2, 0));
        takes_.setBounds(row.removeFromLeft(100).reduced(2, 0));
        end_.setBounds(expanded_ ? row : juce::Rectangle<int>());
        end_.setVisible(expanded_);
        status_.setBounds(bounds);
    }

private:
    class ModeButton : public juce::Button {
    public:
        ModeButton(const char* name, IconId icon) : Button(name), icon_(icon) {}
        void paintButton(juce::Graphics& g, bool over, bool down) override {
            const bool selected = getToggleState();
            auto colour = selected ? theme::highlightBackground : (over ? theme::controlHover : theme::control);
            if (down) colour = colour.darker(0.15f);
            theme::drawSurface(g, getLocalBounds().toFloat().reduced(1), colour,
                theme::controlRadius, selected || hasKeyboardFocus(true) ? theme::accent : theme::border);
            Icons::draw(g, icon_, (selected ? theme::accent : theme::textBright)
                .withMultipliedAlpha(isEnabled() ? 1.0f : 0.45f),
                getLocalBounds().toFloat().withSizeKeepingCentre(24, 24));
        }
    private:
        IconId icon_;
    };
    struct Binding {
        ClipId clipId = InvalidClipId;
        juce::String error;
        std::function<void()> changed;
        unsigned commands = 0;
        bool closeRequested = false;
    };
public:
    // Also used by the transport/editor pollers: a service notification may close
    // the owning editor even when the command originated in another view.
    struct ScopedRecorderCommand {
        std::shared_ptr<Binding> binding;
        std::shared_ptr<Binding> activeOwner;
        MidiRecorder& recorder;
        explicit ScopedRecorderCommand(MidiRecorder& service) : ScopedRecorderCommand({}, service) {}
        ScopedRecorderCommand(std::shared_ptr<Binding> owner, MidiRecorder& service)
            : binding(std::move(owner)), activeOwner(activeBinding_.lock()), recorder(service) {
            if (binding) ++binding->commands;
            if (activeOwner && activeOwner != binding) ++activeOwner->commands;
        }
        ~ScopedRecorderCommand() {
            bool close = binding && --binding->commands == 0 && binding->closeRequested;
            if (activeOwner && activeOwner != binding)
                close = (--activeOwner->commands == 0 && activeOwner->closeRequested) || close;
            if (close) {
                recorder.stop();
                recorder.poll();
                if (!recorder.hasPendingContent()) recorder.endSession();
            }
        }
        ScopedRecorderCommand(const ScopedRecorderCommand&) = delete;
        ScopedRecorderCommand& operator=(const ScopedRecorderCommand&) = delete;
    };
private:
    using Command = ScopedRecorderCommand;
    inline static std::weak_ptr<Binding> activeBinding_;
    Project& project_;
    bool clipFocused_, expanded_;
    std::shared_ptr<Binding> binding_;
    juce::TextButton setup_{"Setup..."}, play_{"Play"}, record_{"Record"}, stop_{"Stop"}, undo_{"Undo rec"}, end_{"End session"};
    juce::ComboBox instrument_, source_, sourcePolicy_, takes_;
    std::array<ModeButton, 4> modeButtons_{{{"Continuous", IconId::continuous}, {"Takes", IconId::takes},
                                          {"Replace", IconId::loop}, {"Overdub", IconId::overdub}}};
    MidiRecorder::Mode selectedMode_ = MidiRecorder::Mode::Continuous;
    juce::TextEditor length_;
    juce::Label status_, title_, modeHint_;
    std::vector<ChannelId> channelIds_;
    struct Target { ClipId clipId = InvalidClipId; juce::String trackId, placementId; };
    std::vector<Target> targets_;
    juce::String error_;
    unsigned takeCount_ = 0;
    juce::Component::SafePointer<juce::CallOutBox> popover_;
    juce::Component::SafePointer<juce::Component> lifetimeOwner_;
    bool hasLifetimeOwner_ = false;

    MidiRecorder& recorder() { return project_.getMidiRecorder(); }
    bool ownerIsAlive() const { return !hasLifetimeOwner_ || lifetimeOwner_ != nullptr; }
    bool canControl() const { return ownerIsAlive() && (!clipFocused_ || ownsSession()); }
    static juce::String modeName(MidiRecorder::Mode mode) {
        switch (mode) {
            case MidiRecorder::Mode::Continuous: return "Continuous";
            case MidiRecorder::Mode::Takes: return "Takes";
            case MidiRecorder::Mode::Replace: return "Replace";
            case MidiRecorder::Mode::Overdub: return "Overdub";
        }
        return {};
    }
    Target selectedTarget() const {
        const auto index = source_.getSelectedId() - 1;
        return index >= 0 && index < static_cast<int>(targets_.size()) ? targets_[static_cast<size_t>(index)] : Target{};
    }
    ClipInstance* selectedPlacement() const {
        const auto target = selectedTarget();
        if (auto* track = project_.getTrackList().getTrackById(target.trackId))
            for (const auto& instance : track->getClipInstances())
                if (instance && instance->getId() == target.placementId) return instance.get();
        return nullptr;
    }
    ClipId selectedClip() const {
        if (!clipFocused_ && selectedTarget().placementId.isNotEmpty()) {
            if (auto* instance = selectedPlacement()) return instance->getClipId();
        }
        return selectedTarget().clipId;
    }
    void updateWarning() {
        if (!ownerIsAlive()) return;
        const auto id = recorder().isSessionActive() ? recorder().getTargetClipId() : selectedClip();
        auto* clip = project_.getClipPool().getClip(id);
        juce::String detail;
        if (!clipFocused_ && recorder().isSessionActive() && activeBinding_.lock() != binding_) {
            auto* current = project_.getClipPool().getClip(recorder().getTargetClipId());
            auto* channel = project_.getChannelList().getChannelById(recorder().getChannelId());
            detail = "Active source: " + (current ? current->getName() : juce::String("New MIDI clip"))
                + "\nInstrument: " + (channel ? channel->getName() : juce::String("Missing instrument"))
                + "\nPlacement is fixed by the active session; end it to choose another.";
        } else if (!clipFocused_ && selectedTarget().placementId.isNotEmpty()) {
            auto* instance = selectedPlacement();
            auto* track = project_.getTrackList().getTrackById(selectedTarget().trackId);
            auto* channel = instance ? project_.getChannelList().getChannelById(instance->getChannelId()) : nullptr;
            if (instance && track && clip && channel) {
                detail = "Track: " + track->getName() + " | Source: " + clip->getName()
                    + "\nInstrument: " + channel->getName() + " | Placement start: " + juce::String(instance->getStartTime(), 3) + " qn";
                if (!recorder().isSessionActive())
                    for (size_t i = 0; i < channelIds_.size(); ++i)
                        if (channelIds_[i] == channel->getId()) instrument_.setSelectedId(static_cast<int>(i + 1), juce::dontSendNotification);
            } else detail = "Placement or its source/instrument was removed. Reopen setup.";
        } else detail = clipFocused_ ? "Source: " + (clip ? clip->getName() : juce::String("New MIDI clip"))
                                    : "Track: new recording lane | Source: new MIDI clip";
        if (!clipFocused_) detail += "\nCapture cursor: " + juce::String(project_.getTransportState().getPositionInBeats(), 3) + " qn";
        source_.setTooltip(detail);
        int placements = 0;
        for (const auto& track : project_.getTrackList().getTracks())
            for (const auto& instance : track->getClipInstances())
                if (instance && instance->getClipId() == id) ++placements;
        sourcePolicy_.changeItemText(1, "Edit shared source (" + juce::String(placements) + " placements)");
        sourcePolicy_.setTooltip(id == InvalidClipId ? "A new MIDI source will be created by the recorder."
            : sourcePolicy_.getSelectedId() == 2
                ? "Creates an independent pooled source. No existing placement is repointed."
                : "Changes affect every placement using this source.");
    }
    void changed() {
        binding_->error = error_;
        refresh();
        const auto callback = binding_->changed;
        if (callback) callback();
    }
    void dismissSetup() {
        if (popover_) {
            popover_->setVisible(false);
            popover_->dismiss();
            popover_ = nullptr;
        }
    }
    void timerCallback() override { refresh(); }

    void run(bool record) {
        if (!ownerIsAlive()) return;
        const juce::Component::SafePointer<RecordingControls> safe(this);
        auto& r = recorder();
        Command command(binding_, r);
        error_.clear();
        if (r.isSessionActive()) {
            if (!canControl()) return;
            if (record) {
                const bool arm = !r.isRecording();
                juce::String error;
                const bool success = r.setRecording(arm, error);
                if (!safe || !safe->ownerIsAlive()) return;
                error_ = error;
                if (success) {
                    if (arm) r.getPlaybackTransport().setPlaying(true);
                } else if (error_.isEmpty()) error_ = "Recording state could not be changed.";
            } else {
                r.getPlaybackTransport().setPlaying(true);
            }
            if (!safe || !safe->ownerIsAlive()) return;
            changed();
            return;
        }
        if (!expanded_) { openSetup(); return; }
        const int channelIndex = instrument_.getSelectedId() - 1;
        const bool hasChannel = channelIndex >= 0 && channelIndex < static_cast<int>(channelIds_.size());
        if (!hasChannel && (clipFocused_ || selectedTarget().placementId.isEmpty())) {
            error_ = "Choose an instrument before starting.";
            changed();
            return;
        }
        MidiRecorder::Options options;
        options.clipId = selectedClip();
        options.channelId = hasChannel ? channelIds_[static_cast<size_t>(channelIndex)] : InvalidChannelId;
        options.mode = selectedMode_;
        options.clipFocused = clipFocused_;
        options.startBeat = clipFocused_ ? 0.0 : project_.getTransportState().getPositionInBeats();
        if (!clipFocused_ && selectedTarget().placementId.isNotEmpty()) {
            auto* instance = selectedPlacement();
            if (!instance || !instance->isValid()) {
                error_ = "The selected placement was removed. Reopen setup.";
                changed();
                return;
            }
            options.clipId = instance->getClipId();
            options.channelId = instance->getChannelId();
            options.trackId = selectedTarget().trackId;
            options.placementId = instance->getId();
        }
        const auto text = length_.getText().trim();
        const char* start = text.toRawUTF8();
        char* end = nullptr;
        options.loopLength = std::strtod(start, &end);
        if (options.mode == MidiRecorder::Mode::Continuous) options.loopLength = 4.0;
        else if (end == start || *end != '\0' || !std::isfinite(options.loopLength)
                 || !TransportState::validLoopRegion(0.0, options.loopLength)) {
            error_ = "Loop length must be 1/64 to 1e9 quarter-note beats.";
            changed();
            return;
        }
        if (!project_.getChannelList().getChannelById(options.channelId)) {
            error_ = "The selected instrument was removed. Reopen setup.";
            changed();
            return;
        }
        if (options.clipId != InvalidClipId) {
            auto* source = dynamic_cast<MidiClip*>(project_.getClipPool().getClip(options.clipId));
            if (!source) {
                error_ = "The selected source was removed. Reopen setup.";
                changed();
                return;
            }
            if (clipFocused_ && sourcePolicy_.getSelectedId() == 2) {
                auto clone = source->clone();
                clone->setName(source->getName() + " (independent)");
                options.clipId = project_.getClipPool().addClip(std::move(clone));
                if (!safe || !safe->ownerIsAlive()) return;
                if (options.clipId == InvalidClipId) {
                    error_ = "An independent source could not be created.";
                    changed();
                    return;
                }
                // Retain an explicitly requested clone even if runtime start fails.
                auto* added = project_.getClipPool().getClip(options.clipId);
                if (!added) {
                    error_ = "The independent source was removed during creation.";
                    changed();
                    return;
                }
                targets_.push_back({options.clipId, {}, {}});
                source_.addItem("Existing: " + added->getName(), static_cast<int>(targets_.size()));
                source_.setSelectedId(static_cast<int>(targets_.size()), juce::dontSendNotification);
                sourcePolicy_.setSelectedId(1, juce::dontSendNotification);
            }
        }
        auto& song = project_.getTransportState();
        const auto previousLoop = song.getLoopRegion();
        if (!clipFocused_) {
            if (options.mode == MidiRecorder::Mode::Continuous) song.setLoopEnabled(false);
            else {
                if (!TransportState::validLoopRegion(options.startBeat, options.startBeat + options.loopLength)) {
                    error_ = "The loop exceeds the song's beat range.";
                    changed();
                    return;
                }
                song.setLoopRegion(options.startBeat, options.startBeat + options.loopLength);
                if (!safe || !safe->ownerIsAlive()) return;
                song.setLoopEnabled(true);
            }
            if (!safe || !safe->ownerIsAlive()) return;
        }
        juce::String error;
        const bool success = r.start(options, record, error);
        if (!safe || !safe->ownerIsAlive()) return;
        error_ = error;
        if (success) {
            activeBinding_ = binding_;
            if (options.clipId != InvalidClipId) binding_->clipId = options.clipId;
        } else {
            if (!clipFocused_) {
                if (previousLoop.exists) {
                    song.setLoopRegion(previousLoop.startBeats, previousLoop.endBeats);
                    if (!safe || !safe->ownerIsAlive()) return;
                    song.setLoopEnabled(previousLoop.enabled);
                } else song.clearLoop();
                if (!safe || !safe->ownerIsAlive()) return;
            }
            if (error_.isEmpty()) error_ = "Recording session could not be started.";
        }
        changed();
    }
};

} // namespace vibedaw
