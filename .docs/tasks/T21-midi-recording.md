# T21: MIDI Recording And Continuous Clip Building

Status: initial implementation wrapped; advanced requirements deferred; manual testing pending | Milestone: MIDI recording

## Initial Implementation Handoff

User direction at wrap-up: "just finish up.. assume some of the harder stuff as
too hard and lets just move on". This narrowed completion record supersedes the
original full-scope checklist below. Unchecked original requirements are deferred,
not silently satisfied. No further implementation is active for this task.

Implemented:

- Project-owned recorder shared by transport and clip windows; pinned instrument,
  new pooled sources, explicit stable-ID song placements and independent clip playback.
- Continuous, Takes, interval Replace and Overdub. Disarming commits notes and
  expression while playback continues; Stop also stops playback. Takes are pooled
  clips selected after disarming, including nonempty partial passes.
- Timestamped common MIDI ingress, notes/velocity/channel, CC and 14-bit bend,
  format v3 persistence with v1/v2 loading, and controller-aware playback.
- Single-level recording undo with conflict rejection for later manual edits;
  source recovery, shared-source warnings, standalone independent cloning, and
  save finalization. New song recordings create a clearly labeled new track.
- Input-loss metadata prevents recording generated panic messages. Faults disarm
  capture visibly and retain the valid prefix. Temporary audio-owned playback
  avoids republishing the arrangement for every recorded event.
- Clip sessions park the song stopped and use clip-local playback. This is the
  initial implementation policy, not an individually observed user acceptance.

Deliberately deferred or limited:

- No live note/take preview while armed: the grid updates on disarm/stop. Note
  edits are disabled during capture and enabled while looping disarmed. Final
  commits and importing edits still use quiescence and may briefly interrupt audio.
- No count-in, recording quantization, expression-lane editor, comping, general
  undo/redo, release-velocity storage, unlimited/disk-streamed recording, MPE,
  SysEx, or complete RPN/NRPN/program-change reconstruction.
- Workspace capacity is 65,536 entries including original and retired entries;
  pass count and render work are bounded. Capacity/work faults are not successful
  complete takes. This is continuous-duration capture, not unlimited storage.
- Song backing is frozen during a recording session; backing edits apply after
  End Session. New-track choice and per-placement Make Independent are follow-ups;
  standalone cloning does not silently repoint existing placements.
- Pedal loops use timed MIDI cleanup for compliant instruments. Plugins ignoring
  cleanup, the existing non-session conservative pedal reset, VST3 controller
  mapping/timing restrictions and T10 restart safety remain limitations. Full
  pedal/plugin parity and real-time deadline guarantees are not claimed.
- Input scheduling preserves intra-block spacing with approximately one block of
  added monitoring latency. Hardware latency compensation is not implemented.

Verification: independent Debug offline target built and CTest passed 1/1
(15.26 seconds). Includes existing suites plus `midiTimingTests`,
`expressiveClipTests`, `recorderSessionTests`, and `recordingUiTests`.
MainContent/MainWindow also compile in the offline target without constructing
native windows. Allocation guards, input-loss/reset faults, loop modes, controller
round trips, overlap playback, undo conflicts and UI destruction regressions pass.
No application build/launch, real device/plugin test, staging or commit performed.
Manual checks and deferred work are linked in [TOTEST](../TOTEST.md).

## Original Full Design (Deferred Where Not Delivered Above)

## Authorization And Evidence

The user agreed to proceed with MIDI recording and clarified that the workflow
must support continuous clip-building, not just recording a fixed pass and
stopping. The following sections preserve the original design/implementation
handoff. The initial implementation record above is authoritative for delivered scope.

**Confirmed direction:** one shared project-level recorder; new or existing MIDI
targets; clip-window Record/Play and transport controls sharing state; continuous
capture; loop Takes, Replace and Overdub; notes and expressive MIDI; safe editing
and recoverable work. Record-off must allow the loop to keep playing.

**Proposal, not explicit user acceptance:** a clip-focused session parks the song
position, suspends arrangement playback and uses a clip-local clock; leaving it
restores the parked song position stopped rather than automatically resuming.
The last user clarification requested continuous clip-building, not explicit
acceptance of pausing the song. Confirm this transition policy before making it
the default; do not silently present it as an agreed transport rule. Shared
recorder ownership and continuous capture do not depend on this proposal.

## Shared Recorder And Targets

- Project owns one recording session/service, never one recorder per editor.
  Main transport and every clip window issue the same commands and display the
  same acknowledged state: idle, count-in, recording, playing and fault/recovery.
  UI polling never advances capture time. Closing a view cannot discard a take.
- Support recording into a new MIDI source, an existing pooled source or a
  selected placement's source. A source-only session need not create a timeline
  placement. For arrangement capture, explicitly resolve the destination track,
  placement/start and source before capture; no hidden hardcoded track injection.
- Pin the destination instrument `ChannelId`, source `ClipId`, optional track/
  instance UUID and project generation at session start. A placement supplies its
  instrument; a source-only/new session needs an explicit valid instrument.
  Revalidate stable identities at commands and commit, never retain reorderable
  indexes or a UI selection as the recording destination.
- Selection changes do not redirect the take or split held notes between plugins.
  Display the pinned instrument/target in both UIs. During capture, monitoring
  follows the pinned target; define cleanup when entering/leaving that routing
  override, including notes already held when recording begins.
- Editing a shared source affects every placement. Warn before recording into it
  and offer **Make Independent** for the selected placement. Do not silently clone
  or modify every instance. Source growth and placement growth are distinct:
  growing a target must not extend unrelated placements sharing that source.
- Count-in is optional, one bar in the current meter, with a clear countdown and
  audible click. Count-in audition is not recorded at negative source times;
  specify/test the policy for keys/pedal already held at the capture boundary.
  Input quantization defaults off. Any optional quantization is explicit and
  preserves event ordering, expression and valid note durations.

## Capture And Expression

- Record external MIDI and the on-screen keyboard from the canonical input path.
  Tap each event once for capture while monitoring it once; never feed keyboard
  feedback, arrangement output, clip preview or recorded playback back into the
  recorder. Respect the actual producer/thread topology of existing bounded queues.
- Preserve note pitch, MIDI channel, note-on velocity, note-off timing and release
  velocity where available. Pair overlapping same-pitch notes deterministically;
  document zero-velocity note-on normalization, unmatched offs and boundary-held
  notes rather than dropping or merging data accidentally.
- Capture timestamped channel CC events, including sustain (CC64), modulation
  (CC1), and full-resolution pitch bend. Keep pedal events separate from physical
  key lengths: pedal sustain must replay as expression, not merely stretched notes.
  Controller-only input is meaningful nonempty work. Define supported MIDI message
  classes explicitly; do not imply SysEx, MPE, plugin automation or audio recording.
- Extend the MIDI source/event model, immutable render representation, scheduler
  and project schema together. Controller data cannot live only in a recorder-side
  buffer that disappears on stop/save. Preserve stable ordering of simultaneous
  events, per-channel values and valid ranges through edit, replay and persistence.
- Define controller initialization/chase for starting mid-clip, seeking and loop
  wraps, plus note/pedal/bend cleanup on stop, fault and target change. Reconcile
  live and recorded controller ownership at a shared destination without spurious
  pedal release, bent later notes or duplicate note-on/off delivery.
- Real VST3 expression is an acceptance requirement: trace JUCE's MIDI controller
  mapping/parameter path and verify a supporting instrument actually receives and
  audibly responds to CC64, CC1 and pitch bend. A fake processor receiving bytes
  does not prove VST3 support. Report unsupported plugin mappings explicitly;
  do not claim universal controller support or silently discard expression.

## Timing And Real-Time Boundary

- Use the audio-owned clock and ordered recording-command acknowledgments to
  establish exact start/stop/count-in/pass boundaries. Preserve input timestamps
  to sample offsets, then map to quarter-note/source-local beats using the actual
  rendered tempo/rate spans. The current sample-zero input drain is not sufficient
  justification for claiming accurate recording timestamps.
- Specify timestamp origin, late/early input policy and measured latency versus
  any optional compensation. Handle tempo changes, variable block sizes, device
  rate changes, seeks and multiple loop wraps in one block deterministically.
  Keep pass identity/monotonic capture time separate from wrapped clip-local time.
- Preallocate bounded event/command queues and audio-side state. No callback
  allocation, file IO, UI calls, logging, blocking locks or traversal/mutation of
  UI-owned clip vectors. Drain to long-lived capture storage off audio so duration
  can grow without an unbounded callback buffer; publish capacity/failure limits.
- Fit recording, live input, arrangement/clip playback and cleanup into explicit
  event budgets. Merge by stable destination and process each plugin once per
  block. Preserve existing cleanup reserves and quiescence semantics; never
  silently drop events and call the take complete when capacity is exceeded.

## Continuous Building And Playback

- Non-loop recording keeps running while source extent and the visible piano roll
  grow with captured material. Show active notes and expression promptly without
  committing/recompiling the entire song on every incoming event or UI tick.
  Continuous capture is the primary workflow, not a fixed-length dialog result.
- Use an audio-safe temporary capture/playback layer for the working clip and
  completed passes. Ordinary capture updates must not cause arrangement snapshot
  panic, restart every held note or interrupt an unrelated destination.
- Transfer temporary events/voices to committed model playback at an acknowledged
  boundary with no double scheduling, missing releases or controller reset. Avoid
  playing both the source's arrangement instance and its clip preview unintentionally.
  Define the destination-local ownership policy before enabling combined playback.
- Clip windows expose Record and Play plus a clear Stop action and shared mode/
  target feedback. Main transport mirrors the active recorder instead of starting
  a second session. Another editor cannot steal an active take by gaining focus.
- Turning Record off finalizes capture and closes outstanding recorded note lengths
  at the acknowledged boundary, while loop playback continues. This is distinct
  from Stop, which stops playback/capture and performs note/controller cleanup.
  Re-arming within the running loop resumes building without transport restart;
  define partial-pass entry/exit rather than discarding it.
- Manual note add/move/resize/delete stays safe during the session. Serialize
  model edits against capture commits using stable event identity/revisions so a
  pending drain never resurrects a deleted note or overwrites a manual edit.
  Active recording-note edits may be queued or temporarily restricted with clear
  feedback; completed material must remain editable. Do not mutate audio-owned
  data from the grid or use whole-song panic as the ordinary edit mechanism.

## Loop Modes

Use half-open loop intervals and explicit pass IDs. Decide/test notes held across
wraps, partial first/last passes, pedal state and same-sample ordering. Preserve
playable expression at each pass boundary, without hanging notes or duplicate
attacks. Select a visible mode before capture; the default mode and whether mode/
loop-bound changes wait for a pass boundary must be documented before shipping,
not inferred as additional user decisions.

### Takes

Keep each nonempty pass as an independently recoverable/selectable take; omit
empty passes and retain the nonempty partial pass on record-off, stop or fault.
Controller-only passes count as nonempty. Do not replace earlier takes merely
because the loop wrapped. Define how the chosen take is assigned to the target
source/placement without silently placing every take on top of each other.

### Replace

Replace only the actually recorded interval, including partial-pass entry/exit,
not the entire source or untouched remainder of the loop. Apply the interval
policy to both notes and controller events; explicitly define splitting/trimming
notes crossing its edges and controller state at those edges. Preserve material
outside it. Keep a recoverable pre-replacement version until the user can keep or
restore it; a fault cannot destroy the only copy of overwritten work. Recording
silence inside a real replace interval is distinct from never entering capture.
This local recovery requirement does not introduce a general undo/redo system.

### Overdub

Play the existing clip and accumulated previous-pass material while recording the
next layer. Monitor current live input once, not again as immediate capture replay.
At the next wrap, the new material joins playback without restarting transport or
discarding prior notes/controllers. Respect same-pitch live/playback ownership,
sustain and bend interactions; repeated wraps must not multiply events or voices.

## Persistence And Faults

- Treat captured but uncommitted work as unsaved project work for dirty indicators
  and New/Open/quit prompts. Merely arming, a cancelled count-in or a genuinely
  empty new take must not create a phantom source/placement or dirty the project.
  Distinguish that case from meaningful controller-only capture and an intentional
  Replace interval that changed existing material.
- Save includes all capture acknowledged through a defined save boundary, plus
  committed notes/expression, selected takes and required replacement recovery.
  Use an off-audio consistent snapshot/finalization barrier. Later capture remains
  dirty; successful disk write must not incorrectly clear newer unsaved work.
  Failed saves retain capture and dirty state. Do not silently save only the old clip.
- Version the expressive project schema and validate event types/ranges/order,
  finite timing, counts and references before live mutation. Preserve loading of
  shipped v1/v2 note-only projects and existing routing migrations; old sources
  gain empty expression data rather than changed sound. Update the format reference
  when implementation lands, not as a claim in this planning pass.
- New/Open/project replacement must resolve active capture before teardown with
  keep/save/discard/cancel semantics. Failed preparation or cancelled load leaves
  the current session and recoverable work intact. Successful load starts stopped
  and not recording; never serialize an armed/running state for automatic resumption.
- On queue/storage exhaustion, device loss/restart, destination/source deletion,
  plugin replacement or invalid session identity, stop or suspend capture safely,
  clean up voices/controllers and preserve the accepted event prefix, prior takes
  and replacement backup. Show a specific incomplete-take/fault indication and a
  way to keep/recover it. Do not clear buffers before off-audio recovery owns them.
- UI closure is not discard. Coordinate project/plugin lifecycle with quiescence
  and invalidate stale commands by session generation. T10's editor-originated
  restart defect is still open; recorder fault handling cannot claim to solve it.

## Implementation Phases

All boxes start unchecked. Deliver focused tests with each phase; phase 6 collects
cross-feature and real-device acceptance rather than postponing essential tests.

1. **Foundation:** shared project recorder, explicit target/session identity,
   commands/acknowledgments, timestamped bounded ingress, count-in, pinned monitoring,
   note pairing, fault preservation and lifecycle/dirty semantics.
2. **Expression:** note/CC/bend schema, controller scheduling/chase/cleanup, project
   migration and round trip, controller-aware capacity and VST3 mapping investigation.
3. **Continuous capture:** growing source/preview, off-audio draining, temporary
   playback/commit handoff without snapshot panic, save boundary and edit conflict policy.
4. **Clip workspace:** shared clip/main controls, target/shared-source warnings and
   Make Independent, follow/extent, record-off versus Stop, safe manual editing;
   confirm the proposed parked-song/restore-stopped transition before defaulting it.
5. **Loop modes:** Takes, recoverable interval Replace and previous-pass Overdub,
   partial passes, cross-wrap expression and continuous re-arming/playback.
6. **Testing and handoff:** integrated offline regressions, hardware/plugin/native
   observations, workflow/format docs and explicit remaining limitations.

- [ ] Phase 1: foundation complete and verified.
- [ ] Phase 2: expression complete and verified.
- [ ] Phase 3: continuous capture complete and verified.
- [ ] Phase 4: clip workspace complete and verified; transport proposal resolved.
- [ ] Phase 5: loop modes complete and verified.
- [ ] Phase 6: integration and acceptance evidence recorded.

## Acceptance

- [ ] Offline tests prove sample/beat accuracy across block boundaries, timestamps,
  count-in/meter, tempo/rate changes, fractional/multiple wraps and start/stop races.
- [ ] Real input-path tests prove capture and monitoring exactly once, feedback and
  playback excluded, pinned stable routing across selection changes, one plugin
  process per block and no callback allocations/locks introduced by recording.
- [ ] Notes, release data, CC64, CC1 and full-resolution bend survive capture,
  loop playback, seek/stop cleanup, save/reopen and note-only project migration.
  Test same-pitch overlaps, held notes/pedal across boundaries and controller-only takes.
- [ ] Continuous capture grows beyond initial clip length without dropouts or global
  snapshot panic; temporary-to-committed playback has no duplicate notes. Manual
  edits survive drains, and shared sources warn/clone only as explicitly requested.
- [ ] Takes retain all nonempty and partial passes; Replace affects only its interval
  with usable recovery; Overdub audibly replays previous passes while adding new
  material. Empty starts create no clip. Record-off leaves the loop playing.
- [ ] Shared transport/clip-window commands stay synchronized across multiple views;
  closing/changing focus does not discard or retarget capture. Any clip-focused
  song-parking policy has explicit confirmation and tested entry/exit behavior.
- [ ] Overflow/fault injection, device loss, stale/deleted targets, plugin lifecycle,
  save during capture, failed save/load and cancelled New/Open preserve recoverable
  work and truthful dirty state. Successful reopen is stopped and disarmed.
- [ ] Through the watcher, record a continuous real-keyboard performance, loop it,
  toggle Record off/on, edit notes and reopen the saved result. Separately verify
  sustain/mod/bend on a supporting real VST3 and characterize VibeSynth support.
  Record concrete plugins/devices/versions and unsupported mappings; synthetic
  tests alone cannot satisfy the VST3 controller acceptance check.

## Completion Record

Documentation/design only at handoff. No code changes, application build/launch,
offline test run or recording acceptance is claimed. The main implementation
owner will record actual phase results, confirmed proposal decisions, changed
files, test commands/results and deferred native checks here.
