#include "TrackHeaderList.h"
#include "ui/components/TextPrompt.h"

namespace vibedaw {

TrackHeaderList::TrackHeaderList(TrackList& list)
    : trackList(list)
{
    trackList.addListener(this);
    rebuildHeaders();
    setOpaque(true);
}

TrackHeaderList::~TrackHeaderList() {
    for (const auto& track : trackList.getTracks()) track->removeChangeListener(this);
    trackList.removeListener(this);
}

void TrackHeaderList::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    
    g.fillAll(juce::Colour(0xff252525));
    
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(0, 0, getWidth(), headerHeight);
    
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawHorizontalLine(headerHeight - 1, 0.0f, static_cast<float>(getWidth()));
    
    g.setColour(juce::Colour(0xff404040));
    g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(headerHeight));
    
    g.setColour(juce::Colour(0xff888888));
    g.setFont(juce::Font(11.0f));
    g.drawText("Tracks", 4, 2, getWidth() - 8, headerHeight - 4, juce::Justification::centredLeft);
}

void TrackHeaderList::resized() {
    updateHeaderPositions();
}

void TrackHeaderList::setScrollOffset(int offset) {
    scrollOffset = offset;
    updateHeaderPositions();
}

int TrackHeaderList::getTotalHeight() const {
    return headerHeight + static_cast<int>(headers.size()) * TrackHeader::defaultHeight;
}

int TrackHeaderList::getScrollableHeight() const {
    return static_cast<int>(headers.size()) * TrackHeader::defaultHeight;
}

void TrackHeaderList::setSelectedTrack(int index) {
    if (selectedTrackIndex == index) return;
    
    if (selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(headers.size())) {
        headers[selectedTrackIndex]->setSelected(false);
    }
    
    selectedTrackIndex = index;
    
    if (selectedTrackIndex >= 0 && selectedTrackIndex < static_cast<int>(headers.size())) {
        headers[selectedTrackIndex]->setSelected(true);
    }
}

void TrackHeaderList::trackAdded(Track* track) {
    if (track) track->addChangeListener(this);
    rebuildHeaders();
}

void TrackHeaderList::trackRemoved(int) {
    rebuildHeaders();
}

void TrackHeaderList::trackChanged(Track*) {
    repaint();
}

void TrackHeaderList::trackListChanged() {
    rebuildHeaders();
}

void TrackHeaderList::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (auto* track = dynamic_cast<Track*>(source))
        for (const auto& header : headers)
            if (header->getTrack() == track) header->setTrackName(track->getName());
}

void TrackHeaderList::renameTrackById(const juce::String& trackId, const juce::String& name) {
    const auto trimmed = name.trim();
    if (trimmed.isEmpty()) return;
    if (auto* track = trackList.getTrackById(trackId)) {
        track->setName(trimmed);
        track->sendChangeMessage(); // Refresh this list and other track listeners.
    }
}

void TrackHeaderList::removeTrackById(const juce::String& trackId) {
    if (auto* track = trackList.getTrackById(trackId)) {
        const int index = trackList.indexOfTrack(track);
        if (index >= 0) trackList.removeTrack(index);
    }
}

void TrackHeaderList::removeTrackWithConfirmation(const juce::String& trackId) {
    auto* track = trackList.getTrackById(trackId);
    if (!track) return;
    const auto name = track->getName();
    const int placements = track->getNumClipInstances();
    confirmAsync("Remove Track",
        "Track \"" + name + "\" contains " + juce::String(placements) + " placement(s). "
            "Removing it permanently deletes those placements; the pooled MIDI source stays in Clips.",
        "Remove Track", this, [safe = juce::Component::SafePointer<TrackHeaderList>(this), trackId] {
            if (safe != nullptr) safe->removeTrackById(trackId);
        });
}

void TrackHeaderList::rebuildHeaders() {
    headers.clear();
    
    const auto& tracks = trackList.getTracks();
    for (size_t i = 0; i < tracks.size(); ++i) {
        auto* track = tracks[i].get();
        auto header = std::make_unique<TrackHeader>(track, static_cast<int>(i));
        
        int index = static_cast<int>(i);
        header->onSelected = [this, index] {
            setSelectedTrack(index);
            if (onTrackSelected) onTrackSelected(index);
        };
        header->onMuteToggled = [this, index](bool muted) {
            if (onTrackMuteToggled) onTrackMuteToggled(index, muted);
        };
        header->onSoloToggled = [this, index](bool solo) {
            if (onTrackSoloToggled) onTrackSoloToggled(index, solo);
        };
        header->onRenameRequested = [safe = juce::Component::SafePointer<TrackHeaderList>(this), id = track->getId()] {
            if (safe == nullptr) return;
            auto* target = safe->trackList.getTrackById(id);
            if (!target) return;
            showTextPrompt("Rename Track", "New track name:", target->getName(), safe.getComponent(),
                [safe, id](const juce::String& name) {
                    if (safe != nullptr && name.isNotEmpty()) safe->renameTrackById(id, name);
                });
        };
        header->onRemoveRequested = [safe = juce::Component::SafePointer<TrackHeaderList>(this), id = track->getId()] {
            if (safe != nullptr) safe->removeTrackWithConfirmation(id);
        };
        
        if (static_cast<int>(i) == selectedTrackIndex) {
            header->setSelected(true);
        }
        
        addAndMakeVisible(*header);
        headers.push_back(std::move(header));
    }
    
    updateHeaderPositions();
}

void TrackHeaderList::updateHeaderPositions() {
    int y = headerHeight - scrollOffset;
    for (auto& header : headers) {
        header->setBounds(0, y, getWidth(), TrackHeader::defaultHeight);
        y += TrackHeader::defaultHeight;
    }
}

} // namespace vibedaw
