# Playing VibeDAW

A short guide to the implemented workflow. Everything below is implemented;
retired bootstrap controls (the old Place/Move/Delete/Assign toolbar) are not
documented because they no longer exist. Initial MIDI recording is available;
audio recording, sample playback, sends and automation remain future work.

## 1. Load an instrument

1. Open the **Browser** sidebar (left tab rail) and find a VST3 instrument.
2. Drag it onto empty **Channel Rack** space to create a channel, or onto an
   existing row to replace that row's plugin (the drop preview distinguishes
   the two). Double-click a browser entry also creates a channel.
3. Select the row and press the **Plugin** button (or right-click the row) to
   open the plugin's editor and configure it. Plugin editors are enabled for
   initial testing; an editor-originated device restart bypasses the audio
   quiescence gate (known limitation, T10).
4. The plugin button/context action reports why nothing opened when the
   channel has no plugin, the plugin has no editor, or it failed to load.

## 2. Create and edit a clip

1. In the **Clips** sidebar (right tab rail) press **New Clip**. This creates
   and selects a pooled MIDI clip; it does not open the piano roll.
2. Double-click the clip (or its Edit context action, or Ctrl+E on the timeline)
   to open the **Piano Roll** editor window.
3. In the piano roll: click to draw, drag to move/resize notes, right-click or
   Ctrl+E removes the selected note. Scrolling follows the playhead while
   playing; manual scroll/zoom suspends following until you re-enable Follow
   or restart playback.

## 3. Place and route on the timeline

1. Drag a clip from the Clips sidebar onto an existing track, onto empty space
   below the lanes (a previewed new track), or use the empty-space context menu
   to place the selected clip.
2. Every placement carries its own destination instrument. Change it from the
   placement's context menu (Assign submenu) - the same source can drive
   several instruments.
3. Drag placements to move them (1/16 snap, hold Alt to bypass); this never
   changes their source or duration.
4. Right-click actions distinguish **removing a placement** from **deleting the
   pooled source** (the latter warns about every affected placement).
5. Double-click a placement to edit its shared source.

## 4. Play, loop, mix

1. Transport bar: play/stop, tempo, time signature, seek by clicking the time
   ruler. The playhead is drawn from the audio clock; the UI never drives it.
2. **Loop**: drag directly on the time ruler to create a region (auto-enables,
   1/16 snap, Alt bypasses), Shift+drag to move it, grab its edges to resize,
   or right-click the loop button for Enable/Disable, Edit Loop... (numeric
   fields) and Clear Loop.
3. **Metronome**: toggle on the transport bar; clicks are pre-master.
4. **Mixer** panel: starts with one independent mixer channel plus Master.
   **Add Channel** creates another; select a strip then **Remove Selected** to
   remove it. Right-click a strip to rename it. Instruments and mixer channels
   are not paired: adding an instrument never creates a mixer channel.
5. Each Channel Rack row's **Output** button chooses Master (the default) or a
   mixer channel. Multiple instruments can share a mixer channel; its fader,
   pan, mute/solo and peak meters operate on their combined audio. Removing a
   mixer channel returns its instruments to Master, without deleting instruments.
6. Mixer selection does not select a live-audition instrument or change clip
   destinations. Mixer mute/solo gates audio only, preserving MIDI notes; solo
   excludes other mixer channels and direct-to-Master instruments, but not the
   metronome. Rack instrument mute/solo retains its separate note-suppression
   behavior. Audio routing edits can briefly pause rendering without clearing
   held notes or queued MIDI. Master gain/mute applies after the final sum.

## 5. Sidebars

- Each sidebar collapses into a compact icon tab rail (28px) on its side of
  the window; click a tab to reopen. Left rail: Browser, Channel Rack. Right
  rail: Clips. `Ctrl+B` toggles sidebars. Remembered widths survive restarts.

## 6. Save and reopen

1. **File menu** (status bar) or `Ctrl+S` / `Ctrl+Shift+S` to save / Save As;
   `Ctrl+N` for a new project; `Ctrl+O` to open. Unsaved changes prompt first.
2. Projects are `.vibedaw` JSON files containing channels (with plugin identity
   and state), tracks/placements, the clip pool, tempo/meter/loop/metronome,
   independent mixer channels, instrument output assignments, and master mix
    state, plus recorded CC and pitch-bend events. New saves use format v3.
    V1/v2 projects still open; V1 retains instrument
   mix controls and routing directly to Master as before.
3. A plugin that is missing on load keeps its channel visible as *unresolved*
   with its saved state preserved; re-saving round-trips it for recovery.

## 7. Record MIDI

1. Select your MIDI device in the status bar and choose an instrument. Click the
   main transport Record button to open shared recording setup. New song capture
   creates a clip on a new track; existing targets identify an exact placement.
2. Alternatively, open a MIDI clip's piano-roll window and use its recording
   controls. This loops the clip independently, parking song playback stopped.
   Choose the instrument, source, mode and loop length (quarter-note beats).
3. **Continuous** grows one recording until stopped. **Takes** retains nonempty
   passes separately. **Replace** replaces the traversed note interval, including
   silence. **Overdub** adds notes and plays previous passes on subsequent loops.
   CC/bend movement replaces only its touched lane from the movement onward.
4. Toggle Record off to finalize and show captured notes while playback continues.
   Edit notes while disarmed, then re-arm. Stop ends playback too. Take selection
   is available after disarming. End Session returns control to song playback.
5. Shared-source recording affects all its placements; setup displays that impact.
   Clip-focused setup can make an independent clone without changing placements.
   **Undo rec** restores the latest recording transaction unless later edits or
   new references would be lost. It is not general editing undo/redo.
6. Save finalizes capture first. Notes, CCs, bend and take sources are saved; the
   in-memory undo transaction is not restored after reopening the project.

Recorded notes/takes appear on disarm or stop, not live during capture. The
workspace has a 65,536-entry bound, not unlimited recording storage. Fault status
means capture was disarmed and only the accepted prefix is retained. Stop and
inspect that material before continuing. Pedal/bend playback depends on plugin
support; real device/plugin acceptance is deferred in [TOTEST](TOTEST.md).

## Current limitations

- MIDI arrangement playback and initial MIDI recording: no audio recording, no sample
  playback rendering, no automation, no sends/effects routing.
- Plugin editor restarts (opening/closing an editor that triggers a device
  restart) bypass the audio quiescence gate - initial-testing override.
- External MIDI selection is per-device; there is no MIDI-through or channel
  filtering. Visual feedback (on-screen keys) is separate from the audio path.
- Clip-local repetition in song placements, trim/stretch, and general undo/redo
  are not implemented. Clip-focused looping and recording-only undo are separate.
- Count-in, capture quantization, live recording preview, expression-lane editing,
  take comping and disk-streamed capture are deferred. Finalizing recording or
  importing manual edits can briefly interrupt rendering. Song backing changes
  become audible only after End Session.
- Input timing adds approximately one audio block of monitoring latency; there
  is no hardware-latency compensation. Native/plugin pedal behavior needs testing.
- Channel reorder has no UI (assignments survive reorders in the model; covered
  by offline tests).
