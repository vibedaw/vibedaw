# T03: Routing, Timing, and Placement

Status: ready | Milestone: M1 | Depends on: none

## Outcome

A user can place an edited pooled MIDI clip on a timeline and assign its instrument. The model has explicit timing and identity rules that playback can rely on.

## Read First

- `src/project/Project.h`, `Track.h`, `Track.cpp`, `ChannelList.h`, `ChannelList.cpp`
- `src/project/ClipInstance.h`, `Clip.h`, `Clip.cpp`, `ClipPool.cpp`, `Note.h`
- `src/ui/sidebar/clips/ClipsSidebar.cpp`
- `src/ui/timeline/TimelineContent.cpp`, `TimelineLane.cpp`, `TimeRuler.cpp`
- `src/ui/panels/TimelinePanel.cpp`, `src/ui/MainContent.cpp`

`ClipInstance` already stores a destination `channelId`; `ChannelList` currently resolves channels by mutable index. No code creates timeline placements through `Track::addClipInstance`. Lanes expose `setClipPool` but are not given the pool.

## Implementation Checklist

- [ ] Document quarter-note beats as the unit for MIDI source length, note start/duration, instance start/duration, and loop bounds. Fix timeline drawing/grid/labels and conversion callers accordingly. Do not blindly convert audio-file durations to beats; audio playback is deferred.
- [ ] Define instance start as the clip-local origin. For M1, play the source once, truncate at the instance end, and leave silence if the placement exceeds source length. Do not silently implement `MidiClip::loopEnabled` as transport looping.
- [ ] Give instrument channels stable IDs and explicit lookup. Keep index-based UI selection separate. Reorder must not change instance destinations; deleted/missing destinations render silence and show an unresolved assignment.
- [ ] Preserve many-to-many routing: one track may hold placements for different channels, and multiple tracks may target one channel. Do not activate `mixerTrackId` as a new bus-routing system.
- [ ] Add the smallest complete placement action: selected clip + selected track + selected destination + start position -> new instance. Use a beat-position field/default zero before T01 provides a seek cursor. Explain/disable the action when a required selection is missing.
- [ ] Pass the clip pool to every existing and newly created lane. Draw placement length, name, destination, instance selection, and mute accurately.
- [ ] Provide selection, delete, and a simple snapped move operation or editable start field. Full drag/drop, resizing, and duplication tools are not required.
- [ ] Notify model changes for placement edits and note edits so T06 can invalidate render data. Do not rely on direct mutable-vector access without notification.
- [ ] On source clip deletion, remove or explicitly invalidate its placements; prevent recycled IDs from resolving stale references to unrelated clips.
- [ ] Record the final unit, identity, deletion, and notification APIs below for T06/T02/T07.

## Acceptance Checks

- [ ] Create a four-beat clip and place it at beat zero. At 120 BPM it spans two seconds, not four; a tempo change leaves its musical position unchanged.
- [ ] Place one source twice; editing its notes updates both placements. Deleting one placement keeps the source and other placement.
- [ ] Put two destinations on one track and one destination across two tracks. Both arrangements retain their assignments after channel reorder.
- [ ] Empty project, missing selection, removed channel, removed source, and invalid duration/start are handled without crashes or silent misrouting.
- [ ] Existing and newly added tracks display instances; no source edits are needed to populate the timeline.

## Verification and Boundaries

Add model regression cases for ID stability, deletion policy, and timing conversions using the test seam established in T06; until it exists, capture cases alongside the changed model code for integration there. UI placement can be checked through the watcher. No scheduler, plugin graph rewrite, audio rendering, or new persistence format belongs here.

## Completion Record

- Changed files: pending
- Final APIs/contracts: pending
- Verification performed: pending
- Unverified checks/blockers: pending
- Handoff to T06/T02: pending
