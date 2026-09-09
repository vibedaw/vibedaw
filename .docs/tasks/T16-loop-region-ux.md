# T16: Loop Region UX

Status: blocked | Milestone: M1.5 follow-up | Depends on: T05, T13, T14

Started 2026-09-09 from user feedback: the T05 loop wrap works audibly once the
Loop toggle is enabled, but the interaction is undiscoverable — the loop
start/end fields + Apply sit on the transport bar's bottom row while the Loop
toggle button sits on the far right, the loop icon "looks nothing like a
loop", and setting a region does not enable looping. Task replaces the
hand-drawn transport icons with vendored Tabler icons and reworks the loop
workflow to DAW conventions. No application build/launch, staging or commit;
independent offline_tests only.

## Outcome

A user can drag a loop region on the time ruler, see looping turn on, edit
bounds numerically through a popover, and clear the loop — with transport
icons that read as their functions.

## Read First

- `src/ui/TransportComponent.{h,cpp}` — hand-drawn icons (`createIcon`),
  loop fields/Apply row, Loop/Metronome toggle buttons.
- `src/ui/timeline/TimeRuler.{h,cpp}` — 24px ruler, mouseDown seek only.
- `src/ui/timeline/TimelineContent.{h,cpp}` — ruler wiring, `onSeek`,
  `transportLoopChanged` -> `setLoopRegion`.
- T05's loop command contract (seek-like discontinuities, validation,
  dim/blue region); T12's drag conventions (threshold, 1/16 snap, Alt bypass);
  T14's dialog-interceptor seam pattern (`TextPrompt.h`).

## Implementation Checklist

- [x] Add `src/ui/Icons.{h,cpp}`: vendored Tabler (MIT) outline path data for
      the eight transport icons (`player-skip-back`, `player-track-prev`,
      `player-stop`, `player-play`, `player-record`, `player-track-next`,
      `repeat`, `metronome`), parsed once via `Drawable::parseSVGPath` and
      stroked with `PathStrokeType` (2/24 scale, round caps/joins), tinted
      per state. `TransportButton` delegates to it; `createIcon` is removed.
- [x] TimeRuler drag gestures: click (< drag threshold) seeks as before;
      dragging creates/moves/resizes the loop region with a local preview
      published to TransportState only on mouseUp (loop edits are seek-like
      discontinuities). 1/16 qn snap with Alt bypass, clamped to
      `[0, 1e9]` and the 1/64 qn minimum length. Commit auto-enables looping.
- [x] Loop button right-click menu: Enable/Disable Loop (state labelled),
      Edit Loop… (ASCII ellipsis), Clear Loop (disable + reset to `[0, 4)`).
      Left-click keeps the enable/disable toggle.
- [x] Edit Loop opens a `CallOutBox` popover anchored to the button holding
      the relocated start/end editors, Apply and validation message; qn float
      format retained. Apply commits both fields atomically and enables
      looping (uniform commit-enables rule). Popover creation sits behind a
      test seam so offline tests never open native windows.
- [x] Remove the loop row from the transport bar; `MainContent`
      `transportBarHeight` 104 -> 64; numeric editing remains reachable at the
      600px minimum through the loop button. Validation messaging lives in
      the popover.
- [x] Offline regressions: icon parsing/rendering, ruler gestures
      (threshold/seek, create/move/resize, snap, Alt bypass, clamps,
      commit-on-release-only, auto-enable), menu actions, popover field
      commit, and layout bounds. Existing suites stay green.

## Acceptance Checks

- [x] Transport icons render as recognizable Tabler glyphs at 36x28 button
      size in both active and inactive tints (offline pixel checks + watcher).
- [x] Ruler drag creates, moves and resizes the loop region; a plain click
      still seeks; nothing publishes until mouseUp.
- [x] Committing a ruler region (or popover Apply) enables looping; disabling
      works via button left-click or menu; Clear Loop resets to the default
      region and disables.
- [x] Numeric editing via the popover produces the same published state as
      the removed bar row, with invalid input retained and reported.
- [x] No native windows are opened by the offline suites; watcher/manual
      observations recorded separately when actually observed.

## Completion Record

Checked acceptance above denotes deterministic offline equivalents, not
observed native menus, popovers, cursors or watcher behavior. T16 is blocked
on watcher/manual acceptance, not missing implementation.

### Changed Files

- New `src/ui/Icons.{h,cpp}`: vendored Tabler (MIT) outline path data for the
  eight transport icons, parsed once via `Drawable::parseSVGPath`, stroked
  with `PathStrokeType` (2/24 scale, curved joints, round caps) and tinted per
  state. Registered in both `CMakeLists.txt` and `tests/CMakeLists.txt`.
- `src/ui/TransportComponent.{h,cpp}`: `TransportButton` draws via
  `Icons::draw` (hand-drawn `createIcon` removed) and reports right-click
  through a new `onContextMenu` (never fires the toggle); new
  `LoopEditorPopover` owns the relocated start/end editors, Apply and
  validation label (Apply commits atomically and enables looping; external
  updates refresh an open popover unless an editor is focused);
  `showLoopMenu`/`handleLoopMenuAction`/`openLoopEditor` implement the
  right-click menu with a `CallOutBox` launch guarded by the
  `openLoopEditorOverride` seam; the bar's loop row is removed and
  `resized` compacted.
- `src/ui/MainContent.h`: `transportBarHeight` 104 -> 64.
- `src/ui/timeline/TimeRuler.{h,cpp}`: click (and sub-5px drag) still seeks;
  drag gestures create/move/resize the loop region with a local preview,
  1/16 qn snap (Alt bypass), 1/16 minimum-length clamp on resizes,
  `[0, 1e9]` bounds, and a single validated commit via new `onLoopCommitted`
  on mouseUp. Edge handles and a brighter region paint during the gesture.
- `src/ui/timeline/TimelineContent.cpp`: commits apply
  `setLoopRegion(start, end)` + `setLoopEnabled(true)` (commit-enables rule).
- `tests/offline_tests.cpp`: new `iconTests`; rewritten `loopUiTests` covering
  bar layout, right-click dispatch, menu actions, popover fields/external
  refresh, and all ruler gestures (threshold seek, create, move, edge resize,
  clamps, commit-once-on-release, auto-enable, Alt bypass).
- `tests/README.md`: T16 section documenting coverage and limits.

### Decisions and Contract Notes

- Commit-enables rule: committing a ruler region or popover Apply turns
  looping on; disabling stays explicit (button left-click, menu Enable/
  Disable, Clear Loop). Round 2 briefly removed the dim band entirely; round
  3 (user-specified state machine) supersedes it: `LoopRegion.exists`
  distinguishes no-loop (cleared/never created: nothing on the ruler,
  ungrabbable) from set-but-disabled (dim band, grabbable) and set-and-
  enabled (blue). `setLoopEnabled(true)` materializes the default `[0, 4)`
  region when none exists (button/menu Enable always yields a visible active
  loop); `clearLoop()` removes it entirely; `setLoopRegion` marks it set.
  Every transportLoopChanged notification re-syncs the ruler scroll to the
  viewport offset.
- Loop edits remain seek-like discontinuities: the ruler publishes exactly
  once per gesture, on mouseUp; nothing publishes per drag frame.
- Numeric loop editing remains available at the 600px window minimum through
  the always-visible Loop button popover (T05's relocated contract).
- qn float fields retained in the popover; bars.beats input is deferred.
- The ruler commit grows the timeline extent and the panel re-syncs the
  ruler's scroll to the viewport offset; the viewport remains the scroll
  source of truth. Ruler drags cannot autoscroll past the visible edge yet.
- Tabler Icons (MIT, Copyright (c) Pawel Kuna) attribution lives in
  `Icons.cpp`; only these eight icons are vendored.

### Verification

Only the permitted independent project was built/run:

```sh
cmake -S tests -B /tmp/opencode/vibedaw-t16-tests -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/opencode/vibedaw-t16-tests --target offline_tests --parallel 4
ctest --test-dir /tmp/opencode/vibedaw-t16-tests --output-on-failure
git diff --check
```

- Full CTest **1/1 passed** including all prior T03/T06/T01/T02/T04/T05/T12/T14
  suites, callback allocation/deallocation probes and leak diagnostics.
- `iconTests`: all eight icons parse to non-empty geometry and render visible
  pixels at 36x28.
- `loopUiTests`: bar compaction and children bounds at 600px, right-click
  dispatch without toggling, menu actions 1/2/3 (via seams, no native
  windows), popover invalid-input retention and valid Apply (+auto-enable),
  external refresh, ruler seek click, sub-threshold drag, create/move/resize
  previews with exact snapped values, single commit on mouseUp, minimum-length
  clamp (109 - 1/16 = 108.9375), Alt-bypass unsnapped commit, and post-commit
  preview clearing.
- Fix iterations recorded: `Label::setFont` single-arg (JUCE 7.0.12),
  `PathStrokeType::rounded` cap name, `strtod(&end)`, `CallOutBox::dismiss`
  pointer call, `createLoopEditor` registering `openPopover_` so external
  updates refresh test-created popovers, and test scroll-offset restoration
  after commits (viewport resync on extent growth).
- `git diff --check` passed. Unverified: native CallOutBox placement/focus,
  PopupMenu popup behavior, drag cursor affordances, and watcher-observed
  looping/icon visuals. No application build/launch, staging or commit.

### Watcher Feedback Round 1 (2026-09-09)

The user ran the app (first T16 watcher observations) and reported two issues,
both fixed with offline regressions; the fixes await re-observation:

- Gesture semantics amended: plain drag now CREATES a replacement region
  anywhere (including inside the existing body — body-drag move was rejected);
  Shift+drag inside the body moves with preserved length and wins over an
  edge grab; plain drag near an edge still resizes. Supersedes the original
  create/move/resize classification in `promote` (now takes the event for the
  Shift check).
- Mouse cursor affordances added: the ruler previews the pending action under
  the pointer — Crosshair = create, DraggingHand = Shift-move, LeftRight =
  edge resize — on hover (`mouseMove`), during the gesture, and reset on
  mouseUp. Offline tests assert each mapping via `getMouseCursor()`
  (JUCE 7.0.12 exposes `operator==(StandardCursorType)`, no getter for the
  standard type).
- Popover visibility fixed: `openLoopEditor` now launches the CallOutBox with
  a null parent (desktop-level temporary window) and screen coordinates. The
  previous parented box was a child of the transport bar and later-added
  sibling panels painted over it. Source-reviewed only; native placement and
  focus remain watcher acceptance.

### Watcher Feedback Round 2 (2026-09-09)

The user expected Clear Loop to remove the ruler highlight completely; the
disabled region still painted as a dim band. Fixed: an enabled region paints
blue, a disabled one paints nothing at all (also applies to button-disable
and startup). Consistently, grab gestures require a visible (enabled) region
— any drag over a disabled region creates. Offline regressions added: pixel
checks that a disabled band paints background only, hover/drag cursor checks,
and a disabled-body drag creating a replacement region (snapped to 1/16,
auto-enabling on commit). Test-environment note: every transportLoopChanged
notification re-syncs the ruler's scroll to the viewport offset, so the suite
restores the test offset after disabling. CTest 1/1 passed; `git diff --check`
passed. No application build/launch, staging or commit.

### Watcher Feedback Round 3 (2026-09-09)

The user specified the full state machine; implemented as specified:

- No loop in the state -> no ruler indication (nothing painted, ungrabbable).
- Creating a loop (ruler drag or popover Apply) -> set and enabled by
  default, visibly active.
- Toggling loop off (button/menu) -> set but disabled, dim band on the ruler,
  still grabbable (Shift-move/edge-resize; committing any gesture on it
  re-enables looping, per the user's "moving it enables it").
- Toggling on -> blue again.
- Clearing -> gone from the model and the ruler; the Loop button or menu
  Enable on a cleared loop materializes the default `[0, 4)` region enabled.

Implementation: `LoopRegion.exists` flag; `TransportState::clearLoop()`,
`isLoopRegionSet()`, enable-materializes-default; TimeRuler paint/grab/cursor
gating on `exists` (dim colours restored for set+disabled);
`TimelineContent::transportLoopChanged` now passes the full `LoopRegion` so
`exists` travels (the old `{enabled, start, end}` brace-init would drop it);
menu Clear routes through `clearLoop()`. Audio untouched — scheduling still
gates on `enabled` only. Offline regressions: dim-band pixel checks with
interior/edge cursor checks on a dim region (using a wide region so the body
has an interior beyond the 0.1-beat edge tolerance), cleared-state background
pixels, drag-creates-on-empty commit, default-region materialization with
blue paint at the ruler origin, and menu/button round-trips. CTest 1/1
passed; `git diff --check` passed. No application build/launch, staging or
commit.