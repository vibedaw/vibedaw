#include "TransportComponent.h"
#include "utils/Logger.h"

namespace vibedaw {

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

void TimeDisplay::setPosition(double positionInSeconds, double tempo) {
    position_ = positionInSeconds;
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
    double beatsPerSecond = tempo_ / 60.0;
    double totalBeats = position_ * beatsPerSecond;
    double beatsPerBar = timeSigNumerator_;
    
    int bar = static_cast<int>(totalBeats / beatsPerBar) + 1;
    int beat = static_cast<int>(totalBeats) % timeSigNumerator_ + 1;
    int ticks = static_cast<int>((totalBeats - static_cast<int>(totalBeats)) * 960.0);
    
    return juce::String::formatted("%d:%d:%03d", bar, beat, ticks);
}

juce::String TimeDisplay::formatTimeCode() {
    int totalSeconds = static_cast<int>(position_);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    int millis = static_cast<int>((position_ - totalSeconds) * 100.0);
    
    return juce::String::formatted("%d:%02d:%02d.%02d", hours, minutes, seconds, millis);
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
    auto iconPath = createIcon();
    g.fillPath(iconPath, iconPath.getTransformToScaleToFit(iconBounds, true));
}

juce::Path TransportButton::createIcon() {
    juce::Path path;
    
    switch (type_) {
        case Type::ReturnToStart:
            path.addTriangle(0.2f, 0.5f, 0.6f, 0.2f, 0.6f, 0.8f);
            path.addRectangle(0.1f, 0.2f, 0.15f, 0.6f);
            break;
            
        case Type::Rewind:
            path.addTriangle(0.5f, 0.5f, 0.9f, 0.1f, 0.9f, 0.9f);
            path.addTriangle(0.1f, 0.5f, 0.5f, 0.1f, 0.5f, 0.9f);
            break;
            
        case Type::Stop:
            path.addRectangle(0.15f, 0.15f, 0.7f, 0.7f);
            break;
            
        case Type::Play:
            path.addTriangle(0.2f, 0.1f, 0.2f, 0.9f, 0.9f, 0.5f);
            break;
            
        case Type::Record:
            path.addEllipse(0.15f, 0.15f, 0.7f, 0.7f);
            break;
            
        case Type::FastForward:
            path.addTriangle(0.1f, 0.1f, 0.1f, 0.9f, 0.5f, 0.5f);
            path.addTriangle(0.5f, 0.1f, 0.5f, 0.9f, 0.9f, 0.5f);
            break;
            
        case Type::Loop:
            path.addRectangle(0.15f, 0.15f, 0.7f, 0.3f);
            path.addRectangle(0.15f, 0.55f, 0.7f, 0.3f);
            path.addTriangle(0.75f, 0.3f, 0.85f, 0.3f, 0.8f, 0.5f);
            path.addTriangle(0.25f, 0.7f, 0.15f, 0.7f, 0.2f, 0.5f);
            break;
            
        case Type::Metronome:
            path.addEllipse(0.35f, 0.15f, 0.3f, 0.3f);
            path.addRectangle(0.45f, 0.45f, 0.1f, 0.4f);
            path.addRectangle(0.25f, 0.8f, 0.5f, 0.1f);
            break;
    }
    
    return path;
}

void TransportButton::mouseDown(const juce::MouseEvent&) {
    pressed_ = true;
    repaint();
}

void TransportButton::mouseUp(const juce::MouseEvent&) {
    pressed_ = false;
    repaint();
    if (onClick) onClick();
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
        double currentPos = transportState_.getPosition();
        transportState_.setPosition(std::max(0.0, currentPos - 1.0));
    };
    
    stopBtn_->onClick = [this]() {
        transportState_.stop();
    };
    
    playBtn_->onClick = [this]() {
        transportState_.togglePlay();
    };
    
    recordBtn_->onClick = [this]() {
        if (!transportState_.isPlaying()) {
            transportState_.setPlaying(true);
        }
        transportState_.toggleRecord();
    };
    
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
    
    auto leftSection = bounds.removeFromLeft(200);
    auto rightSection = bounds.removeFromRight(250);
    
    timeDisplay_->setBounds(bounds.removeFromLeft(180).withTrimmedTop(2).withTrimmedBottom(2));
    
    bounds.removeFromLeft(20);
    
    auto transportButtons = bounds.removeFromLeft(240);
    int btnWidth = 36;
    int btnHeight = 28;
    
    returnToStartBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
    transportButtons.removeFromLeft(4);
    rewindBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
    transportButtons.removeFromLeft(4);
    stopBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
    transportButtons.removeFromLeft(4);
    playBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
    transportButtons.removeFromLeft(4);
    recordBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
    transportButtons.removeFromLeft(4);
    fastForwardBtn_->setBounds(transportButtons.removeFromLeft(btnWidth).withSizeKeepingCentre(btnWidth, btnHeight));
    
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

void TransportComponent::transportPositionChanged(double positionInSeconds) {
    timeDisplay_->setPosition(positionInSeconds, transportState_.getTempo());
}

void TransportComponent::transportTempoChanged(double tempo) {
    tempoControl_->setTempo(tempo);
    timeDisplay_->setPosition(transportState_.getPosition(), tempo);
}

void TransportComponent::transportTimeSignatureChanged(int numerator, int denominator) {
    timeSigControl_->setTimeSignature(numerator, denominator);
    timeDisplay_->setTimeSignature(numerator, denominator);
}

void TransportComponent::transportLoopChanged(bool enabled, double, double) {
    loopBtn_->setActive(enabled);
}

void TransportComponent::transportMetronomeChanged(bool enabled) {
    metronomeBtn_->setActive(enabled);
}

} // namespace vibedaw
