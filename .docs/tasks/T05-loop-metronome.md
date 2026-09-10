# T05: Loop and Metronome

Status: done | Milestone: M1 | Depends on: T01, T02, T04

Acceptance cleared 2026-09-10: user override ("assume everything is complete.. clear
this backlog") marks this task done; its pending watcher/audible observations are
accepted by the override rather than individually observed. Unchecked boxes in this
file are cleared by the same override. See the backlog clearance record in
`.docs/ROADMAP.md`.

Started 2026-09-08 under explicit user continuation override despite earlier
T03/T06/T01/T02/T04 runtime blocks. Earlier statuses and existing work are preserved.
No application build/launch, staging or commit; independent offline_tests only.
Implementation and offline verification completed 2026-09-08. Blocked on unobserved
watcher/audible acceptance and the M1 release workflow, not missing loop/click code.

## Outcome

A user can set a musical loop and play along with an audible, sample-timed click. This closes M1.

## Read First

- `src/core/TransportState.h`, `TransportState.cpp`, `ChannelMixer.h`, `ChannelMixer.cpp`
- `src/ui/TransportComponent.cpp`, `src/ui/timeline/TimeRuler.cpp`
- T01's block timing contract, T02's note cleanup, T04's master stage

Historical baseline had overshoot-dropping loop logic. T01 removed that path;
T05 now implements bounded span traversal and actual synthesized click audio.

## Implementation Checklist

- [x] Add usable loop-start/end editing in beats/bars, plus a visible loop region. Numeric fields are enough; do not require drag handles. Validate finite nonnegative start and end greater than start.
- [x] Split transport/scheduler traversal into segments at exact loop boundaries while keeping MIDI offsets relative to the original block. Preserve overshoot and support more than one wrap in a block within an explicit minimum-loop/capacity policy.
- [x] Do not process plugins separately per timeline segment unless the render contract requires it; aggregate scheduled events into the original block where possible.
- [x] At each wrap, release notes from the old segment and trigger new attacks deterministically. Avoid duplicated events at loop start/end. Sustains crossing loop end are cut for M1, not implicitly tied.
- [x] Define enabling/disabling/changing the loop while playing, including a cursor outside the new range. Apply changes at a safe block boundary and use the same discontinuity/cleanup rules as seek.
- [x] Generate a short bounded click without file loading or callback allocation. Schedule it at sample offsets and keep its tail across blocks.
- [x] Use quarter-note model units, with one click per notated denominator beat (`4 / denominator` quarter notes) and an accent every numerator beats. For M1, 6/8 is six eighth-note clicks, not compound dotted-quarter grouping.
- [x] Click only while playing and enabled, including with no instrument channels. Inject before master gain/mute; channel mute/solo does not suppress it. Disable clears any pending click tail.
- [x] Test wrap segmentation, note ordering, click offsets/tails, tempo/time-signature changes, and loop edits with offline rendering.

## Acceptance Checks

- [x] A four-bar loop repeats with no drift at different tempos, sample rates, and buffer sizes.
- [x] A boundary inside a block, exactly at its end, and multiple boundaries in one block preserve elapsed samples without duplicate/missing attacks.
- [x] Held notes at loop end are released; repeated passes, seeks, and loop edits do not accumulate voices.
- [x] Clicks are sample-timed, accented correctly in 4/4, 3/4, and 6/8, and aligned with notes/playhead after wrap or seek.
- [x] Disabled/stopped metronome is silent; enabled metronome works with zero channels and obeys master mute/gain.
- [ ] Complete the roadmap's M1 release checklist and record any remaining blocker rather than declaring the milestone finished.

## Completion Record

Checked acceptance above denotes deterministic offline equivalents, not observed
hardware, native UI, or third-party plugin behavior. M1 and T05 are not done.

### Changed Files

- `src/core/TransportState.{h,cpp}`: shared loop validation, audio-owned fixed spans,
  carried fractional wrap ownership, compensated final position and command handling.
- `src/core/ChannelMixer.{h,cpp}`: span-relative event traversal with original-block
  offsets, internal wrap barriers, bounded process-once merge and pre-master click.
- New `src/core/Metronome.h`: fixed click-event storage and single bounded oscillator.
- `src/project/Channel.{h,cpp}`: arrangement-delivery history latch for the existing
  off-audio sustained-voice reset; cleared on replacement/completed reset.
- `src/ui/TransportComponent.{h,cpp}`, `MainContent.h`: numeric loop editing,
  validation, enabled loop/click controls, 104px bar and compact minimum-width layout.
- `src/ui/timeline/{TimeRuler,TimelineContent}.{h,cpp}`, `ui/panels/TimelinePanel.cpp`:
  loop ruler region, scroll-extent update including loop end, existing polled playhead.
- `tests/{CMakeLists.txt,offline_tests.cpp,README.md}`: real transport/timeline UI in
  independent target and T05 regression suites. Roadmap and prior task handoff notes.
- Initial `git status --short` was clean in this workspace. Existing implementation
  was preserved; nothing was reverted, staged or committed during this task.

### Loop Timing and Commands

- Bounds are zero-based quarter notes, `[start,end)`, independent of meter. Reject
  malformed/nonfinite/negative bounds, end above 1e9 qn, and length below **1/64 qn**.
  Invalid edits retain the previous model region and show a red validation message.
  Enter/Apply commits both fields atomically; typing alone does not publish edits.
  The region is dim when disabled and blue when enabled. Numeric editing remains
  available at the 600px window minimum; compact layout hides redundant navigation
  buttons, not Play/Stop/Loop/Click. Enter rewind remains in the existing hotkey path.
- Audio captures tempo, meter, loop and click controls once per admitted block.
  Loop enable/disable or changed bounds are seek-like discontinuities, including
  editing disabled bounds: clear old note ownership at sample zero, no note chase.
  While playing with loop enabled, a cursor outside the new range jumps to its start;
  an inside cursor stays at its beat. Stopped edits/seeks hold position until play.
  Tempo/meter changes retain musical position. No setter/listener executes on audio.
- `TransportClock::Block` contains at most **128 continuous musical spans** with
  fractional original-block sample origins and a wrap-before-span flag. Quarter-note
  overshoot is retained, never rounded to integer samples per beat or discarded.
  Final loop position uses extended-precision addition/modulo and a retained double
  rounding residual. Nonloop T01 compensated advancement is unchanged. One-hour
  44.1kHz/137BPM loop position tests use the prior 1e-10 qn absolute tolerance.
- Events use T02 containing-sample quantization: floor(original span sample origin
  + (event beat - span start) * samplesPerBeat + epsilon), with the existing
  `1e-7 + 8*DBL_EPSILON*abs(blockStart)*samplesPerBeat` sample tolerance. Exact final
  span end maps to numSamples. Notes shorter than a quantized sample, including
  loop-end truncation, are silent. Loop end attacks are excluded.
- Per-destination consumed indices remain monotonic within each pass; only a real
  wrap or discontinuity resets them by beat lower-bound. Deferred boundary events
  still belong to sample zero of the next block, even across tempo changes. No
  snapshot pointers survive acquisition. Rejected/unavailable destinations consume
  their selected ranges rather than retrying attacks later.
- A wrap quantized to numSamples is deferred with its cleanup/cursor reset. Its
  next span retains the exact loop-start beat, not the tiny overshot block position;
  this fixes a regression caught by the new partition tests. A wrap exactly at block
  end releases/reattacks at the following block's sample zero, never out of range.

### Notes and Capacity

- Wrap barriers are internal gather events, not MIDI. They release arrangement
  tokens only; final sorting puts explicit offs before controllers/ons at the wrap.
  Loop-start attacks then reuse T02 live-priority collision and union-token rules.
  Cross-loop arrangement sustains are cut, not tied or chased. Live notes/controllers
  keep their active stable destination and ordinary wraps do not release them.
- Preserve **976 normal / 1072 cleanup / 2048 total JUCE input messages**. Wrap
  barriers consume gather slots; wrap explicit offs and collision offs consume normal
  output capacity alongside arrangement/live. Reserve remains 1024 explicit old-note
  offs plus 48 CCs and is never borrowed. One plugin process per original block,
  except the existing pending-reset skip. No per-span plugin processing or bus/sends.
- Gather/merge overflow or final 1024-outstanding-note preflight failure rejects the
  **whole destination block** before plugin delivery. Cleanup of original deliveries
  replaces speculative events at sample zero. This may cut that destination's live
  notes early, deliberately, rather than strand voices or deliver a prefix. A wrap
  with 1024 held arrangement notes consequently uses fallback 1072-message cleanup,
  not 1024 wrap offs plus new attacks borrowed from the reserve. Other destinations
  continue; tests retain a live pedal owner on a separate destination throughout.
- More than 128 spans, or a loop shorter than one physical sample at an unusual
  rate/tempo, rejects arrangement traversal and click audio for the block. The clock
  still advances by all samples modulo the region; global sample-zero cleanup and
  mixer overflow count expose failure. Bounded live ingress can still be admitted.
  The next valid block resets cursor ownership/cleans through existing recovery.
  This exceptional global policy can cut live notes; it is not ordinary wrap behavior.
- Pedal-held voices cannot reliably be killed with CC cleanup through all JUCE
  adapters. If a wrapping destination has arrangement delivery history and pedal
  history (or pedal-down anywhere in the current live batch), reject its entire
  speculative block, deliver old-note cleanup, mute output, and request the existing
  quiescent off-audio plugin reset. This also cuts its live audition by necessity.
  Other destinations continue. Pending-reset blocks stay silent without plugin calls.
  No fresh arrangement history means ordinary wraps do not reset a live-only pedal
  owner. History includes released arrangement notes, since pedals may retain them.
  Very short loops plus retained pedal state can repeatedly reject complete blocks;
  this is conservative M1 safety, not transparent pedal-aware loop playback.
- Fixed spans, gather/merge arrays, key ledgers and click storage only. No callback
  C++ new/delete, locks, logging, UI notification or file loading added. Existing
  off-audio sustain reset and disabled hosted-editor paths remain unchanged.

### Click Contract

- One click per `4 / denominator` qn; absolute timeline beat indices determine the
  accent every numerator ticks. 6/8 means six eighth notes. A non-grid loop start or
  seek does not invent a beat/chase: wait for the next grid tick. Wrap retriggers only
  when a tick exists in the new span. A monotonic tick index owns ordinary boundaries.
- One oscillator, cosine attack at phase zero, 1760Hz/0.25 peak for accents and
  1320Hz/0.15 otherwise, with exponential `exp(-7*t/0.02)` decay, hard bounded at
  ceil(sampleRate*20ms). Each new tick replaces the previous tail, bounding overlap.
  Phase/amplitude/tail count persist sample by sample across normal blocks and tempo
  changes. Meter changes, seeks/loop edits/lifecycle discontinuities clear the tail
  and select the next tick in the new grid. Disable/stop clears it immediately at the
  next admitted boundary; re-enable off-grid does not resurrect a tail.
- At most **1024 click triggers per block**. Excess clears the whole click contribution
  and tail, increments mixer overflow, and resumes from the next admitted block's
  current grid. No partially rendered click batch. MIDI/transport continue independently.
- Same click on both output channels (or mono), independent of plugins, channel
  count/mute/solo. Added after channel meters/summing, before master gain/mute and
  master meter. Master gating does not restart oscillator phase; its meter includes
  the actual gated click. No limiter or extra gain smoothing was introduced.

### Verification

Executed only the permitted independent project, using local JUCE:

```sh
cmake --build /tmp/opencode/vibedaw-t06-tests --target offline_tests --parallel 2
ctest --test-dir /tmp/opencode/vibedaw-t06-tests --output-on-failure
git diff --check
```

- Full CTest **1/1 passed**, including all prior T03/T06/T01/T02/T04 suites and
  callback C++ allocation/deallocation, reset race, editor denial, exact reported
  boundary-carry and total-2048 input regressions. Assertion/leak output fails CTest.
- `loopTimestampTests`: actual plugin absolute timestamps at 44.1/48kHz, 120/137BPM,
  partitions 1/7/127/512/4096 (1-sample tested on short loops), four-bar and minimum
  loops, starts 3.125 and 2165.993971306134, partial final blocks, repeated off-before-on order, exclusive
  end, held live alongside arrangement, process-once counts and wrapped feedback.
- `loopControlCapacityTests`: invalid/short bounds, enable outside range, disable,
  seek/stop/edits while playing, colliding live notes, multiwrap traversal overflow
  with exact final position/recovery, dense atomic destination rejection.
- `loopPedalAndLedgerTests`: previous-block/current-block pedals, silent reset wait,
  off-audio quiescent reset, independent destination, 1024 held arrangement notes
  rejected without a new-attack prefix, exactly 1072 cleanup inputs, live pedal owner
  unchanged even with its own future-event scheduler range.
- `metronomeTests`: every output sample compared to independently calculated click
  offsets/envelopes/phases, 44.1/48kHz, 120/137BPM, 4/4, 3/4, 6/8, three partitions;
  empty graph, stereo equality, master gain/mute, disable/stopped silence.
- `clickMasterAndOverflowTests`: exact one-sample post-master meter, muted-solo
  channel independence, master mute/unmute preserving oscillator phase, click
  capacity rejection with silent output and complete elapsed transport progression.
- `loopClickAndClockTests`: one-hour fractional four-bar loop drift at two partitions,
  sample-for-sample minimum-loop retrigger/tail identity with 1/7/127/4096 blocks,
  exact-end ownership, tempo/meter/seek changes (including tempo-tail continuity and
  next-grid retiming without a seek), accents, no off-grid tail chase,
  below-one-sample loop rejection. Prior nonloop timing/listener tests remain intact.
- `loopUiTests`: actual TransportComponent actions, invalid text retention/feedback,
  external updates, disabled recording, 600px bounds; actual TimelinePanel range
  update for a distant loop and ruler painting into an offscreen image. No native
  window, peer, device, or application is created. MainContent height integration
  is source-reviewed; no application watcher result has been observed.

### M1 Release Result and Next Action

**Blocked, not released.** The deterministic-test release item is satisfied. No
watcher/manual M1 workflow observations were performed or reported. The other
release items retain their unchecked runtime/independent-review state:

- Steps 1-4: source/model and offline placement/routing coverage exist, but actual
  instrument loading, piano-roll edits and two placements through the watcher remain
  unobserved. Hosted instrument editors remain disabled under T06.
- Steps 5-6: transport/loop/click timing and cleanup are offline-tested. Still verify
  audible four-bar repetition, busy-UI playhead, ruler seek and loop edits, 4/4/3/4/6/8
  accents, live collisions/pedals, tempo/device changes and empty-channel click.
- Steps 7-8: mixer/master signal and structural/reset ownership regressions pass;
  actual faders/meters, native resize/pop-out, audio-active editing and device restart
  remain watcher checks. Earlier task acceptance statuses have not changed.
- Independent review has been performed; its reported P2 deferred-wrap offset
  finding is corrected and regression-tested below. Runtime checks remain pending.
  No ASan/TSan, C malloc/realloc interception, third-party RT proof, actual hardware
  sound, native focus/paint acceptance, or milestone completion is claimed.
- T07 remains backlog behind M1 acceptance or another explicit override. Recording,
  sampler audio, clip-local looping, note chasing, sends, plugin playhead reporting,
  persistence and pedal-transparent looping are not implemented by T05.

### Independent Review Correction

- Reproduced P2 at 48kHz, 512 samples, initial 120BPM, loop
  `[0, 0.021333333332)`, and a beat-zero arrangement note crossing loop end. The
  first block defers the wrap with about 1.3333334629661792e-12 qn of overshoot.
  Changing to 20BPM makes the carried barrier map to -1, while the new attack maps
  to zero. Before the fix, the synthetic plugin received an off at absolute sample
  511/offset -1 in its second block; the new input-range regression failed.
- `ChannelMixer.cpp` now clamps only wrap-barrier delivery to sample zero, like
  ordinary pending arrangement events. Fractional span origins, clock advancement,
  cursors and later event timing are unchanged. No upper clamp pulls a future wrap
  into the current block: the clock still defers quantized end-boundary barriers.
- Added `deferredWrapOffsetTests` for the exact report, adjacent `nextafter` loop
  ends, the exact 512-sample loop end and its adjacent values, each with subsequent
  tempos 20/120/300. Every plugin input, including cleanup controllers, must have
  an offset in `[0,512)`. Wrap off/on both occur at absolute sample 512, off first,
  aligned with the click retrigger. A later attack must remain at offset 99, not
  100, proving that the negative fractional span origin was not clamped away.
- The synthetic probe also checks input ranges throughout ordinary multiwrap
  timestamp tests. Source audit found one wrap-barrier insertion path; generated
  wrap offs inherit its corrected offset. Arrangement events already clamp pending
  negatives and reject offsets at/after block end; live input is range-clamped;
  panic/audition cleanup is at zero; collision releases inherit admitted offsets.
  Metronome scheduling already clamps negative offsets and defers end offsets.
- Test-only rebuild succeeded. Full CTest **1/1 passed (6.97s)** after the fix,
  with all previous regressions/allocation probes intact; `git diff --check` passed.
  Independent review's reported correction is complete. T05/M1 and earlier tasks
  retain their runtime acceptance states. No application build/launch, staging or
  commit; existing worktree changes were preserved.
