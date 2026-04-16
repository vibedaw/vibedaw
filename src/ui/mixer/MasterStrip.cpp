#include "MasterStrip.h"
#include <cmath>

namespace vibedaw {

class MasterStrip::MasterFader : public juce::Component {
public:
    MasterFader() = default;
    
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds();
        int width = bounds.getWidth();
        int height = bounds.getHeight();
        
        g.fillAll(juce::Colour(0xff1a1a1a));
        
        int trackWidth = 10;
        int trackX = (width - trackWidth) / 2;
        int trackMargin = 2;
        int trackHeight = height - trackMargin * 2;
        
        g.setColour(juce::Colour(0xff2a2a2a));
        g.fillRect(trackX, trackMargin, trackWidth, trackHeight);
        
        g.setColour(juce::Colour(0xff3a3a3a));
        g.drawRect(trackX, trackMargin, trackWidth, trackHeight, 1);
        
        int capHeight = 16;
        int capWidth = width - 4;
        int capY = static_cast<int>(trackMargin + (1.0f - value) * (trackHeight - capHeight));
        int capX = 2;
        
        g.setColour(juce::Colour(0xff4a4a4a));
        g.fillRoundedRectangle(static_cast<float>(capX), static_cast<float>(capY), 
                                static_cast<float>(capWidth), static_cast<float>(capHeight), 4.0f);
        
        juce::Colour capColour = juce::Colour(0xff666666);
        if (value < 0.1f) {
            capColour = juce::Colour(0xffaaaaaa);
        }
        
        g.setColour(capColour);
        g.fillRoundedRectangle(static_cast<float>(capX + 2), static_cast<float>(capY + 2), 
                                static_cast<float>(capWidth - 4), static_cast<float>(capHeight - 4), 3.0f);
        
        g.setColour(juce::Colour(0xffaaaaaa));
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
        value = 0.85f;
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
    float value = 0.85f;
    int dragStartY = 0;
    float dragStartValue = 0.85f;
};

MasterStrip::MasterStrip() {
    leftMeter = std::make_unique<LevelMeter>();
    leftMeter->setMeterWidth(6);
    addAndMakeVisible(*leftMeter);
    
    rightMeter = std::make_unique<LevelMeter>();
    rightMeter->setMeterWidth(6);
    addAndMakeVisible(*rightMeter);
    
    fader = std::make_unique<MasterFader>();
    fader->onValueChanged = [this](float v) {
        volume = v;
        if (onVolumeChanged) {
            onVolumeChanged(v);
        }
    };
    addAndMakeVisible(*fader);
}

MasterStrip::~MasterStrip() = default;

void MasterStrip::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    g.fillAll(juce::Colour(0xff1e1e1e));
    
    g.setColour(juce::Colour(0xff444444));
    g.fillRect(0, 0, 3, bounds.getHeight());
    
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawVerticalLine(0, 0.0f, static_cast<float>(bounds.getHeight()));
    
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("MSTR", 5, 5, bounds.getWidth() - 10, 16, juce::Justification::centred);
    
    g.setColour(juce::Colour(0xff888888));
    g.setFont(juce::Font(9.0f));
    
    float dbValue = 20.0f * std::log10(volume > 0.0f ? volume : 0.0001f);
    g.drawText(juce::String(dbValue, 1) + " dB", 5, bounds.getHeight() - 18, 
               bounds.getWidth() - 10, 14, juce::Justification::centred);
}

void MasterStrip::resized() {
    updateComponentPositions();
}

void MasterStrip::updateComponentPositions() {
    auto bounds = getLocalBounds();
    int width = bounds.getWidth();
    int height = bounds.getHeight();
    int margin = 4;
    int topHeight = 24;
    int bottomHeight = 20;
    int meterWidth = 12;
    int meterSpacing = 2;
    int totalMetersWidth = meterWidth * 2 + meterSpacing;
    
    int meterAreaX = (width - totalMetersWidth) / 2;
    int faderWidth = width - margin * 2;
    int faderStart = topHeight;
    int faderHeight = height - topHeight - bottomHeight - margin;
    
    leftMeter->setBounds(meterAreaX, faderStart, meterWidth, faderHeight);
    rightMeter->setBounds(meterAreaX + meterWidth + meterSpacing, faderStart, meterWidth, faderHeight);
    
    fader->setBounds(margin, faderStart + faderHeight + 4, faderWidth, bottomHeight - 8);
}

void MasterStrip::setVolume(float v) {
    volume = v;
    fader->setValue(v);
}

void MasterStrip::setLevels(float left, float right) {
    leftMeter->setLevels(left, right);
    rightMeter->setLevels(right, left);
}

} // namespace vibedaw
