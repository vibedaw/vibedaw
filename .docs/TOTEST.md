# Deferred Testing

## Evidence And Status

This is the manual testing handoff for the closed implementation workstream, not
a new claim that checks passed. Use the running watcher; no application build or
launch is requested. Check boxes only after observation, recording date, device,
plugin/version, relevant setup and result. Link failures to a defect rather than
silently treating them as acceptance.

- [Archived roadmap](ROADMAP_ARCHIVE.md) preserves the full prior record.
  Task completion records contain historical offline results; these were not
  rerun for this documentation cleanup and do not certify real hardware/plugins.
- Observed and user-accepted: T13 sidebar behavior on September 8, 2026; T14
  contextual UI on September 9; T03's documented placement/routing workflow on
  September 9. Channel reorder had offline coverage, not a UI observation.
- Partial observation: T05 loop repetition was audibly confirmed September 9
  after enabling Loop. Subsequent T16 workflow changes are not thereby verified.
- September 10 clearance accepted remaining T01/T02/T04/T05/T06/T07/T08/T09/
  T10/T11/T12/T16 checks by user override, not individual observation. Keep that
  historical acceptance separate from this deferred observation/regression list.
- T15 and T20 are implementation-complete, manual acceptance deferred. The old
  September 10 override does not establish acceptance of later T15 routing work.
  Unchecked task acceptance lists remain useful detail, not fresh test results.

## Playback, Loops And Mix

- [ ] Follow [PLAYING](PLAYING.md) to make a two-instrument sketch with shared
  sources, independent placement destinations and later placements. Hear each
  event at its displayed beat; source edits affect shared placements, deleting
  a placement preserves its source, and audition selection does not reroute songs.
- [ ] Play/stop/seek and change tempo while busy with UI edits. Check timing,
  sustained/overlapping notes, mute/unmute and deletion for stuck notes or drift.
- [ ] Exercise T16 loop create/move/resize, snap/Alt bypass, enable/disable,
  clear/default recreation and numeric popover. Check visible region state,
  repeated audible wraps, short loops and tempo changes across wraps.
- [ ] Check metronome timing and meter accents, master gain/mute, instrument
  controls and meter decay. Separate instrument note suppression from T15's
  audio-only mixer mute/solo; confirm no unexpected retrigger or missing cleanup.
- [ ] Check piano-roll scrolling, zoom limits, all pitch rows, hit testing,
  audition, long-clip extent and playhead alignment. Follow suspends on manual
  navigation and resumes explicitly without seeking audio unexpectedly.

## Devices And Plugin Lifecycle

- [ ] Select a real MIDI input, then None/another device; play notes, sustain,
  modulation and pitch bend. Confirm exactly-once live delivery to the selected
  instrument, independent arrangement delivery and no on-screen-keyboard echo.
- [ ] Hot-unplug/reconnect with notes and pedal held; verify cleanup, selection
  feedback and recovery. Exercise audio-device/rate/block-size changes during
  playback and live input, noting dropouts or stranded notes.
- [ ] Observe supported real-plugin preset/configuration and restart behavior
  with live MIDI and playback, recording plugin/version and trigger. This is
  risk characterization under the existing initial-testing override, not proof
  that the T10 safety defect is fixed. Preserve a saved sketch before trying it.

## Save And Reopen

- [ ] Save/Save As, quit and reopen a real sketch with non-default plugin state,
  shared sources, routed placements, loop/meter/tempo and mix controls. Compare
  audible sound and plugin parameters, not just names or JSON fields.
- [ ] Exercise native file dialogs, cancel/discard/unsaved prompts and failed
  saves. Failures must not report success or clear dirty state. Open malformed
  files without changing the running session; successful loads start stopped.
- [ ] Reopen with a missing plugin, re-save and recover it later: identity and
  opaque state must survive. Check open clip/plugin editors across project loads
  and source/channel deletion for stale targets or windows.

## Native UI, Drag And Windows

- [ ] Browser plugin drops distinguish empty-rack creation from row replacement;
  failed/cancelled loads preserve the old instrument. Clips drag onto lanes or
  a previewed new lane, move across tracks, autoscroll and cancel cleanly.
- [ ] Check cross-window clip drags, source deletion during a gesture, stale
  menus and prompts, context-menu placement, keyboard focus and shortcuts.
- [ ] Open/focus/reopen plugin editors; selection must not retarget them.
  Replace/delete plugins and close the app with editors open. Record native
  behavior separately from the unresolved restart defect below.
- [ ] Pop out/dock panels; resize, collapse/reopen sidebars in either order and
  test narrow layouts. Check main, clip and plugin windows for title-bar drag,
  edge resize, maximize/restore, KDE panel avoidance and remembered geometry.
- [ ] Check HiDPI/mixed-scale displays, monitor moves, text/icons/hit targets,
  menus/popovers and native plugin editor sizing. Record desktop/session/scaling.

## T15 Routing

See [T15](tasks/T15-mixer-routing-model.md) for the implemented contract and
recorded offline tests; these checks remain deferred.

- [ ] New project starts with one independent mixer channel plus Master.
  Adding instruments adds no mixer channels and defaults their outputs to Master.
- [ ] Add/rename a destination and send two instruments to it. Hear their summed
  gain/pan/mute/solo and see truthful meters; mixer selection leaves live audition
  unchanged. Solo excludes direct-Master instruments but not the metronome.
- [ ] Reroute held notes and remove an assigned destination, including the last.
  Instruments return to Master without deletion, stuck notes or lost queued MIDI.
- [ ] Check output menus, narrow mixer scrolling, pop-out/dock and independent
  instrument controls upstream of mixer controls.
- [ ] Save/reopen real plugin state with shared routes and zero mixer channels.
  Open a V1 sketch: old instrument controls/sound survive, direct-Master routing
  remains and one empty mixer channel is supplied by migration.

## T20 And VibeSynth

See [T20](tasks/T20-ui-overhaul.md) for its implementation checklist and historical
offline report. VibeSynth checks below are pending observations, not an additional
claim of T20 test coverage or expressive-controller support.

- [ ] Check theme contrast/readability, transport readouts/timecode toggle,
  clip previews/metadata/filters, selection and muted/unresolved appearance.
  Drag/edit/context actions still reach the correct targets after restyling.
- [ ] Check mixer knob/fader scales, peak readouts and meter decay; piano octave
  and velocity controls; rack M/S/meters; browser search; live CPU/RAM/device
  indicators without excessive polling or misleading device state.
- [ ] Create/load VibeSynth, open/reopen its editor and adjust sound controls.
  Audition from keyboard and hardware, play an arrangement, route through an
  independent mixer channel, then save/reopen non-default synth state.
- [ ] Check VibeSynth editor sizing/focus/HiDPI and note cleanup. Characterize
  sustain, pitch bend and modulation separately; do not infer controller support
  from note playback. T21 must explicitly verify any claimed recording playback.

## Known Defect

**T10 plugin restart safety remains open, not merely untested.** Stock JUCE
editor-originated restart mutations bypass the host's audio quiescence gate;
layout/MIDI mapping reconciliation and device/message-thread lock ordering remain
unresolved. Stopping transport is not audio quiescence. Native success on one
plugin cannot close this source-identified defect.

The initial-testing override permits editor access; it does not implement a fix.
See [T10's investigation](tasks/T10-plugin-editor-safety.md) for the required
restart interception/lifecycle work and separate verification. Do not turn a
manual checkbox or the old acceptance override into a safety guarantee.

## Recording Handoff

[T21](tasks/T21-midi-recording.md) is wrapped as a limited initial version at the
user's request. The current offline suite passes, but no hardware/native result
is inferred from it. Its original full design is not completely implemented.

- [ ] Record from the hardware device into a new song clip and an explicitly
  selected existing placement. Check timing, velocity, routing, clip lengths and
  source-local position when starting midway through an existing placement.
- [ ] Open a clip window, use Overdub, record several passes, disarm while the
  loop continues, edit notes, and re-arm. The grid updates on disarm/stop only.
- [ ] Compare Takes, Replace and Continuous; preserve partial final passes, select
  a take while looping, undo a recording, and verify later manual edits are not lost.
- [ ] Record sustain, pitch bend and modulation on a supporting instrument.
  Check wrap/stop cleanup and save/reopen. Document plugin mapping limitations;
  plugins that ignore MIDI cleanup are not supported by the clip-loop policy.
- [ ] Unplug the device, change audio settings or exceed capacity while capturing.
  Verify visible fault/disarm and retained prefix; cleanup MIDI must not become a take.
- [ ] Check setup popovers, narrow clip-window controls, editor closing, New/Open,
  save failures, dirty prompts and shared-source warnings with active sessions.

Deferred features/engineering: live capture preview, count-in, quantization,
expression-lane editing, comping, release velocity, unlimited/disk-backed capture,
seamless off-audio finalization, fully indexed scheduling, latency compensation,
independent simultaneous song/clip timelines, and broad VST3 pedal/expression parity.
These are follow-up implementation work, not checkboxes that testing alone can close.
