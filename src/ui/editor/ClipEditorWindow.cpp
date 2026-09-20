#include "ClipEditorWindow.h"
#include "ui/Theme.h"
#include "project/Clip.h"
#include "core/MidiManager.h"
#include "ui/components/RecordingControls.h"
#include "PianoRollGeometry.h"
#include "utils/Logger.h"

namespace vibedaw {

ClipEditorWindow::ClipEditorWindow(MidiClip* clip, ClipId clipId, MidiManager* midiManager,
                                   TransportState* transport,
                                   std::function<bool(double, double&)> localBeatProvider,
                                   bool addToDesktop, Project* project)
    : DawWindow("Piano Roll", DocumentWindow::allButtons, addToDesktop)
    , midiClip_(clip)
    , clipId_(clipId)
    , project_(project)
    , songTransport_(transport)
    , songBeatProvider_(std::move(localBeatProvider))
    , songClipId_(clipId)
{
    LOG_INFO("ClipEditorWindow: Creating window for clip");
    setResizeLimits(500, 300, 10000, 10000);
    
    editor_ = std::make_unique<PianoRollEditor>(midiManager);
    editor_->setListener(this);
    editor_->setMidiClip(clip, clipId);
    editor_->setTransport(transport);
    editor_->setLocalBeatProvider(songBeatProvider_);
    editor_->setFollowEnabled(transport != nullptr);
    
    auto content = std::make_unique<juce::Component>();
    
    toolbar_ = std::make_unique<juce::Component>();
    
    clipNameLabel_ = std::make_unique<juce::Label>("clipName", clip ? clip->getName() : "Untitled");
    clipNameLabel_->setColour(juce::Label::textColourId, theme::white);
    clipNameLabel_->setFont(juce::Font(14.0f, juce::Font::bold));
    toolbar_->addAndMakeVisible(clipNameLabel_.get());
    
    gridResolutionCombo_ = std::make_unique<juce::ComboBox>("gridRes");
    gridResolutionCombo_->addItem("1/4", 1);
    gridResolutionCombo_->addItem("1/8", 2);
    gridResolutionCombo_->addItem("1/16", 3);
    gridResolutionCombo_->addItem("1/32", 4);
    gridResolutionCombo_->addItem("1/4 Triplet", 5);
    gridResolutionCombo_->addItem("1/8 Triplet", 6);
    gridResolutionCombo_->addItem("1/16 Triplet", 7);
    gridResolutionCombo_->setSelectedId(3);
    gridResolutionCombo_->onChange = [this]() { updateGridResolution(); };
    toolbar_->addAndMakeVisible(gridResolutionCombo_.get());
    
    followButton_.setButtonText("Follow");
    followButton_.setToggleState(editor_->isFollowEnabled(), juce::dontSendNotification);
    followButton_.onClick = [this]() {
        editor_->setFollowEnabled(followButton_.getToggleState());
    };
    toolbar_->addAndMakeVisible(followButton_);

    content->addAndMakeVisible(toolbar_.get());
    
    content->addAndMakeVisible(editor_.get());

    if (project_) {
        project_->getClipPool().addListener(this);
        recordingControls_ = std::make_unique<RecordingControls>(*project_, true, clipId);
        recordingControls_->setLifetimeOwner(this);
        recordingControls_->setChangedAction([safe = juce::Component::SafePointer<ClipEditorWindow>(this)] {
            if (safe) safe->timerCallback(1);
        });
        content->addAndMakeVisible(*recordingControls_);
        // Block note-grid mouse edits while preserving the piano keyboard for audition.
        recordingEditShield_.setComponentID("recordingEditShield");
        recordingEditShield_.setWantsKeyboardFocus(true);
        content->addChildComponent(recordingEditShield_);
        juce::MultiTimer::startTimer(1, 1000 / 30);
    }
    
    content->setSize(800, 532);
    setContentOwned(content.release(), true);
    
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
    
    LOG_INFO("ClipEditorWindow: Window created and visible");
}

ClipEditorWindow::~ClipEditorWindow() {
    juce::MultiTimer::stopTimer(1);
    if (recordingControls_) {
        recordingControls_->setChangedAction({});
        recordingControls_->closeOwnedSession();
    }
    if (project_) project_->getClipPool().removeListener(this);
    LOG_INFO("ClipEditorWindow: Destroyed");
}

void ClipEditorWindow::resized() {
    DawWindow::resized();

    auto* content = getContentComponent();
    if (content == nullptr || !toolbar_ || !editor_ || !clipNameLabel_ || !gridResolutionCombo_)
        return;

    auto bounds = content->getLocalBounds();
    toolbar_->setBounds(bounds.removeFromTop(juce::jmin(32, bounds.getHeight())));
    if (recordingControls_) recordingControls_->setBounds(bounds.removeFromTop(62));
    editor_->setBounds(bounds);
    recordingEditShield_.setBounds(bounds.withTrimmedLeft(PianoRollEditor::keyboardWidth)
                                        .withTrimmedTop(PianoRollEditor::timeRulerHeight));

    const auto toolbarBounds = toolbar_->getLocalBounds();
    clipNameLabel_->setBounds(toolbarBounds.getIntersection(juce::Rectangle<int>(10, 4, 200, 24)));
    gridResolutionCombo_->setBounds(toolbarBounds.getIntersection(juce::Rectangle<int>(220, 4, 100, 24)));
    followButton_.setBounds(toolbarBounds.getIntersection(juce::Rectangle<int>(332, 4, 80, 24)));
}

void ClipEditorWindow::timerCallback(int) {
    if (!project_ || !recordingControls_) return;
    const juce::Component::SafePointer<ClipEditorWindow> safe(this);
    auto& recorder = project_->getMidiRecorder();
    RecordingControls::ScopedRecorderCommand command(recorder);
    recorder.poll();
    if (!safe) return;
    recordingControls_->refresh();
    const bool ownsSession = recordingControls_->ownsSession();
    const auto target = ownsSession ? recorder.getTargetClipId() : recordingControls_->getBoundClipId();
    auto* source = dynamic_cast<MidiClip*>(project_->getClipPool().getClip(target));
    const bool rebound = target != clipId_ || source != midiClip_;
    if (rebound) {
        midiClip_ = source;
        clipId_ = target;
        editor_->setMidiClip(source, target);
    }
    clipNameLabel_->setText(source ? source->getName() : "New MIDI clip", juce::dontSendNotification);

    const bool clipPlayback = recorder.isSessionActive() && recorder.isClipFocused()
        && (ownsSession || (target != InvalidClipId && recorder.getTargetClipId() == target));
    if (clipPlayback != usingClipTransport_ || rebound) {
        usingClipTransport_ = clipPlayback;
        editor_->setTransport(clipPlayback ? &recorder.getPlaybackTransport() : songTransport_);
        if (clipPlayback) {
            editor_->setLocalBeatProvider([](double beat, double& local) { local = beat; return true; });
        } else if (clipId_ == songClipId_) {
            editor_->setLocalBeatProvider(songBeatProvider_);
        } else {
            // Takes/clones may bind a different source. Resolve its placements by
            // stable ID, never keep using the original source's captured pointer.
            editor_->setLocalBeatProvider([this](double beat, double& local) {
                auto* clip = project_->getClipPool().getClip(clipId_);
                if (!clip) return false;
                bool found = false;
                double earliest = 0;
                for (const auto& track : project_->getTrackList().getTracks())
                    for (const auto& instance : track->getClipInstances()) {
                        double candidate = 0;
                        if (instance && instance->isValid() && instance->getClipId() == clipId_
                            && sourceLocalBeat(beat, instance->getStartTime(), instance->getDuration(),
                                               clip->getDuration(), candidate)
                            && (!found || instance->getStartTime() < earliest)) {
                            found = true;
                            earliest = instance->getStartTime();
                            local = candidate;
                        }
                    }
                return found;
            });
        }
    }
    if (clipPlayback) recorder.getPlaybackTransport().pollRenderPosition();
    if (!safe) return;
    const bool blockEdits = recorder.isRecording() && recorder.getTargetClipId() == clipId_;
    recordingEditShield_.setVisible(blockEdits);
    if (blockEdits) recordingEditShield_.toFront(false);
    for (auto* child : editor_->getChildren()) {
        if (auto* viewport = dynamic_cast<juce::Viewport*>(child)) {
            auto* grid = viewport->getViewedComponent();
            if (!grid) continue;
            if (blockEdits && grid->hasKeyboardFocus(true)) grid->giveAwayKeyboardFocus();
            grid->setEnabled(!blockEdits);
        }
    }
}

void ClipEditorWindow::clipWillBeRemoved(ClipId id) {
    if (id != clipId_) return;
    // MainContent normally closes the view first; standalone/project-bound views
    // still detach the grid's source listener before the pool destroys the source.
    editor_->setMidiClip(nullptr, InvalidClipId);
    midiClip_ = nullptr;
    clipId_ = InvalidClipId;
}

void ClipEditorWindow::closeButtonPressed() {
    LOG_INFO("ClipEditorWindow: Close button pressed");
    const juce::Component::SafePointer<ClipEditorWindow> safe(this);
    if (listener_) {
        listener_->clipEditorClosed(this);
    }
    if (safe) delete this;
}

void ClipEditorWindow::clipModified(ClipId clipId) {
    juce::ignoreUnused(clipId);
    if (midiClip_) {
        clipNameLabel_->setText(midiClip_->getName(), juce::dontSendNotification);
    }
}

void ClipEditorWindow::updateGridResolution() {
    if (!editor_) return;
    
    int selected = gridResolutionCombo_->getSelectedId();
    GridResolution res = GridResolution::Sixteenth;
    
    switch (selected) {
        case 1: res = GridResolution::Quarter; break;
        case 2: res = GridResolution::Eighth; break;
        case 3: res = GridResolution::Sixteenth; break;
        case 4: res = GridResolution::ThirtySecond; break;
        case 5: res = GridResolution::QuarterTriplet; break;
        case 6: res = GridResolution::EighthTriplet; break;
        case 7: res = GridResolution::SixteenthTriplet; break;
    }
    
    editor_->setGridResolution(res);
}

} // namespace vibedaw
