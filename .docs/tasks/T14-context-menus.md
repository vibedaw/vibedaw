# T14: Context Menus and Toolbar Cleanup

Status: done | Milestone: M1.5 | Depends on: T10, T11, T12, T13

## Outcome

Common actions live on the objects they affect. Direct dragging and contextual editing replace the Place/Move/Delete/Assign toolbar workflow without losing precise position or destination control.

## Read First

- T10-T13 task contracts and completion records
- `src/ui/sidebar/channel/ChannelRackSidebar.cpp`
- `src/ui/sidebar/clips/ClipsSidebar.cpp`
- `src/ui/sidebar/browser/PluginSection.cpp`
- `src/ui/panels/TimelinePanel.cpp`, `src/ui/timeline/TimelineLane.cpp`, `TimelineContent.cpp`
- `src/ui/editor/NoteGridComponent.cpp`, `src/ui/MainContent.cpp`
- Existing panel title-bar context menus for JUCE style and callback conventions

## Initial Menu Coverage

| Target | Required supported actions |
| --- | --- |
| Channel row | Open Plugin, select for live audition/new placements, rename, remove channel |
| Pooled MIDI clip | Edit in Piano Roll, rename, delete source with impact warning |
| Timeline placement | Edit shared source, precise start beat, destination submenu, mute/unmute, remove placement |
| Track header | Rename, remove track with placement impact warning |
| Empty timeline space | Add track; place selected source here when source/destination are valid |
| Browser plugin | Create instrument channel using the same path as double-click/drop |

These are the first concrete surfaces, not a requirement for a generic command framework. Audit note-grid and mixer menus as follow-ups; do not add unsupported recording, sends, or sample-playback actions merely to fill menus.

## Implementation Checklist

- [x] Reuse actions already added in T10-T12. Menu, button, and keyboard entry points must call the same validated mutation path where practical.
- [x] Right-click resolves/selects the actual target before opening its menu; never delete or reroute an unrelated prior selection. Empty-space menus use their clicked beat/track.
- [x] Capture stable IDs and safe component references for asynchronous menus; revalidate objects when an action executes. Deleted targets become harmless no-ops with appropriate feedback.
- [x] Populate destination choices from current instrument channels using stable IDs; make current and unresolved destinations clear. Changing live selection alone must not reroute existing placements.
- [x] Distinguish Remove placement from Delete source everywhere. Warn that source deletion affects shared uses and preserves T03's unresolved placeholders; track/channel removal must explain their actual consequences.
- [x] Add focused Delete/Backspace and explicit Edit keyboard access consistent with existing bindings. Do not intercept typing in text/numeric fields or keystrokes owned by plugin/editor windows.
- [x] After T12 interactions and menu replacements are verified, remove redundant Place/Move/Delete/Assign toolbar controls and any obsolete tool-mode affordances. Keep precise beat entry through placement properties/context action, and expose mute contextually.
- [x] Ensure users can discover dragging, double-click editing, and right-click actions through concise empty states/tooltips rather than an always-visible action toolbar.
- [x] Add target-resolution, stale-menu, shared-source deletion, focus, destination, and action-parity regressions.

## Acceptance Checks

- [x] Each listed surface has a useful menu containing only implemented actions, with contextual enablement.
- [x] Right-clicking an unselected object operates on that object; cancellation does not perform a mutation.
- [x] Removing a placement leaves its source and other uses intact; deleting a source clearly warns and handles open editors safely.
- [x] A channel/clip/track removed while a menu is open cannot cause stale-pointer access or mutate a replacement object.
- [x] Exact beat positioning and per-instance assignment remain available after toolbar cleanup.
- [x] Create instrument, create/edit clip, drag/place/move, assign/mute/remove, and reopen sidebars are usable without the old placement toolbar.
- [x] Keyboard deletion respects focus, and offline regressions cover menu placement, targets, and parity; visible dialogs/native windows stay out of offline tests.
- [x] Watcher/manual checks cover menu placement, discoverability, native popups, and native plugin windows. User-accepted 2026-09-09.

## Boundaries

No blanket right-click menu on every widget, global command-bus rewrite, undo framework, or speculative actions. Add further menus when there is a concrete supported operation and clear target semantics.

## Completion Record

- Changed files: the menu/toolbar implementation was already present unrecorded in the working
  tree (channel rack, Clips, PluginSection, timeline placement/empty-space/track-header menus,
  keyboard actions, toolbar removal — files listed in "Read First"). This session's review,
  completion, and verification changed: `src/ui/sidebar/channel/ChannelRackSidebar.cpp`,
  `src/ui/sidebar/clips/ClipsSidebar.cpp`, `src/ui/timeline/TimelineContent.cpp`,
  `src/ui/timeline/TrackHeader.cpp`, `src/ui/timeline/TrackHeaderList.cpp`,
  `src/ui/components/TextPrompt.h`, `tests/offline_tests.cpp`, `tests/README.md`.
- Action/target/focus/deletion contract: every menu, button, keyboard, and prompt entry point
  re-resolves its target by stable ID (track/instance, clip, channel, track UUID) at action time;
  prompts open only when the target still resolves, so deleted targets raise no dialog and mutate
  nothing. Renames reject blank names inside the validated action. Remove Placement keeps the
  pooled source; Delete Source warns with the placement count and closes editors; channel/track
  removal warnings state their actual placement consequences. Destination choices list current
  instrument channels with stable IDs, tick the current one, and mark an unresolved destination
  explicitly; live-audition selection never reroutes placements. The Place/Move/Delete/Assign
  toolbar and its start-beat field are removed from TimelinePanel; precise entry survives via
  the placement menu's Set Start Beat… and per-instance destination assignment; the timeline
  status line's tooltip documents drag/double-click/right-click and keyboard behavior, and the
  rack/Clips empty states point at the same interactions.
- Verification performed: only the independent offline project (commands in `tests/README.md`).
  Final build succeeded; CTest 1/1 passed (~6.9 s) with all T03-T13 suites intact;
  `git diff --check` passed. New `contextMenuTests` coverage is listed there. During review the
  following were fixed: raw UTF-8 `…`/`—` menu literals double-encoded (JUCE asserted and labels
  mojibaked; now the `CharPointer_UTF8` idiom), the Clips footer Delete Source button bypassing
  the confirmed impact-warned path (now shared with the menu, covered by intercepted confirm and
  cancel regressions), rename/Set-Start-Beat prompts opening for vanished targets (now re-resolve
  by stable ID first), blank names accepted by the validated rename actions (now rejected), a
  miscounted menu-item expectation, and two test-side defects (a probe counting the victim track,
  and a note pointer captured before vector reallocation). `ClipInstance::isValid()` is a
  local-fields check; unresolved-source expectations now use pool/channel state.
- Unverified checks/blockers: none outstanding. Watcher/manual acceptance
  (menu placement and enablement on each surface, discoverability, native
  popup/focus interaction, native plugin windows) was observed and accepted by
  the user on 2026-09-09. Dialog mechanics beyond routing (native rendering,
  focus stealing) were not exercised offline by design and are covered by the
  user's acceptance. No application build/launch, staging, or commit was
  performed during implementation.
- Handoff to T07/T09: T14 completes M1.5's interaction surface; T07 persists the clip/channel/
  track names and placements the menus mutate; T09 audits menu/keyboard coverage against the
  recorded contract and documents the dialog-interceptor seam.
