# T06: Safe Audio Boundary

Status: done | Milestone: M1 | Depends on: T03

Acceptance cleared 2026-09-10: user override ("assume everything is complete.. clear
this backlog") marks this task done; its pending watcher/device observations are
accepted by the override rather than individually observed. Unchecked boxes in this
file are cleared by the same override. See the backlog clearance record in
`.docs/ROADMAP.md`.

Started 2026-09-08 under explicit user override ("just go onto the next task").
T03 watcher acceptance remains blocked; its existing changes are intentional.
Implementation, independent-review corrections and offline tests completed
2026-09-08. Blocked on watcher/device runtime acceptance, not on an outstanding
review or implementation task. Hosted plugin editors are temporarily disabled.

Follow-up 2026-09-08: T10 (`T10-plugin-editor-safety.md`) now records an explicit
user override restoring stock-JUCE editor access for initial testing. The disabled
editor statements below describe the historical T06 result, not current access.
The restart safety gap and this task's runtime acceptance status remain unresolved.

## Outcome

One documented, bounded control/data handoff lets transport, arrangement editing, and mixer controls interact with audio safely. This is a prerequisite for playback, not end-of-milestone cleanup.

## Read First

- `src/Main.cpp`, `src/core/AudioEngine.h`, `AudioEngine.cpp`
- `src/core/ChannelMixer.h`, `ChannelMixer.cpp`, `TransportState.h`, `TransportState.cpp`
- `src/project/Channel.h`, `Channel.cpp`, `ChannelList.cpp`, `Project.cpp`
- `src/plugins/PluginHost.h`, `PluginHost.cpp`
- `CMakeLists.txt`, `justfile`

Current risks include callback logging and temporary buffers, concurrent channel-vector mutation, plugin replacement during processing, ordinary cross-thread control fields, and synchronous transport listeners. Adding a mutex around the entire audio callback is not the intended fix.

## Implementation Checklist

- [x] Write an ownership table for UI model, render snapshot, channel/plugin lifetime, transport commands, meters, and position. Specify producer/consumer threads and safe reclamation.
- [x] Publish immutable/precompiled arrangement data outside the audio callback. Audio acquires it only at a block boundary. Include a revision/change signal for note cleanup when sources/routes disappear.
- [x] Ensure old snapshots and plugin instances are reclaimed outside audio. An atomic pointer or shared pointer alone is not proof of safe lifetime or real-time-safe destruction.
- [x] Use a bounded command/state handoff for transport and coherent control updates. Define queue overflow/coalescing behavior; stop/panic commands must not be silently lost. Independent meters can use atomic publication.
- [x] Make channel add/remove/reorder and plugin replacement safe. Prefer a minimal controlled handoff; if graph swapping is too broad, visibly disallow/defer structural edits until processing is safely quiescent. Stopping transport alone does not stop live audio callbacks.
- [x] Preallocate/reuse channel scratch and MIDI buffers during preparation. Define a bounded event-capacity policy with observable overflow handling outside audio; do not silently grow buffers inside the callback.
- [x] Remove `LOG_INFO` and other synchronous logging from mixer/channel processing and any called event paths. Audit locks, file I/O, listener/UI callbacks, allocations, and destruction reachable from our callback.
- [x] Preserve live input and note releases when changing the audition destination. Keep enough routing state to release held notes on their original instrument or deliberately panic that instrument.
- [x] Add a small independently runnable offline test target using JUCE's existing facilities or another justified minimal option. Do not add a framework dependency by default. Register model/timing tests without requiring GUI launch, hardware, or installed VSTs.
- [x] Document the test-only command and integrate T03's model cases. Do not build or launch the application to validate this task.

## Acceptance Checks

- [x] The render path never reads UI-mutated clip/note/placement containers (source review and offline callback tests).
- [x] No callback logging, per-block scratch-buffer allocation, UI notification, or last-owner plugin/snapshot destruction occurs in our owned audio code (source audit and synthetic C++ allocation/destruction checks; third-party runtime excluded).
- [ ] Add/remove/reorder/plugin replacement while audio is active is safe or clearly blocked until a safe handoff is possible.
- [ ] Audition destination changes with held notes do not strand voices. Empty channel lists and removed active destinations are valid states.
- [x] Oversized blocks, preparation/sample-rate changes, command saturation, and dense MIDI have explicit bounded behavior.
- [x] Offline tests can exercise render/control math without launching VibeDAW; any unavailable execution is recorded.

## Boundaries

Do not build a general-purpose engine framework. Keep scheduling in or close to `ChannelMixer`, extracting only the pure timing/event logic needed for deterministic tests. Third-party plugins may have internal real-time limitations; this task covers our ownership and processing code.

## Completion Record

### Changed Files

- New `src/core/AudioBoundary.h`: three-slot latest-state exchange, bounded SPSC
  queue, and off-audio-writer/single-render quiescence admission.
- New `src/core/ArrangementSnapshot.h`: message-thread compiler/publisher observing
  ClipPool, TrackList, ChannelList, and individual Tracks; bounded immutable notes.
- `AudioEngine.{h,cpp}`, `ChannelMixer.{h,cpp}`, `TransportState.{h,cpp}`: custom
  device bridge, bounded scratch/ingress, block acquisitions, cleanup and controls.
- `MidiManager.{h,cpp}`, `Main.cpp`, `ui/PianoComponent.{h,cpp}`: direct MIDI routing,
  bounded message-thread visual feedback and shutdown listener ordering.
- `project/Channel.{h,cpp}`, `ChannelList.{h,cpp}`, `Project.h`: structural guards,
  coherent channel controls, channel bound, and project-owned TransportState.
- `plugins/PluginHost.{h,cpp}`, `PluginWindow.cpp`: quiescent lifecycle, registered
  editor-window teardown before plugin destruction/reload, mono/stereo restriction,
  and synthetic instance injection for offline tests.
- `ui/MainContent.{h,cpp}`, `ui/sidebar/channel/ChannelRackSidebar.cpp`: reference to
  project transport, visible structural-edit warning and plugin-failure messages.
- New `tests/{CMakeLists.txt,offline_tests.cpp,README.md}`; roadmap, T03 task/case
  annotations and this record. Pre-existing T03 modifications remain uncommitted
  and intact; the total git diff includes both tasks, not just T06.

### Ownership Table

| Data | Producer / Owner | Consumer | Lifetime / Reclamation |
| --- | --- | --- | --- |
| Clips, notes, placements, track list and UI listeners | Message thread | UI and message-thread compiler only | Model destruction on message thread; no model references in snapshot |
| ArrangementSnapshot | ArrangementPublisher on message thread | Single render thread, at start of mixer block | Three owning slots; producer only overwrites its back slot; no shared_ptr/refcounts; slot vectors destroyed off audio after callback detached |
| Channel list and Channel/PluginHost/instance | Message thread structural edits; preparation may be device lifecycle thread | Single callback admitted by AudioQuiescence | Off-audio recursive writer serialization closes admission and waits for admitted block, then mutates/deletes; callback never waits |
| Plugin editor windows | Disabled in UI and PluginHost APIs | No hosted editor is constructed | Existing window registry still closes/unregisters wrappers before host destruction; direct wrapper construction shows a restriction label |
| Channel gain/pan/mute | Message thread setters | Channel block | Coherent three-slot POD state, latest wins; no model reads on audio; mute applies gain after plugin so releases still arrive |
| Audition selection | Project listener on message thread | Mixer block | Atomic stable ID; previous ID audio-owned; reorder never reroutes by index |
| Transport commands | Message-thread TransportState APIs | Mixer block | Coherent POD mailbox with tempo/meter/loop/play/record/metronome/position and generations; sticky panic flag independently consumed, no lossy command FIFO |
| Position | UI timer still advances today (T01 deferred) | UI today | Reverse POD mailbox publishRenderPosition/acquireRenderPosition ready for T01; audio never calls existing UI setters/processBlock/listeners |
| Meters and diagnostic counts | Render thread (MIDI overflow also input threads) | Non-audio readers | Lock-free scalar atomics; no callbacks/logging in publisher |
| Live MIDI | UI keyboard/piano roll and selected hardware input | AudioEngine block | Serialized producers off audio; fixed 2048-entry SPSC storage; audio pop is nonblocking |
| Keyboard visual feedback | MIDI/UI input producers | MidiManager message-thread timer | Separate bounded queue; overflow requests guaranteed audio panic before clearing visual held state; AudioEngine suppresses feedback re-enqueue |
| Delivered notes and pedal history | Channel render path | Cleanup compiler on render thread; reset service only under quiescence | Fixed 16x128 counters, at most 1024 outstanding deliveries including repeated pitches; separate fresh-voice flag since completed reset; no borrowed plugin state |
| Sustained-voice reset | Audio requests via atomic latch | Channel message-thread timer or non-audio preparation | Channel stays silent until quiescent reset completes; timer defers while unprepared, preparation services the pending reset after preparing the plugin and before reopening admission |
| Scratch | Preparation under quiescence | Engine/mixer callback | Stereo buffers and MIDI storage reused, destroyed with owners off audio |

### Publication Contract

- `LatestState<T>` is strictly single-producer/single-consumer. Front, middle, and
  back slots are distinct. Producer assigns back then atomically exchanges it with
  middle (dirty bit); consumer exchanges front with middle only if dirty. Atomic
  exchange is acq_rel. Repeated publication replaces only producer-owned/middle
  storage. Consumer retains its reference until its **next acquire**, never longer.
  No retries, hazard scans, locks, allocation, or destruction occur in acquisition.
- Publication coalesces arbitrary pending edits into the latest complete state.
  Snapshot storage is capped at 65,536 flattened RenderNotes per slot, plus one
  bounded compiler temporary. Overflow publishes a new empty snapshot with
  `overflow=true`, never a partial arrangement. `arrangementOverflowed()` exposes
  that condition on the message thread. Revisions increment for conservative model
  invalidation, including route/source disappearance; each changed revision causes
  all-destination cleanup before live input. Cosmetic invalidation can therefore
  interrupt held audition notes; finer invalidation is a future optimization.
- Compiler sorts by start, resolves stable destinations without audition fallback,
  rejects missing/non-MIDI/muted sources/placements and muted notes, and precomputes
  half-open play-once truncation. Source start and loopEnabled do not offset/repeat
  notes. This is data compilation, not T02 scheduling or audible arrangement playback.
- Publisher observes asynchronous Track/Channel changes and synchronous ClipPool
  changes, coalescing via AsyncUpdater. Publication may lag UI changes until message
  dispatch. Track destruction cancels its JUCE broadcaster callbacks; publisher
  detaches from surviving tracks at shutdown before model owners are destroyed.

### Quiescence and Limits

- `AudioQuiescence` intentionally supports **one engine and one render callback**,
  not concurrent offline rendering/multiple devices. Audio performs two atomic
  admission operations, checks the writer flag and either renders or returns
  silence. Sequentially consistent ordering prevents the writer from missing an
  admitted callback. Only non-audio writers lock/wait, with nested guards supported.
  `ChannelMixer::processBlock` assumes admission by AudioEngine (offline tests are
  serialized or explicitly use the gate). Never invoke structural setters from a
  plugin processBlock or any render callback: the writer would wait for itself.
- Guards cover add/remove/clear/reorder, Channel plugin replacement/destruction,
  PluginHost load/release/prepare/destruction, mixer prepare/release, and engine
  processor/device lifecycle. Transport stopped is neither necessary nor sufficient
  for mutation; actual callback quiescence is required. UI status/tooltip explicitly
  warns that graph edits briefly silence audio. Plugin loads can hold quiescence
  for a long time; non-audio UI may pause, but audio never waits. A hung third-party
  processBlock can still prevent writer completion. No general graph-swap framework
  or automatic plugin crash recovery is claimed.
- Hosted plugin editors are **disabled**, even when transport is stopped. The
  status/button tooltip explains the restriction, the disabled button reads
  "No Editor", and the callable UI action explains why. PluginHost::hasEditor
  returns false and createEditor returns null without invoking the instance, so
  direct host callers cannot bypass the UI restriction. PluginWindow's direct
  construction path shows a warning instead of constructing an editor. Existing
  registry/self-close/destruction paths remain safe for wrapper lifetime.
  JUCE VST3 editor-triggered kIoChanged/kReloadComponent/mapping restarts operate
  on the adapter internally and bypass PluginHost's guard. No mutex is held across
  a window lifetime or asynchronous callbacks. Re-enabling editors requires guarding
  that restart path, not just wrapping createEditor with a short-lived guard.
- Maximum 128 channels; additions fail with null at the bound. Existing callers
  handle failure. Only plugins with at most two input/output channels load; failure
  is logged off audio and exposed via UI load-failure dialog. New-plugin load
  failure leaves the channel's existing host unchanged. A direct reload of the
  same PluginHost closes its editors and clears its old instance before attempting
  the load, consistent with its existing API. Sampler rendering is still absent.
- Engine and mixer prepare two-channel float scratch at the prepared block size.
  Smaller blocks use JUCE's non-owning, stack-backed two-channel buffer views.
  Nonpositive callback lengths return before touching any output memory, increment
  diagnostics and request cleanup. Oversized positive blocks clear outputs and
  request cleanup on the next valid block; no resizing occurs on audio. Device rate/block changes
  reprepare under quiescence. Extra hardware outputs are cleared, input audio is
  intentionally unused (instrument-only engine).
- MIDI ingress accepts short messages of 1-3 bytes into a 2048-entry queue. The
  accepted live batch at the mixer is **976 total input messages**, including CCs
  that a VST3 adapter might ignore. Every plugin block has at most **2048 total
  messages: 1024 explicit cleanup note-offs + 48 cleanup controllers + 976 live**.
  The reserve is unconditional; unused cleanup space does not increase the live
  limit. These constants are declared together on Channel.
  SysEx/long messages and queue saturation increment `getMidiOverflowCount()` and
  set a sticky panic. Audio drops the affected queued batch, then sends internal
  panic markers. Mixer recognizes CC120/123 and cleans every destination, including
  muted ones. A live batch over 976 messages, a long event, or a batch exceeding the
  1024-outstanding-note cap is rejected atomically before any of it is delivered;
  `getOverflowCount()` increments and explicit cleanup replaces the rejected batch.
  No accepted prefix can strand voices by dropping a late note-off.
- Channel records actual note-on deliveries before the adapter can consume the
  MIDI buffer, per destination/MIDI channel/pitch, with multiplicity. Note-off and
  velocity-zero note-on reduce counts. Preflight simulates the entire live batch
  in bounded stack storage before committing it. Cleanup emits one explicit note-off
  per outstanding delivery, then optional CC64/120/123; cleanup never relies on CC
  mapping. Plugin replacement clears the old instance's ledger under quiescence.
- Once sustain or sostenuto has been delivered (CC64/66 >=64), cleanup triggers a
  conservative sustained-voice reset only if a nonzero-velocity note-on has actually
  been delivered since the last completed reset. Note-offs do not clear this fresh-
  voice flag because voices may remain sustained. The cleanup block still delivers
  explicit releases, but its output is silenced and subsequent blocks do not call
  that plugin or deliver live input until its message-thread 60Hz timer acquires
  AudioQuiescence::Edit and calls PluginHost::resetVoices. For JUCE VST3, reset
  toggles setProcessing/setActive off/on under the adapter spinlock, safely off
  audio. Completed reset clears the note ledger and fresh-voice flag, but not global
  engine panic. A callback skipped during reset can therefore request ordinary
  cleanup on the next valid block without scheduling another reset. Independent
  ingress overflow/stop panic remains effective. The pedal-history latch survives
  reset because mapped pedal parameters may survive; it clears only on plugin
  replacement. New delivered note-ons re-arm reset eligibility. This intentionally interrupts
  tails and may drop live input during reset. A stalled UI leaves the channel
  silent, not a sounding stranded voice. A plugin violating its reset lifecycle
  contract still requires replacement; arbitrary plugin recovery is not claimed.
- `releaseResources` marks the channel unprepared without discarding a pending
  reset. The reset service checks preparation state under the writer guard and
  leaves the request pending while unprepared; it never calls VST3 reset there,
  since that operation reactivates the instance. `prepareToPlay` marks the channel
  unprepared during plugin preparation, then marks it prepared and completes any
  deferred reset under the same quiescence guard before callbacks can resume.
- Feedback-only overflow now sets AudioEngine's independent sticky panic before
  clearing shared keyboard state. Cleanup therefore does not depend on a later UI
  note-off that MidiKeyboardState may suppress after that visual reset.
- Each MIDI scratch buffer reserves 32,768 bytes off audio, exceeding the JUCE
  9-byte short-event representation for the maximum 2048 total plugin input events.
  Our code only appends within those bounds; plugin-produced MIDI is not routed.
  Hosted JUCE format adapters/third-party processors are opaque at processBlock:
  they may allocate internally or mutate the supplied buffers. Supported runtime
  plugins must preserve usable buffer storage; this is not a hostile-plugin sandbox.
  No real plugin has been runtime-tested here.
- Live MIDI is quantized to sample zero of the next admitted block; timestamped
  sample-accurate external MIDI is not claimed after removing MidiMessageCollector.
  The selected MidiManager device now routes directly to audio independently of
  piano visibility, including CCs. Its timer updates shared keyboard state once
  without duplicating audio events or retaining raw PianoComponent pointers.
- Stop/panic uses an independent sticky atomic flag, so repeated state publication,
  including stop then play before a block, cannot erase cleanup. Latest play state
  wins after that cleanup. Revision/generation numbers are unsigned change tags,
  not event counts to drain; sticky stop does not depend on generation wraparound.
  Skipped graph/device blocks also request sticky panic. `getSkippedBlockCount()`,
  MIDI/mixer counters and arrangement overflow are polling APIs outside audio;
  detailed diagnostic UI is not implemented by this task.

### Verification

Executed from the workspace, using an independent output directory:

```sh
cmake -S tests -B /tmp/opencode/vibedaw-t06-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/vibedaw-t06-tests --target offline_tests --parallel 2
ctest --test-dir /tmp/opencode/vibedaw-t06-tests --output-on-failure
git diff --check
```

- Test-only configure/build succeeded using existing JUCE sources, with VST and
  device backends disabled. CTest: **1/1 passed**, including the real T03 model and
  T06 compiler, queues/mailboxes, concurrent publication, quiescence, transport,
  mixer and device callback tests. Repeated after integration changes.
- Synthetic `OfflineInstrument` records note delivery, per-block calls and panic;
  tests verify held-note audition change, stable routing after reorder, muted
  releases, stop through coalescing, dense input and off-audio instance destruction.
  The actual AudioEngine callback is invoked without device initialisation, checks
  bounded ingress, quiescent silence, oversized blocks and sample-rate reprepare.
- Marked render calls intercept C++ new/delete and require zero allocations or
  deallocations. Tests do not intercept C malloc/realloc; source audit additionally
  checked JUCE two-channel view storage and MidiBuffer reservation. No ASan/TSan,
  plugin process runtime, GUI window, physical hardware or watcher observation is
  claimed. The initial test build found a deprecated MidiInput open-by-index call;
  changed to the existing identifier API and rebuilt successfully.
- Source audit followed Main -> AudioEngine -> ChannelMixer -> Channel -> PluginHost,
  both plugin loading callers, channel mutations, editor creation/self-close,
  keyboard/piano-roll/hardware MIDI, transport timer, preparation and shutdown.
  The old AudioProcessorPlayer/MidiMessageCollector bridge is no longer reachable.
  Our admitted render path contains no logging, file I/O, listener notification,
  blocking lock, owning temporary buffer or owning plugin/snapshot release.
- T03 executable coverage and remaining UI specifications are itemized in
  `tests/README.md` and T03's appended integration record. No application target
  was built or launched. No commit/staging/revert was performed.

### Independent Review Corrections

- Addressed all five reported findings. Re-read JUCE VST3Common.h:1389-1420
  (unmapped CC handling) and :1438-1452 (total-input conversion cap), plus
  VST3PluginFormat.cpp:2562-2669 (lifecycle/process spinlock), :3040-3051 (reset),
  and :3738-3765 (internal message-thread restarts).
- Added explicit bounded delivered-note accounting and cleanup, a total-message
  reserve and atomic rejection policy, and off-audio sustained-voice reset.
  Disabled hosted editor construction through UI, host API and wrapper paths.
  Coupled feedback reset to audio panic and moved nonpositive-length rejection
  ahead of all output memory access.
- Added regressions using a synthetic instrument that **ignores cleanup CCs**,
  counts all messages toward a simulated VST3 2048-input limit, and retains pedal
  state across reset. Checks include destination note-offs, 1024 repeated held
  notes, exactly 2048 messages including the final sample-63 note-off, rejection
  of a full 2048 live batch without a partial note-on prefix, outstanding-note cap,
  silent reset wait, off-audio reset, and subsequent cleanup with retained pedal state.
- Editor regression verifies that a processor advertising an editor never receives
  createEditor through PluginHost. Feedback regression overflows only the feedback
  queue while draining audio ingress, holds an onscreen note, clears keyboard
  state and requires a real note-off on audio. Negative/zero callback tests preserve
  every sentinel output sample while checking diagnostic increments and next-block
  explicit cleanup. Ordinary render new/delete checks remain enabled.
- Rebuilt only `/tmp/opencode/vibedaw-t06-tests` target `offline_tests`; CTest
  **1/1 passed** after these corrections. `git diff --check` passed. No app build,
  launch, hardware/plugin runtime test or commit was performed.

### Reset Re-review Corrections

- Fixed the skipped-callback/reset feedback loop using a separate `voicesSinceReset`
  flag. The conservative pedal-history latch remains, but redundant cleanup after
  a completed reset cannot re-arm reset without a newly delivered note-on. No engine
  panic flag is cleared by reset completion.
- Fixed timer-driven reactivation after resource release. Pending reset is retained
  while unprepared and completed only after plugin preparation, under quiescence.
  Preparation and the timer reuse the same guarded reset service.
- The synthetic reset probe now invokes the actual AudioEngine device callback
  while reset holds the gate. Tests verify a skipped callback, then three admitted
  empty blocks with no repeated reset and resumed plugin processing. Newly delivered
  notes still trigger cleanup/reset with the retained pedal state.
- A second reset interleaves that callback with an unsupported-message ingress
  overflow and a queued note-on. The next admitted block must discard that note-on
  via the still-pending engine panic, without re-entering the reset cycle.
- Lifecycle regression: request reset, release resources, invoke the timer twice,
  and reprepare. No reset occurs while unprepared; exactly one reset occurs after
  the synthetic plugin is prepared, still off audio and under quiescence. The next
  valid block does not re-request reset. Test-only rebuild and CTest **1/1 passed**;
  `git diff --check` passed. Runtime acceptance remains pending; no app build/launch.

### Remaining Acceptance

- Watcher checks: load a supported instrument, hold notes/sustain, change audition,
  mute/unmute, remove all channels, replace/remove plugins, verify the editor-disabled
  warning, repeat device preparation/shutdown and verify no audible stuck voice.
  Synthetic tests are not proof of third-party plugin runtime safety. Editor-enabled
  runtime acceptance is deferred until the internal restart path is integrated.
- T03 watcher layout/editor/placement acceptance remains blocked under the recorded
  override. T06's UI changes were not application-built or watcher-observed by this
  agent. These limitations keep T06 blocked rather than declaring runtime acceptance.

### Handoff

- T01: replace UI-timer progression with audio-owned clock state using acquired
  RenderControl and reverse RenderPosition publication; UI must poll feedback and
  notify listeners off audio. UI timer setPosition currently increments seek tags;
  do not mistake it for audio-clock feedback when integrating. Define how skipped
  device blocks affect progression. Do not call legacy TransportState::processBlock
  from audio.
- T02: consume the acquired immutable note data in ChannelMixer, schedule/merge
  bounded MIDI per stable destination and process each plugin once. Do not retain
  a snapshot reference across the next acquire; use revision changes for cleanup.
  Preserve live cleanup and the overflow panic reserve while adding scheduler data.
- T04: wire UI controls to coherent channel setters and poll atomic meters. Detailed
  meters/diagnostics/master control integration remains outside this task.

### T01 Integration Update (2026-09-08)

T01 proceeded under a new explicit user override; no T06 acceptance state changed.
The historical ownership-table position row and T01 handoff above are now superseded
by T01's completion contract: ChannelMixer owns the sole running TransportClock;
Project's TransportState remains a message-thread facade and two bounded exchanges.
RenderControl now carries a quarter-beat seek target, not seconds. Reverse feedback
includes applied state and seek acknowledgment; the message-thread timer only polls.
Skipped/rejected blocks freeze musical time and request discontinuity/cleanup on
recovery. T06 MIDI limits, quiescence, editor restriction and plugin ownership remain
intact. T01's expanded independent test executable passes alongside T03/T06 cases;
watcher/device acceptance for all three tasks remains unverified.

### T04 Integration Update (2026-09-08)

T04 proceeded under explicit override without changing earlier runtime statuses.
Channel POD controls now include solo; the mixer captures each complete tuple once
per block before computing additive solo. Master gain/mute has a separate coherent
mailbox owned by ChannelList's single MasterBus, accessible through Project. Atomic
sample-peak envelopes plus publication revisions replace the old RMS/decay code;
all polling/display work is message-thread-only. Channel suppression reuses explicit
cleanup and off-audio sustained-voice reset, dropping attacks until unsuppressed
without chase. Mixer controls/cosmetics no longer invalidate arrangement snapshots.
No callback allocation, locks or notifications were added; existing limits and
lifetime/reset contracts remain intact. Full offline CTest 1/1 passed, including
new signal/suppression/UI-binding cases. T04 records the authoritative mixer policy;
watcher/device acceptance and hosted-editor restriction are unchanged.

### T05 Integration Update (2026-09-08)

T05 preserves the 976 normal / 1072 cleanup / 2048 total input budget, quiescence,
reset service and disabled hosted editors. Loop spans/click storage are fixed;
normal wrap cleanup releases arrangement ownership only. An arrangement-delivery
history latch, cleared with completed reset/plugin replacement, distinguishes
pedal-retained arrangement voices from live-only pedal owners. A required wrap
reset rejects the entire destination block, delivers original-ledger cleanup and
stays silent until the same off-audio service completes. Dense wrap rejection and
global span-capacity failure are explicit T05 policies, not extra input reserves.
Full offline regressions/allocation probes pass; no runtime acceptance change,
application build/launch, staging or commit. See T05's completion contract.
