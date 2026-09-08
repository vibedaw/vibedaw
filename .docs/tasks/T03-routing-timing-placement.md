# T03: Routing, Timing, and Placement

Status: blocked | Milestone: M1 | Depends on: none

Implementation present, 2026-09-08. Blocked on watcher/manual acceptance, not marked
done. Core model cases now pass in T06's offline suite. User override on 2026-09-08
("just go onto the next task") authorizes T06 without asserting T03 acceptance.

## Outcome

A user can place an edited pooled MIDI clip on a timeline and assign its instrument. The model has explicit timing and identity rules that playback can rely on.

## Read First

- `src/project/Project.h`, `Track.h`, `Track.cpp`, `ChannelList.h`, `ChannelList.cpp`
- `src/project/ClipInstance.h`, `Clip.h`, `Clip.cpp`, `ClipPool.cpp`, `Note.h`
- `src/ui/sidebar/clips/ClipsSidebar.cpp`
- `src/ui/timeline/TimelineContent.cpp`, `TimelineLane.cpp`, `TimeRuler.cpp`
- `src/ui/panels/TimelinePanel.cpp`, `src/ui/MainContent.cpp`

Baseline before this task: `ClipInstance` stored a destination `channelId`, but
`ChannelList` resolved only mutable indexes. There was no placement creation caller
and lanes were not given the pool. The implementation below replaces that baseline.

## Implementation Checklist

- [x] Document quarter-note beats as the unit for MIDI source length, note start/duration, instance start/duration, and loop bounds. Fix timeline drawing/grid/labels and conversion callers accordingly. Do not blindly convert audio-file durations to beats; audio playback is deferred.
- [x] Define instance start as the clip-local origin. For M1, play the source once, truncate at the instance end, and leave silence if the placement exceeds source length. Do not silently implement `MidiClip::loopEnabled` as transport looping.
- [x] Give instrument channels stable IDs and explicit lookup. Keep index-based UI selection separate. Reorder must not change instance destinations; deleted/missing destinations render silence and show an unresolved assignment.
- [x] Preserve many-to-many routing: one track may hold placements for different channels, and multiple tracks may target one channel. Do not activate `mixerTrackId` as a new bus-routing system.
- [x] Add the smallest complete placement action: selected clip + selected track + selected destination + start position -> new instance. Use a beat-position field/default zero before T01 provides a seek cursor. Explain/disable the action when a required selection is missing.
- [x] Pass the clip pool to every existing and newly created lane. Draw placement length, name, destination, instance selection, and mute accurately.
- [x] Provide selection, delete, and a simple snapped move operation or editable start field. Full drag/drop, resizing, and duplication tools are not required.
- [x] Notify model changes for placement edits and note edits so T06 can invalidate render data. Do not rely on direct mutable-vector access without notification.
- [x] On source clip deletion, remove or explicitly invalidate its placements; prevent recycled IDs from resolving stale references to unrelated clips.
- [x] Record the final unit, identity, deletion, and notification APIs below for T06/T02/T07.

Checked implementation boxes mean source implementation/contracts are present, not
runtime acceptance. Silence/truncation are renderer requirements for T02; T03 adds
no scheduler or arrangement audio playback.

## Acceptance Checks

- [ ] Create a four-beat clip and place it at beat zero. At 120 BPM it spans two seconds, not four; a tempo change leaves its musical position unchanged.
- [ ] Place one source twice; editing its notes updates both placements. Deleting one placement keeps the source and other placement.
- [ ] Put two destinations on one track and one destination across two tracks. Both arrangements retain their assignments after channel reorder.
- [ ] Empty project, missing selection, removed channel, removed source, and invalid duration/start are handled without crashes or silent misrouting.
- [ ] Existing and newly added tracks display instances; no source edits are needed to populate the timeline.

## Verification and Boundaries

Add model regression cases for ID stability, deletion policy, and timing conversions using the test seam established in T06; until it exists, capture cases alongside the changed model code for integration there. UI placement can be checked through the watcher. No scheduler, plugin graph rewrite, audio rendering, or new persistence format belongs here.

## Completion Record

### Changed Files

- Model: `src/project/Channel.{h,cpp}`, `ChannelList.{h,cpp}`, `Project.{h,cpp}`, `Clip.{h,cpp}`, `ClipInstance.h`, `ClipPool.{h,cpp}`, `Note.h`, `Track.{h,cpp}`.
- Transport units/input validation only: `src/core/TransportState.{h,cpp}`. No advancement or audio ownership changes.
- Placement integration: `src/ui/MainContent.{h,cpp}`, `src/ui/panels/TimelinePanel.{h,cpp}`, `src/ui/sidebar/clips/ClipsSidebar.{h,cpp}`, `src/ui/sidebar/channel/ChannelRackSidebar.{h,cpp}`.
- Timeline: `src/ui/timeline/TimelineContent.{h,cpp}`, `TimelineLane.{h,cpp}`, `TimeRuler.{h,cpp}`, `TrackHeader.{h,cpp}`.
- Essential editor fixes: `src/ui/editor/NoteGridComponent.{h,cpp}`, `PianoRollEditor.{h,cpp}`, `PianoRollKeyboard.{h,cpp}`. The viewport height/pitch mismatch was brought forward from T08 because it could make visible note entry clamp above pitch 127.
- Regression specifications: `src/project/T03-regression-cases.md`. No test target was introduced or claimed to run.
- Progress/contracts: `.docs/ROADMAP.md` and this task file.

### Final APIs and Contracts

- MIDI `Note::startTime/duration`, `MidiClip::duration`, `ClipInstance::startTime/duration`, and `LoopRegion` bounds use quarter-note beats, independent of time-signature denominator. AudioClip times remain seconds. Transport's existing seconds API remains explicitly seconds; `setPositionInBeats/getPositionInBeats` convert using 60/BPM. Four beats at 120 BPM correspond to two seconds.
- A placement's start maps source-local beat zero into the arrangement. `MidiClip::startTime` does not offset its notes. Its playable interval is half-open and bounded by both placement duration and source duration. There is no automatic repetition: `loopEnabled` remains reserved for future clip-local looping, not transport looping. Notes outside the source end are retained for editing but omitted from the placement preview/future playback.
- `Channel::getId()` returns a constructor-assigned const ID. `ChannelList::getChannelById` resolves destinations; `getChannel(index)`, `indexOfChannel`, and reorder/remove APIs remain index-based UI operations. IDs are not recycled after clear/removal within a list/pool lifetime. Exhausted integer ID allocation fails instead of wrapping. Persistence/ID restoration belongs to T07.
- Channel model setters (`setName`, plugin/sample, mute, colour, gain/pan, and reserved mixer-track ID) are message-thread-only and queue coalesced `juce::ChangeBroadcaster` notifications. ChannelList subscribes before announcing additions, unsubscribes before removal/clear/destruction, and forwards `channelChanged(Channel*)` on the message thread. Reorder retains the subscription and identity. Processing, preparation, resource release, and meter writes do not notify. This does not establish audio-safe model mutation; that remains T06.
- `Project::getActiveChannelId()` retains audition/placement-choice identity through reorder. `getActiveChannel()` computes the current index and existing project listeners still receive indexes for the live-MIDI bridge. Removed/invalid selection becomes -1, never a different instrument. Channel-rack highlight follows project selection. Choosing another active instrument does not rewrite any placement.
- Changes to the active Channel refresh existing project listeners with the same current index, without changing selection. TimelinePanel also observes all ChannelList changes so non-active destinations repaint. Sidebar plugin/sample drop handlers now rely on model notifications instead of manually reselecting the active channel or rebuilding rows inside the drop callback.
- Sources are shared by pooled ID, not cloned for placement. Each instance has its own destination, start, duration, mute, and selection. Track is only an arrangement container here; many-to-many routing remains intact and `mixerTrackId` is untouched.
- `Track` is a `juce::ChangeBroadcaster`: add/remove/clear and all owned-instance setters send asynchronous, coalesced invalidation. Collection access does not expose a mutable vector; instance setters retain the owning Track's private notification callback. Invalid timing setters preserve the previous value; invalid/null additions are rejected.
- `MidiClip::getNotes()` is const-only; `findNoteAt()` returns a borrowed const pointer. Use `addNote`, `updateNote(pointer, replacement)`, `removeNote`, and `clearNotes`. Structural mutations synchronously emit `Clip::Listener::notesInvalidated` before vector changes; grids clear all borrowed selection/hover/drag pointers. Updates do not relocate notes. Notes are insertion-ordered, not guaranteed time-sorted; T02 must sort render events.
- `Clip::Listener::clipChanged` is synchronous for MIDI note edits and source timing/name/colour/mute/loop setters. ClipPool forwards conservative `clipChanged(id, clip)` invalidation for all pooled sources on any source edit. This is deliberately not a realtime callback. Listeners must not reentrantly mutate collections during notification.
- Source deletion leaves unresolved placements with original IDs, timing, and destinations. `ClipPool::Listener::clipWillBeRemoved(id)` fires while the source is alive; MainContent closes all its editor windows then, allowing grids to detach safely. `clipRemoved(id)` follows destruction. `clearClips` uses the same path without resetting the allocator. Source deletion has a Clips-sidebar button; placement deletion never deletes its source.
- Timeline geometry uses `set/getPixelsPerBeat` and zero-based `b0`, `b1`, etc. Lane construction always receives both pool and channel list. Rendering uses instance length/selection/mute, actual shared-note previews, stable destination lookup, and explicit unresolved/silent labels, not fake waveform/note decorations. A numeric Start beat field defaults to zero; Place creates, Move edits the selected start, Assign changes its destination, and Delete/Mute affect the selected instance. Audio/Pattern sources cannot use the MIDI Place action.
- Structural track-list changes clear track/placement selection rather than silently moving selection to another track. Controls re-resolve the current model on each action. Numeric input rejects malformed, negative, and nonfinite starts; source/note/placement timing validation rejects nonpositive lengths and overflowing ends. The timeline scroll extent is capped at 1e9 pixels for safe GUI coordinates; extremely distant model placements beyond that extent are not navigable in this minimal UI.
- Track-header name/background/colour-strip left clicks invoke the existing `TrackHeader::onSelected` -> TrackHeaderList -> TimelinePanel path. Mute/solo remain independent child controls; no recursive mouse listener or child-event interception was introduced.

### Verification Performed

- Read repository instructions, roadmap/task, model, timeline, sidebar, editor, and live-selection integration paths. Confirmed initial worktree was clean. Marked T03 in_progress before implementation.
- Ran `git diff --check` with no findings during source audit; repeated after the completion record update.
- Source searches found no remaining `pixelsPerSecond`/`PixelsPerSecond` timeline API, old project index storage, old clip-index selection storage, `1.0 / gridResolution` math, mutable note-vector caller, or direct drag-note setters. Reviewed caller changes and the actual Main.cpp index bridge; it remains compatible with index notifications.
- Inspected both note deletion callbacks: they copy note data before removal. Inspected structural invalidation and source pre-removal editor teardown paths, existing/new lane construction, placement action guards, and ID allocation/clear behavior.
- Captured identity, routing, deletion ordering, timing conversion/bounds, mutation notification, multi-editor pointer lifetime, and watcher UI cases alongside the model. Inspected CMake: only the GUI application target exists, no independent runnable regression target.
- No application build, launch, test compilation, audio execution, watcher observation, or commit was performed. Source checks are not compilation or runtime proof.

### Independent Review Corrections

- Fixed the uninvoked track-header selection callback and dead ChannelList content-change notifier reported by independent review. Added regression specifications for header/child-control separation, direct Channel setters, coalescing/thread identity, reorder and queued-deletion lifetimes, active/non-active destination refresh, and absence of notifications from audio processing paths.
- Inspected Channel and ChannelMixer processing/preparation/release paths and Channel setter callers. Notification calls are confined to model setters; the audio paths were not changed. Inspected registration before channelAdded, unsubscription before erase/clear, and the owning-list check before forwarding changes.
- Ran `git diff --check` after the corrections and inspected the changed source. These corrections have not been compiled or exercised through the watcher. T03 remains blocked; T06 has not started.

### Unverified Checks and Blockers

- All acceptance boxes remain unchecked. Next action: independent source review plus watcher verification of source editing, placement at b0/b4, repeated/shared placements, Move/Delete/Assign/Mute, empty/invalid selections, and source deletion with multiple editors open.
- Channel reorder/removal and timing/notification cases need the future model test seam or an existing external harness; there is no new channel reorder/delete UI in this task. Do not claim they were exercised through the watcher.
- Watcher compilation, JUCE callback/layout behavior, narrow/popped-out panel usability, editor resize behavior, and multi-editor interaction cancellation have not been observed.
- Existing plugin-editor lifetime and channel/plugin mutation versus the audio callback are not solved here. In particular, deletion/replacement of a channel with an open plugin editor is not verified safe. T06 must establish ownership/structural safety before arrangement playback; T03's clip-editor cleanup does not solve plugin-editor ownership.
- Arrangement playback does not exist yet. Missing references and play-once truncation/silence are explicit data/display contracts, not tested audible output. No engine work should be inferred from this task.

### Handoff to T06/T02/T07

- Original gate: do not start T06 until acceptance or explicit user redirection. Superseded by the user's 2026-09-08 "just go onto the next task" override; T03 acceptance itself is still blocked.
- T06 should turn `src/project/T03-regression-cases.md` into plugin/hardware-independent executable tests and consume message-thread changes through a lifetime-safe snapshot boundary. Subscribe to ClipPool, TrackList, ChannelList, and per-Track changes; never traverse these UI-owned vectors from audio.
- T02 must resolve instance destinations by stable ID, sort/merge events per destination, honor source/placement/note mute and half-open bounds, play sources once, and render missing/unsupported references silently. Active audition selection is never an arrangement fallback.
- T07 must preserve source/channel IDs and allocator uniqueness when defining persistence. No legacy persistence format or index-to-ID compatibility layer was added.
- T08 retains broader editor/navigation polish. Only the invalidation, subdivision, pitch-coordinate, and viewport/ruler synchronization defects needed for reliable note entry were brought forward here.

### T06 Offline Integration

- The real T03 model now runs in `tests/offline_tests.cpp`, built by the independent
  `tests/CMakeLists.txt`. Stable channel/source IDs, many-to-many assignments, active
  selection after reorder/removal, shared source edits, deletion order/unresolved
  placements, timing conversions and half-open bounds, invalid input, note-pointer
  invalidation, channel notifications/queued deletion, and source/track snapshot
  publication passed. The compiler also tests play-once source/placement truncation.
- Commands/results and test scope are in `tests/README.md` and T06's completion
  record. This replaces the earlier absence of a runnable model seam; it does not
  retroactively assert any watcher acceptance box or all UI regression specifications.
- UI grid/zoom, multi-editor interaction cancellation, header selection, display
  refresh/layout and actual plugin loading still need the watcher. T03 remains
  truthfully blocked. T06 additionally fixes plugin window teardown and graph
  mutation contracts; those changes are not claimed as T03 runtime observations.
