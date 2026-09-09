#include "TimelineLane.h"
#include "project/Track.h"
#include "project/ClipPool.h"
#include "project/ChannelList.h"
#include <cmath>
#include "TimelineGeometry.h"

namespace vibedaw {

TimelineLane::TimelineLane(Track* owner, int index)
    : track(owner), trackIndex(index) {
    setOpaque(true);
}

void TimelineLane::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(selected ? 0xff2a2a3a : 0xff1e1e1e));
    double firstBeat = std::ceil(scrollOffset / pixelsPerBeat);
    double endBeat = (scrollOffset + getWidth()) / pixelsPerBeat;
    for (double beat = firstBeat; beat <= endBeat; ++beat) {
        g.setColour(juce::Colour(std::fmod(beat, 4.0) == 0.0 ? 0xff484848 : 0xff333333));
        g.drawVerticalLine(static_cast<int>(beat * pixelsPerBeat - scrollOffset), 0.0f, static_cast<float>(getHeight()));
    }
    g.setColour(juce::Colour(0xff505050));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
    drawClips(g);
}

void TimelineLane::setPixelsPerBeat(double value) {
    if (std::isfinite(value) && value >= 1.0) pixelsPerBeat = value;
    repaint();
}

void TimelineLane::setScrollOffset(double offset) { scrollOffset = offset; repaint(); }
void TimelineLane::setSelected(bool value) { selected = value; repaint(); }
void TimelineLane::setClipPool(ClipPool* pool) { clipPool = pool; repaint(); }

void TimelineLane::mouseDown(const juce::MouseEvent& e) {
    if (!track) return;
    if (!e.mods.isLeftButtonDown() || e.mods.isPopupMenu()) return;
    const double beat = TimelineGeometry::beatAt(e.x, scrollOffset, pixelsPerBeat);
    int hit = -1;
    for (int i = track->getNumClipInstances() - 1; i >= 0; --i) {
        if (track->getClipInstance(i)->containsTime(beat)) { hit = i; break; }
    }
    if (onPlacementSelected) onPlacementSelected(trackIndex, hit);
}

void TimelineLane::drawClips(juce::Graphics& g) {
    if (!track) return;
    for (const auto& instance : track->getClipInstances()) {
        if (!instance || !instance->isValid()) continue;
        const double left = TimelineGeometry::xAt(instance->getStartTime(), scrollOffset, pixelsPerBeat);
        const double right = TimelineGeometry::xAt(instance->getEndTime(), scrollOffset, pixelsPerBeat);
        if (right < 0.0 || left >= getWidth()) continue;
        // Clip in floating point before integer conversion, including very distant placements.
        const int x = static_cast<int>(juce::jmax(0.0, left));
        const int width = juce::jmax(1, static_cast<int>(juce::jmin(static_cast<double>(getWidth()), right)) - x);
        juce::Rectangle<int> bounds(x, 2, width, getHeight() - 4);
        auto* clip = clipPool ? clipPool->getClip(instance->getClipId()) : nullptr;
        auto* channel = channels ? channels->getChannelById(instance->getChannelId()) : nullptr;
        const bool unresolved = !clip || clip->getType() != Clip::Type::Midi ||
                                !channel || channel->getType() != Channel::Type::Instrument;
        const bool muted = instance->isMuted() || (clip && clip->isMuted()) || (channel && channel->isMuted());
        auto colour = unresolved ? juce::Colour(0xffad6464) : clip->getColour();
        if (muted) colour = colour.withMultipliedBrightness(0.45f);
        g.setColour(colour);
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        g.setColour(instance->isSelected() ? juce::Colours::white : colour.darker(0.4f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 4.0f, instance->isSelected() ? 2.0f : 1.0f);
        g.setColour(juce::Colours::white);
        g.setFont(12.0f);
        auto text = bounds.reduced(5, 3);
        auto name = clip ? clip->getName() : "Missing source #" + juce::String(instance->getClipId());
        if (muted) name += " [Muted]";
        g.drawText(name, text.removeFromTop(18), juce::Justification::centredLeft, true);
        const auto destination = channel ? channel->getName() + " (#" + juce::String(channel->getId()) + ")"
                                         : "Missing destination #" + juce::String(instance->getChannelId());
        g.drawText(destination, text.removeFromTop(18), juce::Justification::centredLeft, true);
        if (unresolved) g.drawText("Unresolved: silent", text, juce::Justification::centredLeft, true);
        else if (auto* midi = dynamic_cast<const MidiClip*>(clip)) {
            const double sourceEnd = juce::jmin(instance->getDuration(), midi->getDuration());
            for (const auto& note : midi->getNotes()) {
                const double end = juce::jmin(note.getEndTime(), sourceEnd);
                if (note.getStartTime() >= end) continue;
                const double noteLeft = (instance->getStartTime() + note.getStartTime()) * pixelsPerBeat - scrollOffset;
                const double noteRight = (instance->getStartTime() + end) * pixelsPerBeat - scrollOffset;
                if (noteRight <= x || noteLeft >= x + width) continue;
                const int nx = static_cast<int>(juce::jmax(static_cast<double>(x), noteLeft));
                const int nr = static_cast<int>(juce::jmin(static_cast<double>(x + width), noteRight));
                const int ny = text.getBottom() - 2 - note.getPitch() * juce::jmax(0, text.getHeight() - 2) / 128;
                g.setColour(juce::Colours::white.withAlpha(note.isMuted() ? 0.2f : 0.7f));
                g.fillRect(nx, ny, juce::jmax(1, nr - nx), 2);
            }
        }
    }
}

} // namespace vibedaw
