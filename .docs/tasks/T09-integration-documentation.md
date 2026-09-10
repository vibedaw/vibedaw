# T09: Integration and Documentation

Status: done | Milestone: M2 | Depends on: T07, T08, T10-T14

Acceptance cleared 2026-09-10: user override ("assume everything is complete.. clear
this backlog") marks this task done; its pending manual acceptance (a human
following `.docs/PLAYING.md` making and reopening a short sketch, and real-hardware
external-MIDI delivery) is accepted by the override rather than individually
observed. Unchecked boxes in this file are cleared by the same override. See the
backlog clearance record in `.docs/ROADMAP.md`.

## Outcome

The playable/saveable workflow has reproducible checks and accurate documentation, making the next increment straightforward rather than another architecture rediscovery.

## Read First

- All task completion records and `.docs/ROADMAP.md`
- `AGENTS.md`, `CMakeLists.txt`, `justfile`, and the test target introduced in T06
- `src/Main.cpp`, `src/core/AudioEngine.cpp`, `src/core/ChannelMixer.cpp`
- `src/core/MidiManager.cpp`, `src/project/Project.cpp`, `src/ui/MainContent.cpp`

The architecture guide still describes the old track-hosted plugin path. External MIDI selection also warrants end-to-end checking: `MidiManager` opens an input separately from the audio device manager/collector path, so visual feedback is not proof that audio receives the events.

## Implementation Checklist

- [x] Review coverage added in T01-T08 and T10-T14; fill cross-feature gaps rather than postponing basic tests to this task. Keep hardware/real-plugin checks separate from deterministic offline tests.
- [x] Add an integration fixture for two instruments, a shared clip, multiple placements/routes, loop, tempo change, mix controls, and save/load. Use fake processors for repeatable expected MIDI/audio output.
- [x] Exercise browser-to-rack creation, explicit plugin/source editing, drag placement onto existing/new tracks, direct cross-track movement, contextual assignment/deletion, and collapse/reopen in both orders. Include failed/cancelled operations and project replacement with open editors or pending drags/menus. (Component-level: already covered by the T11/T12/T13/T14 suites; the T09 review found no uncovered cross-feature gap to add. Project replacement during active sessions is covered by T07 + the T09 integration fixture.)
- [x] Stress the supported editing lifecycle: held notes, route changes, mute/solo, source removal, project replacement, plugin failure, and command/event capacity. Assert no stuck notes or invalid references. (Held notes/route changes/mute/project replacement combined in `integrationWorkflowTests`; solo precedence, source removal, plugin failure and capacity live in the T02/T04/T06 suites.)
- [x] Verify both on-screen and external MIDI end-to-end. If external input never reaches the collector, correct the selection/routing path with a focused regression check; prevent duplicate delivery and handle disconnect/None cleanly. (`externalMidiTests`; path verified correct, no fix needed. AGENTS.md's collector claim was documentation-rot and was corrected.)
- [x] Update `AGENTS.md` to the implemented ownership/signal flow, timing units, stable routing IDs, render handoff, transport, persistence, and test entry points. Preserve its no-build/no-launch watcher instruction.
- [x] Write a short user workflow and current-limitations section in `.docs/PLAYING.md`: drag in/configure instrument, create clip without auto-opening, explicitly edit, drag/place/move/route, play/loop/mix, collapse/reopen sidebars, save/reopen. Explain right-click actions and source versus placement removal. Document only implemented controls, not the retired placement toolbar.
- [x] Document test-only commands and watcher limitations accurately. Do not claim CMake changes are picked up by a source-only watcher; record any required user-side configuration separately.
- [x] Re-run available independent tests and record actual watcher/user-observed smoke results. Leave unavailable checks visibly pending.
- [ ] Reconcile roadmap statuses with evidence and turn remaining nonblocking gaps into named backlog items. Choose the next feature only after the workflow is dependable. (Recorded below; final reconciliation happens when the manual walkthrough is observed.)

## Acceptance Checks

- [ ] Someone following `.docs/PLAYING.md` can make and reopen a short sketch without code edits or undocumented setup.
- [ ] The M1.5 interaction workflow works without Place/Move/Delete/Assign toolbar controls, and its pending native-window/visual acceptance is not hidden by offline test results.
- [x] Offline checks cover sample/block boundaries, routing, transport discontinuities, looping, mixer behavior, and session round trips without hardware or VST requirements.
- [ ] External MIDI, when hardware is available, audibly reaches exactly the selected live instrument; arrangement routing remains independent. If unavailable, explicitly retain the manual verification gap.
- [x] Architecture docs match current code rather than the original scaffold, and each earlier task has an honest completion record.
- [ ] Remaining limitations and next tasks are concrete; no unchecked critical acceptance criteria are hidden behind a `done` status.

## Completion Record

- Changed files: `tests/offline_tests.cpp` (`externalMidiTests`, `integrationWorkflowTests`), `tests/README.md`, `AGENTS.md` (rewritten to implemented architecture), `.docs/PLAYING.md` (new), `.docs/ROADMAP.md`, this file.
- Test commands/results: `cmake -S tests -B /tmp/opencode/vibedaw-t09-tests -DCMAKE_BUILD_TYPE=Debug`; `cmake --build /tmp/opencode/vibedaw-t09-tests --target offline_tests --parallel 4`; `ctest --test-dir /tmp/opencode/vibedaw-t09-tests --output-on-failure` — 1/1 passed (7.2-7.4s) with all earlier suites intact; `git diff --check` passed.
- Findings: AGENTS.md described an `AudioDeviceManager`/`MidiMessageCollector` MIDI path and track-hosted plugins that do not exist; actual path is `MidiManager`'s own `MidiInput` -> engine bounded queue -> `ChannelMixer` routing by stable `ChannelId`, with the feedback timer guarded against echo. External MIDI delivery, duplicate prevention, and disconnect/None handling verified offline; no code defect found in the selection/routing path.
- Watcher/manual observations: none performed this session.
- Unverified checks/blockers: human walkthrough of `.docs/PLAYING.md` (make and reopen a sketch); external MIDI with real hardware (audible, exactly the selected instrument, hot-unplug); M1/M1.5 runtime acceptance items still open from earlier tasks.
- Recommended next task: after the manual walkthrough passes, create the named backlog items from remaining limitations (see `.docs/PLAYING.md`) and pick the next feature (recording or sample playback per roadmap "Later" backlog).
