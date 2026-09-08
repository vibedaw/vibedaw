# T01: Audio-Clocked Transport

Status: blocked | Milestone: M1 | Depends on: T06

Started 2026-09-08 under explicit user authorization to proceed despite T03/T06
watcher/runtime blocks. Their acceptance states remain unchanged. No app build,
launch or commit is permitted; independent offline test builds are authorized.
Implementation and offline verification completed 2026-09-08. Blocked on watcher
UI/device acceptance; no application build or launch was performed.

## Outcome

Audio owns playback time. Play, stop, seek, and tempo commands produce coherent block timing even if the UI stalls.

## Read First

- `src/core/TransportState.h`, `TransportState.cpp`
- `src/core/ChannelMixer.h`, `ChannelMixer.cpp`
- `src/core/AudioEngine.cpp`, `src/Main.cpp`
- `src/ui/MainContent.h`, `MainContent.cpp`, `TransportComponent.cpp`
- `src/ui/timeline/TimeRuler.cpp`, `TimelineContent.cpp`

Historical starting point: transport lived in `MainContent` and advanced from its
30 Hz timer. T06 moved the facade into Project; T01 now replaces that UI advancement
and deletes the legacy audio-style `processBlock` and overshoot-discarding loop path.

## Implementation Checklist

- [x] Move authoritative playback state to an engine-owned lifetime, using T06's command/publication contract. Keep a UI-facing facade if useful, not a second clock.
- [x] Capture block-start position, sample rate, tempo, time signature, playing state, and discontinuity revision once per block. Specify scheduling-before-advancement order for T02.
- [x] Advance from rendered samples only while playing; preserve fractional musical position without long-term rounding drift. A tempo change preserves the musical position rather than preserving elapsed seconds.
- [x] Remove wall-clock advancement from `MainContent::timerCallback`; UI polls published state and repaints on the message thread.
- [x] Define controls: play resumes; stop holds position and requests note cleanup; rewind seeks to zero; seek works stopped or playing and publishes a discontinuity. Preserve or deliberately document any changed shipped control semantics.
- [x] Feed transport into the mixer entry point that already runs on audio; avoid installing a second device callback or advancing time twice.
- [x] Add a visible arrangement playhead and basic ruler seek or beat-position input. Convert presentation seconds using the shared timing contract. (Source implemented; watcher appearance remains unverified.)
- [x] Validate tempo/time signature/seek positions; handle prepare/release and sample-rate changes coherently. Keep recording disabled or explicitly unavailable until recording exists.
- [x] Add deterministic transport tests using sample counts, not wall-clock sleeps.

## Acceptance Checks

- [x] At 48 kHz and 120 BPM, rendering 96,000 samples from beat zero advances exactly four quarter-note beats within a documented numerical tolerance.
- [x] Different block sizes and a final partial block produce the same final position.
- [x] Stopped blocks do not advance. Play/stop/seek commands apply at the documented boundary and generate cleanup/discontinuity signals for T02.
- [x] A tempo change preserves beat position and changes subsequent advancement; a sample-rate change produces no position jump.
- [ ] UI stalls do not alter progression. The playhead reflects audio-published state, not an independent prediction clock.
- [x] No UI/listener callbacks execute on audio and no second transport writer remains. (Owned-source audit plus offline listener/allocation probes.)

## Boundaries

No clip rendering yet. Establish discontinuity hooks now; T02 consumes them for note cleanup. Loop splitting and click rendering belong to T05. Do not enable the old overshoot-discarding loop logic as an interim implementation.

## Completion Record

### Changed Files

- `src/core/TransportState.{h,cpp}`: message-thread command/display facade, reverse
  polling, seek acknowledgment, denominator-aware formatter and render-only clock.
- `src/core/ChannelMixer.{h,cpp}`: owns TransportClock in the engine render graph,
  acquires one timing value per valid block, cleans before processing, commits after.
- `src/core/AudioEngine.cpp`: reject invalid prepare rate/capacity under quiescence.
- `src/ui/MainContent.{h,cpp}`: poll-only timer, removed wall-clock bookkeeping.
- `src/ui/TransportComponent.{h,cpp}`: direct beat display without seconds round-trip,
  denominator-based bars/beats/ticks, initial state sync, rewind-to-zero, disabled
  recording/loop/metronome controls and accessible unavailable labels.
- `src/ui/timeline/TimelineContent.{h,cpp}`, `TimeRuler.{h,cpp}` and
  `src/ui/panels/TimelinePanel.cpp`: transport listener wiring, green overlay playhead
  and left-click ruler seek using the existing quarter-beat pixel/scroll transform.
- `tests/offline_tests.cpp`, `tests/README.md`, roadmap, this task and appended T06
  handoff update. Existing T03/T06 changes are preserved; total git diff is cumulative.

### Block and Ownership Contract

- Project retains the T06 facade and exchanges, not a running clock. The one
  `ChannelMixer` attached to AudioEngine owns `TransportClock`. Main already detaches
  the processor before destroying mixer/project; no new callback/lifetime is added.
  Preparation/release under T06 quiescence changes admission/rate, not clock position.
- Exactly once per valid admitted mixer block: acquire arrangement, bound/preflight
  live input, call `clock.beginBlock`, perform cleanup, schedule (T02), process each
  destination once, then `clock.endBlock`. The returned value captures start/end
  quarter-note beats, sample count, rate, BPM, meter, playing, discontinuity and revision.
  T02 uses `[startBeats, endBeats)` and the captured rate/tempo to calculate sample
  offsets. It must not reacquire controls or advance a second time. Clock begin/end
  must be paired; no early return is allowed between them.
- Position advances by `samples * (BPM / 60) / sampleRate` only while playing.
  Compensated double summation preserves sub-block fractions; comparisons use an
  absolute tolerance of **1e-10 quarter-note beats** in the tested ranges, including
  one hour at 44.1 kHz/137 BPM. No integer samples-per-beat rounding is used.
- UI is the sole command producer. `RenderControl.positionBeats` is the last explicit
  seek target, not the latest displayed position. Only a changed seek generation
  resets the clock; unrelated publications cannot replay an old seek or UI snapshot.
  Latest complete state wins without a drain/retry loop. Stop's independent sticky
  panic and generation survive stop->play coalescing. Seeks, including equal-target
  seeks, increment a generation. Generations/revisions remain unsigned change tags
  under T06's contract, not event counts.
- Render is the sole reverse producer. End-of-block feedback coherently publishes
  beats, applied tempo/meter/play state, rate, discontinuity revision and seek ack.
  `MainContent` is the sole production poller at 30 Hz. It accepts beats only if
  feedback acknowledges the latest requested seek, and notifies listeners on the
  message thread without publishing a command. Repeated polls never advance time.
  Controls display requested state immediately; position is audio-published except
  for an optimistic explicit seek held until acknowledgment. Seconds are a current
  tempo presentation conversion, not elapsed wall time or a historical tempo map.
- Stop holds the audio position at the next admitted block and requests cleanup.
  Play resumes; the Play button/Space retain toggle-to-stop behavior. Return-to-start,
  reset/Enter and now Rewind seek to zero without changing play state. **Deliberate
  change:** Rewind previously stepped backward one second. Fast-forward retains its
  existing one-presentation-second seek. Live audition still works while stopped.
- A discontinuity/revision change is emitted for seek, stop/panic, preparation/rate
  change, invalid/released mixer recovery, and engine cleanup markers after skipped
  blocks. Snapshot invalidation/MIDI overflow conservatively also invalidate timing.
  Cleanup is at sample zero before new input; T06 explicit note-off/reset policy stays
  intact. No loop wrapping, click synthesis, arrangement MIDI or plugin playhead is added.
- Rejected/oversized/nonpositive/quiescence-blocked/unprepared callbacks produce no
  transport advancement or feedback, and the next valid block requests cleanup.
  No catch-up for missed wall time. Empty graphs and admitted silent/muted/reset-wait
  instrument blocks still advance, because the mixer rendered those sample spans.
  Invalid prepare values release processing; a later valid prepare resumes position.
- Tempo rejects non-finite values and clamps to 20..300; numerator clamps to 1..32,
  denominator accepts 2/4/8/16 (otherwise 4). Seeks reject non-finite/negative values
  or values above **1e9 quarter-note beats**, and running position saturates at that
  bound (no wrap). This bounds display conversions. Meter does not change musical
  position. Display beats are denominator note units with 960 ticks per displayed
  beat: quarter beat 3 in 6/8 is `2:1:000`, not halfway through a six-quarter bar.
- Render clock functions call no listeners, logging, allocation, locks, wall clock
  or UI APIs. Exchanges use T06 lock-free atomics and fixed POD slots. Structural
  guards, third-party limitations and one-render-consumer restriction remain unchanged.

### Verification

Executed successfully, then rebuilt/retested after additional regressions:

```sh
cmake -S tests -B /tmp/opencode/vibedaw-t06-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/vibedaw-t06-tests --target offline_tests --parallel 2
ctest --test-dir /tmp/opencode/vibedaw-t06-tests --output-on-failure
git diff --check
```

- CTest **1/1 passed**: existing T03/T06 suite plus new `transportClockTests` and
  `transportEngineTests` in the independent executable. No application dependency,
  device initialization, plugin runtime or GUI launch. Existing local JUCE only.
- Deterministic tests: 96,000 samples with block sizes 1/7/64/127/512/4096/96000 and
  final partial blocks; one-hour drift; no UI polling during rendering; concurrent
  forward/reverse exchanges; stopped/resumed/seeking state; stale feedback and equal
  seeks; stop through 10,000 coalesced controls; tempo/rate continuity; invalid inputs;
  meter formats for denominators 2/4/8/16; no interim loop wrap; recording unavailable.
- Actual engine callback with synthetic instrument: one advancement per block,
  quiescent/zero/negative/oversized/invalid-prepare/released blocks freeze position,
  changed revision on recovery, sample-rate reprepare, stopped audition, seek cleanup
  stopped/playing with explicit sample-zero note-off. Marked callbacks require zero
  C++ new/delete; listener probe requires message-thread notification only.
- Source callers reviewed: Main lifetime/attachment, Project facade, mixer callback,
  transport setters/polling, MainContent timer/hotkeys, TransportComponent display,
  TimelinePanel constructor and ruler/overlay transform. No legacy transport
  `processBlock`, UI elapsed-time advancement or other production feedback writer
  remains. UI source changes were reviewed but not application-compiled.
- Independent review found no actionable T01 issues in timing, command/feedback
  handoffs, lifecycle recovery, or UI source integration. Final CTest rerun passed
  1/1 and `git diff --check` passed. This does not verify watcher UI behavior.

### Remaining Acceptance and Handoff

- Watcher/manual next action: verify playhead motion with UI busy, stop/resume,
  ruler seeks stopped/playing at several scroll offsets, rewind/Enter, tempo changes
  without musical jumps, 6/8/7/8 display boundaries, disabled controls, device stop/
  restart and valid rate changes. Observe an actual watcher build result. The UI
  stall/playhead acceptance box remains unchecked despite deterministic clock proof.
- No app build/launch, hardware or third-party plugin test, ASan/TSan, staging or
  commit. T03/T06 remain runtime-blocked under their existing truthful records.
- T02 consumes the immutable block timing at the marked scheduler insertion point
  and discontinuity revision for note cleanup, preserving T06 bounded MIDI budgets
  and stable routing. It remains todo pending acceptance or explicit authorization.
  T05 owns loop spans/splitting and metronome synthesis; no premature span framework
  was introduced. Loop/metronome state APIs remain preparatory only and UI is disabled.

### T05 Integration Update (2026-09-08)

T05 supersedes the temporary no-loop/no-click passages above: the captured block
now includes fixed fractional loop spans and metronome state. Loop enable/disable/
bound changes are block-boundary discontinuities; normal wraps preserve overshoot
and publish wrapped feedback through the same poll-only path. Nonloop compensated
clock and seek/stop acknowledgment regressions remain intact. See T05's exact
minimum/capacity/carry contract. T01 stays runtime-blocked; no app build/launch.
