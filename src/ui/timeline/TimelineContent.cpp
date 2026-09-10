#include "TimelineContent.h"
#include "ui/Theme.h"
#include <cmath>
#include <cstdlib>
#include "TimelineGeometry.h"
#include "ui/DragPayload.h"
#include "ui/components/TextPrompt.h"
#include "project/ClipPool.h"
#include "project/ChannelList.h"

namespace vibedaw {

TimelineContent::TimelineContent(TrackList& list, ClipPool& pool, ChannelList& channelList, TransportState& state)
    : transport(state), trackList(list), clipPool(pool), channels(channelList)
{
    trackList.addListener(this);
    
    timeRuler = std::make_unique<TimeRuler>();
    timeRuler->setLoopRegion(transport.getLoopRegion());
    timeRuler->onSeek = [this](double beats) { transport.setPositionInBeats(beats); };
    // Committing a ruler region turns looping on: DAW-conventional "set = use".
    timeRuler->onLoopCommitted = [this](double start, double end) {
        transport.setLoopRegion(start, end);
        transport.setLoopEnabled(true);
    };
    transport.addListener(this);
    addAndMakeVisible(*timeRuler);
    
    rebuildLanes();
    setOpaque(true);
    setWantsKeyboardFocus(true);
}

TimelineContent::~TimelineContent() {
    transport.removeListener(this);
    trackList.removeListener(this);
}

void TimelineContent::paint(juce::Graphics& g) {
    g.fillAll(theme::windowBackground);
}

void TimelineContent::paintOverChildren(juce::Graphics& g) {
    if (preview.visible) {
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(0, TimeRuler::rulerHeight, getWidth(), getHeight() - TimeRuler::rulerHeight);
        const int y = TimeRuler::rulerHeight + preview.lane * TimelineLane::defaultHeight - verticalScrollOffset;
        const auto colour = juce::Colour(preview.valid ? theme::dragValid : theme::dragInvalid);
        g.setColour(colour.withAlpha(0.15f));
        g.fillRect(0, y, getWidth(), TimelineLane::defaultHeight);
        const double left = TimelineGeometry::xAt(preview.beat, horizontalScrollOffset, pixelsPerBeat);
        const double right = TimelineGeometry::xAt(preview.beat + preview.duration, horizontalScrollOffset, pixelsPerBeat);
        const float x = static_cast<float>(juce::jlimit(0.0, double(getWidth()), left));
        const float end = static_cast<float>(juce::jlimit(0.0, double(getWidth()), right));
        g.setColour(colour.withAlpha(0.4f));
        g.fillRect(x, float(y + 2), juce::jmax(1.0f, end - x), float(TimelineLane::defaultHeight - 4));
        g.setColour(colour);
        g.drawRect(0, y, getWidth(), TimelineLane::defaultHeight, 2);
        g.drawFittedText(preview.label, 6, juce::jmax(TimeRuler::rulerHeight, y) + 4,
                        getWidth() - 12, 40, juce::Justification::centredLeft, 2);
    }
    const double x = transport.getPositionInBeats() * pixelsPerBeat - horizontalScrollOffset;
    if (x < 0 || x >= getWidth()) return;
    g.setColour(theme::accent);
    g.fillRect(static_cast<float>(x), 0.0f, 2.0f, static_cast<float>(getHeight()));
}

void TimelineContent::resized() {
    updateLayout();
    repaint();
}

void TimelineContent::setScrollOffset(int verticalOffset, double horizontalOffset) {
    verticalScrollOffset = verticalOffset;
    horizontalScrollOffset = horizontalOffset;
    
    timeRuler->setScrollOffset(horizontalOffset);
    
    for (auto& lane : lanes) {
        lane->setScrollOffset(horizontalOffset);
        lane->setPixelsPerBeat(pixelsPerBeat);
    }
    
    updateLayout();
    repaint();
}

int TimelineContent::getTotalHeight() const {
    return TimeRuler::rulerHeight + static_cast<int>(lanes.size()) * TimelineLane::defaultHeight;
}

int TimelineContent::getScrollableHeight() const {
    return (static_cast<int>(lanes.size()) + 1) * TimelineLane::defaultHeight;
}

double TimelineContent::getTotalWidth() const {
    double beats = juce::jmax(timeRuler->getTotalDuration(), transport.getLoopRegion().endBeats + 4.0);
    for (const auto& track : trackList.getTracks())
        for (const auto& instance : track->getClipInstances())
            if (instance->isValid()) beats = juce::jmax(beats, instance->getEndTime() + 4.0);
    // Bound the scrollable pixel extent so finite but enormous model times cannot
    // overflow GUI coordinates or stall beat-by-beat ruler iteration.
    return pixelsPerBeat * juce::jmin(beats, 1.0e9 / pixelsPerBeat);
}

void TimelineContent::setPixelsPerBeat(double value) {
    if (!std::isfinite(value) || value < 1.0) return;
    pixelsPerBeat = value;
    timeRuler->setPixelsPerBeat(value);
    
    for (auto& lane : lanes) {
        lane->setPixelsPerBeat(value);
    }
    
    repaint();
}

void TimelineContent::setSelectedTrack(int index) {
    if (selectedTrackIndex == index) return;
    
    if (selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(lanes.size())) {
        lanes[selectedTrackIndex]->setSelected(false);
    }
    
    selectedTrackIndex = index;
    
    if (selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(lanes.size())) {
        lanes[selectedTrackIndex]->setSelected(true);
    }
}

void TimelineContent::trackAdded(Track*) {
    rebuildLanes();
}

void TimelineContent::trackRemoved(int) {
    selectedTrackIndex = -1;
    rebuildLanes();
}

void TimelineContent::trackChanged(Track*) {
    repaint();
}

void TimelineContent::trackListChanged() {
    selectedTrackIndex = -1;
    rebuildLanes();
}

void TimelineContent::rebuildLanes() {
    DragDropInfo::cancelClip(pooledGesture);
    cancelDrag();
    lanes.clear();
    
    const auto& tracks = trackList.getTracks();
    for (size_t i = 0; i < tracks.size(); ++i) {
        auto lane = std::make_unique<TimelineLane>(tracks[i].get(), static_cast<int>(i));
        lane->setPixelsPerBeat(pixelsPerBeat);
        lane->setClipPool(&clipPool);
        lane->setChannelList(&channels);
        lane->addMouseListener(this, false);
        lane->onPlacementSelected = [this](int track, int placement) {
            setSelectedTrack(track);
            if (onTrackSelected) onTrackSelected(track);
            if (onPlacementSelected) onPlacementSelected(track, placement);
        };
        lane->setScrollOffset(horizontalScrollOffset);
        
        if (static_cast<int>(i) == selectedTrackIndex) {
            lane->setSelected(true);
        }
        
        addAndMakeVisible(*lane);
        lanes.push_back(std::move(lane));
    }
    
    updateLayout();
}

void TimelineContent::refresh() {
    for (auto& lane : lanes) lane->repaint();
    repaint();
}

void TimelineContent::updateLayout() {
    auto bounds = getLocalBounds();
    
    timeRuler->setBounds(0, 0, bounds.getWidth(), TimeRuler::rulerHeight);
    
    int laneY = TimeRuler::rulerHeight - verticalScrollOffset;
    for (auto& lane : lanes) {
        lane->setBounds(0, laneY, bounds.getWidth(), TimelineLane::defaultHeight);
        laneY += TimelineLane::defaultHeight;
    }
    timeRuler->toFront(false);
}

bool TimelineContent::isInterestedInDragSource(const SourceDetails& details) {
    // JUCE discovery supplies SOURCE coordinates, not target-local coordinates.
    return DragDropInfo::fromDragDescription(details.description).type == DragSourceType::Clip;
}

void TimelineContent::itemDragEnter(const SourceDetails& details) {
    cancelDrag();
    if (!isInterestedInDragSource(details)) return;
    pooledGesture = details.description;
    if (DragDropInfo::clipCancelled(pooledGesture)) return;
    draggedClip = DragDropInfo::fromDragDescription(details.description).clipId;
    externalSource = details.sourceComponent;
    hadExternalSource = externalSource != nullptr;
    dragging = true;
    itemDragMove(details);
    startTimerHz(30);
}

void TimelineContent::itemDragMove(const SourceDetails& details) {
    if (DragDropInfo::clipCancelled(pooledGesture)) { cancelDrag(); return; }
    if (dragging) updatePreview(details.localPosition, juce::ModifierKeys::getCurrentModifiersRealtime().isAltDown());
}

void TimelineContent::itemDragExit(const SourceDetails&) { cancelDrag(); }

void TimelineContent::itemDropped(const SourceDetails& details) {
    if (!isInterestedInDragSource(details) || DragDropInfo::clipCancelled(details.description)) { cancelDrag(); return; }
    // mouseUp discovers the final target without sending enter/move first.
    if (!dragging || pooledGesture != details.description) itemDragEnter(details);
    commitDrag(details.localPosition, juce::ModifierKeys::getCurrentModifiersRealtime().isAltDown());
}

ClipInstance* TimelineContent::movingInstance() const {
    if (auto* track = trackList.getTrackById(sourceTrackId))
        for (const auto& instance : track->getClipInstances())
            if (instance->getId() == movingId) return instance.get();
    return nullptr;
}

ClipInstance* TimelineContent::hitInstance(juce::Point<int> point, Track*& track) const {
    const auto lane = TimelineGeometry::laneAt(point.x, point.y, getWidth(), getHeight(),
        TimeRuler::rulerHeight, verticalScrollOffset, TimelineLane::defaultHeight, trackList.getNumTracks());
    track = trackList.getTrack(lane);
    if (track) {
        const double beat = TimelineGeometry::beatAt(point.x, horizontalScrollOffset, pixelsPerBeat);
        // Last painted wins, including unresolved placeholders; intervals are half-open.
        for (int i = track->getNumClipInstances() - 1; i >= 0; --i)
            if (track->getClipInstance(i)->containsTime(beat)) return track->getClipInstance(i);
    }
    return nullptr;
}

void TimelineContent::mouseDown(const juce::MouseEvent& event) {
    cancelDrag();
    pooledGesture = juce::var();
    const auto point = event.getEventRelativeTo(this).getPosition();
    if (event.mods.isPopupMenu()) {
        showContextMenuAt(point);
        return;
    }
    if (!event.mods.isLeftButtonDown()) return;
    Track* track = nullptr;
    if (auto* instance = hitInstance(point, track)) {
        sourceTrackId = track->getId();
        movingId = instance->getId();
        sourceWasResolved = clipPool.getClip(instance->getClipId()) != nullptr;
        destinationWasResolved = channels.getChannelById(instance->getChannelId()) != nullptr;
        grabOffset = TimelineGeometry::beatAt(point.x, horizontalScrollOffset, pixelsPerBeat) - instance->getStartTime();
        if (getPeer() != nullptr) grabKeyboardFocus();
    }
}

void TimelineContent::showContextMenuAt(juce::Point<int> point) {
    Track* track = nullptr;
    if (auto* instance = hitInstance(point, track)) {
        // Resolve and select the clicked target before showing its menu.
        const int lane = trackList.indexOfTrack(track);
        setSelectedTrack(lane);
        if (onTrackSelected) onTrackSelected(lane);
        int instanceIndex = -1;
        for (int i = 0; i < track->getNumClipInstances(); ++i)
            if (track->getClipInstance(i) == instance) instanceIndex = i;
        if (onPlacementSelected) onPlacementSelected(lane, instanceIndex);
        createPlacementMenu(*track, *instance).showMenuAsync(juce::PopupMenu::Options());
        return;
    }
    const int lane = TimelineGeometry::laneAt(point.x, point.y, getWidth(), getHeight(),
        TimeRuler::rulerHeight, verticalScrollOffset, TimelineLane::defaultHeight, trackList.getNumTracks());
    if (lane < 0) return; // The ruler, headers, and outside space own no menu.
    const double beat = TimelineGeometry::startAt(point.x, horizontalScrollOffset, pixelsPerBeat, 0.0,
        juce::ModifierKeys::getCurrentModifiersRealtime().isAltDown());
    auto* target = trackList.getTrack(lane);
    createEmptySpaceMenu(target ? target->getId() : juce::String(), target == nullptr, beat)
        .showMenuAsync(juce::PopupMenu::Options());
}

ClipInstance* TimelineContent::resolvePlacement(const juce::String& trackId, const juce::String& instanceId) const {
    auto* track = trackList.getTrackById(trackId);
    if (!track) return nullptr;
    for (const auto& instance : track->getClipInstances())
        if (instance->getId() == instanceId) return instance.get();
    return nullptr;
}

juce::PopupMenu TimelineContent::createPlacementMenu(Track& track, ClipInstance& instance) const {
    juce::PopupMenu menu;
    const auto safeSelf = juce::Component::SafePointer<TimelineContent>(const_cast<TimelineContent*>(this));
    const auto trackId = track.getId(), instanceId = instance.getId();
    auto* clip = clipPool.getClip(instance.getClipId());
    menu.addSectionHeader(track.getName() + juce::String(juce::CharPointer_UTF8(" \xe2\x80\x94 ")) +
        (clip ? clip->getName() : "Missing source #" + juce::String(instance.getClipId())));
    menu.addItem("Edit Shared Source", clip != nullptr && clip->getType() == Clip::Type::Midi && onEditSource != nullptr,
        false, [safeSelf, trackId, instanceId] {
            if (safeSelf != nullptr) safeSelf->editPlacementSourceById(trackId, instanceId);
        });
    menu.addItem(juce::String(juce::CharPointer_UTF8("Set Start Beat\xe2\x80\xa6")), true, false,
        [safeSelf, trackId, instanceId] {
            if (safeSelf == nullptr) return;
            // The placement may vanish while pending: no prompt, no mutation.
            const auto* target = safeSelf->resolvePlacement(trackId, instanceId);
            if (!target) return;
            showTextPrompt("Set Start Beat", "Placement start in quarter-note beats (zero-based):",
                juce::String(target->getStartTime(), 6), safeSelf.getComponent(),
                [safeSelf, trackId, instanceId](const juce::String& text) {
                    if (safeSelf == nullptr) return;
                    const auto trimmed = text.trim();
                    const auto* begin = trimmed.toRawUTF8();
                    char* end = nullptr;
                    const double beat = std::strtod(begin, &end);
                    if (end != begin && *end == '\0') safeSelf->setPlacementStartById(trackId, instanceId, beat);
                });
        });
    juce::PopupMenu destinations;
    bool currentResolved = false;
    for (const auto& entry : channels.getChannels()) {
        auto* channel = entry.get();
        if (!channel || channel->getType() != Channel::Type::Instrument) continue;
        const auto channelId = channel->getId();
        currentResolved |= channelId == instance.getChannelId();
        destinations.addItem(channel->getName() + " (#" + juce::String(channelId) + ")", true,
            channelId == instance.getChannelId(),
            [safeSelf, trackId, instanceId, channelId] {
                if (safeSelf != nullptr) safeSelf->assignPlacementDestinationById(trackId, instanceId, channelId);
            });
    }
    if (!currentResolved)
        destinations.addItem("(current destination unresolved #" + juce::String(instance.getChannelId()) + ")",
            false, false, {});
    menu.addSubMenu("Assign to Destination", destinations, true);
    menu.addItem(instance.isMuted() ? "Unmute" : "Mute", true, instance.isMuted(),
        [safeSelf, trackId, instanceId] {
            if (safeSelf != nullptr) safeSelf->togglePlacementMuteById(trackId, instanceId);
        });
    menu.addSeparator();
    menu.addItem("Remove Placement (source stays in Clips)", true, false,
        [safeSelf, trackId, instanceId] {
            if (safeSelf != nullptr) safeSelf->removePlacementById(trackId, instanceId);
        });
    return menu;
}

juce::PopupMenu TimelineContent::createEmptySpaceMenu(const juce::String& targetId, bool newTrack, double beat) const {
    juce::PopupMenu menu;
    const auto safeSelf = juce::Component::SafePointer<TimelineContent>(const_cast<TimelineContent*>(this));
    auto* target = newTrack ? nullptr : trackList.getTrackById(targetId);
    menu.addSectionHeader((newTrack ? juce::String("New Track") : (target ? target->getName() : targetId)) +
        juce::String(juce::CharPointer_UTF8(" \xe2\x80\x94 beat ")) + juce::String(beat, 2));
    menu.addItem("Place Selected Clip Here", pooledPlacementIsValid(beat), false,
        [safeSelf, targetId, newTrack, beat] {
            if (safeSelf == nullptr) return;
            // A target deleted while the menu was open must not become a new lane.
            auto* resolved = newTrack ? nullptr : safeSelf->trackList.getTrackById(targetId);
            if (!newTrack && !resolved) return;
            safeSelf->placePooledClipHere(resolved, beat);
        });
    menu.addItem("Add Track", true, false, [safeSelf] {
        if (safeSelf != nullptr) safeSelf->trackList.addTrack();
    });
    return menu;
}

bool TimelineContent::pooledPlacementIsValid(double beat) const {
    const auto clipId = selectedClipSource ? selectedClipSource() : InvalidClipId;
    auto* clip = clipPool.getClip(clipId);
    auto* channel = channels.getChannelById(activeDestination ? activeDestination() : InvalidChannelId);
    return clip && clip->getType() == Clip::Type::Midi && channel &&
        channel->getType() == Channel::Type::Instrument &&
        std::isfinite(clip->getDuration()) && clip->getDuration() > 0.0 &&
        std::isfinite(beat) && beat >= 0.0 && std::isfinite(beat + clip->getDuration());
}

bool TimelineContent::placePooledClipHere(Track* target, double beat) {
    const auto clipId = selectedClipSource ? selectedClipSource() : InvalidClipId;
    auto* clip = clipPool.getClip(clipId);
    auto* channel = channels.getChannelById(activeDestination ? activeDestination() : InvalidChannelId);
    if (!clip || clip->getType() != Clip::Type::Midi || !channel ||
        channel->getType() != Channel::Type::Instrument ||
        !std::isfinite(clip->getDuration()) || clip->getDuration() <= 0.0 ||
        !std::isfinite(beat) || beat < 0.0 || !std::isfinite(beat + clip->getDuration())) return false;
    auto candidate = std::make_unique<ClipInstance>(clipId, channel->getId(), beat, clip->getDuration());
    auto* committed = trackList.commitPlacement(target ? target->getId() : juce::String(), beat, {}, {},
                                               std::move(candidate));
    if (!committed) return false;
    for (int t = 0; t < trackList.getNumTracks(); ++t) {
        auto* track = trackList.getTrack(t);
        for (int i = 0; i < track->getNumClipInstances(); ++i) if (track->getClipInstance(i) == committed) {
            setSelectedTrack(t);
            if (onTrackSelected) onTrackSelected(t);
            if (onPlacementSelected) onPlacementSelected(t, i);
        }
    }
    return true;
}

bool TimelineContent::setPlacementStartById(const juce::String& trackId, const juce::String& instanceId, double beat) {
    auto* instance = resolvePlacement(trackId, instanceId);
    if (!instance || !std::isfinite(beat) || beat < 0.0 ||
        !std::isfinite(beat + instance->getDuration())) return false;
    instance->setStartTime(beat);
    return true;
}

bool TimelineContent::assignPlacementDestinationById(const juce::String& trackId, const juce::String& instanceId,
                                                     ChannelId destination) {
    auto* instance = resolvePlacement(trackId, instanceId);
    auto* channel = channels.getChannelById(destination);
    if (!instance || !channel || channel->getType() != Channel::Type::Instrument) return false;
    instance->setChannelId(destination);
    return true;
}

bool TimelineContent::togglePlacementMuteById(const juce::String& trackId, const juce::String& instanceId) {
    auto* instance = resolvePlacement(trackId, instanceId);
    if (!instance) return false;
    instance->setMuted(!instance->isMuted());
    return true;
}

bool TimelineContent::removePlacementById(const juce::String& trackId, const juce::String& instanceId) {
    auto* track = trackList.getTrackById(trackId);
    if (!track) return false;
    for (int i = 0; i < track->getNumClipInstances(); ++i)
        if (track->getClipInstance(i)->getId() == instanceId) {
            track->removeClipInstance(i);
            return true;
        }
    return false;
}

bool TimelineContent::editPlacementSourceById(const juce::String& trackId, const juce::String& instanceId) {
    auto* instance = resolvePlacement(trackId, instanceId);
    auto* clip = instance ? clipPool.getClip(instance->getClipId()) : nullptr;
    if (!clip || clip->getType() != Clip::Type::Midi || !onEditSource) return false;
    onEditSource(instance->getClipId());
    return true;
}

ClipInstance* TimelineContent::selectedPlacementInstance() const {
    for (int t = 0; t < trackList.getNumTracks(); ++t) {
        auto* track = trackList.getTrack(t);
        for (int i = 0; i < track->getNumClipInstances(); ++i)
            if (track->getClipInstance(i)->isSelected()) return track->getClipInstance(i);
    }
    return nullptr;
}

void TimelineContent::mouseDrag(const juce::MouseEvent& event) {
    if (movingId.isEmpty() || !event.mods.isLeftButtonDown() || event.getDistanceFromDragStart() < 5) return;
    dragging = true;
    updatePreview(event.getEventRelativeTo(this).getPosition(), event.mods.isAltDown());
    startTimerHz(30);
}

void TimelineContent::mouseUp(const juce::MouseEvent& event) {
    if (dragging && movingId.isNotEmpty()) commitDrag(event.getEventRelativeTo(this).getPosition(), event.mods.isAltDown());
    else cancelDrag();
}

void TimelineContent::mouseDoubleClick(const juce::MouseEvent& event) {
    cancelDrag();
    if (event.mods.isPopupMenu() || event.mouseWasDraggedSinceMouseDown()) return;
    Track* track = nullptr;
    if (auto* instance = hitInstance(event.getEventRelativeTo(this).getPosition(), track))
        if (auto* clip = clipPool.getClip(instance->getClipId()))
            if (clip->getType() == Clip::Type::Midi && onEditSource) onEditSource(instance->getClipId());
}

bool TimelineContent::removeSelectedPlacement() {
    for (int t = trackList.getNumTracks() - 1; t >= 0; --t) {
        auto* track = trackList.getTrack(t);
        for (int i = track->getNumClipInstances() - 1; i >= 0; --i)
            if (track->getClipInstance(i)->isSelected()) {
                track->removeClipInstance(i);
                return true;
            }
    }
    return false;
}

bool TimelineContent::editSelectedPlacementSource() {
    if (auto* instance = selectedPlacementInstance()) {
        auto* clip = clipPool.getClip(instance->getClipId());
        if (clip && clip->getType() == Clip::Type::Midi && onEditSource) {
            onEditSource(instance->getClipId());
            return true;
        }
    }
    return false;
}

bool TimelineContent::keyPressed(const juce::KeyPress& key) {
    if (key == juce::KeyPress::escapeKey) {
        DragDropInfo::cancelClip(pooledGesture);
        cancelDrag();
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::deleteKey) || key.isKeyCode(juce::KeyPress::backspaceKey))
        return removeSelectedPlacement();
    if ((key.getKeyCode() == 'E' || key.getKeyCode() == 'e') && key.getModifiers().isCtrlDown())
        return editSelectedPlacementSource();
    return false;
}

void TimelineContent::cancelDrag() {
    stopTimer();
    preview = {};
    draggedClip = InvalidClipId;
    sourceTrackId.clear();
    movingId.clear();
    grabOffset = 0;
    dragging = false;
    sourceWasResolved = destinationWasResolved = false;
    hadExternalSource = false;
    externalSource = nullptr;
    repaint();
}

void TimelineContent::updatePreview(juce::Point<int> point, bool bypass) {
    dragPosition = point;
    preview = {};
    const int lane = TimelineGeometry::laneAt(point.x, point.y, getWidth(), getHeight(),
        TimeRuler::rulerHeight, verticalScrollOffset, TimelineLane::defaultHeight, trackList.getNumTracks());
    if (!dragging || lane < 0) { repaint(); return; }
    preview.visible = true;
    preview.lane = lane;
    auto* target = trackList.getTrack(lane);
    preview.newTrack = target == nullptr;
    preview.trackId = target ? target->getId() : juce::String();
    preview.beat = TimelineGeometry::startAt(point.x, horizontalScrollOffset, pixelsPerBeat, grabOffset, bypass);
    auto* instance = movingInstance();
    auto* clip = clipPool.getClip(instance ? instance->getClipId() : draggedClip);
    preview.duration = instance ? instance->getDuration() : (clip ? clip->getDuration() : 0);
    preview.destination = instance ? instance->getChannelId() : (activeDestination ? activeDestination() : InvalidChannelId);
    auto* channel = channels.getChannelById(preview.destination);
    const bool validChannel = channel && channel->getType() == Channel::Type::Instrument;
    preview.valid = std::isfinite(preview.duration) && preview.duration > 0 &&
        std::isfinite(preview.beat + preview.duration) &&
        (movingId.isNotEmpty() ? instance != nullptr : clip && clip->getType() == Clip::Type::Midi && validChannel);
    if ((sourceWasResolved && !clip) || (destinationWasResolved && !channel)) preview.valid = false;
    preview.label = (preview.newTrack ? "New track" : target->getName()) + " | b" + juce::String(preview.beat, 2);
    if (movingId.isNotEmpty() && !instance) preview.label += " | Placement removed: cancel";
    else if (!clip) preview.label += " | Missing source: silent";
    else if (clip->getType() != Clip::Type::Midi) preview.label += " | Requires a MIDI source";
    if (!validChannel) preview.label += instance ? " | Unresolved destination: silent" : " | Select an instrument in Channel Rack";
    else {
        preview.label += " | To: " + channel->getName() + " (#" + juce::String(channel->getId()) + ")";
        if (!channel->hasPlugin()) preview.label += " | Destination has no plugin";
    }
    repaint();
}

void TimelineContent::commitDrag(juce::Point<int> point, bool bypass) {
    if (hadExternalSource && externalSource == nullptr) { cancelDrag(); return; }
    // A deleted hovered target must not silently become another lane/new track.
    const auto oldTarget = preview.trackId;
    const auto oldDestination = preview.destination;
    const bool hadValidPreview = preview.visible && preview.valid;
    if (oldTarget.isNotEmpty() && !trackList.getTrackById(oldTarget)) { cancelDrag(); return; }
    updatePreview(point, bypass);
    if (movingId.isEmpty() && hadValidPreview && oldDestination != preview.destination) { cancelDrag(); return; }
    if (!preview.visible || !preview.valid) { cancelDrag(); return; }
    const auto target = preview.trackId;
    const auto beat = preview.beat;
    std::unique_ptr<ClipInstance> candidate;
    if (movingId.isEmpty()) candidate = std::make_unique<ClipInstance>(draggedClip, preview.destination, beat, preview.duration);
    pooledGesture = juce::var(); // Our own committed lane addition is not a stale-structure cancellation.
    auto* committed = trackList.commitPlacement(target, beat, sourceTrackId, movingId, std::move(candidate));
    cancelDrag();
    if (committed) {
        for (int t = 0; t < trackList.getNumTracks(); ++t) {
            auto* track = trackList.getTrack(t);
            for (int i = 0; i < track->getNumClipInstances(); ++i) if (track->getClipInstance(i) == committed) {
                setSelectedTrack(t);
                if (onTrackSelected) onTrackSelected(t);
                if (onPlacementSelected) onPlacementSelected(t, i);
            }
        }
        if (onExtentChanged) onExtentChanged();
    }
}

void TimelineContent::timerCallback() {
    if (!dragging) return;
    if (DragDropInfo::clipCancelled(pooledGesture)) { cancelDrag(); return; }
    // JUCE can omit exit when the drag's source component has been destroyed.
    if (hadExternalSource && externalSource == nullptr) { cancelDrag(); return; }
    if (juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::escapeKey)) {
        keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
        return;
    }
    const int dx = TimelineGeometry::edgeDelta(dragPosition.x, 0, getWidth());
    const int dy = TimelineGeometry::edgeDelta(dragPosition.y, TimeRuler::rulerHeight, getHeight());
    if (getLocalBounds().contains(dragPosition) && dragPosition.y >= TimeRuler::rulerHeight && onAutoScroll)
        onAutoScroll(dx, dy);
    updatePreview(dragPosition, juce::ModifierKeys::getCurrentModifiersRealtime().isAltDown());
}

} // namespace vibedaw
