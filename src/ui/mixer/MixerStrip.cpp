#include "MixerStrip.h"
#include "ui/Theme.h"
#include <cmath>

namespace vibedaw {

class MixerStrip::FaderComponent : public juce::Component {
public:
    FaderComponent() {
        setMouseClickGrabsKeyboardFocus(false);
    }
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds();
        int width = bounds.getWidth();
        int height = bounds.getHeight();
        
        theme::drawWell(g, bounds.toFloat().reduced(0.5f));
        
        int trackWidth = 6;
        int trackX = (width - trackWidth) / 2;
        int trackMargin = 2;
        int trackHeight = height - trackMargin * 2;
        
        theme::drawWell(g, juce::Rectangle<float>(static_cast<float>(trackX), static_cast<float>(trackMargin),
                                                static_cast<float>(trackWidth), static_cast<float>(trackHeight)), 2.0f);

        // dB marks on the linear fader (gain = value * 2), mapped to cap travel.
        g.setColour(theme::tickMark);
        const float gainMarks[] = {1.0f, 0.501f, 0.251f, 0.126f, 0.032f};
        int capHeight = 12;
        for (float gain : gainMarks) {
            const int y = trackMargin + static_cast<int>((1.0f - gain * 0.5f) * (trackHeight - capHeight)) + capHeight / 2;
            if (y < trackMargin || y >= height - trackMargin) continue;
            g.drawHorizontalLine(y, 5.0f, static_cast<float>(trackX - 3));
            g.drawHorizontalLine(y, static_cast<float>(trackX + trackWidth + 3), static_cast<float>(width - 5));
        }

        int capWidth = width - 4;
        int capY = static_cast<int>(trackMargin + (1.0f - value) * (trackHeight - capHeight));
        int capX = 2;
        
        const auto cap = juce::Rectangle<float>(static_cast<float>(capX), static_cast<float>(capY),
                                                 static_cast<float>(capWidth), static_cast<float>(capHeight));
        g.setColour(theme::black.withAlpha(0.4f));
        g.fillRoundedRectangle(cap.translated(0.0f, 2.0f), 3.0f);
        juce::ColourGradient metal(isHovered ? theme::textBright : theme::textDefault, 0.0f, cap.getY(),
                                   theme::controlHover, 0.0f, cap.getBottom(), false);
        metal.addColour(0.45, theme::textSecondary);
        metal.addColour(0.5, theme::controlSelected);
        g.setGradientFill(metal);
        g.fillRoundedRectangle(cap, 3.0f);
        g.setColour(theme::textBright.withAlpha(0.35f));
        g.drawRoundedRectangle(cap.reduced(0.5f), 3.0f, 1.0f);
        g.setColour(theme::deepWell);
        g.drawHorizontalLine(capY + capHeight / 2, capX + 3, capX + capWidth - 3);
    }
    
    void mouseDown(const juce::MouseEvent& e) override {
        dragStartY = e.y;
        dragStartValue = value;
    }
    
    void mouseDrag(const juce::MouseEvent& e) override {
        int height = getHeight();
        float dragDistance = static_cast<float>(e.y - dragStartY) / juce::jmax(1, height);
        float newValue = dragStartValue - dragDistance;
        
        newValue = juce::jlimit(0.0f, 1.0f, newValue);
        
        if (std::abs(newValue - value) > 0.001f) {
            setValue(newValue);
            if (onValueChanged) {
                onValueChanged(value);
            }
        }
    }
    
    void mouseEnter(const juce::MouseEvent&) override {
        isHovered = true;
        repaint();
    }
    
    void mouseExit(const juce::MouseEvent&) override {
        isHovered = false;
        repaint();
    }
    
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        float delta = -wheel.deltaY * 0.05f;
        float newValue = juce::jlimit(0.0f, 1.0f, value + delta);
        
        if (newValue != value) {
            setValue(newValue);
            if (onValueChanged) {
                onValueChanged(value);
            }
        }
    }
    
    void setValue(float v) {
        value = juce::jlimit(0.0f, 1.0f, v);
        repaint();
    }
    
    float getValue() const { return value; }
    
    std::function<void(float)> onValueChanged;
    
private:
    float value = 0.5f;
    int dragStartY = 0;
    float dragStartValue = 0.0f;
    bool isHovered = false;
};

class MixerStrip::PanKnobComponent : public juce::Component {
public:
    PanKnobComponent() = default;
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds();
        int size = std::min(bounds.getWidth(), bounds.getHeight()) - 4;
        int x = (bounds.getWidth() - size) / 2;
        int y = (bounds.getHeight() - size) / 2;
        
        float radius = static_cast<float>(size) / 2.0f;
        float centreX = static_cast<float>(x) + radius;
        float centreY = static_cast<float>(y) + radius;

        const float startAngle = juce::MathConstants<float>::pi * -0.75f;
        const float sweep = juce::MathConstants<float>::pi * 1.5f;

        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                              startAngle, startAngle + sweep, true);
        g.setColour(theme::border);
        g.strokePath(trackArc, juce::PathStrokeType(2.0f));

        float angle = startAngle + value * sweep;
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                              startAngle + sweep * 0.5f, angle, true);
        const bool centred = std::abs(value - 0.5f) < 0.02f;
        g.setColour(centred ? theme::textSecondary : theme::accent);
        g.strokePath(valueArc, juce::PathStrokeType(2.0f));

        float bodyRadius = radius * 0.78f;
        g.setColour(theme::black.withAlpha(0.45f));
        g.fillEllipse(centreX - bodyRadius, centreY - bodyRadius + 2.0f, bodyRadius * 2.0f, bodyRadius * 2.0f);
        g.setGradientFill(juce::ColourGradient(theme::controlSelected, centreX, centreY - bodyRadius,
                                             theme::deepWell, centreX, centreY + bodyRadius, false));
        g.fillEllipse(centreX - bodyRadius, centreY - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
        g.setColour(theme::borderStrong);
        g.drawEllipse(centreX - bodyRadius, centreY - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

        g.setColour(theme::white);
        g.drawLine(centreX + std::sin(angle) * bodyRadius * 0.25f, centreY - std::cos(angle) * bodyRadius * 0.25f,
                   centreX + std::sin(angle) * bodyRadius * 0.85f, centreY - std::cos(angle) * bodyRadius * 0.85f, 2.0f);
        g.setColour(theme::textSecondary);
        g.fillEllipse(centreX - 1.5f, centreY - 1.5f, 3.0f, 3.0f);
    }
    
    void mouseDown(const juce::MouseEvent& e) override {
        dragStartY = e.y;
        dragStartValue = value;
    }
    
    void mouseDrag(const juce::MouseEvent& e) override {
        int height = getHeight();
        if (height > 0) {
            float dragDistance = static_cast<float>(dragStartY - e.y) / static_cast<float>(height);
            float newValue = juce::jlimit(0.0f, 1.0f, dragStartValue + dragDistance);
            
            if (std::abs(newValue - value) > 0.001f) {
                setValue(newValue);
                if (onValueChanged) {
                    onValueChanged(value);
                }
            }
        }
    }
    
    void mouseDoubleClick(const juce::MouseEvent&) override {
        setValue(0.5f);
        if (onValueChanged) {
            onValueChanged(0.5f);
        }
    }
    
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        float delta = wheel.deltaY * 0.05f;
        float newValue = juce::jlimit(0.0f, 1.0f, value + delta);
        
        if (newValue != value) {
            setValue(newValue);
            if (onValueChanged) {
                onValueChanged(value);
            }
        }
    }
    
    void setValue(float v) {
        value = juce::jlimit(0.0f, 1.0f, v);
        repaint();
    }
    
    float getValue() const { return value; }
    
    std::function<void(float)> onValueChanged;
    
private:
    float value = 0.5f;
    int dragStartY = 0;
    float dragStartValue = 0.5f;
};

class MixerStrip::SmallButton : public juce::Component {
public:
    SmallButton(const juce::String& label) : buttonLabel(label) {}
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds();
        
        juce::Colour bgColour = isDown ? theme::controlPressed : 
                                isHovered ? theme::controlHover : theme::control;
        juce::Colour textColour = isDown ? theme::white : theme::textDefault;
        
        const auto activeColour = buttonLabel == "M" ? theme::muteRed : theme::accent;
        if (isToggled) {
            bgColour = theme::control.interpolatedWith(activeColour, 0.2f);
            textColour = activeColour;
        }

        theme::drawSurface(g, bounds.toFloat().reduced(0.5f), bgColour, theme::controlRadius,
                           isToggled ? activeColour.withAlpha(0.6f) : theme::border);
        
        g.setColour(textColour);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(buttonLabel, bounds, juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent&) override {
        isDown = true;
        repaint();
    }
    
    void mouseUp(const juce::MouseEvent&) override {
        isDown = false;
        isToggled = !isToggled;
        repaint();
        if (onClicked) {
            onClicked(isToggled);
        }
    }
    
    void mouseEnter(const juce::MouseEvent&) override {
        isHovered = true;
        repaint();
    }
    
    void mouseExit(const juce::MouseEvent&) override {
        isHovered = false;
        isDown = false;
        repaint();
    }
    
    void setToggled(bool toggled) {
        isToggled = toggled;
        repaint();
    }
    
    bool getToggled() const { return isToggled; }
    
    std::function<void(bool)> onClicked;
    
private:
    juce::String buttonLabel;
    bool isHovered = false;
    bool isDown = false;
    bool isToggled = false;
};

MixerStrip::MixerStrip(int id)
    : mixerChannelId(id)
{
    setTooltip("Independent audio mixer channel, output to Master. Multiple instruments can feed this channel. Click to select; right-click to rename. Live instrument selection is unchanged.");
    meter = std::make_unique<LevelMeter>();
    meter->setMeterStyle(true);
    addAndMakeVisible(*meter);
    
    fader = std::make_unique<FaderComponent>();
    fader->onValueChanged = [this](float v) {
        volume = v * 2.0f;
        repaint();
        if (onVolumeChanged) {
            onVolumeChanged(volume);
        }
    };
    addAndMakeVisible(*fader);
    
    panKnob = std::make_unique<PanKnobComponent>();
    panKnob->onValueChanged = [this](float v) {
        pan = (v - 0.5f) * 2.0f;
        if (onPanChanged) {
            onPanChanged(pan);
        }
    };
    addAndMakeVisible(*panKnob);
    
    muteButton = std::make_unique<SmallButton>("M");
    muteButton->onClicked = [this](bool toggled) {
        muted = toggled;
        if (onMuteToggled) {
            onMuteToggled(toggled);
        }
    };
    addAndMakeVisible(*muteButton);
    
    soloButton = std::make_unique<SmallButton>("S");
    soloButton->onClicked = [this](bool toggled) {
        solo = toggled;
        if (onSoloToggled) {
            onSoloToggled(toggled);
        }
    };
    addAndMakeVisible(*soloButton);
    
    fxButton = std::make_unique<SmallButton>("FX");
    addAndMakeVisible(*fxButton);
    fxButton->setEnabled(false);
    fxButton->setAlpha(0.35f);
}

MixerStrip::~MixerStrip() = default;

juce::PopupMenu MixerStrip::createContextMenu() const {
    juce::PopupMenu menu;
    menu.addItem("Rename Mixer Channel...", true, false, onRenameRequested);
    return menu;
}

void MixerStrip::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) {
        createContextMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
        return;
    }
    if (e.mods.isLeftButtonDown() && onStripSelected) onStripSelected();
}

void MixerStrip::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    g.fillAll(theme::panelBackground);
    theme::drawSurface(g, bounds.toFloat().reduced(0.5f),
                       selected ? theme::selectedSurface : theme::raised, theme::panelRadius,
                       selected ? theme::accent.withAlpha(0.5f) : theme::border);
    g.setColour(trackColour.withAlpha(selected ? 0.14f : 0.07f));
    g.fillRoundedRectangle(bounds.toFloat().reduced(2.0f).withHeight(28.0f), theme::controlRadius);

    g.setColour(selected ? theme::white : theme::textDefault);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(trackName, 3, 4, bounds.getWidth() - 6, 15, juce::Justification::centred, true);

    {
        g.setColour(theme::textSecondary);
        g.setFont(juce::Font(9.0f));
        const auto label = volume > 0 ? juce::String(juce::Decibels::gainToDecibels(volume), 1) + " dB" : "-inf dB";
        g.drawText(label, 3, 18, bounds.getWidth() - 6, 11,
                   juce::Justification::centred);
    }

    g.setColour(trackColour.withAlpha(selected ? 0.9f : 0.6f));
    g.fillRect(6, 29, bounds.getWidth() - 12, 2);

    g.setColour(theme::textMuted);
    g.setFont(9.0f);
    g.drawText("L", 9, 56, 12, 18, juce::Justification::centred);
    g.drawText("R", bounds.getWidth() - 21, 56, 12, 18, juce::Justification::centred);
}

void MixerStrip::resized() {
    updateComponentPositions();
}

void MixerStrip::updateComponentPositions() {
    auto bounds = getLocalBounds();
    int width = bounds.getWidth();
    int margin = 6;
    int spacing = 4;

    int topSectionHeight = 32;
    int panKnobSize = 28;
    int buttonWidth = (width - margin * 2 - spacing) / 2;
    int buttonHeight = 16;
    int meterWidth = 18;
    int faderWidth = width - margin * 2 - meterWidth - spacing;

    int y = topSectionHeight;

    fxButton->setBounds(margin, y, width - margin * 2, buttonHeight);
    y += buttonHeight + spacing;

    panKnob->setBounds((width - panKnobSize) / 2, y, panKnobSize, panKnobSize);
    y += panKnobSize + spacing;

    muteButton->setBounds(margin, y, buttonWidth, buttonHeight);
    soloButton->setBounds(margin + buttonWidth + spacing, y, buttonWidth, buttonHeight);
    y += buttonHeight + spacing;

    int remainingHeight = bounds.getHeight() - y - margin;
    int faderHeight = std::max(40, remainingHeight);

    meter->setBounds(width - margin - meterWidth, y, meterWidth, faderHeight);
    fader->setBounds(margin, y, faderWidth, faderHeight);
    meter->setShowPeakReadout(true);
}

void MixerStrip::setTrackName(const juce::String& name) {
    trackName = name;
    setTitle("Mixer channel: " + name);
    repaint();
}

void MixerStrip::setTrackColour(const juce::Colour& colour) {
    trackColour = colour;
    repaint();
}

void MixerStrip::setVolume(float v) {
    volume = v;
    fader->setValue(v * 0.5f);
    repaint();
}

void MixerStrip::setPan(float p) {
    pan = p;
    panKnob->setValue((p + 1.0f) * 0.5f);
}

void MixerStrip::setMuted(bool m) {
    muted = m;
    muteButton->setToggled(m);
}

void MixerStrip::setSolo(bool s) {
    solo = s;
    soloButton->setToggled(s);
}

void MixerStrip::pollMeter(const StereoMeter& source) {
    meter->poll(source);
}

void MixerStrip::setSelected(bool s) {
    selected = s;
    repaint();
}

} // namespace vibedaw
