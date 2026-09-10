# T20: UI Visual Overhaul

Status: in_progress | Milestone: M4 (new) | Depends on: none (T17 explicitly out of scope)

Progress 2026-09-10: All phases (0-6) are implemented and the offline suite
passes (fresh build, all checks green). Remaining: the final acceptance sweep.
Handoff notes: `src/ui/Theme.h` is
header-only (JUCE 7's `juce::Colour(uint32)` is not constexpr, so tokens are
`inline const`, not `inline constexpr`); the mixer strip width 72 -> 96 is
asserted in `tests/offline_tests.cpp:2576` (updated to 97 per-strip pitch);
Clips/Mixer child-index contracts are preserved (filter tabs and M/S toggles
use painted hit-testing, not child components); a new `raised` Theme token
covers 0xff252525. Phase 6 note: CPU% flows `AudioEngine::getCpuUsage()` ->
`MainWindow` -> `MainContent` (wiring change in `src/Main.cpp`); RAM reads
`/proc/self/statm` (Linux-guarded, hidden otherwise); both labels refresh at
1 Hz off the existing 30 Hz `MainContent` timer.

Planned 2026-09-10 from a ChatGPT-generated mockup of the VibeDAW shell (user:
"how we could even get starting implementing these improvements"). A source audit
of `src/ui/**` compared the mockup against current behaviour; gaps and existing
foundations are recorded per phase below. Planning decisions recorded by the user:

- Scope: all phases (0-6) in this pass.
- Audio waveform previews: deferred entirely. `AudioClip::setWaveform`
  (`src/project/Clip.cpp:66`) is never called and the engine has no audio clip
  playback, so no waveform data exists anywhere in the model. Waveform rendering
  arrives with real audio support, not as display-only plumbing.
- Colour centralization: full sweep in Phase 0, not lazy per-phase migration.
- BPM editing interaction: covered by T17 (`tempo drag scrub + inline edit`);
  this task restyles the transport bar only and changes no transport interaction.

Only the independent offline_tests target may be built/run; no application
build/launch (watcher owns that).

## Outcome

The UI reads as one cohesive theme matching the mockup: labeled transport
readouts, clips that preview their contents in timeline lanes, mixer strips with
proper knobs and scaled meters, clip cards with metadata and mini previews,
octave/velocity controls on the piano, and a status bar with CPU/RAM. Public
component shapes used by the offline suites (`ClipRow(clipId, clip, index)`,
`LevelMeter::tick()` test access, `TransportComponent`) stay stable; where a
signature must change, `tests/offline_tests.cpp` is updated in the same commit.

## Read First

- `src/ui/TransportComponent.{h,cpp}` — `TimeDisplay` paint (bars:beats:ticks,
  24px mono green), `TempoControl`/`TimeSignatureControl` boxes, `resized`
  layout; `setDisplayMode` (timecode toggle) currently dead code.
- `src/ui/timeline/TimelineLane.cpp:48-96` — `drawClips`; the existing primitive
  note preview (2px lines, linear pitch mapping under the text rows).
- `src/ui/mixer/{MixerStrip,MasterStrip,LevelMeter}.{h,cpp}`,
  `src/ui/panels/MixerPanel.cpp` — strip layout (72px), flat knob, stereo
  `LevelMeter` with decay timer, 33 ms meter/control polling.
- `src/ui/sidebar/clips/ClipsSidebar.cpp` — text-only `ClipRow`s; constructor
  shape used by tests at `tests/offline_tests.cpp:573,785`.
- `src/ui/PianoComponent.cpp` — `MidiKeyboardComponent` subclass; dead
  `setOctaveRange()`; no velocity control.
- `src/core/MixerState.h` — `StereoMeter` (sample-peak envelope, revision
  counter, `poll()` pattern) consumed by every meter in the UI.
- `tests/offline_tests.cpp` — suites compile the real UI sources
  (`tests/CMakeLists.txt:15-28`); mixer/timeline/transport components are
  constructed directly.

## Implementation Checklist

### Phase 0: Theme foundation + full colour sweep

- [x] New `src/ui/Theme.{h,cpp}`: named colour constants (background tiers,
      borders, text primary/muted, accent green, record red, add-green, meter
      palette) plus shared paint helpers (rounded panel fill/border, section
      header). No `LookAndFeel` rewrite of stock widgets unless a phase needs it.
- [x] Mechanical sweep of every inline `juce::Colour(0xff...)` literal in
      `src/ui/**` to Theme tokens. Collapse true duplicates; keep semantically
      distinct greys (lane background vs panel background) as separate tokens.
      Zero behaviour change expected; suites must pass untouched.
- [x] Cosmetic alignment of `Panel`/`PanelTitleBar` chrome (title bar styling,
      spacing) with the mockup.

### Phase 1: Transport bar restyle (no interaction changes)

- [x] `TimeDisplay`: BAR / BEAT / TICK caption labels under the digit groups
      (mockup "4 : 4 : 441"), keeping the mono-green rendering.
- [x] Transport buttons: segmented pill group with hover/active states; record
      button gains the red accent but remains the disabled stub.
- [x] Right side: larger labeled boxed readouts ("TIME SIGNATURE", "BPM")
      replacing the current 50/70x28 boxes. Existing menus, listeners, and the
      `MainContent` 30 Hz position poll unchanged.
- [x] Wire the unused `setDisplayMode` timecode toggle to a click on
      `TimeDisplay` (bars:beats:ticks <-> h:mm:ss.xx), or remove the dead mode
      if it tests poorly; either way, no silent dead code remains.

### Phase 2: Clip rendering

- [x] New shared painter `src/ui/components/ClipMiniPreview.h`: draws a mini
      piano-roll preview — pitch range fitted to the clip's min/max note
      (padded, minimum span), rounded note rects with velocity alpha, faint
      black-key row shading behind. Takes notes, bounds, clip colour, muted.
- [x] `TimelineLane::drawClips`: replace the 2px-line block (lines 80-94) with
      the painter; keep name/destination overlay, unresolved and muted logic,
      and all hit-testing/drag behaviour untouched (paint-only change).
- [x] Audio/Pattern clips: styled body + name only (waveforms deferred; see
      planning decisions).

### Phase 3: Mixer strips

- [x] Widen strips 72 -> ~96px (`MixerPanel::layoutStrips`).
- [x] `MixerStrip` repainted: compact header (name + dB), plugin/FX button, pan
      knob redrawn with arc track + value arc + centre detent, M/S row, fader
      with dB scale ticks (0/-6/-12/-24/-48/-60), meter column.
- [x] `LevelMeter`: optional dB tick overlay + numeric peak-hold readout;
      preserve the decay timer, `tick()` test access, and the zero-if-no-update
      pattern exactly.
- [x] `MasterStrip` restyled to match (name, mute, fader + meter, dB readout).
- [x] Keep `pollMeter`/refresh wiring and the revision-counter poll unchanged.

### Phase 4: Clips sidebar cards

- [x] `ClipRow` -> ~56px card: name + kebab (opens the existing
      context menu), metadata line ("N bars - M notes" for MIDI; audio gets a
      placeholder line), right-side mini preview via the Phase 2 painter,
      selected accent border.
- [x] Preserve the `ClipRow(clipId, clip, index)` public shape used by
      `tests/offline_tests.cpp`; extend without breaking it.
- [x] MIDI/AUDIO segmented filter tabs above the list (row filtering only).

### Phase 5: Piano, Channel Rack, Browser

- [x] `PianoPanel`: left control column — octave -/value/+ stepper reviving
      `setOctaveRange()` (drives `setLowestVisibleKey`), velocity slider wired
      to `MidiKeyboardComponent::setVelocity`; keyboard fills the remainder.
- [x] `ChannelRackSidebar` rows: icon tile, name, compact M/S, small horizontal
      level meter polling `Channel::getMeter()` on its own ~30 Hz timer
      (mirroring `MixerPanel`'s timer pattern), kebab menu.
- [x] `BrowserSidebar`: search box filtering all three section trees; row icons
      for plugin/sample/preset entries.

### Phase 6: Status bar

- [x] CPU%: new `AudioEngine::getCpuUsage()` wrapping
      `AudioDeviceManager::getCpuUsage()`; polled via the existing `MainContent`
      timer but label-throttled to ~1 Hz.
- [x] RAM: process RSS from `/proc/self/statm` (Linux-guarded, field hidden
      gracefully where unavailable).
- [x] MIDI device green-dot indicator driven by `MidiManager` open state.

## Acceptance Checks

- [ ] No inline hex colour literals remain in `src/ui/**`; every colour routes
      through Theme tokens.
- [ ] Timeline clips show a pitch-fitted note preview that matches what the
      piano roll plays; muted/unresolved clips stay visually distinct; drag,
      select, and seek gestures behave exactly as before.
- [ ] Mixer strips show scaled meters with peak readouts and a readable knob;
      meter motion, decay, and master behaviour are unchanged (T04 suites pass
      unmodified apart from layout-driven updates).
- [ ] Clips sidebar cards carry metadata + preview; drag-to-arrange, double-click
      edit, and context menus still work (T12/T14 suites pass).
- [ ] Piano octave stepper and velocity slider affect the on-screen keyboard
      immediately; external MIDI path unaffected (T09).
- [ ] Status bar shows live CPU% and RAM without leaking file handles or
      polling overhead (1 Hz class), and degrades gracefully off Linux.
- [ ] `cmake --build /tmp/opencode/vibedaw-tests --target offline_tests --parallel 4`
      and `ctest --test-dir /tmp/opencode/vibedaw-tests --output-on-failure`
      pass after every phase (re-run `cmake -S tests -B /tmp/opencode/vibedaw-tests`
      only when test sources/CMake change).
