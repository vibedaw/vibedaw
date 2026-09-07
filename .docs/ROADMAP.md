# VibeDAW Roadmap

## Goal

Get to a small, dependable MIDI DAW: load an instrument, write notes, place a clip, press play, loop it, and adjust the mix. Build outward from that working path rather than adding more disconnected controls.

Baseline inspected: 2026-09-08, commit `1edf80f`. Findings are source-verified, not runtime-verified. No application build or launch was performed.

## Milestones

### M1: Play, Loop, Mix

Tasks T01-T06. Completion means this entire workflow works through the UI:

1. Create two instrument channels and load plugins.
2. Create a MIDI clip and edit its notes in the piano roll.
3. Place it on a timeline track and choose its destination instrument.
4. Place another instance later, optionally routed to the other instrument.
5. Play, stop, seek, change tempo, and hear the arrangement at the displayed position.
6. Set a loop and enable an audible metronome without drift or stuck notes.
7. Adjust channel gain/pan/mute/solo and master gain/mute; see truthful meters.
8. Continue editing safely while audio is active, or receive an explicit temporary restriction for structural operations that cannot yet be safely applied.

The earliest audible checkpoint is T02. Do not wait for all M1 polish before trying that workflow using the running watcher.

### M2: Keep Your Work and Improve Editing

T07 adds explicit project save/load, including plugin state. It is the first task after M1 because sketches should survive a restart. T08 fixes editor navigation and alignment; promote any editor defect that prevents the M1 workflow into the relevant M1 task. T09 closes integration/documentation gaps, not a deferred bucket for essential tests.

### Later: Samples and Recording

Backlog only, not dependencies of M1: actual sampler rendering and sample placement, MIDI recording into clips, audio recording, automation, effects/sends, undo/redo, clip-local repetition, advanced drag/trim tools, plugin tempo/playhead reporting, and complete device/sidebar settings restoration. Create detailed tasks when these become next in line.

## Task Index

Task numbers retain the original discussion's IDs; execute in the order below, not numeric order.

| Order | Task | Milestone | Status | Depends On |
| --- | --- | --- | --- | --- |
| 1 | [T03 Routing, Timing, and Placement](tasks/T03-routing-timing-placement.md) | M1 | ready | None |
| 2 | [T06 Safe Audio Boundary](tasks/T06-safe-audio-boundary.md) | M1 | todo | T03 |
| 3 | [T01 Audio-Clocked Transport](tasks/T01-audio-clocked-transport.md) | M1 | todo | T06 |
| 4 | [T02 MIDI Arrangement Playback](tasks/T02-midi-arrangement-playback.md) | M1 | todo | T01, T03, T06 |
| 5 | [T04 Mixer Wiring](tasks/T04-mixer-wiring.md) | M1 | todo | T02, T06 |
| 6 | [T05 Loop and Metronome](tasks/T05-loop-metronome.md) | M1 | todo | T01, T02, T04 |
| 7 | [T07 Project Save and Load](tasks/T07-project-save-load.md) | M2 | backlog | M1 |
| 8 | [T08 Editor Navigation](tasks/T08-editor-navigation.md) | M2 | backlog | T03, T01 |
| 9 | [T09 Integration and Documentation](tasks/T09-integration-documentation.md) | M2 | backlog | T07, T08 |

Recommended single workstream: **T03 -> T06 -> T01 -> T02 -> T04 -> T05 -> T07 -> T08 -> T09**. T08 can be brought forward if piano-roll navigation blocks writing a usable clip. Avoid concurrent edits to shared audio/model files until T06's ownership contract is settled.

## Verified Starting Point

- Audio output currently runs through `AudioEngine -> AudioProcessorPlayer -> ChannelMixer -> Channel -> PluginHost`. Live MIDI is routed to the active channel.
- Tracks hold clip instances; each instance already has a destination `channelId`. Tracks and instruments are intentionally separate concepts, not necessarily competing models.
- There is no caller of `Track::addClipInstance`, and the clip pool is not passed to timeline lanes. Arrangement placement needs a usable UI entry point.
- Note-grid timing is interpreted as beats, timeline placement timing as seconds. Model fields do not establish a consistent timing contract.
- Transport advances from a UI timer. Calling its existing `processBlock` on audio unchanged would introduce unsafe listener callbacks and incorrect loop-boundary handling.
- Mixer audio exists, but the mixer panel is disconnected. Metronome state/UI exists, but click synthesis does not.
- Mutable channels/plugins and editor data need a safe ownership boundary before the scheduler reads them on audio.
- Settings are not project files. There is no arrangement/plugin-state persistence or automated test target.

## Shared Decisions

- Keep per-instance instrument routing. Do not force one channel per track or treat reorderable channel indexes as stable identities.
- Use quarter-note beats for MIDI notes, source length, placement positions/lengths, and loop bounds. Convert to samples at the render boundary; seconds are a display conversion. T03 documents the exact model contract.
- The audio callback owns running transport advancement. UI sends commands and reads published state; it never drives playback time.
- Audio must not traverse UI-mutated note/placement vectors. T06 establishes a bounded, lifetime-safe handoff used by later tasks.
- Merge arrangement events by destination and process each plugin once per audio block. Live audition selection must not reroute the arrangement.
- Correct note cleanup and deterministic boundary timing are required for basic playback, not optional polish.
- Prefer a simple placement action and numeric loop controls over elaborate drag-and-drop workflows.
- Unsupported recording, sends, and sampler controls must not pretend to work. Disable or clearly label them until implemented.

## Working Protocol

1. Read this roadmap and the next ready task; re-check its source pointers against the current code.
2. Mark the task `in_progress` here and in its file. Keep one implementation task active at a time.
3. Follow the checklist in order. Record any changed contract before dependent tasks start.
4. Add focused regression tests with the feature, not at the end of M2. Keep ordinary tests independent of third-party plugins and audio hardware.
5. Follow `AGENTS.md`: **do not build or launch the application; a watcher is running**. A test-only command is appropriate only if it does not build or launch the app. If no runnable test target is available, record verification as pending rather than claiming success.
6. Record changed files, verification performed, unverified checks, and decisions in the task's completion record. Use watcher/manual observations only when actually observed or reported.
7. Mark `done` only when acceptance checks are met; otherwise use `blocked` with a concrete next action. Update this index and ready the next task.

Status vocabulary: `backlog`, `todo`, `ready`, `in_progress`, `blocked`, `done`. Unchecked acceptance boxes are intentionally unverified.

## M1 Release Check

- [ ] The eight-step M1 workflow above is usable without source edits or hardcoded clip injection.
- [ ] Tempo and playback stay stable when the UI is busy.
- [ ] Block boundaries, seeks, looping, overlapping notes, and deletion do not leave stuck notes.
- [ ] Two destinations work independently; shared clips and channel reorder do not corrupt routing.
- [ ] Audio-thread ownership and cleanup have been reviewed; no callback logging or per-block scratch allocation remains in our processing path.
- [ ] Deterministic tests cover transport, scheduling, routing, and loop math; any unavailable runtime verification is explicitly recorded.
- [ ] Limitations are clear: MIDI arrangement playback, not recording or sample playback; saving arrives in T07.
