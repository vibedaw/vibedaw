# VibeDAW - Architecture Overview

DO NOT TRY TO BUILD OR LAUNCH THE APPLICATION AFTER A TASK. WE HAVE A WATCHER RUNNING!

## Framework
- **JUCE 7.x** - Cross-platform C++ framework for audio applications
- Uses modular JUCE modules (audio_devices, audio_processors, audio_utils, gui_basics, gui_extra)

## Entry Point
- `src/Main.cpp` - `VibeDawApplication` class (JUCEApplication subclass)
- Initializes: AudioEngine, MidiManager, Project, MidiKeyboardState, MainWindow

## Core Classes

### AudioEngine (`src/core/AudioEngine.h/cpp`)
Manages audio device I/O and processor chain.
- `AudioDeviceManager` - Hardware audio device management
- `AudioProcessorPlayer` - Bridges audio device callbacks to AudioProcessor
- `MidiMessageCollector` - Collects MIDI for sample-accurate timing in processBlock

### MidiManager (`src/core/MidiManager.h/cpp`)
Handles external MIDI input devices.
- `MidiListener` interface - For UI components receiving MIDI notifications
- Currently used for UI feedback; audio routing goes through AudioEngine's MidiMessageCollector

### ProcessorBase (`src/core/ProcessorBase.h`)
Abstract base for audio processors (JUCE AudioProcessor pattern).

## Project Structure

### Project (`src/project/Project.h/cpp`)
Top-level container for tracks and settings.
- Owns `masterTrack` (Track instance)
- `Settings` - JSON-based application settings

### Track (`src/project/Track.h/cpp`)
Audio/midi track with plugin hosting.
- Extends `ProcessorBase`
- Hosts single `PluginHost` instance
- Volume/Pan controls (applied after plugin processing)

## Plugin System

### PluginHost (`src/plugins/PluginHost.h/cpp`)
Wraps VST3/AU plugin instances.
- `AudioPluginFormatManager` - Plugin format discovery
- `AudioPluginInstance` - Loaded plugin wrapper
- `createEditor()` - Creates plugin UI window

### PluginWindow (`src/plugins/PluginWindow.h/cpp`)
Standalone window for plugin editor UI.

## UI Components

### MainWindow (`src/ui/MainWindow.h/cpp`)
Main application window (DocumentWindow).
- Contains `MainContent` component
- Forwards key events to `MainContent::handleKeyPress()`

### MainContent (`src/ui/MainContent.h/cpp`)
Primary UI layout container.
- `TransportComponent` - Play/stop controls (placeholder)
- `PanelContainer` - Flex column layout for panels
- `MidiDeviceCombo` - MIDI input selector
- `PluginButton` - Open plugin editor
- Hotkeys: `Ctrl+P` (toggle piano), `Ctrl+M` (toggle mixer)

### PianoComponent (`src/ui/PianoComponent.h/cpp`)
On-screen keyboard using `MidiKeyboardComponent`.
- Implements `MidiListener` for external MIDI visualization

### TransportComponent (`src/ui/TransportComponent.h/cpp`)
Transport controls (play, stop, tempo - placeholder).

## Panel System

### Panel (`src/ui/panels/Panel.h/cpp`)
Base class for dockable panels with three display modes:
- **Flex**: Part of vertical layout, resizable via splitter
- **Floating**: Overlay, can be positioned anywhere
- **PopOut**: Separate native window (like PluginWindow)

Features:
- `PanelState`: Expanded, Collapsed, Collapsing, Expanding
- Title bar with [−]/[+] collapse/expand buttons
- Context menu (right-click title bar): hide, display mode, reset height
- 200ms ease-in-out animation for collapse/expand
- `isFlexFill`: Panel fills remaining space (e.g., Timeline)

### PanelContainer (`src/ui/panels/PanelContainer.h/cpp`)
Flex column layout container with draggable splitters.
- Manages panel heights and splitter positions
- Collapsed panels only show title bar (24px)
- Splitters between visible panels for resizing

### PanelTitleBar (`src/ui/panels/PanelTitleBar.h/cpp`)
Panel header with title and controls.
- Left-click: toggle collapse
- Right-click: context menu
- [−] button: collapse panel
- [+] button: expand panel

### Concrete Panels
- `TimelinePanel` - Main arrangement view (flex-fill)
- `MixerPanel` - Volume/fader strips (resizable)
- `PianoPanel` - MIDI keyboard (resizable)

## Signal Flow

```
External MIDI Device
    └─> AudioDeviceManager.addMidiInputDeviceCallback()
            └─> MidiMessageCollector
                    └─> processBlock(MidiBuffer)

On-screen Keyboard (MidiKeyboardState)
    └─> addListener(MidiMessageCollector)
            └─> processBlock(MidiBuffer)

Audio Callback
    └─> AudioProcessorPlayer.audioDeviceIOCallbackWithContext()
            └─> Track::processBlock()
                    └─> PluginHost::processBlock()
                            └─> pluginInstance->processBlock(audio, midi)
```

## Naming Conventions
- `vibedaw` namespace for all application code
- `*Component` suffix for UI components
- `*Manager` suffix for subsystem controllers
- `*Host` suffix for plugin wrapper classes
- Private members use trailing underscore convention or `pImpl` idiom via JUCE macros

## Build
- CMake-based build system
- Requires JUCE installed via CMake FetchContent or system install
