# T02: MIDI Arrangement Playback

Status: blocked | Milestone: M1 | Depends on: T01, T03, T06

Started 2026-09-08 under explicit user authorization to continue despite blocked
T03/T06/T01 watcher acceptance. Earlier statuses and uncommitted changes are
preserved. No application build/launch or commit; offline_tests/CTest only.
Implementation and offline verification completed 2026-09-08. Blocked on watcher
audible/UI acceptance, not missing scheduler implementation. Independent review is
the next source-level check; no prior task's acceptance status has changed.

## Outcome

The first audible arrangement: edited MIDI clips play through their assigned instruments, alongside live audition, without stuck notes.

## Read First

- `src/core/ChannelMixer.h`, `ChannelMixer.cpp`, `TransportState.h`
- `src/project/Track.h`, `ClipInstance.h`, `Clip.h`, `Note.h`, `Channel.cpp`
- T03's unit/routing contract and T06's snapshot/ownership APIs

Historical starting point: the mixer sent only live MIDI to the active channel.
T02 now consumes compiled destination event ranges, never UI note-vector order.

## Implementation Checklist

- [x] Compile source notes plus placement start/end into destination-tagged render events outside audio. Resolve stable IDs, sanitize timing/velocity/channel values, and ignore invalid references deterministically.
- [x] Define clip-local-to-arrangement conversion, source/placement truncation, muted source/instance/note behavior, and zero-duration/zero-velocity policy. MIDI channel 1-16 is distinct from instrument destination ID.
- [x] Schedule half-open block intervals `[start, end)` with a documented beat-to-sample rounding rule. Events on the next block boundary belong to the next block; no negative or out-of-range offsets.
- [x] Aggregate all placements by destination, merge live MIDI into its audition destination, and process each plugin once per block. Clear consumed MIDI without dropping releases.
- [x] Track active arrangement notes by destination, MIDI channel, and pitch. Choose and test an explicit policy for overlapping same-pitch voices and for collisions with live notes; ordinary MIDI cannot uniquely address duplicate same-pitch voices.
- [x] Define deterministic ordering for simultaneous note-offs and note-ons. Clamp note ends to placement/source boundaries according to T03's policy.
- [x] Flush affected active notes on stop, seek, mute, source/instance deletion, route change, plugin/channel removal, or snapshot replacement that invalidates them. Muted rendering must still deliver cleanup before suppressing audio.
- [x] For M1, seeking into a sustained note waits for its next attack; document that note chasing is deferred. Do not emit orphan releases or carry stale active-note state.
- [x] Add fake-processor/event-capture tests for scheduling and routing. Keep correctness tests independent of commercial plugins.
- [ ] Exercise the UI workflow from clip creation through placement and audible playback using the watcher; record actual observations separately from offline test results.

## Acceptance Checks

- [x] A four-beat clip with notes at known beats produces the expected sample offsets at 120 BPM, including events exactly on block start/end.
- [x] One source placed twice plays twice; two destinations on one track and one destination across two tracks route correctly.
- [x] Selecting a different audition instrument does not change arrangement routing. Live keyboard playback still works while stopped and playing.
- [x] Short notes, simultaneous chords, overlapping same-pitch notes, muted notes, placement truncation, and unsorted edits follow documented policies.
- [x] Stop/seek/delete/mute/reassign while a long note is sounding leaves no stuck notes or invalid memory access.
- [x] Tempo changes, sample-rate changes, multiple block sizes, and dense events remain deterministic and respect T06's capacity bounds.

Checked acceptance above means executable synthetic-plugin verification, not
audible hardware/UI acceptance or sanitizer proof. The watcher checklist remains
unchecked and is required before marking done.

## Audible Checkpoint

At completion, a user can write, place, play, stop, and edit MIDI music. Do not hold this checkpoint for loop UI or mixer polish. Transport looping, clip repetition, recording, and audio clips remain out of scope.

## Completion Record

### Changed Files

- `src/core/ArrangementSnapshot.h`: message-thread compilation of unioned lifetimes,
  token-tagged on/off events and sorted stable-destination ranges; channel mute and
  zero-velocity filtering, defensive finite/range validation.
- `src/core/ChannelMixer.{h,cpp}`: bounded half-open scheduling, live merging and
  collision handling, destination-local atomic rejection, selective audition cleanup.
- `src/project/Channel.{h,cpp}`: render-owned per-key arrangement token ledger,
  cleared with delivered-note state on plugin replacement and completed reset;
  documented shared normal-input capacity. Mixer friendship avoids a public mutable
  ledger API. Existing T06 note accounting and reset service remain authoritative.
- `tests/offline_tests.cpp`: timestamp capture in the existing synthetic plugin;
  `arrangementPlaybackTests`, `arrangementLifecycleTests`, `arrangementCapacityTests`.
- `tests/README.md`, this task and `.docs/ROADMAP.md`: contracts, verification and
  acceptance state. The worktree remains cumulative T03/T06/T01/T02, not a clean
  T02-only diff. No earlier changes were reverted, staged or committed.

### Compilation and Timing

- Quarter-note beats throughout. Placement start maps source-local zero; playable
  length is `min(source.duration, placement.duration)`. End is truncated to that
  length; a longer placement has a silent tail. Source start and loopEnabled do not
  offset/repeat playback. Missing source/destination, non-MIDI source, muted source,
  placement, note or destination channel contributes no arrangement events. MIDI
  channel 1..16 remains independent of stable instrument ID. No audition fallback.
- Invalid/nonfinite or nonpositive intervals are omitted. Zero-velocity notes are
  silent, not releases. Compiler clamps MIDI fields defensively; T03 model APIs
  already reject invalid timing. Notes outside the source end remain editable.
- Original bounded flattened notes remain in the snapshot for its existing contract;
  a compiler-local sorted copy unions voices, then creates at most 131,072 events
  and 128 destination ranges. A snapshot still admits at most 65,536 flattened notes
  **before union**. Exceeding it publishes an empty, overflow-tagged revision, never
  a partial arrangement. All vector growth, sorting and reclamation occur off audio
  through T06's unchanged three-slot ownership exchange.
- Per admitted block, use T01's one captured tempo/rate/start/end. For event beat b,
  the sample mapping is `floor((b - startBeats) * sampleRate * 60 / BPM + epsilon)`;
  `epsilon = 1e-7 + 8 * DBL_EPSILON * abs(startBeats) * samplesPerBeat` samples.
  This is containing-sample quantization (less than one sample early), with a small
  floating-point edge tolerance, not nearest-sample rounding or rounded samples per
  beat. Exact equality to the captured start/end maps to 0/numSamples respectively.
  **Ownership is not recomputed from this mapping at the next block's start.** The
  mixer retains one monotonically consumed integer event index per destination in
  the current snapshot. Starting at that index, a binary partition selects events
  with `beat < endBeats && mappedSample < numSamples`. The remainder stays pending.
  A previously deferred event that maps below zero in the next block is delivered
  at sample zero, not skipped. Once consumed, an event cannot be selected again,
  even if tempo or floating-point rounding changes its apparent sample bin.
- This deliberately permits a boundary-tolerance-deferred event to be infinitesimally
  late at the following sample zero instead of losing it or replaying it. Floor
  quantization otherwise remains unchanged. Exact block-end events cannot be emitted
  early, and delivered offsets are always `[0, numSamples)`. A note whose on/off
  quantize to the same sample is silent, including a pending attack whose end has
  already reached its clamped sample. Token matching still prevents orphan releases.
- Revision/discontinuity resets indices with a beat-domain lower-bound at the new
  captured start (no epsilon-based backward search and no note chasing). Indices
  advance over the whole selected span even for unavailable or rejected destinations,
  so reset/overflow does not retry old attacks. Stopped spans consume nothing.
  Only 128 integer indices survive acquisition; no snapshot pointer, iterator or
  reference is retained. Existing cleanup invalidates all old note tokens first.
- Rate changes clean via T01 discontinuity; tempo changes retain beat position and
  active tokens and recalculate subsequent sample offsets. Rejected/skipped blocks
  still freeze time per T01. Silent/overflow/reset-wait admitted spans advance once.

### Lifetime and Ordering

- For each stable destination/MIDI channel/pitch, strictly overlapping arrangement
  intervals are unioned into one physical voice. Earliest attack supplies velocity;
  equal starts use stable source/track/placement order. Nested/transitive overlaps
  extend the final release. Touching intervals reattack (release then attack);
  duplicate same-pitch articulation inside a union is deliberately lost in M1.
- Tokens identify compiled union lifetimes, not UI pointers. A release is emitted
  only for a token whose attack was actually accepted. Seek into a note, edit into
  a note, reset recovery, or release after rejection/preemption never chases notes
  or emits an orphan arrangement release. A new future attack can resume playback.
- Live MIDI owns a colliding pitch. A live attack explicitly releases a sounding
  arrangement voice at that sample and invalidates its token. Arrangement attacks
  while live holds the key are suppressed for that entire lifetime, with no resume
  on the live release. A stale live release cannot kill an arrangement-owned key.
  Repeated live attacks retain T06's bounded multiplicity policy; this task does not
  invent separately addressable ordinary-MIDI duplicate voices.
- Same-sample live on->off pairs are cancelled (last pending same-key attack first)
  before sorting, so block-quantized fast taps cannot become stuck off->on pairs.
  Live off->on remains a reattack. Final input ordering is cleanup at sample zero,
  then for each sample note-offs, non-note messages, note-ons. Live attacks win ties
  against arrangement attacks. Original live order within each category is retained;
  compiled order breaks arrangement ties. Collision-generated releases are sorted
  again so they precede every attack at that sample, not just the matching pitch.
- Audition selection releases only previous-destination live deliveries, preserving
  its independent arrangement voice. The exception is T06's conservative pedal
  reset: a required plugin-wide reset also silences arrangement until serviced.
- Snapshot changes conservatively clean **all destinations**, including cosmetic
  model edits/reorder, before accepting new attacks. This retains T06's policy;
  finer affected-note invalidation is deferred. Async publication may lag setters
  until message dispatch. Channel mute suppresses compiled attacks after publication;
  gain mute still processes cleanup before silencing output. Live audition on a muted
  instrument remains gain-silent under the existing channel contract.
- Stop, seek, panic, preparation/recovery and invalidation clear tokens and deliver
  explicit releases. Plugin replacement clears ledgers under quiescence and destroys
  the old voice owner off audio; channel removal likewise destroys its instrument,
  never rerouting to a new index. Sustained voices still use the untouched T06 fresh-
  voice/pedal latch and off-audio reset; pending reset admits no new arrangement MIDI.
  Hosted plugin editors remain intentionally disabled, including direct host APIs.

### Capacity and Callback Audit

- T06's 976-message live ingress bound is unchanged. Arrangement and live now share
  that **976 normal-input** allowance per destination; it is not an extra arrangement
  allowance. Up to 1024 explicit cleanup releases plus 48 controllers remain reserved
  unconditionally: at most 2048 total input messages, even if the adapter ignores CCs.
  Collision releases also consume normal capacity; unused cleanup reserve is never
  borrowed. At most 1024 physical notes may remain outstanding, repeats included.
- Global ingress validation checks only message format/count (and explicit panic
  markers), not the delivered-note ledger. Outstanding-note preflight runs on each
  destination's **final merged buffer**, after scheduled releases, collision releases
  and cleanup. Thus a valid replacement attack at 1024 held notes is admitted, while
  a genuinely over-capacity destination is cleaned without panicking other channels.
- Fixed 976-entry gather and merge arrays bound callback work/storage. Destination
  and event-range binary searches avoid scanning all notes each block. Only bounded
  in-block events, fixed key ledgers, and the quiescence-protected channel graph are
  read on audio. The 32,768-byte JUCE MIDI scratch reservation remains unchanged.
- Excess raw events (even subsequently suppressible ones), excess collision output,
  or failed final outstanding-note preflight reject the **entire destination batch**
  before its plugin call. Speculative tokens are cleared; original delivered-note
  cleanup replaces the batch and increments the mixer overflow counter. No prefix
  is delivered. Other destinations continue. T06 invalid/oversized live ingress still
  requests global panic/rejection. Compiler overflow is separately pollable through
  `arrangementOverflowed()` and cleans globally through its new revision.
- Each available instrument processes once per block after merging; reset-pending
  instruments intentionally skip their underlying plugin call. No callback model
  traversal of tracks/clips/placements/notes, allocation/reclamation, listener call,
  log, I/O or lock was added. C++ allocation probes pass; C malloc/realloc and opaque
  plugin/adapter internals remain outside the probe, as in T06.

### Verification

Executed against the existing independent test project (no configure was necessary):

```sh
cmake --build /tmp/opencode/vibedaw-t06-tests --target offline_tests --parallel 2
ctest --test-dir /tmp/opencode/vibedaw-t06-tests --output-on-failure
git diff --check
```

- Build succeeded; CTest **1/1 passed**, including every previous T03/T06/T01 test.
  Repeated after scheduler, live-tap and capacity/reset integration corrections.
- Actual PluginHost/Channel/Mixer with two synthetic AudioPluginInstances: absolute
  event timestamps and local offsets at 48/44.1 kHz, 120/137 BPM, block sizes
  7/127/512/4096 plus partial tails; shared placements and many-to-many routing,
  stable IDs, process-once counts, unsorted notes, source/placement truncation,
  silent tails, zero velocity/mute, chords, MIDI channel independence and unions.
- Exact 64-sample edges, sub-sample silence, touching-note off/on order; live
  preemption at sample 17/release at 101, stale release suppression, held live before
  playback, cancelled repeated same-sample taps, stopped audition, selective audition
  cleanup, seek through notes with no chase/orphan release, exact next attack.
- Real asynchronous snapshot publication during playback: source/placement/channel
  mute, note timing/pitch edit, route/reorder, placement/source deletion, missing route,
  plugin replacement/removal. Revisions produce explicit sample-zero cleanup.
- Tempo change exact end offset, rate-reprepare cleanup, shared 976 boundary, oversized
  normal batch and outstanding-note cap, per-destination rejection isolation, compiler
  overflow while held and subsequent recovery, arrangement sustain-reset waiting and
  future-attack recovery. Existing reset/skipped-callback/ingress-race, editor denial,
  cleanup-CC-ignoring plugin and exact 2048-input-limit regressions remain enabled.
- Tests need no hardware, plugin installation, application launch or GUI window.
  No ASan/TSan, actual VST3 runtime, application watcher result or audible observation
  is claimed. Source review includes the merge/ledger/reset path; independent review
  should focus on numerical edges, collision semantics and bounded rejection.

### Remaining Acceptance and Handoff

- Follow-up independent review found no actionable issues in the boundary-carry,
  cursor lifecycle, or merged-capacity corrections. Final CTest rerun passed 1/1
  and `git diff --check` passed. Pending boundary events combined with stop/resume,
  snapshot replacement, reset waiting, missing-plugin recovery, and dense rejection
  were source-reviewed but are not individually combined in regression tests.
- Watcher/manual playback observations: **none performed or reported**. Next action:
  observe watcher compilation, create/edit a four-beat source, place it twice and on
  two destinations, play/stop/seek, switch audition while holding notes, use sustain,
  mute/delete/reassign/edit during playback, and verify audible cleanup/recovery.
  Repeat tempo/device-rate changes and busy UI with supported instruments. Existing
  T03/T06/T01 UI/device acceptance remains blocked under the recorded override.
- No loops, metronome synthesis, mixer UI, recording, note chasing, sample rendering,
  plugin playhead reporting or persistence was added. Existing placement and play
  UI already connects to the graph, so no extra UI/hardcoded demo source was needed.
- T04 can wire controls using the existing coherent channel API after acceptance or
  another explicit override. T05 must preserve this sample/event and collision budget
  when splitting loops; loop wrapping/click synthesis is not silently enabled here.
  T02 implementation is ready for independent review; blocked status truthfully
  reflects the unobserved audible checkpoint rather than unfinished code.

### Independent Review Corrections

- P1 fixed in `ChannelMixer.{h,cpp}`: independent block-relative epsilon lower-bounds
  could defer a release as sample 127, then exclude it as sample -1 forever. The
  reported 48 kHz/137 BPM case seeks to `2165.993971306134`, with a note ending at
  `2166.00001262557`. Destination-relative consumed indices now provide shared
  ownership across consecutive blocks, including tempo changes. The exact timing,
  discontinuity and unavailable-destination rules above supersede the original
  stateless membership claim; no extra snapshot ownership or callback allocation.
- P2 fixed in `ChannelMixer.cpp`: removed raw-live `canDeliverMidi` preflight before
  scheduling. It incorrectly counted arrangement voices that would be released by
  the same merged block and promoted local note-cap rejection to global panic.
  Final merged preflight and the unchanged 976/1072/2048 reserves remain authoritative.
- Added `arrangementBoundaryOwnershipTests`: actual synthetic-plugin on/off capture
  for the exact P1 reproduction, zero-origin and large-origin epsilon thresholds,
  exact boundaries and adjacent `nextafter` values on both sides. Block partitions
  1/7/64/127, with tempo remaining at 137 or changing to 20/300 at sample 127, verify
  exactly one attack/release per note, shared release/touching-attack timestamps,
  off-first order, no early exact-edge release, bounded floor/tolerance timing and
  no stranded voices. Existing ordinary partition-invariant timestamp tests remain.
- Added `arrangementMergedCapacityTests`: fill 1024 arrangement keys across two
  blocks; admit a same-key live preemption at sample 37, or an unrelated live attack
  at sample zero after a scheduled arrangement release. Require exact release/on
  timestamps, unchanged 1024 held count, zero overflow increments, eventual exactly
  matched releases, and an independently sustained second destination with no
  cleanup until its natural sample-288000 release.
- Rebuilt only `offline_tests` in `/tmp/opencode/vibedaw-t06-tests`; CTest **1/1
  passed**, including all previous suites and allocation/reset-race probes.
  `git diff --check` passed. No configure, application build/launch, hardware/plugin
  install, staging or commit. Watcher acceptance and earlier task statuses remain
  unchanged; these corrections are ready for independent re-review.

### T04 Integration Update (2026-09-08)

T04 proceeded under an explicit user override; T02 acceptance remains blocked.
Its mixer contract supersedes the destination-mute/compiler and channel-change
invalidation passages above: destination mute/solo now use captured coherent block
controls and destination-local explicit cleanup/reset. Suppressed event spans are
consumed without attacks or chase. Channel gain/pan/mute/solo/cosmetics no longer
recompile snapshots and cut unrelated voices. Structural graph and source/placement
changes retain conservative revision cleanup. Plugin replacement already clears
its ledger under quiescence. Master mute/gain zero are output-only; MIDI keeps
processing. T02 boundary indices, tokens, live-priority collision policy and the
976/1072 budget remain unchanged and pass in the full offline suite. See T04's
completion record for exact signal/voice semantics and remaining runtime checks.
