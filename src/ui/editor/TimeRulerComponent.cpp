#include "TimeRulerComponent.h"
#include "ui/Theme.h"

namespace vibedaw {

TimeRulerComponent::TimeRulerComponent() {
    setOpaque(false);
}

void TimeRulerComponent::setPixelsPerBeat(int pixels) {
    pixelsPerBeat_ = juce::jmax(10, pixels);
    repaint();
}

void TimeRulerComponent::setTimeOffset(double beats) {
    timeOffset_ = beats;
    repaint();
}

void TimeRulerComponent::setBeatsPerMeasure(int beats) {
    beatsPerMeasure_ = juce::jmax(1, beats);
    repaint();
}

void TimeRulerComponent::setTempo(double bpm) {
    tempo_ = juce::jmax(1.0, bpm);
    repaint();
}

juce::String TimeRulerComponent::formatTime(double beats) const {
    int measure = static_cast<int>(beats / beatsPerMeasure_) + 1;
    int beat = static_cast<int>(beats) % beatsPerMeasure_ + 1;
    return juce::String(measure) + "." + juce::String(beat);
}

void TimeRulerComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    g.fillAll(theme::controlDim);
    
    g.setColour(theme::separator);
    g.drawHorizontalLine(bounds.getHeight() - 1, 0.0f, static_cast<float>(bounds.getWidth()));
    
    g.setColour(theme::textDim);
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(bounds.getWidth()));
    
    g.setFont(10.0f);
    g.setColour(theme::white.withAlpha(0.7f));
    
    int beatIndex = static_cast<int>(timeOffset_);
    double fracOffset = timeOffset_ - beatIndex;
    int startX = -static_cast<int>(fracOffset * pixelsPerBeat_);
    
    for (int x = startX; x < bounds.getWidth(); x += pixelsPerBeat_) {
        double beats = timeOffset_ + static_cast<double>(x - startX) / pixelsPerBeat_;
        int beatInMeasure = static_cast<int>(beats) % beatsPerMeasure_;
        
        bool isDownbeat = beatInMeasure == 0;
        
        if (isDownbeat) {
            g.setColour(theme::textSubtle);
            g.drawVerticalLine(x, 0.0f, static_cast<float>(bounds.getHeight()));
            
            g.setColour(theme::white);
            g.setFont(11.0f);
            g.drawText(formatTime(beats), x + 3, 2, 40, bounds.getHeight() - 4,
                       juce::Justification::centredLeft, true);
        } else {
            g.setColour(theme::gridBarStrong);
            g.drawVerticalLine(x, bounds.getHeight() / 2, static_cast<float>(bounds.getHeight()));
            
            g.setColour(theme::white.withAlpha(0.5f));
            g.setFont(9.0f);
            g.drawText(juce::String(beatInMeasure + 1), x + 2, 2, 20, bounds.getHeight() - 4,
                       juce::Justification::centredLeft, true);
        }
    }

    if (playheadBeats_ >= 0.0) {
        const int x = static_cast<int>((playheadBeats_ - timeOffset_) * pixelsPerBeat_);
        g.setColour(theme::accent);
        g.fillRect(static_cast<float>(x), 0.0f, 2.0f, static_cast<float>(bounds.getHeight()));
    }
}

} // namespace vibedaw