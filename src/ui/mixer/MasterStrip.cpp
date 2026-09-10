#include "MasterStrip.h"
#include "ui/Theme.h"
#include <cmath>

namespace vibedaw {

class MasterStrip::MasterFader : public juce::Component {
public:
    MasterFader() = default;
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds();
        int width = bounds.getWidth();
        int height = bounds.getHeight();
        
        theme::drawWell(g, bounds.toFloat().reduced(0.5f));
        
        int trackWidth = 10;
        int trackX = (width - trackWidth) / 2;
        int trackMargin = 2;
        int trackHeight = height - trackMargin * 2;
        
        theme::drawWell(g, juce::Rectangle<float>(static_cast<float>(trackX), static_cast<float>(trackMargin),
                                                static_cast<float>(trackWidth), static_cast<float>(trackHeight)), 2.0f);

        // Same linear dB marks as the channel faders (gain = value * 2).
        g.setColour(theme::tickMark);
        const float gainMarks[] = {1.0f, 0.501f, 0.251f, 0.126f, 0.032f};
        int capHeight = 16;
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
        juce::ColourGradient metal(theme::textBright, 0.0f, cap.getY(),
                                   theme::controlHover, 0.0f, cap.getBottom(), false);
        metal.addColour(0.45, theme::textSecondary);
        metal.addColour(0.5, theme::controlSelected);
        g.setGradientFill(metal);
        g.fillRoundedRectangle(cap, 3.0f);
        g.setColour(theme::textBright.withAlpha(0.4f));
        g.drawRoundedRectangle(cap.reduced(0.5f), 3.0f, 1.0f);
        g.setColour(theme::deepWell);
        g.drawHorizontalLine(capY + capHeight / 2, capX + 4, capX + capWidth - 4);
    }
    
    void mouseDown(const juce::MouseEvent& e) override {
        dragStartY = e.y;
        dragStartValue = value;
    }
    
    void mouseDrag(const juce::MouseEvent& e) override {
        int height = getHeight();
        if (height > 0) {
            float dragDistance = static_cast<float>(e.y - dragStartY) / height;
            float newValue = dragStartValue - dragDistance;
            newValue = juce::jlimit(0.0f, 1.0f, newValue);
            
            if (std::abs(newValue - value) > 0.001f) {
                value = newValue;
                repaint();
                if (onValueChanged) {
                    onValueChanged(value);
                }
            }
        }
    }
    
    void mouseDoubleClick(const juce::MouseEvent&) override {
        value = 0.5f;
        repaint();
        if (onValueChanged) {
            onValueChanged(value);
        }
    }
    
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        float delta = -wheel.deltaY * 0.03f;
        float newValue = juce::jlimit(0.0f, 1.0f, value + delta);
        
        if (newValue != value) {
            value = newValue;
            repaint();
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

MasterStrip::MasterStrip() {
    leftMeter = std::make_unique<LevelMeter>();
    leftMeter->setMeterWidth(8);
    addAndMakeVisible(*leftMeter);
    
    fader = std::make_unique<MasterFader>();
    fader->onValueChanged = [this](float v) {
        volume = v * 2.0f;
        repaint();
        if (onVolumeChanged) {
            onVolumeChanged(volume);
        }
    };
    addAndMakeVisible(*fader);
    muteButton.setClickingTogglesState(true);
    muteButton.setColour(juce::TextButton::buttonColourId, theme::control);
    muteButton.setColour(juce::TextButton::buttonOnColourId, theme::control.interpolatedWith(theme::muteRed, 0.2f));
    muteButton.setColour(juce::TextButton::textColourOffId, theme::textDefault);
    muteButton.setColour(juce::TextButton::textColourOnId, theme::muteRed);
    muteButton.onClick = [this] { if (onMuteToggled) onMuteToggled(muteButton.getToggleState()); };
    addAndMakeVisible(muteButton);
}

MasterStrip::~MasterStrip() = default;

void MasterStrip::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    g.fillAll(theme::panelBackground);

    theme::drawSurface(g, bounds.toFloat().reduced(0.5f), theme::control, theme::panelRadius);
    g.setColour(theme::accent.withAlpha(0.65f));
    g.fillRoundedRectangle(6.0f, 23.0f, static_cast<float>(bounds.getWidth() - 12), 2.0f, 1.0f);

    g.setColour(theme::textBright);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("MASTER", 5, 4, bounds.getWidth() - 10, 15, juce::Justification::centred);

    g.setColour(theme::textSecondary);
    g.setFont(juce::Font(9.0f));

    const auto label = volume > 0 ? juce::String(juce::Decibels::gainToDecibels(volume), 1) + " dB" : "-inf dB";
    g.drawText(label, 5, bounds.getHeight() - 18,
               bounds.getWidth() - 10, 14, juce::Justification::centred);
}

void MasterStrip::resized() {
    updateComponentPositions();
}

void MasterStrip::updateComponentPositions() {
    auto bounds = getLocalBounds().reduced(6);
    bounds.removeFromTop(22);
    muteButton.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(4);
    bounds.removeFromBottom(20);
    leftMeter->setBounds(bounds.removeFromRight(18));
    bounds.removeFromRight(4);
    fader->setBounds(bounds);
    leftMeter->setShowPeakReadout(true);
}

void MasterStrip::setVolume(float v) {
    volume = v;
    fader->setValue(v * 0.5f);
    repaint();
}

void MasterStrip::pollMeter(const StereoMeter& source) {
    leftMeter->poll(source);
}

} // namespace vibedaw
