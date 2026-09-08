#include "LevelMeter.h"
#include <cmath>

namespace vibedaw {

LevelMeter::LevelMeter() {
    startTimerHz(30);
    lastTime = juce::Time::getMillisecondCounterHiRes();
}

LevelMeter::~LevelMeter() {
    stopTimer();
}

void LevelMeter::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    int width = bounds.getWidth();
    int height = bounds.getHeight();
    
    if (width < 4 || height < 4) return;
    
    int totalMeterWidth = meterWidth * 2 + meterSpacing;
    int startX = (width - totalMeterWidth) / 2;
    
    auto bgColour = juce::Colour(0xff1a1a1a);
    auto meterBgColour = juce::Colour(0xff0d0d0d);
    auto trackColour = juce::Colour(0xff2a2a2a);
    
    g.fillAll(bgColour);
    
    auto drawMeter = [&](float level, float peak, int x) {
        int meterHeight = height - 4;
        int meterY = 2;
        
        g.setColour(meterBgColour);
        g.fillRect(x, meterY, meterWidth, meterHeight);
        
        g.setColour(trackColour);
        g.drawRect(x, meterY, meterWidth, meterHeight);
        
        float clampedLevel = juce::jlimit(0.0f, 1.0f, level);
        float clampedPeak = juce::jlimit(0.0f, 1.0f, peak);
        
        int levelHeight = static_cast<int>(clampedLevel * meterHeight);
        int peakY = meterY + meterHeight - static_cast<int>(clampedPeak * meterHeight);
        
        if (levelHeight > 0) {
            juce::Colour lowColour(0xff666666);
            juce::Colour midColour(0xffaaaaaa);
            juce::Colour highColour(0xffffffff);
            
            float greenZone = 0.6f;
            float yellowZone = 0.85f;
            
            if (meterStyle == MeterStyle::Gradient) {
                int gradientStart = meterY + meterHeight - levelHeight;
                int gradientEnd = meterY + meterHeight;
                
                juce::ColourGradient gradient(
                    highColour, static_cast<float>(x), static_cast<float>(gradientStart),
                    lowColour, static_cast<float>(x), static_cast<float>(gradientEnd),
                    false
                );
                
                gradient.addColour(juce::jlimit(0.0f, 1.0f, (1.0f - yellowZone) * meterHeight / levelHeight), midColour);
                gradient.addColour(juce::jlimit(0.0f, 1.0f, (1.0f - greenZone) * meterHeight / levelHeight), lowColour);
                
                g.setGradientFill(gradient);
                g.fillRect(x, gradientStart, meterWidth, levelHeight);
            } else if (meterStyle == MeterStyle::Segmented) {
                int segmentHeight = 3;
                int segmentGap = 1;
                int totalSegmentHeight = segmentHeight + segmentGap;
                
                for (int y = meterY + meterHeight - segmentHeight; y > meterY + meterHeight - levelHeight; y -= totalSegmentHeight) {
                    float normalizedY = static_cast<float>(meterY + meterHeight - y) / meterHeight;
                    juce::Colour segColour;
                    
                    if (normalizedY > yellowZone) {
                        segColour = highColour;
                    } else if (normalizedY > greenZone) {
                        segColour = midColour;
                    } else {
                        segColour = lowColour;
                    }
                    
                    g.setColour(segColour);
                    g.fillRect(x + 1, y, meterWidth - 2, segmentHeight);
                }
            } else {
                float normalizedLevel = clampedLevel;
                juce::Colour solidColour;
                
                if (normalizedLevel > yellowZone) {
                    solidColour = highColour;
                } else if (normalizedLevel > greenZone) {
                    solidColour = midColour;
                } else {
                    solidColour = lowColour;
                }
                
                g.setColour(solidColour);
                g.fillRect(x, meterY + meterHeight - levelHeight, meterWidth, levelHeight);
            }
            
            if (peakY > meterY && peakY < meterY + meterHeight - 2) {
                g.setColour(juce::Colours::white.withAlpha(0.8f));
                g.fillRect(x, peakY, meterWidth, 2);
            }
        }
    };
    
    drawMeter(leftDisplayLevel, leftPeakDisplay, startX);
    drawMeter(rightDisplayLevel, rightPeakDisplay, startX + meterWidth + meterSpacing);
}

void LevelMeter::resized() {
}

void LevelMeter::setLevels(float leftLevel, float rightLevel) {
    float leftLinear = juce::jlimit(0.0f, 1.0f, leftLevel);
    float rightLinear = juce::jlimit(0.0f, 1.0f, rightLevel);
    
    this->leftLevel = leftLinear;
    this->rightLevel = rightLinear;
}

void LevelMeter::poll(const StereoMeter& source) {
    const auto revision = source.getRevision();
    if (!hasSource || revision != lastRevision) {
        setLevels(source.getLeft(), source.getRight());
        lastRevision = revision;
        hasSource = true;
    }
}

void LevelMeter::setLevelsDecibels(float leftDb, float rightDb) {
    auto dbToLinear = [](float db) -> float {
        if (db <= -60.0f) return 0.0f;
        return std::pow(10.0f, db / 20.0f);
    };
    
    setLevels(dbToLinear(leftDb), dbToLinear(rightDb));
}

void LevelMeter::timerCallback() {
    double currentTime = juce::Time::getMillisecondCounterHiRes();
    double deltaTime = (currentTime - lastTime) / 1000.0;
    lastTime = currentTime;
    
    // If audio stops publishing (device stop or quiescence), do not pin stale peaks.
    const float decay = static_cast<float>(std::pow(10.0, -decayRate * deltaTime / 20.0));
    leftDisplayLevel = std::max(leftLevel.exchange(0), leftDisplayLevel * decay);
    rightDisplayLevel = std::max(rightLevel.exchange(0), rightDisplayLevel * decay);
    leftPeakDisplay = leftDisplayLevel;
    rightPeakDisplay = rightDisplayLevel;
    
    repaint();
}

} // namespace vibedaw
