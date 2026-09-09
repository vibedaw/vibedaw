#include "TransportComponent.h"
#include "utils/Logger.h"
#include <cstdlib>

namespace vibedaw {

namespace {

IconId iconForType(TransportButton::Type type) {
    switch (type) {
        case TransportButton::Type::ReturnToStart: return IconId::returnToStart;
        case TransportButton::Type::Rewind: return IconId::rewind;
        case TransportButton::Type::Stop: return IconId::stop;
        case TransportButton::Type::Play: return IconId::play;
        case TransportButton::Type::Record: return IconId::record;
        case TransportButton::Type::FastForward: return IconId::fastForward;
        case TransportButton::Type::Loop: return IconId::loop;
        case TransportButton::Type::Metronome: return IconId::metronome;
    }
    return IconId::play;
}

} // namespace

TimeDisplay::TimeDisplay() {
    setInterceptsMouseClicks(true, false);
}

void TimeDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRoundedRectangle(bounds, 4.0f);
    
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    
    g.setColour(juce::Colour(0xff00ff88));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 24.0f, juce::Font::plain));
    
    juce::String timeStr = (displayMode_ == 0) ? formatBarsBeatsTicks() : formatTimeCode();
    g.drawText(timeStr, bounds, juce::Justification::centred);
}

void TimeDisplay::setPosition(double positionInBeats, double tempo) {
    position_ = positionInBeats;
    tempo_ = tempo;
    repaint();
}

void TimeDisplay::setTimeSignature(int numerator, int denominator) {
    timeSigNumerator_ = numerator;
    timeSigDenominator_ = denominator;
    repaint();
}

void TimeDisplay::setDisplayMode(int mode) {
    displayMode_ = mode;
    repaint();
}

juce::String TimeDisplay::formatBarsBeatsTicks() {
    return TransportState::formatBarsBeatsTicks(position_,
                                               {timeSigNumerator_, timeSigDenominator_});
}

juce::String TimeDisplay::formatTimeCode() {
    const double positionSeconds = position_ * 60.0 / tempo_;
    auto totalSeconds = static_cast<juce::int64>(positionSeconds);
    auto hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    int millis = static_cast<int>((positionSeconds - totalSeconds) * 100.0);
    
    return juce::String(hours) + juce::String::formatted(":%02d:%02d.%02d", minutes, seconds, millis);
}

TransportButton::TransportButton(Type type)
    : type_(type)
{
    setInterceptsMouseClicks(true, false);
}

void TransportButton::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    
    juce::Colour bgColour = juce::Colour(0xff2a2a2a);
    juce::Colour iconColour = juce::Colour(0xffaaaaaa);
    
    if (active_) {
        switch (type_) {
            case Type::Play:
                bgColour = juce::Colour(0xff4a8a4a);
                iconColour = juce::Colour(0xff00ff88);
                break;
            case Type::Record:
                bgColour = juce::Colour(0xff8a4a4a);
                iconColour = juce::Colour(0xffff4444);
                break;
            case Type::Loop:
            case Type::Metronome:
                bgColour = juce::Colour(0xff3a5a7a);
                iconColour = juce::Colour(0xff66aaff);
                break;
            default:
                break;
        }
    }
    
    if (pressed_) {
        bgColour = bgColour.brighter(0.1f);
    }
    
    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds, 4.0f);
    
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    
    g.setColour(iconColour);
    auto iconBounds = bounds.reduced(4.0f);
    Icons::draw(g, iconForType(type_), iconColour, iconBounds);
}

void TransportButton::mouseDown(const juce::MouseEvent& e) {
    if (!isEnabled()) return;
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu()) {
        if (onContextMenu) onContextMenu(e);
        return;
    }
    pressed_ = true;
    repaint();
}

void TransportButton::mouseUp(const juce::MouseEvent& e) {
    if (!pressed_) return;
    pressed_ = false;
    repaint();
    if (isEnabled() && !e.mods.isRightButtonDown() && !e.mods.isPopupMenu() && onClick) onClick();
}

void TransportButton::setActive(bool active) {
    if (active_ != active) {
        active_ = active;
        repaint();
    }
}

TempoControl::TempoControl() {
    setInterceptsMouseClicks(true, false);
}

void TempoControl::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRoundedRectangle(bounds, 4.0f);
    
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    
    g.setColour(juce::Colour(0xffcccccc));
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(juce::String(tempo_, 1), bounds, juce::Justification::centred);
    
    g.setColour(juce::Colour(0xff666666));
    g.setFont(juce::Font(10.0f));
    g.drawText("BPM", bounds.removeFromBottom(12), juce::Justification::centred);
}

void TempoControl::resized() {
}

void TempoControl::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu()) {
        if (onTempoTapped) onTempoTapped();
    } else if (e.mods.isLeftButtonDown()) {
        juce::PopupMenu menu;
        menu.addItem(1, "Tap Tempo...", true, false);
        menu.addSeparator();
        menu.addItem(2, "120 BPM", true, tempo_ == 120.0);
        menu.addItem(3, "140 BPM", true, tempo_ == 140.0);
        menu.addItem(4, "160 BPM", true, tempo_ == 160.0);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [this](int result) {
                if (result > 0) {
                    if (result == 1 && onTempoTapped) onTempoTapped();
                    else if (result == 2 && onTempoChanged) onTempoChanged(120.0);
                    else if (result == 3 && onTempoChanged) onTempoChanged(140.0);
                    else if (result == 4 && onTempoChanged) onTempoChanged(160.0);
                }
            });
    }
}

void TempoControl::setTempo(double tempo) {
    tempo_ = tempo;
    repaint();
}

TimeSignatureControl::TimeSignatureControl() {
    setInterceptsMouseClicks(true, false);
}

void TimeSignatureControl::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRoundedRectangle(bounds, 4.0f);
    
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    
    g.setColour(juce::Colour(0xffcccccc));
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(juce::String(numerator_) + "/" + juce::String(denominator_), bounds, juce::Justification::centred);
}

void TimeSignatureControl::mouseDown(const juce::MouseEvent&) {
    juce::PopupMenu menu;
    menu.addItem(1, "2/4", true, numerator_ == 2 && denominator_ == 4);
    menu.addItem(2, "3/4", true, numerator_ == 3 && denominator_ == 4);
    menu.addItem(3, "4/4", true, numerator_ == 4 && denominator_ == 4);
    menu.addItem(4, "5/4", true, numerator_ == 5 && denominator_ == 4);
    menu.addItem(5, "6/8", true, numerator_ == 6 && denominator_ == 8);
    menu.addItem(6, "7/8", true, numerator_ == 7 && denominator_ == 8);
    
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [this](int result) {
            if (result > 0 && onTimeSignatureChanged) {
                switch (result) {
                    case 1: onTimeSignatureChanged(2, 4); break;
                    case 2: onTimeSignatureChanged(3, 4); break;
                    case 3: onTimeSignatureChanged(4, 4); break;
                    case 4: onTimeSignatureChanged(5, 4); break;
                    case 5: onTimeSignatureChanged(6, 8); break;
                    case 6: onTimeSignatureChanged(7, 8); break;
                }
            }
        });
}

void TimeSignatureControl::setTimeSignature(int numerator, int denominator) {
    numerator_ = numerator;
    denominator_ = denominator;
    repaint();
}

TransportComponent::TransportComponent(TransportState& state)
    : transportState_(state)
{
    timeDisplay_ = std::make_unique<TimeDisplay>();
    addAndMakeVisible(*timeDisplay_);
    
    returnToStartBtn_ = std::make_unique<TransportButton>(TransportButton::Type::ReturnToStart);
    rewindBtn_ = std::make_unique<TransportButton>(TransportButton::Type::Rewind);
    stopBtn_ = std::make_unique<TransportButton>(TransportButton::Type::Stop);
    playBtn_ = std::make_unique<TransportButton>(TransportButton::Type::Play);
    recordBtn_ = std::make_unique<TransportButton>(TransportButton::Type::Record);
    fastForwardBtn_ = std::make_unique<TransportButton>(TransportButton::Type::FastForward);
    tempoControl_ = std::make_unique<TempoControl>();
    timeSigControl_ = std::make_unique<TimeSignatureControl>();
    loopBtn_ = std::make_unique<TransportButton>(TransportButton::Type::Loop);
    metronomeBtn_ = std::make_unique<TransportButton>(TransportButton::Type::Metronome);
    
    addAndMakeVisible(*returnToStartBtn_);
    addAndMakeVisible(*rewindBtn_);
    addAndMakeVisible(*stopBtn_);
    addAndMakeVisible(*playBtn_);
    addAndMakeVisible(*recordBtn_);
    addAndMakeVisible(*fastForwardBtn_);
    addAndMakeVisible(*tempoControl_);
    addAndMakeVisible(*timeSigControl_);
    addAndMakeVisible(*loopBtn_);
    addAndMakeVisible(*metronomeBtn_);
    
setupButtons();

    transportState_.addListener(this);
    updateButtonStates();
    transportPositionChanged(transportState_.getPosition());
    transportTempoChanged(transportState_.getTempo());
    const auto meter = transportState_.getTimeSignature();
    transportTimeSignatureChanged(meter.numerator, meter.denominator);
    const auto loop = transportState_.getLoopRegion();
    transportLoopChanged(loop.enabled, loop.startBeats, loop.endBeats);
    
    LOG_INFO("TransportComponent: Created with full transport controls");
}

TransportComponent::~TransportComponent() {
    transportState_.removeListener(this);
    LOG_INFO("TransportComponent: Destroyed");
}

void TransportComponent::setupButtons() {
    returnToStartBtn_->onClick = [this]() {
        transportState_.reset();
    };
    
    rewindBtn_->onClick = [this]() {
        transportState_.reset();
    };
    
    stopBtn_->onClick = [this]() {
        transportState_.stop();
    };
    
    playBtn_->onClick = [this]() {
        transportState_.togglePlay();
    };
    
    recordBtn_->setEnabled(false);
    recordBtn_->setAlpha(0.35f);
    recordBtn_->setTitle("Recording unavailable");
    recordBtn_->setDescription("Recording is not implemented.");
    loopBtn_->setTitle("Loop: click to toggle, right-click for options");
    loopBtn_->setComponentID("loopToggle");
    metronomeBtn_->setTitle("Enable audible metronome");
    metronomeBtn_->setComponentID("metronomeToggle");
    recordBtn_->setComponentID("record");
    
    fastForwardBtn_->onClick = [this]() {
        double currentPos = transportState_.getPosition();
        transportState_.setPosition(currentPos + 1.0);
    };
    
    tempoControl_->onTempoChanged = [this](double tempo) {
        transportState_.setTempo(tempo);
    };
    
    tempoControl_->onTempoTapped = [this]() {
        transportState_.tapTempo();
    };
    
    timeSigControl_->onTimeSignatureChanged = [this](int num, int denom) {
        transportState_.setTimeSignature(num, denom);
    };
    
    loopBtn_->onClick = [this]() {
        transportState_.setLoopEnabled(!transportState_.isLoopEnabled());
    };

    loopBtn_->onContextMenu = [this](const juce::MouseEvent&) { showLoopMenu(); };
    
    metronomeBtn_->onClick = [this]() {
        transportState_.setMetronomeEnabled(!transportState_.isMetronomeEnabled());
    };
}

void TransportComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    g.setColour(juce::Colour(0xff252525));
    g.fillRect(bounds);
    
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(bounds.removeFromBottom(1));
}

void TransportComponent::resized() {
    auto bounds = getLocalBounds().reduced(10, 5);
    auto rightSection = bounds.removeFromRight(250);
    const bool compact = getWidth() < 800;
    timeDisplay_->setBounds(bounds.removeFromLeft(compact ? 130 : 180).withTrimmedTop(2).withTrimmedBottom(2));
    
    bounds.removeFromLeft(20);
    
    auto transportButtons = bounds.removeFromLeft(compact ? 120 : 240);
    int btnWidth = 36;
    int btnHeight = 28;
    
    returnToStartBtn_->setVisible(!compact);
    rewindBtn_->setVisible(!compact);
    fastForwardBtn_->setVisible(!compact);
    if (!compact) {
        returnToStartBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
        transportButtons.removeFromLeft(4);
        rewindBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
        transportButtons.removeFromLeft(4);
    }
    stopBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
    transportButtons.removeFromLeft(4);
    playBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
    transportButtons.removeFromLeft(4);
    recordBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
    transportButtons.removeFromLeft(4);
    if (!compact) fastForwardBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
    
    auto rightControls = rightSection;
    timeSigControl_->setBounds(rightControls.removeFromLeft(50).withSizeKeepingCentre(50, btnHeight));
    rightControls.removeFromLeft(10);
    tempoControl_->setBounds(rightControls.removeFromLeft(70).withSizeKeepingCentre(70, btnHeight));
    rightControls.removeFromLeft(10);
    loopBtn_->setBounds(rightControls.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
    rightControls.removeFromLeft(4);
    metronomeBtn_->setBounds(rightControls.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
}

void TransportComponent::updateButtonStates() {
    playBtn_->setActive(transportState_.isPlaying());
    recordBtn_->setActive(transportState_.isRecording());
    loopBtn_->setActive(transportState_.isLoopEnabled());
    metronomeBtn_->setActive(transportState_.isMetronomeEnabled());
}

void TransportComponent::transportPlayingChanged(bool) {
    updateButtonStates();
}

void TransportComponent::transportRecordingChanged(bool) {
    updateButtonStates();
}

void TransportComponent::transportPositionChanged(double) {
    timeDisplay_->setPosition(transportState_.getPositionInBeats(), transportState_.getTempo());
}

void TransportComponent::transportTempoChanged(double tempo) {
    tempoControl_->setTempo(tempo);
    timeDisplay_->setPosition(transportState_.getPositionInBeats(), tempo);
}

void TransportComponent::transportTimeSignatureChanged(int numerator, int denominator) {
    timeSigControl_->setTimeSignature(numerator, denominator);
    timeDisplay_->setTimeSignature(numerator, denominator);
}

void TransportComponent::transportLoopChanged(bool enabled, double start, double end) {
    juce::ignoreUnused(start, end);
    loopBtn_->setActive(enabled);
    if (openPopover_) openPopover_->refreshFromState();
}

void TransportComponent::transportMetronomeChanged(bool enabled) {
    metronomeBtn_->setActive(enabled);
}

void TransportComponent::showLoopMenu() {
    juce::PopupMenu menu;
    const bool enabled = transportState_.isLoopEnabled();
    menu.addItem(1, enabled ? "Disable Loop" : "Enable Loop", true, enabled);
    menu.addSeparator();
    menu.addItem(2, "Edit Loop...", true, false);
    menu.addItem(3, "Clear Loop", true, false);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(loopBtn_.get()),
        [this](int result) {
            if (result > 0) handleLoopMenuAction(result);
        });
}

void TransportComponent::handleLoopMenuAction(int action) {
    switch (action) {
        case 1: transportState_.setLoopEnabled(!transportState_.isLoopEnabled()); break;
        case 2: openLoopEditor(); break;
        case 3: transportState_.clearLoop(); break; // Gone from the model and the ruler.
        default: break;
    }
}

std::unique_ptr<LoopEditorPopover> TransportComponent::createLoopEditor() {
    auto popover = std::make_unique<LoopEditorPopover>(transportState_);
    openPopover_ = popover.get(); // External loop updates refresh it, as when launched.
    return popover;
}

void TransportComponent::openLoopEditor() {
    if (openLoopEditorOverride) {
        openLoopEditorOverride();
        return;
    }
    if (openPopover_) return; // One popover at a time.
    auto popover = std::make_unique<LoopEditorPopover>(transportState_);
    openPopover_ = popover.get();
    // Desktop-level (null parent): a parented box is a child of the transport
    // bar, which later-added panels paint over. Without a parent the area is
    // in screen coordinates and the box floats above every panel.
    juce::CallOutBox::launchAsynchronously(std::move(popover), loopBtn_->getScreenBounds(), nullptr);
}

LoopEditorPopover::LoopEditorPopover(TransportState& state)
    : transportState_(state)
{
    for (auto* editor : {&start_, &end_}) {
        addAndMakeVisible(editor);
        editor->setSelectAllWhenFocused(true);
        editor->setTextToShowWhenEmpty(editor == &start_ ? "Start (qn)" : "End (qn)",
                                      juce::Colour(0xff777777));
    }
    start_.setComponentID("loopStart");
    end_.setComponentID("loopEnd");
    apply_.setComponentID("applyLoop");
    validation_.setComponentID("loopValidation");
    addAndMakeVisible(apply_);
    addAndMakeVisible(validation_);
    validation_.setFont(juce::Font(11.0f));
    apply_.onClick = [this] { commit(); };
    start_.onReturnKey = end_.onReturnKey = [this] { commit(); };
    setSize(240, 60);
    refreshFromState();
}

void LoopEditorPopover::refreshFromState() {
    if (start_.hasKeyboardFocus(true) || end_.hasKeyboardFocus(true)) return;
    const auto loop = transportState_.getLoopRegion();
    start_.setText(juce::String(loop.startBeats, 9), false);
    end_.setText(juce::String(loop.endBeats, 9), false);
    validation_.setColour(juce::Label::textColourId, juce::Colour(0xffbbbbbb));
    validation_.setText("Quarter notes; min 1/64. Enter or Apply.", juce::dontSendNotification);
}

void LoopEditorPopover::commit() {
    const auto parse = [](const juce::String& text, double& value) {
        const auto trimmed = text.trim();
        const char* start = trimmed.toRawUTF8();
        char* end = nullptr;
        value = std::strtod(start, &end);
        return end != start && *end == '\0';
    };
    double start = 0, end = 0;
    if (!parse(start_.getText(), start) || !parse(end_.getText(), end) ||
        !TransportState::validLoopRegion(start, end)) {
        validation_.setColour(juce::Label::textColourId, juce::Colour(0xffff8888));
        validation_.setText("Invalid: 0 <= start; end <= 1e9; length >= 1/64 qn", juce::dontSendNotification);
        return;
    }
    transportState_.setLoopRegion(start, end);
    transportState_.setLoopEnabled(true);
    if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
}

void LoopEditorPopover::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff252525));
}

void LoopEditorPopover::resized() {
    auto bounds = getLocalBounds().reduced(8);
    validation_.setBounds(bounds.removeFromBottom(14));
    auto row = bounds.removeFromTop(26);
    start_.setBounds(row.removeFromLeft(72).reduced(1));
    row.removeFromLeft(6);
    end_.setBounds(row.removeFromLeft(72).reduced(1));
    row.removeFromLeft(6);
    apply_.setBounds(row.removeFromLeft(64).reduced(1));
}

} // namespace vibedaw
