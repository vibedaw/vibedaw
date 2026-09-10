# VibeDAW - Architecture Overview

DO NOT TRY TO BUILD OR LAUNCH THE APPLICATION AFTER A TASK. WE HAVE A WATCHER RUNNING!

## Framework
- **JUCE 7.0.12** (pinned via CMake FetchContent) - modular JUCE (audio_devices, audio_processors, audio_utils, gui_basics, gui_extra)
- CMake 3.16+, C++17. ALSA + JACK + VST3 hosting on Linux.

## Entry Point (`src/Main.cpp`)
`VibeDawApplication` (JUCEApplication) wires, in order:
`AudioEngine` -> `MidiManager` -> `Project` -> `MidiKeyboardState` ->
`ChannelMixer(Project models)` -> `MainWindow`. `ChannelMixer` is created in
Main.cpp (not owned by Project) and set as the engine's sole processor.

## Concurrency Primitives (`src/core/AudioBoundary.h`)
- `LatestState<T>` - three-slot mailbox, single producer/single consumer; the
  consumer's acquired reference survives until its next `acquire()`.
- `BoundedQueue<T, Capacity>` - lock-free SPSC queue; full queues reject (all-or-nothing).
- `AudioQuiescence` - single writer/one-render-consumer gate. Structural edits
  (`AudioQuiescence::Edit`) block render admission off audio; render callbacks
  call `enter()`/`leave()`. All model/plugin replacement must happen under an `Edit`.

## Ownership / Signal Flow (implemented)
```
External MIDI device (MidiManager opens its own juce::MidiInput)
    └─> MidiManager::handleIncomingMidiMessage
            ├─> AudioEngine::handleIncomingMidiMessage -> bounded midiQueue (2048)
            └─> feedback queue -> timer -> AudioEngine::updateKeyboardFeedback
                    (marks on-screen keys; the updatingKeyboardFeedback guard
                     prevents the echo from re-entering the audio queue)

On-screen keyboard (MidiKeyboardState) -> AudioEngine::handleNoteOn/Off
    -> same bounded midiQueue (never duplicated)

Audio callback (AudioEngine::audioDeviceIOCallbackWithContext)
    └─> drain midiQueue at sample 0 (or panic cleanup CCs)
            └─> ChannelMixer::processBlock
                    ├─> ArrangementPublisher snapshots (LatestState) per destination
                    ├─> live input -> active channel only
                    ├─> Metronome (pre-master click synthesis)
                    └─> per channel: Channel::processBlock
                            └─> PluginHost::processBlock -> plugin instance
                    └─> MasterBus gain/mute post-sum + meters
```
The audio callback owns transport advancement (TransportClock with tempo/rate
compensation). UI never drives playback time; it sends commands and polls
published position. Live input routes to the active channel; arrangement events
route by each instance's stable `ChannelId`. Processors are merged per
destination so each plugin is processed once per block.

## Timing Units
- Quarter-note beats everywhere in models: notes, clip source length, placement
  start/length, loop bounds, tempo (BPM), time signature.
- Conversion to samples happens only at the render boundary (48000 Hz/120 BPM =
  24000 samples per beat). Seconds are a display-only conversion.

## Core
- `AudioEngine` (`src/core/`) - AudioDeviceManager + callback; bounded queues in,
  scratch buffers preallocated; never allocates or logs in the callback.
- `ChannelMixer` - AudioProcessor over the whole graph: snapshots, per-destination
  merging, capacity contracts (976 normal / 1072 cleanup messages, <= 2048 total),
  solo/mute suppression, destination-local cleanup, note-capacity preflight.
- `TransportState` (`src/core/TransportState.h/cpp`) - audio-owned compensated
  beat clock, play/stop/seek commands, `LoopRegion` (`exists`/`enabled`,
  default `[0, 4)`), metronome enable. Audio publishes position; UI polls.
- `MidiManager` - external device selection/open (`connectToDevice`/`disconnect`),
  `MidiListener` for UI feedback; forwards raw messages into the engine queue.
- `MixerState.h` - `MasterBus` (gain 0..2, mute, meters) owned by the instrument
  graph, never by an arrangement Track.

## Project Model (`src/project/`)
- `Project` - owns `TrackList`, `ChannelList`, `ClipPool`, `TransportState`,
  `Settings`, and the `ProjectDocument` state (New/Open/Save/Save As, dirty
  tracking). Models are repopulated in place on load (never swapped) so
  `ChannelMixer`/UI references survive.
- `Channel`/`ChannelList` - instruments (stable `ChannelId`), one `PluginHost`
  each, volume/pan/mute/solo; live audition routes to the active channel.
- `Track`/`TrackList` - arrangement lanes holding `ClipInstance`s (clip ID +
  destination `ChannelId` + start/duration beats + stable UUID). Tracks are not
  instruments.
- `ClipPool`/`Clip`/`MidiClip`/`AudioClip`/`PatternClip` - pooled sources
  (stable `ClipId`); placements reference them; deleting a placement never
  deletes the source.
- `ProjectDocument` - versioned JSON v1 (see `.docs/PROJECT_FORMAT.md`),
  parse-validate-stage then two-phase `prepareLoad`/`commitLoad`; failed loads
  leave the session intact; saves are atomic temp-file writes.

## Plugins (`src/plugins/`)
- `PluginHost` - wraps a `juce::AudioPluginInstance` (VST3); identity is a full
  `PluginDescription` plus opaque base64 state (T07).
- `PluginScanner` - format discovery for the Browser.
- `PluginWindow` - pop-out editor window; editors are enabled by explicit user
  override for initial testing (T10); editor-originated restarts still bypass
  the quiescence gate.

## UI (`src/ui/`)
- `DawWindow` - shared custom title bar/border for MainWindow, ClipEditorWindow,
  and PluginWindow. Linux/XWayland maximize uses EWMH requests and polled WM state
  (not JUCE fullscreen bounds), leaving KDE panel avoidance/restore geometry to
  the WM. Client drag/resize is suspended while maximized; native calls stay off audio.
- `MainWindow`/`MainContent` - DocumentWindow; status bar (File menu button,
  MIDI device combo, status label), transport bar (`TransportComponent` with
  vendored Tabler icons, loop menu/popover), `PluginButton`, hotkeys
  (Ctrl+N/O/S/Ctrl+Shift+S project, Ctrl+B sidebars, Ctrl+P piano, Ctrl+M mixer,
  Delete/Ctrl+E timeline, Ctrl+E note delete in the focused grid).
- `panels/` - `Panel`/`PanelContainer`/`PanelTitleBar` (flex/floating/pop-out
  modes, 24px collapsed title bars) hosting `TimelinePanel`, `MixerPanel`,
  `PianoPanel`.
- `sidebar/` - `SidebarContainer` tab rails (28px collapsed) binding
  `BrowserSidebar`, `ChannelRackSidebar`, `ClipsSidebar` with remembered widths.
- `timeline/` - `TimelineContent`/lanes/headers, `TimeRuler` (loop region drag
  gestures, playhead, ruler seek), `TimelineGeometry`.
- `editor/` - piano roll: `NoteGridComponent`, `PianoRollKeyboard`,
  `TimeRulerComponent`, `PianoRollEditor`, `ClipEditorWindow`, shared
  `PianoRollGeometry.h` (single scroll source, grid-local transform).
- `mixer/` - `MixerStrip`/`MasterStrip`/`LevelMeter` (polled envelopes).
- `components/TextPrompt.h` - `textPromptInterceptor()`/`confirmInterceptor()`
  seams (also used by offline tests).
- `Icons.cpp` - vendored Tabler (MIT) transport icon paths.

## Persistence
- Projects: `.docs/PROJECT_FORMAT.md` (`.vibedaw` JSON v1). `ProjectDocument`
  parse-validate-stage -> `Project::prepareLoad`/`commitLoad` (atomic; failed
  loads change nothing). Saves are temp-file + move, never false success.
  Missing plugins become unresolved channels preserving identity+blob.
- Settings (`Settings`, JSON) are application preferences, not the project file.

## Tests (`tests/`)
Independent CMake project; never defines, builds, or launches VibeDAW. No
network, native window, audio device, or installed plugin needed.

```sh
cmake -S tests -B /tmp/opencode/vibedaw-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/vibedaw-t09-tests --target offline_tests --parallel 4
ctest --test-dir /tmp/opencode/vibedaw-t09-tests --output-on-failure
```

Builds the real model/engine/mixer/UI sources with ALSA/JACK/VST3 compiled out;
`OfflineInstrument`/`StatefulInstrument` fake processors; `AudioEngineTestAccess`
invokes the real device callback; interceptor seams for file dialogs, prompts,
and popovers. Coverage: T03 routing/IDs, T06 boundary/quiescence, T01 clock,
T02 scheduler, T04 mixer, T05 loop/metronome, T10 editor access, T11/T12/T13
drag/rail components, T14 context menus, T16 loop UX, T07 project round trip,
T08 editor navigation, T09 external-MIDI path + integration fixture. See
`tests/README.md` for exact suites and remaining manual checks. The watcher
rebuilds the application only; it does not re-run CMake, so CMake/test changes
are verified manually via the commands above.

## Naming Conventions
- `vibedaw` namespace for all application code
- `*Component` suffix for UI components, `*Manager`/`*Host` for controllers/wrappers
- Stable identity types: `ChannelId`, `ClipId`, instance/track UUIDs - never
  treat reorderable indexes as IDs
- Private members use trailing underscore or JUCE leak-detector macros

## Build
- CMake-based build system (`cmake -S . -B build`, JUCE via FetchContent)
- `just build` / `just dev` / `just watch` (watchexec watcher)
