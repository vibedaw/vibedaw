#include "TransportComponent.h"
#include "ui/Theme.h"
#include "utils/Logger.h"
#include <cmath>
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

    theme::drawWell(g, bounds, theme::controlRadius);

    if (displayMode_ != 0) {
        g.setColour(theme::accent);
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::plain));
        g.drawText(formatTimeCode(), bounds, juce::Justification::centred);
        return;
    }

    // Same decomposition as TransportState::formatBarsBeatsTicks, split so the
    // BAR/BEAT/TICK captions sit under their own groups.
    const double units = position_ * timeSigDenominator_ / 4.0;
    const auto whole = static_cast<juce::int64>(std::floor(units));
    const juce::String values[] = {
        juce::String(whole / timeSigNumerator_ + 1),
        juce::String(whole % timeSigNumerator_ + 1),
        juce::String(static_cast<int>((units - whole) * 960.0)).paddedLeft('0', 3)
    };
    const juce::String captions[] = {"BAR", "BEAT", "TICK"};

    auto captionRow = bounds.removeFromBottom(14).withTrimmedBottom(3);
    auto digits = bounds.reduced(8, 2);
    const float columnWidth = digits.getWidth() / 3.0f;

    for (int i = 0; i < 3; ++i) {
        auto column = digits.removeFromLeft(columnWidth);
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 20.0f, juce::Font::plain));
        g.setColour(theme::accent);
        g.drawText(values[i], column, juce::Justification::centred);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.setColour(theme::textSecondary);
        g.drawText(captions[i], juce::Rectangle<float>(column.getX(), captionRow.getY(),
                                                       columnWidth, captionRow.getHeight()),
                   juce::Justification::centred);
        if (i < 2) {
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::plain));
            g.setColour(theme::textFaint);
            g.drawText(":", juce::Rectangle<float>(column.getRight() - 3.0f, digits.getY(),
                                                   6.0f, digits.getHeight()),
                       juce::Justification::centred);
        }
    }
}

void TimeDisplay::mouseDown(const juce::MouseEvent&) {
    setDisplayMode(displayMode_ == 0 ? 1 : 0);
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
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    juce::Colour bgColour = borderless_ ? theme::transparent : theme::control;
    juce::Colour iconColour = theme::textBright;
    juce::Colour outlineColour = borderless_ ? theme::transparent : theme::border;

    if (active_) {
        switch (type_) {
            case Type::Play:
                bgColour = theme::toggleActiveBackground;
                iconColour = theme::textBright;
                outlineColour = theme::accent.withAlpha(0.5f);
                break;
            case Type::Record:
                bgColour = theme::dangerDim;
                iconColour = theme::danger;
                outlineColour = theme::danger.withAlpha(0.5f);
                break;
            case Type::Loop:
            case Type::Metronome:
                bgColour = theme::toggleActiveBackground;
                iconColour = theme::accent;
                outlineColour = theme::accent.withAlpha(0.5f);
                break;
            default:
                bgColour = theme::controlSelected;
                iconColour = theme::white;
                break;
        }
    } else if (type_ == Type::Record) {
        // Keep the record affordance red at rest.
        iconColour = theme::danger;
        outlineColour = theme::dangerDim;
    }

    if (hovered_ && isEnabled()) {
        bgColour = bgColour.brighter(0.06f);
        outlineColour = outlineColour.brighter(0.06f);
    }
    if (pressed_) {
        bgColour = bgColour.darker(0.12f);
    }

    if (!borderless_ || active_ || pressed_ || (hovered_ && isEnabled()))
        theme::drawSurface(g, bounds, bgColour, theme::controlRadius, outlineColour);

    g.setColour(iconColour);
    auto iconBounds = bounds.reduced(7.0f);
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

void TransportButton::mouseEnter(const juce::MouseEvent&) {
    if (!hovered_) {
        hovered_ = true;
        repaint();
    }
}

void TransportButton::mouseExit(const juce::MouseEvent&) {
    if (hovered_) {
        hovered_ = false;
        repaint();
    }
}

TempoControl::TempoControl() {
    // Children must stay clickable: the inline editor overlays the box while
    // it is open (the parent's mouseDown ignores clicks while editing).
    setInterceptsMouseClicks(true, true);
    setComponentID("tempoControl");
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    editor_.setComponentID("tempoEditor");
    editor_.setSelectAllWhenFocused(true);
    editor_.setJustification(juce::Justification::centred);
    editor_.setFont(juce::Font(14.0f, juce::Font::bold));
    editor_.setIndents(4, 0);
    editor_.setColour(juce::TextEditor::backgroundColourId, theme::deepWell);
    editor_.setColour(juce::TextEditor::outlineColourId, theme::transparent);
    editor_.setColour(juce::TextEditor::focusedOutlineColourId, theme::transparent);
    editor_.setColour(juce::TextEditor::textColourId, theme::textBright);
    editor_.setColour(juce::TextEditor::highlightColourId, theme::highlightBackground);
    editor_.setInputFilter(new juce::TextEditor::LengthAndCharacterRestriction(8, "0123456789."), true);
    editor_.addListener(this);
    addChildComponent(editor_);
}

TempoControl::~TempoControl() {
    editor_.removeListener(this);
}

void TempoControl::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    const bool captioned = getHeight() >= 40 && getWidth() >= 88;
    theme::drawWell(g, bounds, theme::controlRadius);
    if (isInvalidEntry()) {
        g.setColour(theme::dangerText);
        g.drawRoundedRectangle(bounds.reduced(0.5f), theme::controlRadius, 1.0f);
    }
    auto caption = captioned ? bounds.removeFromBottom(14).withTrimmedBottom(3)
                             : juce::Rectangle<float>();

    g.setColour(theme::textBright);
    g.setFont(juce::Font(17.0f, juce::Font::bold));
    g.drawText(juce::String(tempo_, 1), bounds, juce::Justification::centred);

    if (captioned) {
        g.setColour(theme::textSecondary);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText("BPM", caption, juce::Justification::centred);
    }
}

void TempoControl::resized() {
    editor_.setBounds(getLocalBounds().reduced(3));
}

void TempoControl::mouseDown(const juce::MouseEvent& e) {
    if (editing_) return; // The inline editor owns clicks while it is open.
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu()) {
        showContextMenu();
        return;
    }
    if (e.mods.isLeftButtonDown()) {
        dragStartY_ = e.y;
        dragStartTempo_ = tempo_;
        moved_ = false;
        scrubbing_ = false;
    }
}

void TempoControl::mouseDrag(const juce::MouseEvent& e) {
    if (editing_ || !e.mods.isLeftButtonDown()) return;
    const int dy = e.y - dragStartY_;
    if (!moved_) {
        if (std::abs(dy) < dragThreshold) return;
        moved_ = true;
        scrubbing_ = true;
    }
    // Dragging up (negative dy) increases the value.
    const double raw = dragStartTempo_ - dy * bpmPerPx;
    const double value = e.mods.isShiftDown()
        ? std::round(raw * 10.0) / 10.0
        : std::round(raw);
    const double clamped = juce::jlimit(minTempo, maxTempo, value);
    if (clamped != tempo_) {
        tempo_ = clamped;
        if (onTempoChanged) onTempoChanged(clamped);
        repaint();
    }
}

void TempoControl::mouseUp(const juce::MouseEvent& e) {
    if (editing_) return;
    if (scrubbing_ || moved_) {
        scrubbing_ = false;
        moved_ = false;
        return;
    }
    if (e.mods.isLeftButtonDown() && !e.mods.isRightButtonDown()) beginEdit();
}

void TempoControl::mouseMove(const juce::MouseEvent&) {
    setMouseCursor(editing_ ? juce::MouseCursor::NormalCursor
                            : juce::MouseCursor::UpDownResizeCursor);
}

void TempoControl::mouseExit(const juce::MouseEvent&) {
    if (!editing_) setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}

void TempoControl::setTempo(double tempo) {
    if (editing_) return; // Never clobber text being typed.
    tempo_ = tempo;
    repaint();
}

const std::vector<double>& TempoControl::presetTempos() {
    static const std::vector<double> presets {
        70.0, 80.0, 90.0, 100.0, 110.0, 120.0, 128.0,
        140.0, 150.0, 160.0, 174.0, 180.0, 200.0
    };
    return presets;
}

juce::PopupMenu TempoControl::buildMenu(double currentTempo) {
    juce::PopupMenu menu;
    menu.addItem(1, "Tap Tempo...", true, false);
    menu.addSeparator();
    int id = 2;
    for (const double preset : presetTempos()) {
        menu.addItem(id++, juce::String(preset, 0) + " BPM", true,
                     juce::approximatelyEqual(currentTempo, preset));
    }
    return menu;
}

void TempoControl::handleMenuAction(int actionId) {
    if (actionId <= 0) return;
    if (actionId == 1) {
        if (onTempoTapped) onTempoTapped();
        return;
    }
    const auto& presets = presetTempos();
    const int index = actionId - 2;
    if (index >= 0 && index < static_cast<int>(presets.size()) && onTempoChanged)
        onTempoChanged(presets[static_cast<std::size_t>(index)]);
}

void TempoControl::showContextMenu() {
    if (showMenuOverride) {
        showMenuOverride();
        return;
    }
    auto menu = buildMenu(tempo_);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
        [safe = juce::Component::SafePointer<TempoControl>(this)](int result) {
            if (safe != nullptr && result > 0) safe->handleMenuAction(result);
        });
}

bool TempoControl::parseTempoText(const juce::String& text, double& value) {
    const auto trimmed = text.trim();
    const char* start = trimmed.toRawUTF8();
    char* end = nullptr;
    value = std::strtod(start, &end);
    return end != start && *end == '\0';
}

bool TempoControl::isInvalidEntry() const {
    if (!editing_) return false;
    double value = 0.0;
    return !parseTempoText(editor_.getText().trim(), value) ||
           value < minTempo || value > maxTempo;
}

void TempoControl::beginEdit() {
    if (editing_) return;
    editing_ = true;
    invalid_ = false;
    editor_.setText(juce::String(tempo_, 1), juce::dontSendNotification);
    editor_.setVisible(true);
    editor_.selectAll();
    if (isShowing()) editor_.grabKeyboardFocus();
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void TempoControl::commitEdit() {
    if (!editing_) return;
    double value = 0.0;
    if (!parseTempoText(editor_.getText().trim(), value) ||
        value < minTempo || value > maxTempo) {
        invalid_ = true; // Retained and tinted; the editor stays open.
        repaint();
        return;
    }
    editing_ = false;
    invalid_ = false;
    editor_.setVisible(false);
    tempo_ = value;
    if (onTempoChanged) onTempoChanged(value);
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    repaint();
}

void TempoControl::cancelEdit() {
    if (!editing_) return;
    editing_ = false;
    invalid_ = false;
    editor_.setVisible(false);
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    repaint();
}

TimeSignatureControl::TimeSignatureControl() {
    setInterceptsMouseClicks(true, false);
}

void TimeSignatureControl::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    const bool captioned = getHeight() >= 40 && getWidth() >= 88;
    theme::drawWell(g, bounds, theme::controlRadius);
    auto caption = captioned ? bounds.removeFromBottom(14).withTrimmedBottom(3)
                             : juce::Rectangle<float>();

    g.setColour(theme::textBright);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText(juce::String(numerator_) + "/" + juce::String(denominator_), bounds, juce::Justification::centred);

    if (captioned) {
        g.setColour(theme::textSecondary);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText("TIME SIGNATURE", caption, juce::Justification::centred);
    }
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
        if (stopAction_) stopAction_();
        else transportState_.stop();
    };
    
    playBtn_->onClick = [this]() {
        if (playAction_) playAction_();
        else transportState_.togglePlay();
    };
    
    recordBtn_->setEnabled(false);
    recordBtn_->setAlpha(0.35f);
    recordBtn_->setTitle("Recording unavailable");
    recordBtn_->setDescription("No recorder is connected to this transport.");
    recordBtn_->onClick = [this] { if (recordAction_) recordAction_(); };
    recordBtn_->onContextMenu = [this](const juce::MouseEvent&) {
        if (recordSetupAction_) recordSetupAction_();
    };
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
    theme::drawSurface(g, getLocalBounds().toFloat(), theme::raised, theme::panelRadius);
}

void TransportComponent::resized() {
    auto bounds = getLocalBounds().reduced(10, 10);
    const bool compact = getWidth() < 800;
    auto rightSection = bounds.removeFromRight(compact ? 250 : 300);
    timeDisplay_->setBounds(bounds.removeFromLeft(compact ? 130 : 200));

    bounds.removeFromLeft(compact ? 10 : 16);

    const int groupWidth = compact ? 118 : 262;
    auto transportButtons = bounds.removeFromLeft(juce::jmin(groupWidth, bounds.getWidth()));
    const int btnWidth = compact ? 38 : 42;
    const int btnHeight = compact ? 36 : 40;
    buttonGroupBounds_ = juce::Rectangle<int>();
    returnToStartBtn_->setVisible(!compact);
    rewindBtn_->setVisible(!compact);
    fastForwardBtn_->setVisible(!compact);
    auto place = [&](TransportButton& button) {
        auto cell = transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight);
        button.setBounds(cell);
        buttonGroupBounds_ = buttonGroupBounds_.isEmpty() ? cell : buttonGroupBounds_.getUnion(cell);
        transportButtons.removeFromLeft(2);
    };
    if (!compact) {
        place(*returnToStartBtn_);
        place(*rewindBtn_);
    }
    place(*stopBtn_);
    place(*playBtn_);
    place(*recordBtn_);
    if (!compact) place(*fastForwardBtn_);

    if (compact) {
        auto rightControls = rightSection;
        timeSigControl_->setBounds(rightControls.removeFromLeft(50).withSizeKeepingCentre(50, btnHeight));
        rightControls.removeFromLeft(10);
        tempoControl_->setBounds(rightControls.removeFromLeft(70).withSizeKeepingCentre(70, btnHeight));
        rightControls.removeFromLeft(10);
        loopBtn_->setBounds(rightControls.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
        rightControls.removeFromLeft(4);
        metronomeBtn_->setBounds(rightControls.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
        return;
    }

    auto rightControls = rightSection;
    timeSigControl_->setBounds(rightControls.removeFromLeft(104).withSizeKeepingCentre(104, 44));
    rightControls.removeFromLeft(10);
    tempoControl_->setBounds(rightControls.removeFromLeft(92).withSizeKeepingCentre(92, 44));
    rightControls.removeFromLeft(12);
    loopBtn_->setBounds(rightControls.removeFromLeft(38).withSizeKeepingCentre(38, btnHeight));
    rightControls.removeFromLeft(2);
    metronomeBtn_->setBounds(rightControls.removeFromLeft(38).withSizeKeepingCentre(38, btnHeight));
}

void TransportComponent::updateButtonStates() {
    playBtn_->setActive(recorderActive_ ? recorderPlaying_ : transportState_.isPlaying());
    recordBtn_->setActive(recordAction_ ? recorderRecording_ : transportState_.isRecording());
    loopBtn_->setActive(transportState_.isLoopEnabled());
    metronomeBtn_->setActive(transportState_.isMetronomeEnabled());
}

void TransportComponent::setRecordAction(std::function<void()> action, std::function<void()> setupAction) {
    recordAction_ = std::move(action);
    recordSetupAction_ = std::move(setupAction);
    recordBtn_->setEnabled(static_cast<bool>(recordAction_));
    recordBtn_->setAlpha(recordAction_ ? 1.0f : 0.35f);
    recordBtn_->setTitle(recordAction_ ? "Record: setup / toggle recording; right-click for setup" : "Recording unavailable");
    recordBtn_->setDescription(recordAction_ ? "Record off keeps playback running. Right-click for target, modes, takes and recording undo."
                                           : "No recorder is connected to this transport.");
    updateButtonStates();
}

void TransportComponent::setPlaybackActions(std::function<void()> play, std::function<void()> stop) {
    playAction_ = std::move(play);
    stopAction_ = std::move(stop);
}

void TransportComponent::setRecorderState(bool active, bool recording, bool playing) {
    recorderActive_ = active;
    recorderRecording_ = recording;
    recorderPlaying_ = playing;
    // Song navigation/loop controls must not appear to seek the independent clip clock.
    for (auto* button : {returnToStartBtn_.get(), rewindBtn_.get(), fastForwardBtn_.get(), loopBtn_.get(), metronomeBtn_.get()}) {
        button->setEnabled(!active);
        button->setAlpha(active ? 0.35f : 1.0f);
    }
    tempoControl_->setEnabled(!active);
    timeSigControl_->setEnabled(!active);
    tempoControl_->setAlpha(active ? 0.35f : 1.0f);
    timeSigControl_->setAlpha(active ? 0.35f : 1.0f);
    updateButtonStates();
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
                                      theme::textMuted);
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
    validation_.setColour(juce::Label::textColourId, theme::textBright);
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
        validation_.setColour(juce::Label::textColourId, theme::dangerText);
        validation_.setText("Invalid: 0 <= start; end <= 1e9; length >= 1/64 qn", juce::dontSendNotification);
        return;
    }
    transportState_.setLoopRegion(start, end);
    transportState_.setLoopEnabled(true);
    if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
}

void LoopEditorPopover::paint(juce::Graphics& g) {
    g.fillAll(theme::raised);
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
