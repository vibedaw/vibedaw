# T12: Timeline Drag-and-Drop

Status: blocked | Milestone: M1.5 | Depends on: T11, T03 implementation

Started 2026-09-08 under explicit user authorization to continue the roadmap
despite prior watcher acceptance blocks. T15 discovery does not block T12.
Preserve dirty work and T10's stock-JUCE initial-testing editor override. Only
the documented independent offline target may be built/run; no app launch,
dependency edits, external agent review, staging or commit.

## Outcome

Place pooled clips by dragging and reposition existing placements directly. Dropping below the existing tracks creates a track with a visible preview, not an invisible side effect. No move mode or Move button is required.

## Read First

- `T03-routing-timing-placement.md`, `T06-safe-audio-boundary.md`
- `src/ui/sidebar/clips/ClipsSidebar.cpp`
- `src/ui/panels/TimelinePanel.h`, `TimelinePanel.cpp`
- `src/ui/timeline/TimelineLane.cpp`, `TimelineContent.cpp`, `TimeRuler.cpp`
- `src/project/ClipInstance.h`, `Track.h`, `Track.cpp`, `TrackList.h`, `ClipPool.h`
- `src/ui/MainContent.cpp`

The current toolbar places/moves/deletes/assigns a selected instance using an explicit start beat. Tracks are arrangement lanes; destination channel IDs belong to instances. There are no pooled-clip drag sources or timeline drop targets yet.

## Interaction Contract

- Drag a typed stable source ID from Clips. New placements use the active instrument channel, explicitly named in the preview; missing/non-instrument destinations reject with actionable feedback. A valid instrument without a loaded plugin retains T03's visible warning policy.
- Drop inside a lane to place on that track. Only unused arrangement space below lanes creates a track; ruler, headers, sidebars, and space outside the arrangement are not new-track targets.
- Show a ghost with snapped start and source duration, target-lane highlighting, and a labeled New track preview where applicable. Use the same coordinates for preview and commit.
- Drag an existing instance after a movement threshold; preserve the pointer's grab offset. Move within/across tracks, including unused space to create a track, without copying it or changing source, duration, destination, or mute state.
- Use a documented beat-grid snap and temporary modifier bypass. Clamp starts to nonnegative finite quarter-note beats. Account for horizontal/vertical scrolling; define bounded edge autoscroll so offscreen locations remain reachable.
- Single-click selects; double-click/Edit opens the shared source, not an independent copy. Unresolved placements stay visible and must not dereference missing sources.
- Escape, drag exit without drop, invalid targets, or vanished source/track/channel leave the model unchanged. Preview never mutates the arrangement.

## Implementation Checklist

- [x] Add typed clip drag sources and validated timeline targets using T11's dispatch conventions.
- [x] Centralize beat/coordinate and snap calculations enough to share rendering, hit testing, preview, and commit without creating a general tool framework.
- [x] Implement ghost/target/new-track feedback and active-destination warnings before drop.
- [x] Commit placement plus optional track creation as one failure-safe operation. Revalidate stable IDs and limits at commit; no empty orphan track on rejection.
- [x] Implement threshold-based direct dragging with grab offset, cross-track relocation, cancellation, and scrolling. Preserve instance identity where supported; otherwise document and test selection/reference repair.
- [x] Publish only committed edits through the existing model/render boundary; do not publish every preview frame or expose partial cross-track moves to audio.
- [x] Connect explicit editing from timeline placements to the existing shared-source editor behavior.
- [x] Keep existing toolbar actions until their replacements are verified; T14 removes redundant controls and retains contextual precise position/routing access.
- [x] Add tests for snap/coordinate transforms, target classification, identity/routing preservation, atomic failure, cancellation, and source removal during a drag.

## Acceptance Checks

- [ ] Drop a source at a visible beat on an existing track and hear it at that position through the previewed destination.
- [ ] Drop below all tracks, including an empty timeline, and create exactly one track and instance with a correct preview.
- [ ] Drag instances horizontally and between tracks without a tool switch; original grab offset, snap, routing, duration, and shared-source identity are preserved.
- [ ] Scrolled/zoomed views, lane boundaries, overlapping instances, and autoscroll select and commit the previewed target. Document deterministic overlap hit order.
- [ ] Cancelled/invalid drops and objects deleted during dragging do not mutate placement or leave orphan tracks.
- [ ] Moving during playback uses the safe publication/cleanup path and does not strand notes; audible verification is separately recorded.
- [ ] Double-click opens the intended source; creating or placing a clip alone does not open Piano Roll.

## Boundaries

No trim/stretch, multi-selection, copy-drag, clip-local looping, automatic per-track instrument assignment, sample placement, or undo framework. Preserve T03's play-once/truncation and unresolved-placeholder contracts.

## Completion Record

### Result and Files

2026-09-08: implementation and independent offline verification complete. Blocked
on unobserved watcher/manual acceptance, not marked done. T15 and earlier dirty
work are preserved; T10's stock-JUCE initial-testing editor override is unchanged.

- `src/ui/DragPayload.h`, `src/ui/sidebar/clips/ClipsSidebar.{h,cpp}`: typed MIDI
  source producer/decoder and threshold-based pool drag startup.
- `src/project/ClipInstance.h`, `Track.h`, `TrackList.{h,cpp}`: immutable session
  identities, stable lookup and candidate-first placement/ownership transfer.
- `src/ui/timeline/TimelineGeometry.h`, `TimelineContent.{h,cpp}`, `TimelineLane.cpp`:
  shared geometry, pooled drop target, direct moves, preview/cancellation/autoscroll.
- `src/ui/panels/TimelinePanel.{h,cpp}`, `src/ui/MainContent.cpp`: active destination,
  actual scrollbar synchronization, selection repair and shared-source editor callback.
- `tests/offline_tests.cpp`, `tests/README.md`, this task and roadmap: regressions,
  contracts, verification and watcher handoff. No test CMake changes were needed.

### Interaction and Commit Contract

- Pooled payload: `{ type: "vibedaw.clip", clipId: <nonnegative int> }`. Only MIDI
  rows produce it. Plugin/sample/unknown payloads, bare IDs/strings and malformed
  clip IDs are rejected. Payload IDs are re-resolved, never interpreted as paths.
- TimelineContent is the single accepting ancestor for both lane children and
  unused background. Interest is payload-only: stock JUCE's discovery supplies
  source coordinates. Enter/move/drop alone classify target-local geometry. The
  ruler, headers, toolbar, sidebars and outside bounds cannot create tracks.
  Half-open lane rectangles select existing tracks; only below the last lane
  selects New track, including zero lanes. Exactly one new lane is created on success.
- The ghost spans the source duration for new placements and the existing duration
  for moves. It shows start beat, lane/New track and named stable destination, with
  a no-plugin warning. Blue denotes valid placement, amber rejection. No valid
  active Instrument means new placement is rejected with Channel Rack guidance;
  an unloaded Instrument remains allowed with the existing warning policy.
- Snap is nearest 1/16 note (0.25 quarter-note beats), with half-grid ties rounding
  upward. Alt temporarily bypasses it, including stationary modifier changes on
  the timer. Starts clamp to zero and must have finite positive-duration ends.
  Horizontal scroll is in pixels, vertical scroll in pixels below the fixed ruler;
  render, hit testing, preview and commit share beat transforms. Zoom remains the
  existing pixels-per-beat API; this task adds no zoom control.
- Pool/direct drag threshold is 5 pixels. Existing moves preserve the beat-valued
  grab offset, object address, immutable UUID session ID, source ID, duration, mute
  and destination ID, regardless of active audition selection. Same-track moves
  preserve insertion order; cross-track moves append at the target's paint order.
  Last painted/insertion-last overlapping placement wins hit testing. Selection
  follows the committed object after any lane rebuild; clicking selects without
  editing. Selection itself remains the existing UI model flag.
- Edge autoscroll runs at 30 Hz, at most 12 pixels per axis per tick inside a
  24-pixel edge band. Outside/ruler positions do not scroll. Real scrollbars clamp
  to their existing extent; horizontal extent retains T03's 1e9-pixel cap and
  arrangement/loop/default duration policy, with no unbounded preview growth.
  One extra empty vertical row makes New track reachable when all lanes fill the
  viewport. Every tick recomputes the preview from the updated scrollbar offsets.
- Escape, outside release and external drag exit clear transient state without
  placement changes. Any structural track-list rebuild cancels the gesture rather
  than reinterpreting a stale lane index. Vanished instance/source/destination IDs
  reject at commit. Already-unresolved instances remain safely movable/silent;
  a source or destination that was resolved at drag start but vanishes cancels that
  move. A just-changed active destination cannot silently replace the previewed
  route at new-placement commit. Deleted external source components clear through
  a SafePointer timer/commit check, because JUCE may omit their exit callback.
- `TrackList::commitPlacement` is message-thread-only. Empty target ID requests a
  candidate lane; moving resolves source-track and instance session IDs. New MIDI
  source/routing validation occurs in TimelineContent immediately before calling
  it, without a message-loop yield; the model operation additionally checks IDs,
  valid timing and finite ends. All destination storage/callback binding is prepared
  before detaching ownership. A new lane enters the list already populated, and
  observers are notified only after the full transfer. Invalid operations create
  no orphan lane. Existing TrackList has no track/placement-count cap; the existing
  65,536-render-note all-or-empty snapshot overflow policy is unchanged.
- Previews never call placement setters, create tracks or publish snapshots.
  Committed mutations use the existing asynchronous Track/ArrangementPublisher
  boundary; audio retains the old complete snapshot until a new complete snapshot
  is acquired. No audio graph mutation or new quiescence scope is needed for lanes.
- Double-click dispatches the stable source ID to MainContent's existing resolve,
  focus/reuse/open path, not a per-instance editor. Dragged double-click releases
  do not open an editor. Missing sources do nothing. Toolbar Place/Move/Delete/
  Assign/Mute and precise Start beat remain; the status tooltip documents gestures.

### Verification and Review

Only the documented independent offline project was configured/built/run:

```sh
cmake -S tests -B /tmp/opencode/vibedaw-t06-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/vibedaw-t06-tests --target offline_tests --parallel 2
ctest --test-dir /tmp/opencode/vibedaw-t06-tests --output-on-failure
git diff --check
```

Final independent build succeeded; full CTest 1/1 passed (6.72 seconds), retaining
all earlier regressions and callback allocation/assertion/leak checks. Diff check
passed. Development-time missing includes/explicit JUCE argument types were fixed.
An experimental global allocation-failure injector aborted inside JUCE construction;
it was removed rather than claiming recoverable process-wide out-of-memory safety.
Invalid/stale-ID and timing failure atomicity is covered by the final suite.

- `timelineDragTests`: actual ClipRow payload, child-first component discovery using
  source coordinates, target-local callbacks, offscreen ghost colour, empty/existing/
  below-lane drops, active/no-plugin warnings, preview revision isolation, actual
  lane selection then parent-listener callbacks, within/across/new-lane moves,
  pointer/UUID/routing/duration/mute/selection preservation, Alt/scroll/zoom/grab math,
  Escape/outside/source deletion, safe unresolved editing/moving, stale model IDs.
- `timelineInvalidationTests`: non-Instrument and vanished active destination
  rejection, deleted hovered lane cancellation, actual panel scrollbars with
  bounded timer ticks to both limits, below-lane reachability, outside/exit stopping,
  source removal during hover and vanished producer component cleanup.
- `timelineCommitTests`: invalid/inconsistent IDs leave ownership/count unchanged;
  a synchronous trackAdded observer sees exactly one complete moved placement;
  retained audio snapshot stays old until asynchronous publication, then contains
  exactly the new position and original route.
- Source review covered changed production paths and stock JUCE
  `juce_DragAndDropContainer.cpp:66-79,143-153,291-308` plus
  `juce_Component.cpp:2133-2272`. This confirmed source-coordinate discovery,
  omitted exit on source destruction and component-before-listener mouse ordering.
  No external agent review was invoked; independent parent review remains separate.

No native peer/window, GUI application, device or installed plugin was launched.
Component tests invoke real callbacks and hit-testing but do not inject OS mouse
events or instantiate JUCE's native drag image. MainContent's editor handoff is
source-reviewed, not compiled by the offline target. No application build/launch,
dependency edit, staging or commit was performed.

### Remaining Watcher Checks

1. Drag real Clips rows into empty timeline, existing lanes and below populated
   lanes. Observe ghost text/colour/duration and exactly one placement/new lane.
   Verify ruler/header/toolbar/sidebar/outside rejection and narrow layout readability.
2. With two instruments, hear new drops through the previewed route. Reject missing/
   non-Instrument active selection, warn for unloaded instruments, and verify moves
   retain routing despite changing active audition selection.
3. Drag within/across/new lanes, including overlapping placements, at scrolled
   positions. Observe grab offset, 1/16 snap/Alt bypass, bounded stationary edge
   autoscroll and agreement between ghost and committed position. Check native
   mouse capture and Escape both docked and in existing pop-out ownership.
4. Cancel/release outside; delete/rebuild sources, placements, channels or tracks
   during gestures. Observe no orphan lane, stale preview or placement mutation.
   Already-unresolved placeholders must remain safe to select/move.
5. Move during audible playback and verify existing snapshot cleanup prevents stuck
   notes. Confirm double-click opens/focuses the shared source once and placement
   alone does not open Piano Roll. All earlier M1/T10/T11 manual blocks remain.

### Handoff

T13 is next in workstream order, pending acceptance or explicit continuation. T14
owns complete target-specific context actions and toolbar removal only after manual
replacement acceptance. T07 must serialize/restore track and instance UUID identities
alongside existing pooled source/channel IDs; no persistence format or compatibility
layer was introduced here. T15 routing discovery is untouched and does not change
per-instance instrument routing.

### Independent Review Corrections

Independent explore review supplied two P2 findings after the initial handoff.
Both are corrected; T12 remains blocked on manual acceptance, not missing these
implementation fixes. No external agent system was invoked for this correction.

1. **Escape gesture lifetime.** Clearing only TimelineContent's preview allowed
   reentry to revive a cancelled pooled drag, while ClipRow could restart JUCE's
   deleted drag image during the same press. Each row now owns one shared typed
   payload per mouse-down and permits at most one start during that press. Mouse-up
   disarms further starts without invalidating the imminent JUCE drop callback;
   a new down creates fresh gesture identity and invalidates the preceding payload.
   Target-side Escape marks the shared payload cancelled. Enter/move/timer/drop
   honor that state across exit/reentry and other timeline targets. Structural lane
   invalidation also marks the gesture, so release-only handling cannot resurrect
   a stale-target drop. Ordinary drag exit clears only transient preview state and
   still permits reentry. Direct instance dragging retains its existing cancellation.
2. **Release-only target discovery.** Stock JUCE `juce_DragAndDropContainer.cpp:100-117`
   calls `findTarget` and then `itemDropped` on mouse-up, without an intervening
   enter/move. Drop now initializes a new, non-cancelled gesture from the typed
   payload and target-local release position when necessary. The final source,
   destination and geometry are still validated. Route-change rejection applies
   only if the preceding preview was visible and valid, not an invisible ruler
   preview. Existing lanes, empty/new lanes, and ruler-to-valid release work without
   weakening explicit Escape, stale structure, missing ID or invalid geometry guards.

The shared payload keeps `type`/`clipId` and adds a transient message-thread
`cancelled` flag when invalidated; it is not arrangement state or persisted data.
Interest remains payload-based, independent of source/target coordinates.
JUCE's Escape deletion at `:202-213` and component-before-listener release ordering
were source-reviewed. When JUCE itself consumes Escape and deletes the drag image,
the source's per-press latch prevents another native drag startup.

**Cross-window policy:** pooled clip startup now explicitly passes
`allowDraggingToOtherJuceWindows=true` using the ScaledImage overload. This enables
pooled drops into the existing popped-out timeline rather than silently restricting
them to the source's component tree. JUCE documents this at
`juce_DragAndDropContainer.h:81-83`. Native drag-image/window behavior still needs
watcher acceptance; no native peer was created by the regression.

`timelineGestureReviewTests` dispatches actual ClipRow mouseDown/drag/up callbacks
through a private starter seam (replacing only native drag-image creation), checking
one start per press and the actual cross-window option. Real timeline callbacks
cover continued movement, Escape/exit/reentry/drop with no new down, a second target,
fresh-down recovery, mouse-up before drop, release-only empty/existing lanes,
ruler-to-lane/new-track release, valid-preview route changes, stale-track reentry,
ordinary exit/release and cancelled release-only new-track rejection. The older
structural-invalidation fixture now supplies a new payload for a genuinely new press.

Negative verification with the old enter-required drop guard restored fails
`tracks.getNumTracks() == 1 && count() == 1` (CTest 0/1, 0.51 seconds). With that fix
restored but persistent Escape marking removed, the regression fails
`!docked.getPreview().visible` (CTest 0/1, 0.52 seconds). Both fixes are restored.

Commands for correction verification:

```sh
cmake --build /tmp/opencode/vibedaw-t06-tests --target offline_tests --parallel 2
ctest --test-dir /tmp/opencode/vibedaw-t06-tests --output-on-failure
git diff --check
```

The two deliberate negative runs used `ctest --test-dir
/tmp/opencode/vibedaw-t06-tests` and inspected its LastTest.log. Final independent
build succeeded; full CTest 1/1 passed (6.62 seconds), with prior suites retained.
Diff check passed. Native OS events, Escape focus routing, drag-image teardown and
docked-to-popped-out dragging remain watcher checks. No application build/launch,
dependency edits, staging or commit. T15, T10's initial-testing override and all
other work remain preserved.
