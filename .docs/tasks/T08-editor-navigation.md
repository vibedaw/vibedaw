# T08: Editor Navigation

Status: done | Milestone: M2 | Depends on: T03, T01

Acceptance cleared 2026-09-10: user override ("assume everything is complete.. clear
this backlog") marks this task done; its pending watcher/manual observations
(alignment before/after scrolling, hit testing, follow suspend/resume with audio)
are accepted by the override rather than individually observed. Unchecked boxes in
this file are cleared by the same override. See the backlog clearance record in
`.docs/ROADMAP.md`.

Promote the blocking subset into M1 if a user cannot reliably write the clip required by T02. Do not delay all playback work for optional editor polish.

Scheduling update 2026-09-08: follows M1.5 and T07 unless navigation blocks the current workflow. T11 owns create-without-opening and explicit source editor entry; T12 owns timeline dragging and placement double-click. Keep those behaviors intact rather than reintroducing automatic editor opening here.

## Outcome

The piano roll's keyboard, notes, ruler, and hit testing agree at every scroll/zoom position. Longer clips can be edited without hidden content.

## Read First

- `src/ui/editor/PianoRollEditor.h`, `PianoRollEditor.cpp`
- `src/ui/editor/NoteGridComponent.cpp`, `PianoRollKeyboard.cpp`, `TimeRulerComponent.cpp`
- `src/ui/editor/ClipEditorWindow.cpp`
- `src/ui/timeline/TimelineContent.cpp`, `TimeRuler.cpp`

The grid alone is inside a viewport while the keyboard/ruler are siblings. The empty sync function is also uncalled; filling it alone will not connect scrolling. Grid and keyboard pitch math uses unequal component heights, and custom offsets coexist with viewport movement.

## Implementation Checklist

- [x] Choose one scroll coordinate source, preferably the viewport's visible area, and connect its actual change notifications to keyboard/ruler updates. (Viewport is the sole scroll source; `GridViewport::visibleAreaChanged` calls `PianoRollEditor::onViewChanged`, which syncs keyboard Y offset and ruler beat offset and distinguishes manual from programmatic moves.)
- [x] Share pitch-to-row and beat-to-x transforms between rendering and hit testing; avoid offsets being applied twice. (`src/ui/editor/PianoRollGeometry.h`: one transform for grid render/hit test, keyboard, and ruler; grid stays grid-local, keyboard/ruler apply the viewport offset once.)
- [x] Make wheel, scrollbars, programmatic scroll, resize, and zoom use that same state. Keep the keyboard aligned with the grid independently of viewport height. (All viewport moves flow through `visibleAreaChanged`; `setZoomLevel` preserves the left-edge beat; resize preserves follow via a programmatic-move guard.)
- [x] Replace the fixed eight-beat extent with one based on clip length/notes plus a documented editing margin. Keep all valid MIDI pitches reachable. (`pianoRollContentBeats`, min 8 beats + 4-beat margin; grid height is `numKeys * keyHeight` for all 128 pitches.)
- [x] Ensure note add/select/drag/delete and audition target the visible pitch/time after scrolling. Publish edits through T03/T06's change boundary. (Grid hit testing uses the shared geometry; keyboard audition uses the same rows; existing note listener boundary unchanged.)
- [x] Add optional playhead following without fighting manual scrolling; define when following suspends and resumes. Use T01's published position only. (Playhead drawn from `TransportState::getPositionInBeats` through a placement-to-source-local provider; auto-scroll only while playing; manual scroll/zoom suspends; the Follow toggle or playback restart resumes; never calls `setPosition`.)
- [x] Handle source deletion/project replacement while an editor is open without stale pointers. (T07's `clipWillBeRemoved`/`closeAllClipEditors` close editors while the source is alive; `NoteGridComponent` detaches its `Clip::Listener` on destruction/`setMidiClip(nullptr)` and clears borrowed selection on invalidation; covered by regression.)
- [x] Test coordinate round trips and clamping in pure logic; perform manual alignment/navigation checks through the watcher. (Offline `editorNavigationTests`; watcher checks below remain pending.)

## Acceptance Checks

- [ ] Notes line up with keyboard pitches and ruler beats before/after horizontal and vertical scrolling.
- [ ] Clicking/dragging after scrolling edits the expected note and auditions the expected pitch.
- [ ] MIDI pitches 0 and 127, fractional-row scrolling, supported zoom limits, and resize/scrollbar changes stay aligned.
- [ ] A clip longer than eight beats is fully navigable; no note becomes inaccessible because of fixed content bounds.
- [ ] Following playback can be disabled/suspended for editing; it does not create a second clock or change audio position.

## Completion Record

- Changed files: New `src/ui/editor/PianoRollGeometry.h` (shared transform,
  content-extent, source-local mapping, follow-band helpers). Changed
  `NoteGridComponent.{h,cpp}` (geometry-backed grid-local transforms, playhead,
  `gridContentChanged`, dropped dead scroll/time fields and `ScrollBar`
  listener), `PianoRollKeyboard.{h,cpp}` (shared geometry rows, no hardcoded
  127), `TimeRulerComponent.{h,cpp}` (playhead line), `PianoRollEditor.{h,cpp}`
  (viewport single source of truth, content extent, zoom anchor, note-centered
  initial view, `TransportListener` playhead + follow, programmatic-move
  guard), `ClipEditorWindow.{h,cpp}` (transport/provider wiring, Follow toggle),
  `MainContent.cpp` (placement-aware source-local provider), `tests/CMakeLists.txt`,
  `tests/offline_tests.cpp`.
- Coordinate/scroll/follow contract: the viewport is the only scroll source.
  Grid coordinates are grid-local (`timeOffset = 0`), the viewport translates
  it, and the keyboard/ruler consume the viewport's pixel offset once through
  `PianoRollGeometry`. Rows run top = pitch 127 to bottom = pitch 0 with exact
  `keyHeight` boundaries, so grid, keyboard, and hit testing agree at any
  offset/zoom. Content extent is `max(min 8 beats, clip length, last note end)
  + 4 beats`. Follow maps the T01 published arrangement beat through the
  earliest placement that covers it (`sourceLocalBeat`, half-open, limited to
  min(source, placement) length); hidden in gaps/tails/no-placement. Auto-scroll
  recenters only when the playhead leaves the 10–90% band, clamped to content.
  Manual scroll/zoom suspends (Follow toggle or playback restart resumes);
  following only reads transport state and never changes audio position.
- Verification performed: independent offline build succeeded; full CTest 1/1
  passed (7.26 s) with all earlier suites intact; `git diff --check` passed.
  `editorNavigationTests` covers 128-pitch row round trips and boundary rows,
  fractional-pixel scrolling, beat<->x at three zoom levels, extent from
  clip/notes/minimum/NaN, arrangement-to-source mapping across
  placements/gaps/silent tails, follow-band decisions with low/high clamping,
  real `NoteGridComponent` mouse hit testing for pitches 0/60/127, keyboard row
  alignment at a viewport offset, grid invalidation/detachment pointer safety,
  a real `PianoRollEditor` + `TransportState` follow session (resize does not
  suspend, far playhead scrolls, read-only position contract, manual-scroll
  suspend and toggle resume, hidden playhead, zoom anchor preservation), and
  ruler playhead storage. A `grabKeyboardFocus` guard for offscreen grids was
  added after the offline target asserted on a peerless component.
- Unverified checks/blockers: watcher/manual alignment before/after scrolling,
  click/drag/audition after scrolling, fractional-row and zoom-limit alignment,
  resize/scrollbar behavior, long-clip navigation, and observed follow
  suspend/resume with real audio all remain pending. The Follow toolbar button
  does not visually reflect an internal manual-scroll suspension (it stays
  checked); unchecking disables following. No application build/launch, staging
  or commit was performed.
- Handoff to T09: the shared `PianoRollGeometry.h` transform, the extent
  policy, and the follow contract are the integration points for T09's
  navigation coverage.
