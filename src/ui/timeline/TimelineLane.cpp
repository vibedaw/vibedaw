#include "TimelineLane.h"
#include "project/Track.h"
#include "project/ClipPool.h"
#include "project/ChannelList.h"
#include "ui/Theme.h"
#include "ui/components/ClipMiniPreview.h"
#include <cmath>
#include "TimelineGeometry.h"

namespace vibedaw {

TimelineLane::TimelineLane(Track* owner, int index)
    : track(owner), trackIndex(index) {
    setOpaque(true);
}

void TimelineLane::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(selected ? theme::selectedLane : theme::panelBackground));
    double firstBeat = std::ceil(scrollOffset / pixelsPerBeat);
    double endBeat = (scrollOffset + getWidth()) / pixelsPerBeat;
    for (double beat = firstBeat; beat <= endBeat; ++beat) {
        g.setColour(juce::Colour(std::fmod(beat, 4.0) == 0.0 ? theme::gridBar : theme::hairline));
        g.drawVerticalLine(static_cast<int>(beat * pixelsPerBeat - scrollOffset), 0.0f, static_cast<float>(getHeight()));
    }
    g.setColour(theme::separator);
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
        auto colour = unresolved ? theme::unresolvedClip : clip->getColour();
        if (muted) colour = colour.withMultipliedBrightness(0.45f);
        g.setColour(colour);
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        g.setColour(instance->isSelected() ? theme::white : colour.darker(0.4f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 4.0f, instance->isSelected() ? 2.0f : 1.0f);

        auto text = bounds.reduced(5, 2);
        auto nameRow = text.removeFromTop(15);
        auto destinationRow = text.removeFromTop(13);
        g.setColour(theme::white);
        g.setFont(12.0f);
        auto name = clip ? clip->getName() : "Missing source #" + juce::String(instance->getClipId());
        if (muted) name += " [Muted]";
        g.drawText(name, nameRow, juce::Justification::centredLeft, true);
        g.setColour(theme::white.withAlpha(0.7f));
        g.setFont(10.0f);
        const auto destination = channel ? channel->getName() + " (#" + juce::String(channel->getId()) + ")"
                                         : "Missing destination #" + juce::String(instance->getChannelId());
        g.drawText(destination, destinationRow, juce::Justification::centredLeft, true);

        auto previewArea = bounds.toFloat().withTrimmedBottom(2.0f);
        if (unresolved) {
            g.setFont(10.0f);
            g.drawText("Unresolved: silent", previewArea.toNearestInt(), juce::Justification::centredLeft, true);
        } else if (auto* midi = dynamic_cast<const MidiClip*>(clip)) {
            const double sourceEnd = juce::jmin(instance->getDuration(), midi->getDuration());
            ClipMiniPreview::drawMiniNotes(g, *midi, sourceEnd, previewArea, muted, pixelsPerBeat);
        }
    }
}

} // namespace vibedaw
