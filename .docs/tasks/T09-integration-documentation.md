# T09: Integration and Documentation

Status: backlog | Milestone: M2 | Depends on: T07, T08

## Outcome

The playable/saveable workflow has reproducible checks and accurate documentation, making the next increment straightforward rather than another architecture rediscovery.

## Read First

- All task completion records and `.docs/ROADMAP.md`
- `AGENTS.md`, `CMakeLists.txt`, `justfile`, and the test target introduced in T06
- `src/Main.cpp`, `src/core/AudioEngine.cpp`, `src/core/ChannelMixer.cpp`
- `src/core/MidiManager.cpp`, `src/project/Project.cpp`, `src/ui/MainContent.cpp`

The architecture guide still describes the old track-hosted plugin path. External MIDI selection also warrants end-to-end checking: `MidiManager` opens an input separately from the audio device manager/collector path, so visual feedback is not proof that audio receives the events.

## Implementation Checklist

- [ ] Review coverage added in T03-T08; fill cross-feature gaps rather than postponing basic tests to this task. Keep hardware/real-plugin checks separate from deterministic offline tests.
- [ ] Add an integration fixture for two instruments, a shared clip, multiple placements/routes, loop, tempo change, mix controls, and save/load. Use fake processors for repeatable expected MIDI/audio output.
- [ ] Stress the supported editing lifecycle: held notes, route changes, mute/solo, source removal, project replacement, plugin failure, and command/event capacity. Assert no stuck notes or invalid references.
- [ ] Verify both on-screen and external MIDI end-to-end. If external input never reaches the collector, correct the selection/routing path with a focused regression check; prevent duplicate delivery and handle disconnect/None cleanly.
- [ ] Update `AGENTS.md` to the implemented ownership/signal flow, timing units, stable routing IDs, render handoff, transport, persistence, and test entry points. Preserve its no-build/no-launch watcher instruction.
- [ ] Write a short user workflow and current-limitations section in `.docs/PLAYING.md`: load instrument, make clip, place/route, play/loop/mix, save/reopen. Document only implemented controls.
- [ ] Document test-only commands and watcher limitations accurately. Do not claim CMake changes are picked up by a source-only watcher; record any required user-side configuration separately.
- [ ] Re-run available independent tests and record actual watcher/user-observed smoke results. Leave unavailable checks visibly pending.
- [ ] Reconcile roadmap statuses with evidence and turn remaining nonblocking gaps into named backlog items. Choose the next feature only after the workflow is dependable.

## Acceptance Checks

- [ ] Someone following `.docs/PLAYING.md` can make and reopen a short sketch without code edits or undocumented setup.
- [ ] Offline checks cover sample/block boundaries, routing, transport discontinuities, looping, mixer behavior, and session round trips without hardware or VST requirements.
- [ ] External MIDI, when hardware is available, audibly reaches exactly the selected live instrument; arrangement routing remains independent. If unavailable, explicitly retain the manual verification gap.
- [ ] Architecture docs match current code rather than the original scaffold, and each earlier task has an honest completion record.
- [ ] Remaining limitations and next tasks are concrete; no unchecked critical acceptance criteria are hidden behind a `done` status.

## Completion Record

- Changed files: pending
- Test commands/results: pending
- Watcher/manual observations: pending
- Unverified checks/blockers: pending
- Recommended next task: pending
