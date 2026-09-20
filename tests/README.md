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

## MIDI Recording (T21 Initial Scope)

The current suite includes `midiTimingTests`, `expressiveClipTests`,
`recorderSessionTests`, and `recordingUiTests`. It tests timestamped ingress and
feedback exclusion, v3 expression persistence and legacy migration, four modes,
explicit placement origins, loop playback, held-note/controller boundaries,
input-loss/reset/work-budget faults, single-level undo conflicts, and UI lifetime
guards. Allocation counters remain enabled for recorder render calls. MainContent
and MainWindow are compiled here, but no native main window is constructed.

Verification at handoff: Debug build succeeded; CTest 1/1 passed in 15.26 seconds.
Live preview, count-in and seamless commit work are deferred, not tested as
implemented. Real devices, VST3 controller mappings, latency and native interaction
remain in [TOTEST](../.docs/TOTEST.md). See the narrowed
[T21 completion record](../.docs/tasks/T21-midi-recording.md), not its original
full-scope checklist, for the delivered contract.

The older per-milestone evidence below is historical; recording-disable and v2
writer descriptions are superseded by the current recording/v3 tests.

The UI suites run with the application `DawLookAndFeel`. Offscreen paint checks
cover button/scrollbar states, rounded controls, surface gradients, enabled-label
contrast, and white/black held-key feedback in both piano keyboards. Drag and loop
probes use the shared theme, and dock-return checks include the inset panel frame.
The audition keyboard and PianoPanel are compiled in this independent target.
These checks are not full-window screenshot comparisons: final desktop/HiDPI
appearance, native menus and the MainContent layout remain watcher/manual checks.

`windowChromeTests` uses the peerless shared DawWindow to check the custom title
bar's button layout, hover/press and restore glyphs, button callback dispatch,
content insets, and thin visible border with a wider resize area. Actual desktop
dragging, edge/corner resizing, maximize/restore, minimize, and window-manager
snapping remain manual checks. Main, piano-roll, and plugin windows share these
decorations; panel pop-out windows retain their existing native title bars.

`nativeMaximizeTests` injects observed window-manager states and records requests
without opening an X connection or a native window. It covers external maximize,
partial states, serialized rapid clicks, rejected/delayed requests, timeout with a
queued inverse, restore-icon synchronization, and captured resize-drag cancellation.
On Linux the real implementation sends EWMH maximize/restore requests to the WM,
not JUCE fullscreen requests, so KDE owns panel avoidance and restore geometry.
`editorWindowChromeTests` constructs the real piano-roll/plugin wrappers peerlessly,
checking piano-roll resize layout, unchanged plugin content dimensions (including
editor-originated resizing), and quiescent editor cleanup. Tests link X11 to compile
the real native request path; actual KWin/XWayland behaviour still needs manual
verification. Maximized custom windows currently require restoring with the button
or title double-click before client-side dragging/resizing (no drag-to-restore).

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
for reset, retained pedal state, editor capability forwarding, feedback-only overflow with a
held onscreen note, and negative/zero callback lengths with sentinel buffers.

Reset re-review regressions invoke the actual engine callback inside the synthetic
plugin's quiescent reset, then verify that its pending panic does not cause repeated
resets without fresh delivered notes. An ingress overflow interleaved with reset
must still discard a queued note-on; reset completion never globally clears panic.
The release->timer->prepare test verifies that pending reset stays deferred while
unprepared and completes once, after plugin preparation and before audio admission.

Still manual: all T03 watcher UI cases, real device/MIDI timing, supported plugin
loading and audible releases/resets, GUI shutdown and device restart. Hosted plugin
editors are enabled for initial testing by explicit user override. Their internal
restart path still bypasses our guard; offline tests do not establish native restart
safety. See T10 for the current override and outstanding acceptance checks.

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
- Real Project/MixerPanel bindings now target independent mixer channels, verifying
  controls/name/colour/bus selection without changing the audition instrument,
  master initialization, no model-refresh feedback, stable actions through rebuild/
  deletion and instrument reorder, narrow scroll extent, collapse, 20/zero channels,
  selection mouse path, and listener teardown. Native acceptance remains separate.

The old T06 muted-live reorder test now explicitly unmutes before its fresh attack,
matching T04's no-attacks-while-suppressed policy; all earlier suites remain enabled.
Final build and CTest 1/1 pass. See T04 for exact linear gain/balance, sample-peak
envelope, master signal flow and no-chase semantics. Watcher/audible acceptance,
device output clipping, actual plugin behavior, native scrolling and pop-out/dock
interaction remain unverified. No loops, metronome, application build or launch.

T04 independent-review regression extends `mixerBindingTests`: a plain external
Component simulates pop-out content ownership at 760x480 without a native peer.
It now reparents the content wrapper (toolbar plus viewport), not the viewport alone.
Add/remove/instrument reorder and hidden dock resizing preserve wrapper/viewport
bounds while updating strip layout; the actual dock-return hook restores dock layout
and further resizing.
This failed on the original unconditional base resize and passes with the ownership
guard. Full CTest 1/1 passed; real native pop-out behavior still requires the watcher.

## Independent Mixer Channels

The independent mixer additions and all earlier suites build and pass in the main
agent's configured offline directory:

```sh
cmake --build /tmp/opencode/vibedaw-routing-tests --target offline_tests --parallel 4
ctest --test-dir /tmp/opencode/vibedaw-routing-tests --output-on-failure
```

Final CTest result: 1/1 passed (11.80 seconds). No application was built or launched.

- `independentMixerModelTests`: exactly one empty default bus, independent
  instrument/bus lifecycles and capacities, new instruments routed to Master,
  stable/non-reused IDs, exact-ID restore validation, control clamps/non-finite
  rejection, many-to-one routing, invalid-route rejection, and removal/clear
  returning instrument outputs to Master without deleting instruments.
- `independentMixerSignalTests`: real fake-instrument stereo samples summed into
  shared buses, direct Master contributions, instrument-before-bus gain/balance,
  bus gain/pan endpoints/continuity/mono behavior, cancellation, mute/solo precedence,
  additive bus solos excluding direct Master audio, post-control bus meters,
  pre-bus instrument meters, master gain/mute, invalid-route fallback, zero buses,
  all 128 preallocated destinations and current-rate meter decay for buses added
  after preparation, using sparse IDs larger than the buffer count. Each marked
  render retains C++ new/delete guards; instruments
  are asserted to process once, not once per route or bus.
- `independentMixerNoteTests`: held arrangement and pedal-sustained live notes
  survive bus mute/solo, rerouting, rename and bus removal without cleanup, reset,
  retrigger or audition-selection changes. New MIDI attacks/releases still reach
  a muted or solo-suppressed bus's instrument; final stop proves the original ledgers
  remain intact.
  Existing instrument-suppression regressions now run through a shared bus, and
  metronome/master regressions include a muted solo bus to prove click bypass.
- `mixerEditEngineTests`: ten scenarios invoke the real engine device callback
  while edits reject audio admission. Actual bus add/remove/clear/exact-ID restore
  operations invoke the callback from synchronous `mixerChannelsChanged` listeners
  inside their structural edits; routing assignment uses an outer PreserveVoices
  edit around the real setter and callback. Rejected buffers are silent, processing
  and clock/publication revision remain frozen, and queued live note-offs/attacks
  survive alongside held live and arrangement voices. Resumed blocks process each
  instrument once, reflect the new output route and deliver the arrangement release
  at its original musical boundary, without cleanup CCs or retriggering.
  Nested default edits upgrade PreserveVoices until outer exit; preserving children
  cannot downgrade default edits. Explicit pending panic, ingress overflow and a
  previous destructive rejection all survive later preserving rejections, discard
  queued attacks and clean held notes exactly once. Every callback retains the
  C++ allocation/deallocation guards. The overlap is deterministic same-thread
  callback injection, not an OS scheduling or multithreaded stress claim.
- `mixerBindingTests` retains the layout/pop-out regressions and adds toolbar
  add/remove, independent selection, capacity button state, rename/blank-name
  validation and stale rename callbacks. `mixerRoutingMenuTests` covers row Output
  controls/context submenu, ticked targets, many-to-one assignment without changing
  audition selection, rename refresh, actions surviving instrument reorder, stale
  bus/instrument/owner closures and a Master-only menu with zero buses. Existing
  rack drag fixtures derive empty-space coordinates from the taller row height,
  preserving child-first/background dispatch assertions with Output controls present.
  Live and retained/stale context-menu iterators both account for Audio Output.
- `projectFileTests` now round-trips v2 mixer IDs/order/name/colour/controls and
  instrument routes alongside the existing plugin/clip/track/transport assertions.
  `mixerProjectFileTests` adds many-to-one save/load, zero-bus save/load, dirty tracking,
  a literal v1 fixture with no mixer section (inert routes discarded, instrument
  controls retained, one empty bus seeded), reserialization as v2, the exact
  128-bus capacity boundary and 21 malformed-v2 cases. Invalid lists, IDs, references,
  capacity, control ranges/types, flags, colour and name must preserve serialized
  live content, object identities, selection, playback position/state, dirty state
  and file path, even after a prior successful prepare followed by failed prepare.

Native menus, rename dialogs, real pop-out/dock interactions and audible third-party
plugin behavior remain watcher/manual checks. Offline tests do not certify plugin
real-time safety or native GUI behavior.

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

## T10 Editor Access

The same independent commands use stock JUCE 7.0.12 with VST3 compiled out.
`pluginEditorBindingTests` compiles the actual PluginButton and Channel Rack/Sidebar
sources and checks active-channel selection, reorder, plugin/name notifications,
empty/unloaded/no-editor reasons, enabled target-specific menu data, failed candidate
load preserving the host, and guarded host replacement/deletion/project teardown.
Synthetic capability and creation calls now reach the processor instead of being
denied. Listener teardown is followed by a queued model notification.

`pluginEditorCreationTests` creates a synthetic editor component without a native
peer, checks that a second editor owner is refused, and releases it under the gate.
It does not open PluginWindow or a popup. Focus/reuse, actual native teardown,
watcher compilation, installed VST configuration and live MIDI/restarts remain
unobserved. Existing callback new/delete checks and earlier suites remain intact.

The unfinished patched-dependency integration and its restart-only tests were
removed at the user's explicit request. Editors are available for initial testing,
not certified safe: stock JUCE editor-originated restarts bypass host quiescence.
No application build/launch is part of this verification. See T10 for results.

## T11 Rack Drops and Clip Creation

The same three commands above compile the actual PluginTreeItem, Channel Rack and
ClipsContent sources in `offline_tests`. No scanner instance, installed plugin,
native window, popup, or device is needed. `rackDropAndClipCreationTests` covers:

- Typed browser plugin payload round-trip; bare strings/IDs, presets, unknown clip
  objects and empty/relative paths rejected rather than misclassified as plugin paths.
- Empty/populated rack creation, active loaded instrument naming, exclusive row
  versus parent dispatch, half-open row edges and add-button space in short racks.
- JUCE-style child-first discovery using original Browser source coordinates before
  target-local enter/move/drop events. Exit-interest rechecks use the next target's
  coordinates (or zero outside), verifying background/row/outside preview and button
  cleanup. Geometry never controls interest; it still gates preview and commit.
- Fake candidate failure, unloaded and oversized layouts preserving count/selection/
  installed plugin; replacement allowed at 128 channels, creation preflight skipping
  the loader at capacity, missing stable targets and quiescent old-host destruction.
- Enter/exit cancellation with no load, actual offscreen create/replace colors and
  unsupported-row hover clearing. Existing sample assignment uses CTest's already
  existing CMakeCache.txt as a file fixture; it makes no sample-playback claim.
- Actual New Clip button callback creates/selects a four-beat source with zero open
  requests or tracks/destination changes. Real double-click and Edit callbacks target
  the source; delayed context actions after source/row/owner deletion are harmless.
  ClipPool's before-removal notification still runs while the source exists.

The private Project loader seam supplies fake processors while the production
candidate validation and commit path remain exercised. Earlier suites and callback
allocation/assertion/leak checks remain enabled. MainContent's create-only callback,
ClipId window reuse/focus, grid listener teardown and shutdown were source-reviewed;
they are not compiled/executed by this component target. Native acceptance remains
manual: real JUCE drag hit testing, text/layout at sidebar widths, real plugin loading,
Piano Roll focus/minimize/reopen/deletion and T10 editor restart/device behavior.
Independent review was performed by an explore agent. Its high-severity coordinate-
dependent interest finding is fixed and regression-tested: temporarily restoring the
original predicate fails rack discovery, while the corrected predicate passes.
Hit-test fixtures set visibility without creating native peers. T11 remains blocked
on manual acceptance, not done.

## T12 Timeline Dragging

The same three independent commands above compile the actual timeline and Clips
components. Final build succeeded and CTest 1/1 passed (6.72 seconds); earlier suites
and callback allocation/assertion/leak probes remain intact. No application target,
native peer/window, installed plugin or audio device was built/launched.

- `timelineDragTests`: shared transforms, quarter-beat snap/Alt bypass, half-open
  target edges, real ClipRow typed payload, child-first JUCE-style discovery with
  SOURCE coordinates, target-local preview/drop, actual offscreen ghost colour,
  empty timeline/new lane/existing lane placement and preview snapshot isolation.
  Actual lane/parent mouse callbacks exercise selection, threshold/grab offset,
  within/across/new-lane movement, original pointer/UUID/source/duration/mute/route,
  overlap hit order, scrolled/zoomed Alt moves, Escape/outside cancellation,
  explicit editing, vanished source rejection and safe unresolved placeholders.
- `timelineInvalidationTests`: non-Instrument/missing destination, hovered track
  deletion, real panel scrollbars driven by bounded timer ticks to both limits,
  reachable below-lane space, outside/exit stopping, vanished source/producer cleanup.
- `timelineCommitTests`: stale/inconsistent IDs and invalid mutations leave ownership
  unchanged; synchronous addition observers see a populated new lane and exactly
  one moved placement. A retained snapshot stays old until asynchronous publication,
  then contains the new position with the original destination, never a partial move.

The timer has a private friend test seam; component event callbacks are real, but
OS mouse capture, drag-image construction and native focus are not simulated.
MainContent's source-editor callback/reuse is source-reviewed, not compiled by this
target. A development-only global allocation-failure experiment terminated in JUCE
construction and was removed; no process-wide out-of-memory recovery is claimed.
See T12 for remaining watcher preview/layout, native drag/scroll/cancel/pop-out,
editor focus/reuse and audible playback-cleanup acceptance. T12 remains blocked.

T12 independent-review regression `timelineGestureReviewTests` covers both P2
findings: Escape survives continued movement/exit/reentry/drop without a new down,
including a second target; real ClipRow callbacks start at most one drag per press
even if JUCE has deleted the drag image. A private starter seam replaces native
startup and checks the cross-window flag, now explicitly enabled. Fresh mouse-down
recovers, and source mouse-up does not invalidate the imminent JUCE listener drop.
Release-only empty/existing-lane drops, ruler-to-lane/new-track releases, actual
valid-preview route changes, stale structure cancellation, and ordinary exit/reentry
are exercised through real TimelineContent callbacks without native peers.

Temporarily restoring the enter-required guard fails the release-only count check
(0/1, 0.51 seconds); removing persistent Escape marking fails the reentry-preview
check (0/1, 0.52 seconds). Both fixes restored: independent build and full CTest 1/1
pass (6.62 seconds), using the same documented build/CTest commands. No native drag
image, OS Escape/focus dispatch or cross-window capture is claimed tested; those
remain watcher acceptance. Earlier suites and T10's stock-JUCE override are intact.

## T13 Sidebar Tab Rail

The same three independent commands compile the actual Sidebar, SidebarContainer
and SidebarTab sources (newly added to the target) plus the real sidebar
factories. `sidebarRailTests` covers:

- Left-rail geometry: one 28px rail whenever any sidebar on that side is
  collapsed, expanded panels adjacent to it, tabs stacked inside rail bounds, no
  per-sidebar blank columns (two collapsed sidebars occupy 28px, not 56px).
- Stable tab identity: tabs rebind by sidebar identity on every membership
  change; equal-count collapse/expand swaps leave no stale bindings, and each
  tab toggles exactly its own panel through `activate()`.
- Right-rail regression: a collapsed right sidebar's tab sits inside the
  container at the outer edge; the previous remembered-expanded-width offset
  (250 - 28 inside a 28px container) no longer reproduces.
- Narrow-window arbitration: proportional display-only clamping of expanded
  sidebars (displayed widths sum within the available space), remembered sidebar
  widths untouched, rail and tabs reachable at extreme narrowness, mixed
  collapsed/expanded layouts keep the rail intact, and constraints restore
  cleanly when space returns.
- Remembered widths survive repeated toggle cycles; one resize notification
  drives a coherent relayout; removing a collapsed sidebar drops exactly its tab.
- Factories wire distinct UTF-8 glyphs (Browser, Channel Rack, Clips) with
  tooltip names; SidebarTab exposes its bound sidebar and tooltip for testing.

IconButton now also carries tooltip support. MainContent's narrow-window
priority (left, then right, center keeps the remainder) and the Ctrl+B toggle
are exercised through the container APIs; the MainContent wiring itself is
source-reviewed, not compiled by this target. No native paint, hover visuals,
tooltip popups or watcher observation is claimed; those remain T13 watcher
acceptance. Final build succeeded and CTest 1/1 passed (6.76 seconds); no
application build/launch, staging or commit.

## T14 Context Menus and Keyboard Actions

The same three independent commands compile the actual Channel Rack, Clips,
TrackHeaderList/TimelinePanel/TimelineContent and PluginSection sources.
`contextMenuTests` covers the six T14 surfaces: channel rows (open editor,
select, rename, remove with impact count), pooled MIDI clips (edit, rename,
distinct Delete Source), timeline placements (edit shared source, Set Start
Beat, destination submenu of live instruments with an unresolved marker,
mute/unmute, remove placement), track headers (rename, remove with placement
impact), empty timeline space (place selected clip through the same commit
path as drags, add track), and browser plugins (create channel via the
double-click path).

Delayed menu/button/prompt actions re-resolve stable track/instance, clip,
channel, and track UUIDs; deleted targets become no-ops and raise no dialog.
Cancellation never mutates. Keyboard access is covered: Delete/Backspace
removes the selected placement only (source stays pooled), Ctrl+E edits its
shared source, the piano-roll grid deletes selected notes when focused, and
invalidation clears borrowed note selections.

Two supporting changes: `TextPrompt.h` now exposes `textPromptInterceptor()`
and `confirmInterceptor()` seams so offline tests intercept rename prompts and
confirmations without creating native windows — visible dialogs remain
watcher/manual acceptance. Rename/beat actions re-resolve their target before
opening any prompt, so a channel/clip/track deleted while pending raises no
dialog (a regression that previously left a modal alert poisoning later
suites). The validated rename paths themselves reject blank names.

Review findings fixed with regressions: menu strings containing `…`/`—` now use
the `juce::CharPointer_UTF8` idiom — raw UTF-8 in `const char*` literals
double-encodes (JUCE asserts in debug and mojibakes labels, and `const char*`
comparisons could never match). The Clips footer Delete Source button now
routes through the same confirmed, impact-warned path as the context menu;
intercepted confirm/cancel coverage asserts no mutation before the dialog
resolves and deletion only on OK. Stale-test arithmetic and a dangling
`addNote` pointer (vector reallocation) were corrected in the tests themselves;
`ClipInstance::isValid()` is a local-fields check, and unresolved-source
expectations use the pool/channel state. Full build and CTest 1/1 pass (6.9
seconds) with all earlier suites intact; `git diff --check` passed. Native
popup placement, focus interaction, discoverability and plugin-window behavior
remain watcher/manual.

## T16 Loop Region UX

`iconTests` renders the vendored Tabler (MIT) transport icon set — parsed once
by `Drawable::parseSVGPath` into `Icons` and stroked with round caps/joins —
at the 36x28 button size, asserting non-empty parsed geometry and visible
pixels for all eight glyphs. The hand-drawn `TransportButton::createIcon`
paths are gone; buttons tint the stroked path per state.

`loopUiTests` now drives the reworked loop workflow:

- Transport bar: the loop start/end fields, Apply and validation label moved
  out of the bar (which compacts 104 -> 64px); the Loop button keeps
  left-click enable/disable and reports right-click through
  `TransportButton::onContextMenu` (right-click never fires the toggle).
- Menu dispatch: `handleLoopMenuAction` mirrors the bar controls — 1 toggles
  looping, 2 opens the editor (via the `openLoopEditorOverride` seam in
  offline tests, never a native `CallOutBox`), 3 clears (disable + reset to
  the default `[0, 4)` region).
- `LoopEditorPopover` (created without launching through `createLoopEditor`)
  retains the removed row's behavior: invalid input ("", "abc", "1foo",
  "nan", "inf", "-1") is retained with a red validation message and no model
  change; a valid Apply commits both fields atomically and enables looping;
  external loop updates refresh an open popover's fields and validation
  label.
- Ruler gestures: a plain click (and any sub-5px drag) still seeks; dragging
  creates on empty space, moves by grabbing the region body (length
  preserved), and resizes by grabbing either edge within the 5px tolerance.
  The gesture previews locally (`isDraggingLoop`/`getLoopPreview`), publishes
  nothing until mouseUp, commits exactly once with 1/16 qn snapping, clamps
  resizes to the 1/16 minimum length, and auto-enables looping on commit
  (Alt bypasses snapping and keeps unsnapped beats).
- Known test-environment note: each commit grows the timeline extent and the
  panel re-syncs the ruler to the viewport's scroll offset, so the suite
  restores the test scroll offset between gestures; in the app the viewport
  is the scroll source of truth and this resync is transparent.

Full build and CTest 1/1 pass with all earlier suites intact; `git diff
--check` passed. No application build/launch, staging or commit. Native
popover placement/focus, menu popup behavior, drag cursor affordances and
watcher-observed looping remain manual acceptance for T16.

T16 round-1 watcher feedback (2026-09-09), fixed with regressions: plain drag
now creates a replacement loop region anywhere (including inside the body);
Shift+drag inside the region moves it and wins over an edge grab; plain edge
grabs still resize. The ruler previews the pending action through the mouse
cursor (crosshair = create, dragging hand = shift-move, left/right = edge
resize) on hover, during the gesture and after mouseUp, asserted via
`getMouseCursor() == StandardCursorType`. The Edit Loop CallOutBox launches
at desktop level (null parent, screen-coordinate anchor) because a box
parented to the transport bar painted beneath later-added sibling panels;
native placement/focus stay watcher-verified. Full build and CTest 1/1 pass
with all earlier suites intact; `git diff --check` passed.

T16 round-2 watcher feedback (2026-09-09): a disabled (or cleared) loop region
no longer paints a dim band — the ruler highlight appears only while the loop
is enabled, so Clear Loop/button-disable removes it completely (also at
startup). Grab gestures require a visible region: any drag over a disabled
region creates a replacement, with hover/drag cursor checks proving no
invisible-region grabbing. Regressions assert background-only pixels across a
disabled band and the created-replacement commit (snapped 1/16, auto-enable).
Every `transportLoopChanged` notification resyncs the ruler's scroll offset
to the viewport, so the suite restores its test offset after disabling. Full
build and CTest 1/1 pass with all earlier suites intact; `git diff --check`
passed.

T16 round-3 (2026-09-09), user-specified loop state machine: `LoopRegion`
gains an `exists` flag distinguishing no-loop (cleared/never created: nothing
painted on the ruler, ungrabbable) from set-but-disabled (dim band, still
grabbable) and set-and-enabled (blue). Enabling a cleared loop materializes
the default `[0, 4)` region enabled (the Loop button and menu Enable always
produce a visible active loop); `TransportState::clearLoop()` removes it
entirely; `setLoopRegion` marks it set. TimeRuler paints/grabs by `exists`
(dim colours restored for disabled), commits of any gesture re-enable
looping, and `TimelineContent::transportLoopChanged` passes the full region
so the flag travels. Regressions cover dim-band pixels, interior/edge cursor
checks on a dim region (wide region so the body exceeds the 0.1-beat edge
tolerance), cleared background pixels, drag-creates-on-empty, default-region
blue paint, and menu/button round-trips. Full build and CTest 1/1 pass with
all earlier suites intact; `git diff --check` passed.

## T07 Project Save and Load

`projectFileTests` exercises the versioned project document end to end using a
`StatefulInstrument` fake plugin (real state capture/restore, no third-party
plugin) and a `pluginRestorer_` seam that mirrors the default restore path:

- Serialize/parse/stage v2 round trip: two channels with distinct plugin state,
  names, instrument controls and output routes, two independent mixer channels
  (stable IDs/order/name/volume/pan/mute/solo/colour), a master gain,
  three pooled clips (MIDI notes with velocity/channel, audio file reference,
  pattern fields), a shared clip placed on two tracks plus an unresolved
  source placeholder, transport tempo/meter/loop (`exists`+`enabled`)/
  metronome. Every field is asserted after commit; plugin identity round-trips
  beyond the bundle path (`uniqueId`, format, fileOrIdentifier, manufacturer,
  version, isInstrument) and the opaque `STATE-BYTES` blob arrives intact.
- Save-then-load into the same project: post-save mutations (tempo change, an
  extra "Doomed" channel) are discarded, IDs/order/names/routing are restored,
  the active channel resolves to the restored stable ID, ID counters advance
  past restored maxima (new clip/channel get non-colliding IDs), and channel
  reorder preserves per-instance routing.
- Two-phase load contract: `prepareLoad` of a nonexistent or corrupt file
  fails with an actionable error and leaves the live session untouched;
  `commitLoad` without a successful prepare is a no-op; a successful prepare
  stages the document without mutating anything (a cancelled discard prompt
  simply never calls `commitLoad`).
- Validation rejections (whole file, session untouched): unsupported
  `formatVersion`, non-array channels, duplicate channel IDs, corrupt base64
  plugin blobs, out-of-range volume, out-of-range tempo, malformed loop
  object.
- Missing plugins: a restorer that refuses the plugin leaves a visible
  unresolved channel named after the saved channel, carrying the identity and
  state blob; re-saving preserves the identity+blob for recovery.
- Failed saves: writing over a directory and saving with no chosen path both
  fail with errors and leave the dirty state intact (no false success).
- Dirty tracking: channel/clip/transport edits mark the document dirty; save
  and New clear it; New resets the models, transport defaults, loop region
  (`exists == false`) and the project file path.
- No native dialogs are involved: file selection and confirmations run through
  the same interceptor seams the UI uses (`fileDialogInterceptor`,
  `confirmInterceptor`), so real FileChooser/AlertWindow behavior remains
  watcher/manual acceptance, as does plugin state through a real restart.

Full build and CTest 1/1 pass with all earlier suites intact; `git diff
--check` passed. No application build/launch, staging or commit.

## T08 Editor Navigation

The same independent commands compile actual `PianoRollKeyboard`,
`TimeRulerComponent`, and `PianoRollEditor` alongside the existing
`NoteGridComponent` (added to the target). `editorNavigationTests` covers:

- Shared `PianoRollGeometry`: all 128 pitch rows round-trip through
  `yFromPitch`/`pitchFromY` with exact key-height boundaries, pitches 0 and 127
  reachable, fractional-pixel scrolling resolves the correct row, and
  `beatFromX`/`xFromBeat` round-trip at 20/80/320 px-per-beat.
- Content extent: minimum beats dominate short clips, long clips and note ends
  win, and non-finite input falls back to the minimum+margin.
- Arrangement-to-source mapping: half-open placement ends, placement longer
  than source, source longer than placement, and gaps before/after placements.
- Follow-band decision (`followedScrollX`): no scroll inside the band,
  recentering past either edge, and clamping at both content ends.
- Real `NoteGridComponent` hit testing at pitches 0/60/127 in grid-local
  coordinates (the viewport would translate the component), plus grid
  invalidation clearing borrowed selection and `setMidiClip(nullptr)` safely
  detaching; a `grabKeyboardFocus` guard keeps peerless grids out of a JUCE
  focus assertion.
- Keyboard rows line up with the grid at a viewport offset, and a real
  `PianoRollEditor` + real `TransportState` follow session: resize does not
  suspend, a far playhead scrolls into the band, following never changes the
  audio position, manual scroll suspends and the Follow toggle resumes, a
  no-placement position hides the playhead, and zoom preserves the left-edge
  beat.

No native peer/window, wheel, scrollbar drag, focus, or audio device is
created. Watcher/manual alignment before/after scrolling, click/drag/audition
after scroll, fractional-row and zoom-limit alignment, resize/scrollbar
behavior, long-clip navigation, and observed follow suspend/resume with audio
remain manual acceptance. The Follow toolbar button does not visually reflect
an internal manual-scroll suspension (unchecking disables following). Full
build and CTest 1/1 pass with all earlier suites intact; `git diff --check`
passed. No application build/launch, staging or commit.

## T09 External MIDI and Integration

`externalMidiTests` covers the device-selection path end to end. Real hardware
cannot be opened here (ALSA/JACK are compiled out and JUCE's stubs return an
empty device list), so `sendMidiMessage` stands in for a connected device's
callback: it enters `MidiManager::handleIncomingMidiMessage` exactly where an
open `MidiInput` would. Coverage:

- Selection API without hardware: empty device list, invalid index/out-of-range/
  unknown-name connects fail and stay disconnected, `disconnect()` with no
  device and sending/draining with no destination are safe no-ops.
- Device event -> engine queue -> block -> exactly one attack on the active
  channel, while a looping arrangement note sounds independently on another
  destination. Switching the active channel reroutes the next external event
  without disturbing the arrangement.
- Feedback drain (`MidiManager::timerCallback`, driven directly - no message
  loop): marks the on-screen keyboard state and notifies `MidiListener`s, and
  the `updatingKeyboardFeedback` guard keeps the echo out of the audio queue
  (plugin receives exactly one attack, asserted across further blocks).
- Note-off through the same path; wind-down leaves empty per-key held ledgers
  and `sounding == 0` on both destinations.

`integrationWorkflowTests` is the cross-feature fixture: real `Project` ->
`ChannelMixer` -> `AudioEngine` wiring with two `StatefulInstrument` channels,
one shared clip placed on two tracks with different destinations, loop
`[0, 8)` enabled, and 120 -> 90 BPM changed mid-playback. Asserts the
volume/master-gain levels of the summed output while playing, muted-channel
attack suppression with clean unmute (no chase), loop-wrap refiring on both
destinations, stuck-note-free stop (held ledgers empty), then save/mutate/load
through `ProjectDocument`: channel IDs/order/routing/tempo/loop/metronome/
tracks/placements/pool restored, old plugins destroyed quiescently off audio,
restored plugins prepared and rendering through the SAME mixer, and zero
callback allocations. Probes are declared before the Project so instrument
destructors outlive them.

Full build and CTest 1/1 pass with all earlier suites intact. Still manual:
external MIDI with real hardware (audible delivery to exactly the selected
instrument, hot-unplug), and a human following `.docs/PLAYING.md`. No
application build/launch, staging or commit.

## Built-in Instrument (VibeSynth)

`builtinSynthTests` covers the built-in instrument, which rides the ordinary
plugin pipeline through the `PluginHost` injection seam via an
`InternalPluginFormat` registered in the host format manager:

- Identity/load: `PluginHost::loadPlugin("internal://vibesynth")` resolves
  without a file (the format claims identifiers before the filesystem check),
  filling a stable `Internal`-format instrument description.
- Audio: a direct note-on renders non-silent stereo output, and after note-off
  the release tail decays to silence (bounded within 120 blocks).
- State: the APVTS blob round-trips through
  `getStateInformation`/`setStateInformation` (gain value preserved).
- Project round trip through the DEFAULT (un-overridden) `pluginRestorer_`:
  save/load a project whose channel hosts VibeSynth; identity and state
  restore via `createFromDescription` like any VST.
- First-run seeding: `newProject()`/`initialise()` seed exactly one channel
  with the built-in instrument; the browser scan path
  (`settings.pluginPath`) is never overwritten with the internal identifier.
- Drag payload: `DragDropInfo` accepts the `internal://` scheme alongside
  absolute plugin paths.

New-session seeding also appears in `projectFileTests` (New resets to one
VibeSynth channel). Manual checks remain: browser "VibeSynth (built-in)"
entry, "+ Add Channel" auto-load, editor pop-out, and audible playback with
real hardware.
