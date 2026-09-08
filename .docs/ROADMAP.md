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
| 1 | [T03 Routing, Timing, and Placement](tasks/T03-routing-timing-placement.md) | M1 | blocked | None |
| 2 | [T06 Safe Audio Boundary](tasks/T06-safe-audio-boundary.md) | M1 | blocked | T03 |
| 3 | [T01 Audio-Clocked Transport](tasks/T01-audio-clocked-transport.md) | M1 | blocked | T06 |
| 4 | [T02 MIDI Arrangement Playback](tasks/T02-midi-arrangement-playback.md) | M1 | blocked | T01, T03, T06 |
| 5 | [T04 Mixer Wiring](tasks/T04-mixer-wiring.md) | M1 | blocked | T02, T06 |
| 6 | [T05 Loop and Metronome](tasks/T05-loop-metronome.md) | M1 | todo | T01, T02, T04 |
| 7 | [T07 Project Save and Load](tasks/T07-project-save-load.md) | M2 | backlog | M1 |
| 8 | [T08 Editor Navigation](tasks/T08-editor-navigation.md) | M2 | backlog | T03, T01 |
| 9 | [T09 Integration and Documentation](tasks/T09-integration-documentation.md) | M2 | backlog | T07, T08 |

Recommended single workstream: **T03 -> T06 -> T01 -> T02 -> T04 -> T05 -> T07 -> T08 -> T09**. T08 can be brought forward if piano-roll navigation blocks writing a usable clip. Avoid concurrent edits to shared audio/model files until T06's ownership contract is settled.

T03 implementation is present (2026-09-08), but watcher/manual acceptance is
unverified. Core model regressions now pass in T06's independent offline target.
Next action: independently review its completion
record and run its watcher acceptance checks. User override on 2026-09-08:
"just go onto the next task" explicitly authorizes T06 despite pending T03 acceptance.
T03 remains blocked; its intentional uncommitted changes are preserved.
No application build/launch or commit was performed for T03.

T06 implementation and offline verification are present (2026-09-08). It was marked
in_progress under the override. Independent review findings have been addressed;
it remains blocked on watcher/device acceptance, not missing implementation. Hosted
plugin editors are temporarily disabled because their restart path bypasses the gate. See its
completion record and `tests/README.md`. No application build/launch or commit was
performed. User override on 2026-09-08 explicitly authorizes T01 end to end
despite T03/T06 watcher/runtime blocks. T01 was marked in_progress under this
override; earlier acceptance states and existing T03/T06 changes remain preserved.

T01 implementation and independent offline verification completed 2026-09-08:
audio-owned compensated beat clock, bounded command/feedback acknowledgments,
poll-only UI, ruler seek/playhead and denominator-aware display. CTest 1/1 passed
with T03/T06 regressions intact. T01 is blocked on watcher UI/device acceptance;
see its completion contract. No application build/launch or commit. T02 was left
todo pending acceptance or explicit user authorization, supplied below.

User override 2026-09-08: continue with T02 end to end despite T03/T06/T01
watcher acceptance blocks. T02 was marked in_progress; all earlier acceptance statuses and
uncommitted work remain intact. Only the independent offline test target may be
built/run; no application build/launch or commit is authorized.

T02 implementation and independent offline verification completed 2026-09-08:
compiled stable-destination event ranges, sample-accurate half-open scheduling,
unioned same-pitch arrangement lifetimes and live-priority collisions, process-once
merging, bounded all-or-cleanup rejection and lifecycle/reset recovery. CTest 1/1
passed with exact timestamp regressions and all T03/T06/T01 cases intact. See T02
for rounding, overlap and the shared 976 normal / 1072 cleanup event budget.
T02 is blocked on unobserved watcher/audible acceptance; no application build/launch
or commit. Next: independent review and watcher workflow checks. T04 remains todo
pending acceptance or explicit authorization. Earlier acceptance states are unchanged.

T02 independent-review corrections: shared monotonic event indices prevent lost
boundary releases across compensated-clock/tempo changes; note-capacity preflight
now checks only destination-local final merged MIDI, not raw live input. Exact
reported values/nextafter/tempo/partition regressions and 1024-held arrangement/live
replacement tests with an unaffected second destination pass with the full offline
suite (CTest 1/1). See T02's revised timing contract and review record. No app
build/launch or acceptance-status change; watcher/manual checks remain pending.

User override 2026-09-08: "next" explicitly authorizes T04 end to end despite
T03/T06/T01/T02 runtime blocks. T04 was marked in_progress; cumulative uncommitted changes
and earlier statuses are preserved. Only the independent offline_tests target may
be built/run. No application build/launch, staging or commit.

T04 implementation and offline verification completed 2026-09-08. Real stable-ID
channel strips, gain/balance/mute/additive solo, destination-local cleanup, actual
post-sum master gain/mute, atomic sample-peak envelopes and message-thread polling.
Mixer/cosmetic controls no longer trigger arrangement-wide snapshot panic. CTest
1/1 passed with synthetic audio, decay, suppression/reset/capacity and component
binding tests, all previous regressions intact, and assertion/leak diagnostics
treated as failures. See T04 for exact pan, no-chase and master output-gate policies.
Independent review's P2 pop-out resize finding is fixed: externally owned mixer
viewport bounds survive structural strip rebuilds; regression reproduces before
the fix and passes after it, including dock return without a native window. Full
offline CTest 1/1 passed. Blocked on unobserved watcher/audible acceptance; next
manual checks, including native pop-out behavior. Earlier statuses remain unchanged;
T05 remains todo. No app build/
launch, staging or commit.

## Verified Starting Point

Historical baseline below, before T03 implementation. See the T03 task for current contracts.

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
