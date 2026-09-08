# Offline Tests

This is a separate CMake project. It never defines, builds, or launches VibeDAW.
No framework dependency, network fetch, GUI window, audio device, or installed
plugin is required. It uses the existing JUCE checkout (override `JUCE_SOURCE`
when it is not at `build/_deps/juce-src`). JUCE's configure-time `juceaide` helper
is built as a dependency; it is not the application.

```sh
cmake -S tests -B /tmp/opencode/vibedaw-t06-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/vibedaw-t06-tests --target offline_tests --parallel 2
ctest --test-dir /tmp/opencode/vibedaw-t06-tests --output-on-failure
```

Tests compile the real model, engine, mixer, and plugin-host code with plugin
formats and audio backends disabled. `OfflineInstrument` is a synthetic processor,
not a third-party plugin runtime test. The engine callback is invoked through a
friend test seam, without initialising AudioDeviceManager. Settings written by
the real Project destructor are isolated under the test binary directory's HOME.

Coverage includes T03 stable IDs/many-to-many routing, selection after reorder and
deletion, shared sources, unresolved deletion/order, beat conversions, half-open
bounds and validation, synchronous note invalidation, asynchronous source/track
publication, channel notifications and queued deletion. The snapshot compiler is
tested for play-once truncation, sorting, missing destinations, and overflow.
T02's additional scheduler playback coverage is listed below.

T06 covers concurrent coherent publication, retaining old snapshots while the UI
publishes repeatedly, bounded FIFO saturation/reuse, nested/waiting quiescence,
sticky stop through coalescing, synthetic held-note cleanup and muted releases,
off-audio processor destruction, empty graphs, dense MIDI, preparation changes,
and oversized/skipped device blocks. Tests intercept C++ new/delete in marked
render calls; they do not intercept arbitrary C malloc/realloc or prove third-party
plugins real-time safe. JUCE's stack-backed two-channel views and MidiBuffer
preallocation are also source-audited. No ASan/TSan execution is claimed.

Independent-review regressions use a synthetic instrument that ignores cleanup
CCs and simulates JUCE's 2048-total-input VST3 limit. The plugin-input contract is
976 live messages + up to 1024 explicit note-offs + 48 optional cleanup CCs. Tests
exercise the exact full budget and late release, atomic rejection of a larger live
batch, repeated-note accounting, sustained-voice reset off audio, silent waiting
for reset, retained pedal state, editor API denial, feedback-only overflow with a
held onscreen note, and negative/zero callback lengths with sentinel buffers.

Reset re-review regressions invoke the actual engine callback inside the synthetic
plugin's quiescent reset, then verify that its pending panic does not cause repeated
resets without fresh delivered notes. An ingress overflow interleaved with reset
must still discard a queued note-on; reset completion never globally clears panic.
The release->timer->prepare test verifies that pending reset stays deferred while
unprepared and completes once, after plugin preparation and before audio admission.

Still manual: all T03 watcher UI cases, real device/MIDI timing, supported plugin
loading and audible releases/resets, GUI shutdown and device restart. Hosted plugin
editors are deliberately disabled because their internal restart path bypasses our
guard; offline tests verify that no hosted editor creation is invoked. See the task
completion records for restrictions and review corrections.

T01 adds `transportClockTests` and `transportEngineTests` to the same independent
executable, using the actual TransportClock/TransportState/mixer/device callback and
existing synthetic instrument. The commands above are unchanged. Sample-count tests
cover 48 kHz/120 BPM with seven block sizes and partial tails, one hour at 44.1 kHz/
137 BPM (absolute tolerance 1e-10 quarter beats), UI polling stalls, concurrent
exchanges, stale seek feedback, coalesced stop/play, tempo/rate continuity, equal and
stopped seeks, denominator-aware bar/beat formatting, validation and disabled recording.
Engine tests check skipped/invalid/released/oversized blocks freeze position, recovery
discontinuities, live audition while stopped and sample-zero seek cleanup. Existing
render C++ new/delete checks remain enabled. T01 alone adds no scheduler/loop/metronome audio.
T01 UI sources are source-reviewed only; watcher playhead, ruler seeking, busy-UI
behavior, control appearance and real device restarts remain manual/unverified.

T02 adds `arrangementPlaybackTests`, `arrangementLifecycleTests`, and
`arrangementCapacityTests`. The synthetic plugin captures absolute sample times,
local offsets, MIDI channels/pitches/velocities and on/off order in preallocated
storage. Actual compiled snapshots feed the real mixer and two actual synthetic
plugin instances, not an independent imitation scheduler. Coverage includes:

- 48/44.1 kHz, 120/137 BPM, four block partitions plus partial tails; exact expected
  event times, process-once routing, shared placements/many-to-many destinations.
- Half-open edges, source/placement truncation and silent tails, unsorted edits,
  chords, muted/zero-velocity notes, strict overlap unions, touching attacks,
  sub-sample silence and MIDI-channel independence.
- Live preemption and exact offsets, held live before play, stale releases,
  cancelled same-sample taps, stopped audition and selective audition cleanup.
- Playing seek/edit/mute/reassign/reorder/source or placement deletion via real
  asynchronous publication; plugin replacement and channel deletion off audio.
- Tempo/rate changes, shared 976 normal-input capacity, 1024 outstanding-note cap,
  atomic destination rejection without affecting other instruments, empty compiler
  overflow publication while a note is sounding, and recovery.
- Arrangement sustain reset, silent waiting, future attacks after reset, with all
  prior T06 reset race, editor restriction and VST3 total-2048 regressions preserved.

T02 shares the existing 976 normal-input budget between arrangement, live and
collision releases; the 1024 explicit-off + 48 controller cleanup reserve is never
borrowed. See T02's completion record for precise quantization, overlap/preemption,
snapshot invalidation and overflow contracts. The same test-only build/CTest
commands pass. Audible playback and the existing UI through the watcher remain
unverified; no application build/launch or loop/metronome work was performed.

T02 review regressions add `arrangementBoundaryOwnershipTests` (reported
48 kHz/137 BPM boundary loss at seek 2165.993971306134, adjacent `nextafter` values,
zero-origin thresholds, tempo changes to 20/300, and partitions 1/7/64/127).
They require exactly matched on/off events, correct touching-event ordering and
bounded sample times. Retained integer event indices, not repeated block-relative
epsilon searches, own consecutive spans; no snapshot reference survives acquisition.
`arrangementMergedCapacityTests` fills 1024 arrangement keys across blocks, then
checks exact sample-37 live preemption and sample-zero scheduled-release/live-attack
replacement with no overflow or cleanup of a second sustained destination. Final
merged MIDI, not raw live input, is the note-ledger preflight input. The full offline
suite passes with earlier reset-race and allocation probes intact.

T04 adds `mixerSignalTests`, `mixerMeterTests`, `mixerSuppressionTests` and
`mixerBindingTests`. Actual mixer/strip/meter UI sources and their Panel dependencies
are compiled only into this independent executable. Scoped JUCE initialization is
framework-only; component tests create no native peer/window or application. CTest
also fails on JUCE assertion/leak diagnostics, including shutdown diagnostics.

- Synthetic constant stereo outputs verify every sample of channel gain/balance,
  summed master gain/mute, cancellation, peak meters, three-channel additive solo
  precedence, pan continuity/endpoints, mono safety, ranges and empty output.
- Meter decay is partition-identical at 44.1/48/96 kHz with 1/7/127/512/4096 blocks;
  actual UI meter polls ignore stale publication and decay using elapsed time.
- Live/arrangement suppression delivers explicit destination-local releases, drops
  suppressed attacks, waits for future attacks, and preserves T02 token protection.
  Ordinary controls/cosmetics do not panic unrelated notes after message dispatch.
  Master mute is output-only. Full 1024-note cleanup and CC-ignoring sustain reset
  fit the existing budget and require zero callback C++ allocations/deallocations.
  Suppression forwards non-note controller input (including pedal-up) when the
  destination is not waiting for reset; only live notes/arrangement attacks drop.
- Real Project/MixerPanel bindings verify current controls/name/colour/selection,
  master initialization, no model-refresh feedback, stable actions through reorder/
  deletion, narrow scroll extent, collapse, 20/zero channels, selection mouse path,
  and listener teardown. These are not native drag/paint/pop-out acceptance tests.

The old T06 muted-live reorder test now explicitly unmutes before its fresh attack,
matching T04's no-attacks-while-suppressed policy; all earlier suites remain enabled.
Final build and CTest 1/1 pass. See T04 for exact linear gain/balance, sample-peak
envelope, master signal flow and no-chase semantics. Watcher/audible acceptance,
device output clipping, actual plugin behavior, native scrolling and pop-out/dock
interaction remain unverified. No loops, metronome, application build or launch.

T04 independent-review regression extends `mixerBindingTests`: a plain external
Component simulates pop-out content ownership at 760x480 without a native peer.
Add/remove/reorder and hidden dock resizing preserve viewport bounds while updating
strip layout; the actual dock-return hook restores dock layout and further resizing.
This failed on the original unconditional base resize and passes with the ownership
guard. Full CTest 1/1 passed; real native pop-out behavior still requires the watcher.

## T05 Loop and Metronome

The same independent build/CTest commands now compile actual TransportComponent,
TimeRuler, TimelineContent/lanes/headers and TimelinePanel, with no application or
native peer/window. All earlier suites remain enabled; only T01's deliberately
temporary no-loop assertion now expects T05's enabled-loop behavior.

- `loopTimestampTests`: exact plugin absolute on/off timestamps across four-bar and
  minimum 1/64-qn loops, 44.1/48kHz, 120/137BPM, 1/7/127/512/4096 partitions (one
  sample on short loops), origins 3.125/2165.993971306134, partial tails, multiple wraps, exclusive
  end, off-before-on, live held alongside arrangement and process-once counts.
- `loopControlCapacityTests`: invalid bounds, enable/disable/seek/edit/stop while
  playing, live collision ownership, span overflow with full modulo advancement,
  recovery and dense all-or-cleanup rejection.
- `loopPedalAndLedgerTests`: previous/current-block pedal history, quiet reset wait,
  off-audio reset, independent destination; 1024 held arrangement voices fall back
  to exactly 1072 cleanup messages without any speculative attack reaching a plugin.
  An independent live pedal owner survives wraps and destination-local overflow.
- `metronomeTests`: every sample compared against independent timing/phase/envelope
  calculations in 4/4, 3/4 and 6/8 at two rates/tempos and three block partitions,
  including zero channels, stereo equality, master gain/mute and stopped/disabled silence.
- `clickMasterAndOverflowTests`: exact post-master meter, channel mute/solo does not
  gate click, master mute preserves phase, bounded 1024-trigger rejection is silent
  while transport advances fully.
- `loopClickAndClockTests`: one hour at 44.1kHz/137BPM with 1e-10-qn tolerance,
  partition-identical fractional-loop click retriggers/tails, exact-end deferred
  ownership, tempo/meter/seek changes, tempo-tail continuity/next-grid retiming without
  seek, accents, no chase and sub-sample loop rejection.
- `loopUiTests`: actual validation/apply/toggle/external-refresh paths, recording
  disabled, 600px layout containment, distant-loop scrollbar extent and an offscreen
  ruler image showing the real enabled region. No native paint/focus observation.

T05 retains T02's 976 normal + 1072 reserved cleanup messages, total <=2048 including
CCs, and T06's reset/editor restrictions. Wrap offs consume normal capacity; dense
destinations reject the entire speculative block with sample-zero original-ledger
cleanup. Up to 128 musical spans per block; span overflow/sub-sample loops clean
globally and suppress arrangement/click, but retain complete modulo clock movement.
Ordinary wraps preserve live notes; pedal history with arrangement deliveries can
require a destination-wide off-audio reset and cut live audition. See the T05 task
for the complete timing, capacity, click synthesis and live/pedal tradeoffs.

Full offline CTest 1/1 passed, including previous regressions and zero callback C++
new/delete probes. `git diff --check` passed. Actual device/plugin sound, watcher
compilation, native focus/layout, busy-UI playhead and the complete M1 workflow remain
unobserved. No application build/launch, staging or commit; T05/M1 remain blocked.

T05 independent-review regression `deferredWrapOffsetTests` reproduces the reported
48kHz/512-sample, loop `[0,0.021333333332)`, 120-to-20BPM negative wrap release.
The new all-plugin-input offset probe failed before the fix and passes after
clamping barrier delivery, not its fractional span origin. Exact/nextafter loop
ends and subsequent tempos 20/120/300 verify `[0,512)` input bounds, off-before-on
at absolute sample 512 and click alignment. A later attack stays at offset 99
rather than moving to 100. Ordinary multiwrap timestamp tests also check every
plugin input's range. Full CTest 1/1 passed with prior suites/allocation probes;
independent review correction complete, runtime acceptance remains blocked.
