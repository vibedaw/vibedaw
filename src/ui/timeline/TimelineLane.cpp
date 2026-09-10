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
        const bool bar = std::fmod(beat, 4.0) == 0.0;
        if (!bar && pixelsPerBeat < 12.0) continue;
        g.setColour(bar ? theme::gridBar.withAlpha(0.65f) : theme::hairline.withAlpha(0.4f));
        g.drawVerticalLine(static_cast<int>(beat * pixelsPerBeat - scrollOffset), 0.0f, static_cast<float>(getHeight()));
    }
    g.setColour(theme::hairline);
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
        const auto body = bounds.toFloat();
        const auto header = body.withHeight(19.0f);
        // Composite the tint onto an opaque base so overlapping placements stay legible.
        g.setColour(theme::panelBackground.interpolatedWith(colour, muted ? 0.08f : 0.14f));
        g.fillRoundedRectangle(body, theme::controlRadius);
        {
            juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(header.toNearestInt());
            g.setColour(theme::panelBackground.interpolatedWith(colour, 0.48f));
            g.fillRoundedRectangle(body, theme::controlRadius);
            g.setColour(colour.withAlpha(0.75f));
            g.fillRect(header.withHeight(2.0f));
        }
        g.setColour(instance->isSelected() ? theme::accent : colour.withAlpha(0.5f));
        g.drawRoundedRectangle(body.reduced(0.5f), theme::controlRadius, instance->isSelected() ? 1.5f : 1.0f);

        auto nameRow = bounds.withHeight(19).reduced(6, 1);
        auto destinationRow = bounds.withTrimmedTop(20).withHeight(13).reduced(6, 0);
        const auto headerColour = theme::panelBackground.interpolatedWith(colour, 0.48f);
        g.setColour(headerColour.contrasting());
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        auto name = clip ? clip->getName() : "Missing source #" + juce::String(instance->getClipId());
        if (muted) name += " [Muted]";
        g.drawText(name, nameRow, juce::Justification::centredLeft, true);
        g.setColour(theme::textSecondary);
        g.setFont(10.0f);
        const auto destination = channel ? channel->getName() + " (#" + juce::String(channel->getId()) + ")"
                                         : "Missing destination #" + juce::String(instance->getChannelId());
        g.drawText(destination, destinationRow, juce::Justification::centredLeft, true);

        auto previewArea = body.withTrimmedTop(35.0f).withTrimmedBottom(4.0f);
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(previewArea.toNearestInt());
        if (unresolved) {
            g.setColour(theme::dangerText);
            g.setFont(10.0f);
            g.drawText("Unresolved: silent", previewArea.toNearestInt().reduced(6, 0), juce::Justification::centredLeft, true);
        } else if (auto* midi = dynamic_cast<const MidiClip*>(clip)) {
            const double sourceEnd = juce::jmin(instance->getDuration(), midi->getDuration());
            // Keep source beat zero anchored to the placement when its left edge is offscreen.
            previewArea.setLeft(static_cast<float>(left));
            ClipMiniPreview::drawMiniNotes(g, *midi, sourceEnd, previewArea, muted, pixelsPerBeat);
        }
    }
}

} // namespace vibedaw
