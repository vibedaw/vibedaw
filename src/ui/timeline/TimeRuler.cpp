#include "TimeRuler.h"

namespace vibedaw {

TimeRuler::TimeRuler() {
    setOpaque(true);
}

void TimeRuler::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    g.fillAll(juce::Colour(0xff2a2a2a));
    
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawHorizontalLine(bounds.getBottom() - 1, 0.0f, static_cast<float>(bounds.getWidth()));
    
    drawTimeMarkers(g, bounds.getWidth());
}

void TimeRuler::setTotalDuration(double duration) {
    totalDuration = duration;
    repaint();
}

void TimeRuler::setPixelsPerSecond(double pps) {
    pixelsPerSecond = pps;
    repaint();
}

void TimeRuler::setScrollOffset(double offset) {
    scrollOffset = offset;
    repaint();
}

void TimeRuler::drawTimeMarkers(juce::Graphics& g, int width) {
    double startTime = scrollOffset / pixelsPerSecond;
    double endTime = startTime + (width / pixelsPerSecond);
    
    double majorInterval = 10.0;
    double minorInterval = 1.0;
    
    if (pixelsPerSecond < 10.0) {
        majorInterval = 60.0;
        minorInterval = 10.0;
    } else if (pixelsPerSecond < 30.0) {
        majorInterval = 30.0;
        minorInterval = 5.0;
    } else if (pixelsPerSecond < 100.0) {
        majorInterval = 10.0;
        minorInterval = 1.0;
    } else {
        majorInterval = 5.0;
        minorInterval = 0.5;
    }
    
    auto snapToInterval = [](double value, double interval) {
        return std::floor(value / interval) * interval;
    };
    
    double firstMinorTick = snapToInterval(startTime, minorInterval) + minorInterval;
    for (double t = firstMinorTick; t <= endTime; t += minorInterval) {
        int x = static_cast<int>((t * pixelsPerSecond) - scrollOffset);
        if (x >= 0 && x < width) {
            g.setColour(juce::Colour(0xff404040));
            g.drawVerticalLine(x, static_cast<float>(getHeight() * 0.6f), static_cast<float>(getHeight()));
        }
    }
    
    double firstMajorTick = snapToInterval(startTime, majorInterval) + majorInterval;
    for (double t = firstMajorTick; t <= endTime; t += majorInterval) {
        int x = static_cast<int>((t * pixelsPerSecond) - scrollOffset);
        if (x >= 0 && x < width) {
            g.setColour(juce::Colour(0xff505050));
            g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));
            
            g.setColour(juce::Colour(0xffaaaaaa));
            g.setFont(juce::Font(11.0f));
            auto label = formatTime(t);
            g.drawText(label, x + 3, 2, 60, getHeight() - 4, juce::Justification::left);
        }
    }
}

juce::String TimeRuler::formatTime(double seconds) const {
    int totalSeconds = static_cast<int>(seconds);
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;
    
    if (seconds < 60.0) {
        return juce::String::formatted("%d:%02d", minutes, secs);
    }
    
    return juce::String::formatted("%d:%02d", minutes, secs);
}

} // namespace vibedaw
