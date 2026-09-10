#include "ClipEditorWindow.h"
#include "ui/Theme.h"
#include "project/Clip.h"
#include "core/MidiManager.h"
#include "utils/Logger.h"

namespace vibedaw {

ClipEditorWindow::ClipEditorWindow(MidiClip* clip, ClipId clipId, MidiManager* midiManager,
                                   TransportState* transport,
                                   std::function<bool(double, double&)> localBeatProvider)
    : DocumentWindow("Piano Roll", theme::control, DocumentWindow::closeButton | DocumentWindow::minimiseButton, true)
    , midiClip_(clip)
    , clipId_(clipId)
{
    LOG_INFO("ClipEditorWindow: Creating window for clip");
    
    editor_ = std::make_unique<PianoRollEditor>(midiManager);
    editor_->setListener(this);
    editor_->setMidiClip(clip, clipId);
    editor_->setTransport(transport);
    editor_->setLocalBeatProvider(std::move(localBeatProvider));
    editor_->setFollowEnabled(transport != nullptr);
    
    auto content = std::make_unique<juce::Component>();
    
    toolbar_ = std::make_unique<juce::Component>();
    toolbar_->setBounds(0, 0, 800, 32);
    
    clipNameLabel_ = std::make_unique<juce::Label>("clipName", clip ? clip->getName() : "Untitled");
    clipNameLabel_->setBounds(10, 4, 200, 24);
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
    gridResolutionCombo_->setBounds(220, 4, 100, 24);
    gridResolutionCombo_->onChange = [this]() { updateGridResolution(); };
    toolbar_->addAndMakeVisible(gridResolutionCombo_.get());
    
    followButton_.setButtonText("Follow");
    followButton_.setBounds(332, 4, 80, 24);
    followButton_.setToggleState(editor_->isFollowEnabled(), juce::dontSendNotification);
    followButton_.onClick = [this]() {
        editor_->setFollowEnabled(followButton_.getToggleState());
    };
    toolbar_->addAndMakeVisible(followButton_);

    content->addAndMakeVisible(toolbar_.get());
    
    editor_->setBounds(0, 32, 800, 500);
    content->addAndMakeVisible(editor_.get());
    
    content->setSize(800, 532);
    setContentOwned(content.release(), true);
    
    setResizable(true, true);
    setUsingNativeTitleBar(true);
    centreWithSize(800, 532);
    setVisible(true);
    
    LOG_INFO("ClipEditorWindow: Window created and visible");
}

ClipEditorWindow::~ClipEditorWindow() {
    LOG_INFO("ClipEditorWindow: Destroyed");
}

void ClipEditorWindow::closeButtonPressed() {
    LOG_INFO("ClipEditorWindow: Close button pressed");
    if (listener_) {
        listener_->clipEditorClosed(this);
    }
    delete this;
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