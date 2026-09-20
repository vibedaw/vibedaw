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

### M1.5: Direct Manipulation and Discoverability

Tasks T10-T14 address user feedback from 2026-09-08. The initial placement toolbar
was a bootstrap workflow, not the intended long-term interaction model. Next,
make instrument setup, clip placement, editing, and sidebar recovery intuitive:

1. Open and configure the selected channel's plugin through a restored Plugin button. Initial-testing access is explicitly authorized despite the unresolved restart boundary (T10).
2. Drag a browser plugin onto empty Channel Rack space to create and activate a channel; clearly distinguish this from replacing an existing row's plugin.
3. Create a clip without opening Piano Roll; explicitly edit by double-click or context action.
4. Drag clips onto existing timeline tracks or onto a previewed new track in unused space. Move placements directly without selecting a tool or pressing Move.
5. Collapse Browser and Channel Rack into one compact left-side tab rail, with predictable independent reopening and no reserved blank columns.
6. Use target-specific context menus and keyboard actions instead of the Place/Move/Delete/Assign toolbar workflow.

Status 2026-09-10: all M1.5 implementation (T10-T14, T16) is complete and
user-accepted via the backlog clearance override recorded below. The T10
restart-safety gap (editor-originated plugin restarts bypass the quiescence
gate) remains a known limitation accepted by that override, not a closed defect.

### M2: Keep Your Work and Improve Editing

T07 adds explicit project save/load, including plugin state, immediately after the interaction milestone so sketches survive a restart. T08 fixes editor navigation and alignment; promote any editor defect that prevents writing a usable clip into the current task. T09 closes integration/documentation gaps across playback, direct manipulation, and persistence, not a deferred bucket for essential tests.

### Later: Mixer Routing, Samples and Recording

Backlog only, not dependencies of M1: actual sampler rendering and sample placement, MIDI recording into clips, audio recording, automation, effects/sends, undo/redo, clip-local repetition, advanced trim/stretch tools, plugin tempo/playhead reporting, and complete device/sidebar settings restoration. Basic clip dragging and sidebar collapse/reopen behavior are now M1.5, not backlog. Create detailed tasks when remaining features become next in line.

T15's post-M2 discussion selected independent mixer channels, many-to-one instrument
audio routing, one initial mixer channel, explicit add/remove, and direct-Master
defaults. Implementation and offline verification are complete; native/audible
acceptance remains pending. Bus chains, sends/effects and sidechaining are deferred.
T12's per-instance instrument routing contract is unchanged.

## Task Index

Task numbers retain the original discussion's IDs; execute in the order below, not numeric order.

| Order | Task | Milestone | Status | Depends On |
| --- | --- | --- | --- | --- |
| 1 | [T03 Routing, Timing, and Placement](tasks/T03-routing-timing-placement.md) | M1 | done | None |
| 2 | [T06 Safe Audio Boundary](tasks/T06-safe-audio-boundary.md) | M1 | done | T03 |
| 3 | [T01 Audio-Clocked Transport](tasks/T01-audio-clocked-transport.md) | M1 | done | T06 |
| 4 | [T02 MIDI Arrangement Playback](tasks/T02-midi-arrangement-playback.md) | M1 | done | T01, T03, T06 |
| 5 | [T04 Mixer Wiring](tasks/T04-mixer-wiring.md) | M1 | done | T02, T06 |
| 6 | [T05 Loop and Metronome](tasks/T05-loop-metronome.md) | M1 | done | T01, T02, T04 |
| 7 | [T10 Restore Plugin Editing Safely](tasks/T10-plugin-editor-safety.md) | M1.5 | done | T06 implementation; runtime checks retained |
| 8 | [T11 Browser-to-Rack and Clip Creation](tasks/T11-browser-rack-clip-creation.md) | M1.5 | done | T10 |
| 9 | [T12 Timeline Drag-and-Drop](tasks/T12-timeline-drag-drop.md) | M1.5 | done | T11, T03 implementation |
| 10 | [T13 Unified Sidebar Tab Rail](tasks/T13-sidebar-tab-rail.md) | M1.5 | done | T12 (workstream order) |
| 11 | [T14 Context Menus and Toolbar Cleanup](tasks/T14-context-menus.md) | M1.5 | done | T10, T11, T12, T13 |
| 12 | [T16 Loop Region UX](tasks/T16-loop-region-ux.md) | M1.5 follow-up | done | T05, T13, T14 |
| 13 | [T07 Project Save and Load](tasks/T07-project-save-load.md) | M2 | done | M1, M1.5 |
| 14 | [T08 Editor Navigation](tasks/T08-editor-navigation.md) | M2 | done | T03, T01 |
| 15 | [T09 Integration and Documentation](tasks/T09-integration-documentation.md) | M2 | done | T07, T08, T10-T14 |
| 16 | [T15 Mixer Routing Model](tasks/T15-mixer-routing-model.md) | Later (post-M2) | blocked: manual acceptance | M2 (workstream order) |

Recommended next: try T15's independent mixer workflow through the running watcher
(see its completion record). The M1/M1.5/M2 acceptance queue was cleared 2026-09-10
by the user override recorded below; T01-T16 except T15 are done. T15 was then
authorized for implementation and is offline-verified, not manually accepted.
The other post-M2 "Later" features remain backlog. Historical per-task
pending-observation notes below are superseded by the September 10 clearance;
that earlier clearance does not accept the new T15 routing implementation.

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
plugin editors were disabled at T06 because their restart path bypasses the gate;
T10 now explicitly overrides that restriction for initial testing. See its
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

User override 2026-09-08: continue authorizes T05 end to end despite the earlier
T03/T06/T01/T02/T04 runtime acceptance blocks. T05 was marked in_progress; earlier statuses
remain unchanged. Preserve all existing work; no application build/launch, staging
or commit. Only the independent offline_tests project may be built and tested.

T05 implementation/offline verification completed 2026-09-08: audio-owned bounded
loop spans with fractional carry, process-once wrap cleanup/live ownership, numeric
validated loop controls/ruler region, sample-timed denominator-aware oscillator and
pre-master injection. Existing 976/1072/2048 capacity/reset contracts are preserved;
see T05 for short-loop, dense-block and pedal fallback policies. Full CTest 1/1
passes with all previous regressions, one-hour loop drift, exact timestamp/waveform,
capacity/reset, master and actual transport/timeline component tests. T05 is blocked
on watcher/audible acceptance; M1 is not released. Earlier
statuses are unchanged; T07 remains backlog. No application build/launch, staging
or commit. The T05 completion record enumerates the remaining eight-step workflow.

T05 independent review's P2 deferred-wrap negative-offset finding is corrected:
the barrier delivery clamps to sample zero while preserving its fractional span
origin. The exact 48kHz/512/120-to-20BPM reproduction failed before the fix and
passes afterward, alongside nextafter/tempo variants, all-input range checks,
off-before-on/click alignment and later-event fractional timing. Full offline
CTest 1/1 passed. Independent review has been performed; runtime/M1 acceptance
remains blocked. No application build/launch, staging or commit.

T10 update 2026-09-08: the user explicitly requested rollback of the unfinished
JUCE patch and restoration of the Plugin button for initial testing. Stock pinned
JUCE 7.0.12 is restored, with active-channel editor access, target-specific rack
action, window focus/reuse and guarded close before host replacement/destruction.
Private editor-originated restarts still bypass host quiescence; no restart safety
or full T10 acceptance is claimed. See T10 for offline verification and remaining
native/device risks. T11 stays todo; earlier statuses are unchanged. No application
build/launch, staging or commit.

User override 2026-09-08: continue roadmap explicitly authorizes T11 end to end
despite T10 and earlier pending manual acceptance. T11 marked in_progress; T10's
stock-JUCE initial-testing risk override and all existing work remain intact.
Only independent offline tests are allowed; no application build/launch,
dependency edits, staging or commit.

T11 implementation/offline verification completed 2026-09-08: explicit typed plugin
payload, candidate-first shared create/replace load path, labeled exclusive rack
targets, bound/failure safety and selection-only New Clip with explicit source Edit
and window reuse. Full offline CTest 1/1 passes, including real producer/component
dispatch with fake plugins, layout/capacity rejection, cancellation, selection,
preview colors and safe delayed source actions. See T11 for native coverage limits.
Independent read-only review was performed by an explore agent. Its high-severity
JUCE target-discovery finding is fixed: interest checks payload/capacity only,
while target-local geometry gates enter/move/drop. Regression coverage mirrors
source-coordinate discovery, child-first row priority and cross-target exit rechecks
with preview cleanup; restoring the original predicate reproduces the failure.
Final independent offline build succeeded and CTest 1/1 passed (6.42 seconds).
T11 remains blocked on watcher/native acceptance. T12 remains todo; earlier statuses and T10's stock-JUCE
initial-testing override remain unchanged. No app build/launch, dependency edits,
staging or commit.

User authorization 2026-09-08: execute T12 end to end despite pending earlier
watcher checks; T15 documentation does not block it. T12 marked in_progress.
Preserve all dirty work and T10's stock-JUCE initial-testing override; only the
independent offline target is authorized, with no application build/launch or commit.

T12 implementation/offline verification completed 2026-09-08: typed pooled MIDI
payload, payload-only JUCE interest, lane/below-lane ghost and destination warning,
5px direct moves with grab offset, 1/16-note snap/Alt bypass, bounded real-scrollbar
autoscroll, stable track/instance session identities and atomic ownership transfer.
New lanes enter the model already populated; previews do not publish snapshots.
Explicit timeline double-click reuses the source editor path; toolbar remains for T14.
Final independent offline build succeeded, CTest 1/1 passed (6.72 seconds), and
diff check passed. See T12 and tests README for component coverage and native limits.
T12 is blocked on unobserved watcher/manual acceptance, not done. T13 remains todo;
T15 and T10's stock-JUCE initial-testing override are untouched. No app build/launch,
dependency edit, external agent review, staging or commit was performed.

T12 independent-review corrections: both P2 findings are fixed. Pooled gestures
now remain cancelled across reentry/windows and cannot restart within one press;
release-only JUCE target discovery and ruler-to-valid drops work without bypassing
stale/cancelled gesture guards. Cross-window pooled startup is explicitly enabled.
Both regressions failed with their respective old behavior restored, then passed
with the fixes. Final offline build succeeded and CTest 1/1 passed (6.62 seconds).
T12 remains blocked on watcher/native acceptance; see its review record. No app
build/launch, dependency edit, staging or commit; T10/T15 remain unchanged.

T13 implementation/offline verification completed 2026-09-08: one 28px rail per
side with vertically stacked icon tabs, identity-stable tab rebinding across
equal-count membership changes, the right-side expanded-width offset defect
removed, proportional narrow-window clamping that never overwrites remembered
widths, and component/layout regressions in the independent target. Final
offline build succeeded and CTest 1/1 passed (6.76 seconds) with all earlier
suites intact; `git diff --check` passed. Watcher/native acceptance was
observed and accepted by the user on 2026-09-08; T13 is done. The user deferred
implementation feedback to a later discussion, which may amend the record or
feed T14. T14 is next in the workstream; earlier statuses and T10's stock-JUCE
initial-testing override are unchanged. No application build/launch, staging
or commit was performed.

T14 context-menu/toolbar work was present unrecorded in the working tree; it was
independently reviewed, completed, and offline-verified 2026-09-08. All six menu
surfaces, stable-ID revalidation with prompt re-resolution, keyboard
Delete/Ctrl+E, and removal of the Place/Move/Delete/Assign toolbar are in
place. Review fixes: `CharPointer_UTF8` menu strings (raw UTF-8 `…`/`—`
literals asserted and mojibaked labels), the Clips footer Delete Source button
routed through the confirmed impact-warned path shared with its menu, rename/
Set-Start-Beat prompts skipping vanished targets, blank-name rejection inside
the validated rename actions, and `TextPrompt` dialog-interceptor seams so
offline tests never create native windows (a stray prompt modal previously
poisoned later suites). Final offline build succeeded and CTest 1/1 passed
(~6.9 seconds) with all earlier suites intact; `git diff --check` passed. T14
is blocked on watcher/manual acceptance: menu placement and enablement on each
surface, discoverability, native popup/focus interaction, and native plugin
windows. No application build/launch, staging or commit was performed. T07 is
next in the workstream; earlier statuses and T10's stock-JUCE initial-testing
override are unchanged.

T14 watcher/manual acceptance was observed and accepted by the user on
2026-09-09; T14 is done. The M1.5 release check is complete. M1.5's other
surfaces (T10 restart safety, T11/T12 native acceptance) remain as recorded.
T07 (Project Save and Load) is the next ready task; earlier statuses and
T10's stock-JUCE initial-testing override are unchanged. No application
build/launch, staging or commit was performed for this acceptance record.

T05 runtime observation 2026-09-09: the user audibly confirmed the loop wrap
repeats playback once the Loop toggle button is enabled (a T05 acceptance
observation), but reported the workflow undiscoverable — the loop fields and
Apply sit on the transport bar's bottom row while the enabling Loop toggle is
on the far right, and the loop button's hand-drawn icon "looks nothing like a
loop". Setting the region does not enable looping. T16 (Loop Region UX) was
created as an M1.5 follow-up: vendored Tabler (MIT) transport icons replacing
the hand-drawn paths, ruler-drag loop editing with auto-enable, a loop button
context menu and Edit Loop popover, and transport bar compaction. T07 remains
the next task after T16; earlier statuses and T10's stock-JUCE initial-testing
override are unchanged.

T16 implementation/offline verification completed 2026-09-09: vendored
Tabler (MIT) transport icons parsed into strokeable paths replacing all
hand-drawn button glyphs, ruler-drag loop creation/move/resize with 1/16 snap
(Alt bypass), minimum-length clamp and local preview committed exactly once
on mouseUp with auto-enable, a loop button context menu (Enable/Disable,
Edit Loop..., Clear Loop) with the numeric fields relocated into a CallOutBox
popover, transport bar compacted 104 -> 64px, and full offline regressions
(`iconTests`, rewritten `loopUiTests`). Final build succeeded and CTest 1/1
passed with all earlier suites intact; `git diff --check` passed. T16 is
blocked on watcher/manual acceptance (native popover/menu behavior, cursor
affordances, observed looping with the new workflow). Round-1 watcher feedback
2026-09-09: plain drag now creates a replacement region anywhere, Shift+drag
moves it (winning over edge grabs), plain edge grabs resize, the ruler shows
the pending action in the mouse cursor (crosshair/dragging hand/left-right),
and the Edit Loop popover launches at desktop level so panels can no longer
paint over it; round 2 removed the disabled-region dim band entirely (the
ruler highlight exists only while looping is enabled, so Clear/disable
removes it completely, and only visible regions can be grabbed); round 3
(user-specified) added a proper no-loop state via `LoopRegion.exists`: no
loop -> no ruler indication, created loops start enabled, toggled-off loops
show a dim grabbable band, toggled-on show blue, Clear removes the loop
entirely, and enabling a cleared loop materializes the default `[0, 4)`
region — all with offline regressions, CTest 1/1, `git diff --check`
passed. Re-observation pending. T07 remains next;
earlier statuses and T10's stock-JUCE initial-testing override are unchanged.
No application build/launch, staging or commit.

T03 watcher/manual acceptance was observed and accepted by the user 2026-09-09:
four-beat placement timing/beat units with tempo changes holding musical position,
shared-source edits updating both placements, placement deletion preserving the
source, two-destinations-on-one-track and one-destination-across-two-tracks
assignments, missing-destination rejection, removed-source unresolved placeholders
with editor teardown, invalid input handling, and instance display on existing and
new tracks. Channel reorder has no UI, so its assignment-retention case remains
covered by the T06 offline suite rather than watcher observation. T03 is done.
T06 is next in the acceptance walkthrough; all other statuses unchanged. No
application build/launch, staging or commit was performed for this record.

User override 2026-09-10: "next task.. i will do testing later" authorizes T07
end to end despite T16's pending re-observation and the T06 acceptance walkthrough
remaining deferred. T07 marked in_progress; all existing work, earlier statuses,
and T10's stock-JUCE initial-testing override remain intact. Only the independent
offline_tests target may be built/run; no application build/launch, staging or
commit. Implementation plan: versioned JSON project format separate from Settings,
plugin identity retained via PluginDescription with opaque base64 state, missing
plugins become unresolved channels preserving identity/blob, parse-validate-stage
before any live mutation, in-place clear+repopulate under AudioQuiescence so
ChannelMixer/ArrangementPublisher references survive, dirty tracking via existing
listener plumbing, New/Open/Save/Save As through a status-bar File menu button and
Ctrl+N/O/S/Ctrl+Shift+S with confirmAsync unsaved-work prompts and a FileChooser
test seam. T07 implementation and offline verification completed 2026-09-10:
versioned JSON schema v1 (`.docs/PROJECT_FORMAT.md`) with stable channel IDs and
order, plugin identity captured as a specific PluginDescription (not just bundle
path) plus opaque base64 state, channel/master mix state, tracks and instances
with restoreable UUIDs, pooled MIDI/audio/pattern clips, tempo/meter/loop
(`exists`)/metronome; parse-validate-stage before any live mutation with
two-phase prepareLoad/commitLoad (failed load leaves the session intact);
atomic temp-file saves that never report false success; missing plugins become
visible unresolved channels preserving identity/blob for recovery (re-save
round-trips them); in-place clear+repopulate under AudioQuiescence so
ChannelMixer/ArrangementPublisher references survive; dirty tracking via
existing listener plumbing with discard prompts before New/Open/quit; status-bar
File menu button plus Ctrl+N/O/S/Ctrl+Shift+S. Full offline build succeeded and
CTest 1/1 passed with all earlier suites intact; `git diff --check` passed.
T07 is blocked on watcher/manual acceptance (native FileChooser flows, prompts,
editors across load, plugin state through a real restart, audible workflow).
T08 is next in the workstream; earlier statuses and T10's stock-JUCE
initial-testing override are unchanged. No application build/launch, staging or
commit was performed.

T08 implementation and offline verification completed 2026-09-10: a new shared
`src/ui/editor/PianoRollGeometry.h` transform makes the piano-roll grid,
keyboard, ruler, and hit testing agree at every scroll/zoom position (the
viewport is the single scroll source; the grid stays grid-local); the fixed
eight-beat extent is replaced by `max(clip length, last note end, 8 beats) + 4`
with all 128 pitches reachable; `setZoomLevel` preserves the left-edge beat;
the initial view centers on the clip's notes; and a playhead is drawn from
T01's published position through a placement-to-source-local provider with
optional following (auto-scroll only while playing, manual scroll/zoom
suspends, the Follow toolbar toggle or playback restart resumes, and following
never changes audio position). `ClipEditorWindow`/`MainContent` wire the
transport and provider; T07's editor teardown path covers source
deletion/replacement. Full offline build succeeded and CTest 1/1 passed with
all earlier suites intact; `git diff --check` passed. `editorNavigationTests`
covers pure transforms/extent/mapping/follow-band math, real grid hit testing
for pitches 0/60/127, keyboard row alignment, invalidation pointer safety, and
a real editor+transport follow session. T08 is blocked on watcher/manual
acceptance (alignment before/after scrolling, click/drag/audition after scroll,
fractional-row and zoom-limit alignment, resize/scrollbar behavior, long-clip
navigation, observed follow suspend/resume with audio). T09 is next in the
workstream; earlier statuses and T10's stock-JUCE initial-testing override are
unchanged. No application build/launch, staging or commit was performed.

T09 implementation and offline verification completed 2026-09-10 (user
authorization: "go midi stuff"): `externalMidiTests` verifies the
device-selection path end to end offline (`sendMidiMessage` enters
`MidiManager::handleIncomingMidiMessage` exactly where an open `MidiInput`
would): invalid/None/disconnect handling, exactly-once delivery to the active
channel alongside independent arrangement playback, active-channel rerouting,
and a drain regression proving the keyboard-echo guard never re-enqueues.
`integrationWorkflowTests` is the cross-feature fixture: real
Project->ChannelMixer->AudioEngine wiring, shared clip on two routed
placements, loop [0,8), 120->90 BPM mid-playback, mix levels (channel volume +
post-sum master gain), mute suppression/unmute without chase, loop-wrap
refiring, stuck-note-free stop, then save/mutate/load through ProjectDocument
with restored plugins rendering through the SAME mixer, quiescent old-plugin
teardown and zero callback allocations. Findings: AGENTS.md's
AudioDeviceManager/MidiMessageCollector signal flow was documentation rot
(no such path exists); it was rewritten to the implemented architecture and
`.docs/PLAYING.md` was created (user workflow + limitations, implemented
controls only). Full offline build succeeded and CTest 1/1 passed with all
earlier suites intact; `git diff --check` passed. T09 is blocked on its manual
acceptance: a human following `.docs/PLAYING.md` must make and reopen a short
sketch, and external MIDI needs real hardware (audible delivery to exactly the
selected instrument, hot-unplug). The remaining "Later" limitations
(recording, sample playback, sends/automation) become named backlog items once
the workflow is confirmed dependable. No application build/launch, staging or
commit was performed.

Backlog clearance 2026-09-10: user override — "assume everything is complete..
ill just go find more things in the app i need changed.. clear it and wait for my
next steps". All pending watcher/manual/device acceptance for T06, T01, T02, T04,
T05, T10, T11, T12, T16, T07, T08, and T09 is considered accepted by this
override rather than individually observed; those tasks are marked done here and
in their files, and the M1/M1.5 release checks below are checked accordingly.
Known limitation retained: T10's editor-originated plugin restart still bypasses
the quiescence gate (the initial-testing override remains in force). T15 stays
backlog — it is a planning question with no implementation to accept; recording,
samples, automation, and the other "Later" items remain future features, not
pending acceptance. No application build/launch, staging or commit was performed
for this record.

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
- Prefer direct manipulation, explicit edit gestures, and contextual actions. The initial placement toolbar is superseded by T12/T14; retain secondary precise position editing and numeric loop controls where useful.
- Tracks remain arrangement lanes, not instruments. New clip drops use the active instrument channel, named in the preview; without a valid instrument destination, reject with actionable feedback. Moving an instance preserves its source, duration, and destination even across tracks.
- Single-click selects and double-click edits. Creating a clip does not implicitly open an editor. Deleting a pooled source and removing one placement must be clearly distinguished.
- Use unambiguous drag payload types for plugins and pooled clips; never interpret a clip ID as a plugin path. Cancelled or invalid drops must not partially mutate the model.
- Collapsed sidebars share one compact tab rail per side, with stable target bindings, remembered expanded widths, and no blank reserved columns.
- Hosted editors are enabled by explicit user override for initial testing (T10). Do not mistake this for closing T06's editor-originated lifecycle/restart gap or full acceptance.
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

- [x] The eight-step M1 workflow above is usable without source edits or hardcoded clip injection.
- [x] Tempo and playback stay stable when the UI is busy.
- [x] Block boundaries, seeks, looping, overlapping notes, and deletion do not leave stuck notes.
- [x] Two destinations work independently; shared clips and channel reorder do not corrupt routing.
- [x] Audio-thread ownership and cleanup have been reviewed; no callback logging or per-block scratch allocation remains in our processing path.
- [x] Deterministic tests cover transport, scheduling, routing, and loop math; any unavailable runtime verification is explicitly recorded.
- [x] Limitations are clear: MIDI arrangement playback, not recording or sample playback; saving exists in T07.

Runtime observations cleared by the 2026-09-10 user override (see the backlog
clearance record), not individually observed.

## M1.5 Release Check

- [x] Plugin configuration is available through a visible button and contextual action, with restart/replacement/deletion behavior (T10's editor-originated restart gap is a known limitation accepted by the 2026-09-10 override).
- [x] Browser-to-rack creation and existing-row replacement have distinct previews and failure-safe results.
- [x] New Clip selects a source without opening Piano Roll; explicit edit gestures work from the pool and timeline.
- [x] Clip drops target existing tracks or visibly create one new track; direct moves work within and across tracks without changing routing.
- [x] Snap, scroll coordinates, cancellation, invalid targets, and missing destinations have tested behavior.
- [x] Browser/Channel Rack collapse into one shared rail and reopen correctly in either order; the right Clips tab remains reachable.
- [x] Context menus act on their actual targets, destructive actions distinguish source versus instance, and redundant placement toolbar controls are removed only after replacements work.
- [x] Focused offline regressions and actual watcher/manual observations are recorded separately; M1's unresolved acceptance remains visible.
