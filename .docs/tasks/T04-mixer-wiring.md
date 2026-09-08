# T04: Mixer Wiring

Status: blocked | Milestone: M1 | Depends on: T02, T06

Started 2026-09-08 under explicit user "next" override despite T03/T06/T01/T02
runtime acceptance blocks. Preserve cumulative work; offline_tests/CTest only,
no application build/launch, staging or commit.
Implementation and offline verification completed 2026-09-08. Blocked on
unobserved watcher/audible acceptance, not missing mixer implementation. Independent
review's P2 pop-out resize finding is corrected and regression-tested below.
Next: the watcher checks below; native pop-out runtime remains unverified.

## Outcome

The mixer shows real instrument channels and controls their sound. Master controls and meters reflect the actual summed output.

## Read First

- `src/ui/MainContent.cpp`, `src/ui/panels/MixerPanel.h`, `MixerPanel.cpp`
- `src/ui/mixer/MixerStrip.cpp`, `MasterStrip.cpp`, `LevelMeter.cpp`
- `src/core/ChannelMixer.h`, `ChannelMixer.cpp`
- `src/project/Channel.h`, `Channel.cpp`, `ChannelList.cpp`

Historical baseline: `MixerPanel` was constructed without project/control wiring
and defaulted to decorative strips/sends. Its meter read channel zero as master.
Channel audio had gain/pan/mute but no solo or real master stage.

## Implementation Checklist

- [x] Populate strips from actual channels, including names, colours, current values, and stable IDs. Handle add/remove/reorder and zero channels without fabricating instruments.
- [x] Connect gain/pan/mute and audition selection through T06's handoff. Ensure strip selection actually invokes its callback and model-to-UI refresh does not feed changes back.
- [x] Align model/UI gain ranges and labels. Use a continuous pan law, including exactly centred pan; cover mono output safely.
- [x] Implement channel solo with explicit precedence: a muted channel remains silent even when soloed; if any channel is soloed, only unmuted soloed channels are audible. Multiple solos are allowed.
- [x] Preserve note cleanup and processor lifecycle when muted/solo-suppressed. Do not simply skip a plugin and withhold note-offs. Define unmute behavior for already-held notes consistently with T02.
- [x] Add master gain/mute after channel summing. Make a defined injection point for T05's metronome before master gain/mute.
- [x] Publish per-channel and true post-master stereo meter levels from audio. Fix mute/stale-meter behavior and block-size-dependent decay; avoid reading channel zero as master.
- [x] Hide/disable unsupported sends and bus controls rather than inventing a new bus system. Remove fake fixed-channel assumptions.
- [x] Test gain/pan/mute/solo/master with fake processors and offline buffers.

## Acceptance Checks

- [ ] Changing a fader audibly affects only its instrument; master gain affects the complete mix. Rebuilding strips preserves values.
- [ ] Channel reorder/removal does not change which instrument a strip controls. External channel changes appear in the mixer.
- [ ] Single/multiple solo and mute combinations follow the stated policy without hanging voices on transitions.
- [ ] Two independent channel outputs produce an accurately summed master signal and truthful meters. Empty/muted states decay to silence.
- [ ] Pan changes continuously through zero; mono/stereo buffers and meter calculations remain finite.
- [ ] Mixer remains usable in a narrow window and with enough channels to require scrolling.

## Completion Record

### Changed Files

- New `src/core/MixerState.h`: shared StereoMeter envelope and MasterBus model,
  coherent master mailbox and actual post-sum gain/mute processing.
- `src/project/Channel.{h,cpp}`: solo model/mailbox, shared meter, continuous
  balance, captured-controls processing entry for the mixer; lifecycle meter clear.
- `src/project/ChannelList.h`, `Project.h`: graph-owned master state and project
  access. Master is not a Channel, Track, send, or separately routed instrument.
- `src/core/ChannelMixer.cpp`: one captured control tuple per channel per block,
  additive solo decision, destination-local suppression cleanup, true per-channel
  metering after reset silencing, master stage/injection point and mono safety.
- `src/core/ArrangementSnapshot.h`: destination mute is no longer a compiler filter;
  channel control/cosmetic changes no longer invalidate all arrangement lifetimes.
- `src/ui/MainContent.cpp`, `ui/panels/MixerPanel.{h,cpp}`: project binding, stable-ID
  actions, structural rebuild/listener teardown, silent refresh, scrollable content,
  selection and master controls/meters. Viewport is actual Panel content for collapse
  and pop-out; dock return reattaches it. No native popup was exercised.
- `src/ui/mixer/{MixerStrip,MasterStrip,LevelMeter}.{h,cpp}`: matched fader ranges,
  dB labels, channel selection mouse path, usable vertical master fader/mute,
  disabled FX and removal of decorative send/bus APIs, real meter polling/decay.
- `tests/{CMakeLists.txt,offline_tests.cpp,README.md}`: signal, suppression, meter
  and component-binding regressions; compile actual mixer UI components in the
  independent target. Proper scoped JUCE framework shutdown; CTest also fails on
  JUCE assertion/leak diagnostics. This task and `.docs/ROADMAP.md`, plus appended
  T02/T06 contract updates. The worktree is cumulative, not a T04-only git diff.

### Signal and Controls

- Gain is linear **0..2**, default **1**, on both channels and master. UI fader
  position 0..1 maps to gain 0..2; labels use `20 log10(gain)` dB, with `-inf dB`
  at zero and approximately +6.0 dB at maximum. Nonfinite setters are rejected;
  finite out-of-range input clamps. Master double-click returns to unity.
- Pan is deliberately **linear stereo balance**, not equal-power mono panning:
  `L *= gain * (1 - max(0, pan))`, `R *= gain * (1 + min(0, pan))`, pan -1..1.
  Centre is unity on both sides, preserving the baseline exact-centre loudness.
  This removes the old discontinuity where an infinitesimal move applied -3 dB
  to both sides. Endpoints retain one side at unity and silence the other; channels
  are not crossmixed. A mono output bus uses left output with gain only, ignores
  balance, and mirrors its meter. Mixer scratch remains stereo for plugin safety;
  zero or >2 output channels reject safely. The existing device bridge is stereo.
- Signal flow: plugin -> channel gain/balance and mute/solo gate -> channel meter
  -> additive channel sum -> **T05 click injection point** -> master gain/mute
  -> master meter -> existing device bridge. No normalization, limiter, sends,
  bus routing or track mixer was added. Samples/meters may exceed 1 internally;
  the display saturates at full scale and is labelled `Peak, linear 0..1 FS`.
- Solo is additive: `suppressed = muted || (anySolo && !solo)`. Muted solo channels
  still count toward anySolo. Multiple unmuted solos sound together; a sole muted
  solo silences the whole instrument mix. Master mute wins over the entire sum.
- Message-thread channel setters publish complete gain/pan/mute/solo POD tuples
  through T06 LatestState. Audio captures each once into a fixed 128-entry array,
  then computes anySolo from those same values. Master gain/mute uses its own
  complete POD mailbox. Latest state wins, not a history of every drag/toggle;
  mute->unmute entirely between acquisitions has no observed suppressed block.
  No model scalar reads, locks, listener calls or allocation were added on audio.

### Voices and Identity

- Entering channel mute/solo suppression cleans **that destination only** at sample
  zero: explicit releases for its delivered-note ledger plus existing 48 cleanup
  controllers, clearing arrangement tokens. Existing sustain/sostenuto history
  requests the T06 quiescent off-audio reset after delivering cleanup. Reset-wait
  stays silent and admits no attacks; its timer/preparation lifecycle is unchanged.
- Suppressed plugins still process once per admitted block (except the existing
  reset-pending skip), with output gated to zero. Live note input to that destination
  is dropped; non-note input still updates its controller state, especially pedal-up.
  The existing reset-wait/drop policy remains unchanged. Arrangement event indices consume spans without
  attacks or orphan releases. **No note chase on unmute/unsolo**: only new future
  attacks resume; surviving plugin release tails can become audible on unmute.
  T02 union tokens, live-priority collisions, selective audition cleanup and
  monotonic event ownership remain intact. Unaffected destinations are not cleaned.
- Master mute and gain zero are **output-only gates**, not note suppression: MIDI
  and normal releases continue, so still-held voices resume when the gate opens.
  Channel gain zero likewise does not clear voices. This distinguishes fader-zero
  and master mute from channel mute/solo, intentionally and deterministically.
- Destination-channel mute moved out of compilation into the coherent block state.
  Channel gain/pan/solo/mute/name/colour notifications no longer rebuild snapshots,
  so ordinary mixer edits cannot panic all instruments after message dispatch.
  Source/note/placement changes and structural channel add/remove/reorder retain
  T02 conservative revision cleanup. Plugin replacement clears its own old ledger
  under quiescence; no arrangement revision is needed merely to replace a plugin.
- Strips represent real stable ChannelIds, not tracks or indexes. Name/colour and
  all controls initialize from the model on creation/rebuild. Callbacks resolve ID
  at action time, including selection via Project's existing active-channel bridge.
  Clicking the strip name/background selects audition; child fader/pan/mute/solo
  controls do not implicitly reroute held live input. Model refresh never invokes
  user callbacks. ChannelList/Project listeners and the 33 ms poller detach before
  destruction. Remove/reorder rebuild on the message thread; late copied callbacks
  for a deleted ID are no-ops. Zero channels shows only master, never fake instruments.

### Meter Contract

- Atomic, per-side **sample-peak envelope**, not RMS, true-peak oversampling, or
  random UI activity. Each sample applies `max(abs(sample), previous * decay)`;
  `decay = 0.01^(1/(sampleRate*0.5))`, approximately -40 dB over 0.5 seconds.
  Values below 1e-6 are zeroed per sample. Arbitrary block partitioning gives the
  same recurrence result; sample-rate changes recompute decay during preparation.
- Channel meter is after channel gate and reset silencing, before master. Master
  meter measures the actual post-master buffer, so cancellation and summed peaks
  are truthful, not sums of magnitudes or channel-zero aliases. Muted/empty admitted
  blocks decay; prepare/release and rejected mixer blocks clear meters. Nonfinite
  plugin samples are ignored for metering (not repaired in the audio stream).
- A scalar publication revision lets each UI meter ignore stale data if audio
  stops publishing during device stop/quiescence. Display polls on the message
  thread, has the same 80 dB/s wall-time release, and decays rather than pinning the
  last output. Audio scalars are independently atomic, not a coherent stereo frame;
  a poll can straddle a block, acceptable for metering only. No callback clock or UI
  notification is used. No maximum-since-last-poll hold or clipping latch is claimed.

### Verification

Executed only:

```sh
cmake --build /tmp/opencode/vibedaw-t06-tests --target offline_tests --parallel 2
ctest --test-dir /tmp/opencode/vibedaw-t06-tests --output-on-failure
git diff --check
```

- Final test-only build succeeded; CTest **1/1 passed**, including every previous
  T03/T06/T01/T02 suite. CMake automatically regenerated the independent target for
  its new UI sources using existing JUCE; no application target was built/launched.
- `mixerSignalTests`: synthetic constant stereo outputs, exact all-sample channel
  gain/balance/master sums, negative-signal cancellation, channel versus master
  meters, single/multiple/muted solos with three distinct channel states, endpoints
  and both sides of pan zero, mono output, invalid/range setters, empty graph.
- `mixerMeterTests`: identical partitioned decay at 44.1/48/96 kHz with blocks
  1/7/127/512/4096 and tails, exact silence reset, plus actual LevelMeter polling
  and deterministic elapsed-time ticks proving stale publication decays/recovery.
- `mixerSuppressionTests`: gain/cosmetic edits do not interrupt held notes after
  real message dispatch; destination-local mute/solo releases preserve the other
  instrument, suppress arrangement/live attacks, do not chase, preserve stale-live
  token protection, resume next attacks, and keep master mute output-only. A full
  1024-note ledger releases in 1072 messages; CC-ignoring sustain uses off-audio
  reset and future-attack recovery, and pedal-up is delivered while suppressed after
  reset. No overflow or callback C++ new/delete.
- `mixerBindingTests`: real Project/MixerPanel without native peer/window: initial
  values/colour/name/selection/master state, silent external refresh, stable-ID
  callbacks after reorder/remove, empty and 20-channel lists, real selection mouse
  method, narrow/short viewport overflow, collapse visibility, listener teardown
  and queued changes after destruction. Fixed test harness JUCE singleton shutdown;
  final CTest rejects assertion/leak output rather than trusting only exit status.
- Kept prior tests, adding explicit unmute to T06's old muted-live reorder case
  because T04 deliberately drops suppressed attacks. Initial compile/test failures
  (timer-base ambiguity, master declaration, test link dependency and cancellation
  fixture pan) were corrected. Source audit additionally caught short-circuit
  channel-index advancement in solo acquisition; three-channel regression covers it.

### Remaining Acceptance

- No watcher result, audible hardware/third-party plugin test, application build,
  native pop-out/drag/paint observation, ASan/TSan, staging or commit. C++ new/delete
  probes do not intercept C malloc/realloc or prove opaque plugin real-time safety.
  Acceptance boxes above remain manual/unverified despite offline equivalents.
- Next watcher checks: two supported instruments with arrangement and live notes;
  faders/pan/master across full range; single/multiple/muted solo combinations;
  sustain and mute/unsolo; active selection without arrangement reroute; empty list,
  add/remove/reorder, external model refresh, narrow horizontal/vertical scrolling,
  collapse/float/pop-out/dock return and teardown. Observe actual output and decay
  after device stop/restart. No new channel reorder/remove UI was added.
- Gain/pan changes are block-stepped, not smoothed or automated, so abrupt changes
  may click. Solo/mute cleanup may cut tails and drop held notes by policy. Floating
  sum has no limiter; positive gain can clip at the device. Hosted editors remain
  disabled under T06; sampler playback, recording, sends and FX remain unsupported.
- T05 must inject clicks before master, preserve scheduler/cleanup budgets and
  acquire controls once for its future loop-split block. No loop or click synthesis
  was implemented. T07 must persist stable channel identities, channel gain/pan/
  mute/solo and the single graph-owned master gain/mute; no project persistence or
  legacy format conversion was added. T05 remains todo pending acceptance/override.

### Independent Review Correction

- Fixed the reported P2 in `src/ui/panels/MixerPanel.cpp`: structural strip rebuilds
  called `resized()`, which unconditionally applied the hidden dock's content bounds
  through `Panel::resized()`. With external pop-out ownership and JUCE's
  resize-to-content setting, that could shrink the native window on add/remove/reorder.
- `MixerPanel::resized()` now applies base dock layout only while the viewport is
  parented to the panel (or before viewport initialization). Externally parented
  content retains its existing bounds; only the strips are laid out inside it.
  The existing dock-return hook reparents first, so normal dock sizing resumes.
  No shared Panel behavior, audio policy or native-window creation path changed.
- Extended `mixerBindingTests` with a plain external Component hosting the real
  viewport at 760x480. Add/remove/reorder and hidden dock resizing must preserve
  viewport bounds/parent, while strip height and scroll width remain correct.
  The actual dock-return hook must restore dock bounds and subsequent resizing.
  No test-only production API or native peer/window was needed.
- Regression reproduced the bug before the fix: CTest failed specifically at
  `viewport->getBounds() == externalBounds`. After the fix, the permitted offline
  target rebuilt successfully and full CTest **1/1 passed**, including all previous
  suites and assertion/leak checks. `git diff --check` passed.
- Independent review has been performed; its one reported finding is addressed.
  Actual native pop-out resize/dock behavior and watcher/audible acceptance remain
  pending, so T04 stays blocked on runtime acceptance. No application build/launch,
  staging, commit or changes to earlier acceptance statuses.
