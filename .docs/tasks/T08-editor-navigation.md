# T08: Editor Navigation

Status: backlog | Milestone: M2 | Depends on: T03, T01

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

- [ ] Choose one scroll coordinate source, preferably the viewport's visible area, and connect its actual change notifications to keyboard/ruler updates.
- [ ] Share pitch-to-row and beat-to-x transforms between rendering and hit testing; avoid offsets being applied twice.
- [ ] Make wheel, scrollbars, programmatic scroll, resize, and zoom use that same state. Keep the keyboard aligned with the grid independently of viewport height.
- [ ] Replace the fixed eight-beat extent with one based on clip length/notes plus a documented editing margin. Keep all valid MIDI pitches reachable.
- [ ] Ensure note add/select/drag/delete and audition target the visible pitch/time after scrolling. Publish edits through T03/T06's change boundary.
- [ ] Add optional playhead following without fighting manual scrolling; define when following suspends and resumes. Use T01's published position only.
- [ ] Handle source deletion/project replacement while an editor is open without stale pointers.
- [ ] Test coordinate round trips and clamping in pure logic; perform manual alignment/navigation checks through the watcher.

## Acceptance Checks

- [ ] Notes line up with keyboard pitches and ruler beats before/after horizontal and vertical scrolling.
- [ ] Clicking/dragging after scrolling edits the expected note and auditions the expected pitch.
- [ ] MIDI pitches 0 and 127, fractional-row scrolling, supported zoom limits, and resize/scrollbar changes stay aligned.
- [ ] A clip longer than eight beats is fully navigable; no note becomes inaccessible because of fixed content bounds.
- [ ] Following playback can be disabled/suspended for editing; it does not create a second clock or change audio position.

## Completion Record

- Changed files: pending
- Coordinate/scroll/follow contract: pending
- Verification performed: pending
- Unverified checks/blockers: pending
- Handoff to T09: pending
