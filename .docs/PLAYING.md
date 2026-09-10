# Playing VibeDAW

A short guide to the implemented workflow. Everything below is implemented;
retired bootstrap controls (the old Place/Move/Delete/Assign toolbar) are not
documented because they no longer exist. Recording, samples, sends, and
automation are not implemented and are not shown as if they were.

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
4. **Mixer** panel: per-channel gain/pan/mute/solo and master gain/mute with
   peak meters. Mute/solo also gate that channel's arrangement playback.

## 5. Sidebars

- Each sidebar collapses into a compact icon tab rail (28px) on its side of
  the window; click a tab to reopen. Left rail: Browser, Channel Rack. Right
  rail: Clips. `Ctrl+B` toggles sidebars. Remembered widths survive restarts.

## 6. Save and reopen

1. **File menu** (status bar) or `Ctrl+S` / `Ctrl+Shift+S` to save / Save As;
   `Ctrl+N` for a new project; `Ctrl+O` to open. Unsaved changes prompt first.
2. Projects are `.vibedaw` JSON files containing channels (with plugin identity
   and state), tracks/placements, the clip pool, tempo/meter/loop/metronome,
   and master mix state.
3. A plugin that is missing on load keeps its channel visible as *unresolved*
   with its saved state preserved; re-saving round-trips it for recovery.

## Current limitations

- MIDI arrangement playback only: no recording (MIDI or audio), no sample
  playback rendering, no automation, no sends/effects routing.
- Plugin editor restarts (opening/closing an editor that triggers a device
  restart) bypass the audio quiescence gate - initial-testing override.
- External MIDI selection is per-device; there is no MIDI-through or channel
  filtering. Visual feedback (on-screen keys) is separate from the audio path.
- Clip-local repetition, trim/stretch, and undo/redo are not implemented.
- Channel reorder has no UI (assignments survive reorders in the model; covered
  by offline tests).
