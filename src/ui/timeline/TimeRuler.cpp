#include "TimeRuler.h"
#include <cmath>

namespace vibedaw {

TimeRuler::TimeRuler() { setOpaque(true); }

void TimeRuler::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xff2a2a2a));
    const float left = static_cast<float>(juce::jlimit(0.0, static_cast<double>(getWidth()),
        loop.startBeats * pixelsPerBeat - scrollOffset));
    const float right = static_cast<float>(juce::jlimit(0.0, static_cast<double>(getWidth()),
        loop.endBeats * pixelsPerBeat - scrollOffset));
    g.setColour(juce::Colour(loop.enabled ? 0xff365978 : 0xff383e44));
    g.fillRect(left, 0.0f, right - left, static_cast<float>(getHeight()));
    g.setColour(juce::Colour(loop.enabled ? 0xff66aaff : 0xff667788));
    g.fillRect(left, 0.0f, right - left, 3.0f);
    const double interval = pixelsPerBeat < 25.0 ? 4.0 : 1.0;
    const double first = std::ceil(scrollOffset / pixelsPerBeat / interval) * interval;
    const double end = (scrollOffset + getWidth()) / pixelsPerBeat;
    for (double beat = first; beat <= end; beat += interval) {
        int x = static_cast<int>(beat * pixelsPerBeat - scrollOffset);
        g.setColour(juce::Colour(0xff505050));
        g.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));
        g.setColour(juce::Colour(0xffbbbbbb));
        g.setFont(11.0f);
        g.drawText("b" + juce::String(beat, 0), x + 3, 2, 70, getHeight() - 4, juce::Justification::left);
    }
}

void TimeRuler::setTotalDuration(double beats) {
    if (std::isfinite(beats) && beats > 0.0) totalDuration = beats;
    repaint();
}
void TimeRuler::mouseDown(const juce::MouseEvent& event) {
    if (event.mods.isLeftButtonDown() && onSeek)
        onSeek(juce::jmax(0.0, (event.position.x + scrollOffset) / pixelsPerBeat));
}
void TimeRuler::setPixelsPerBeat(double value) {
    if (std::isfinite(value) && value >= 1.0) pixelsPerBeat = value;
    repaint();
}
void TimeRuler::setScrollOffset(double offset) { scrollOffset = offset; repaint(); }

} // namespace vibedaw
