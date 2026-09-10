# VibeDAW Project File Format

Reference for the versioned project document introduced by T07. This format is
separate from `~/.config/vibedaw/settings.json` (device/sidebar preferences);
projects are user-chosen `.vibedaw` files. Current version: **1**
(`ProjectDocument::currentVersion`).

Files are pretty-printed JSON written through a temporary file plus safe
replacement. Loading parses and validates the entire document into a temporary
model before any live mutation; a failed load never changes the current session.

## Unit conventions

- All timing (`startBeats`, `durationBeats`, note times, loop bounds) is in
  quarter-note beats, independent of tempo — the T03 model contract.
- `gain`/`volume` are linear multipliers 0..2; `pan` is -1..1 (linear stereo
  balance, unity at centre).
- `velocity` 0..127, `pitch` 0..127, MIDI `channel` 1..16.
- Colours are 6- or 8-digit hex (`RRGGBB` or `AARRGGBB`).

## Never serialized

Playing/recording state, playhead position, transient selections (clip/instance
note selection), active-note/voice ledgers, waveform caches, meter state, or
audio-device connections. A reopened project always starts stopped.

## Schema

```json
{
  "application": "vibedaw",
  "formatVersion": 1,
  "channels": [
    {
      "id": 0,
      "name": "Alpha",
      "type": "instrument",
      "volume": 0.75,
      "pan": -0.25,
      "muted": false,
      "solo": true,
      "mixerTrackId": 3,
      "colour": "ff6a6aff",
      "sampleFile": "/path/to/sample.wav",
      "plugin": {
        "description": {
          "name": "TyrellN6",
          "descriptiveName": "u-he TyrellN6",
          "format": "VST3",
          "fileOrIdentifier": "~/.vst3/u-he/TyrellN6.vst3",
          "manufacturer": "u-he",
          "version": "1.0",
          "category": "Instrument",
          "uid": 1234567,
          "isInstrument": true,
          "numInputChannels": 0,
          "numOutputChannels": 2,
          "hasSharedContainer": false
        },
        "state": "<base64>"
      }
    }
  ],
  "master": { "gain": 1.0, "muted": false },
  "clips": [
    {
      "id": 0, "type": "midi", "name": "Sketch", "colour": "ff6ad94a",
      "startBeats": 0.0, "durationBeats": 4.0, "loopEnabled": false,
      "notes": [
        { "pitch": 60, "startBeats": 0.0, "durationBeats": 1.0, "velocity": 100, "channel": 1 }
      ]
    },
    { "id": 1, "type": "audio", "name": "tone", "colour": "ff4a90d9",
      "startBeats": 0.0, "durationBeats": 8.0, "file": "/path/to/audio.wav" },
    { "id": 2, "type": "pattern", "name": "Pattern", "colour": "ffd94a6a",
      "startBeats": 0.0, "durationBeats": 4.0, "patternLength": 1.0, "loopCount": 1 }
  ],
  "tracks": [
    {
      "id": "7823d61a1f1a4f00a06afc8016bc4120",
      "name": "One",
      "height": 80,
      "colour": "ffaaaaaa",
      "instances": [
        { "id": "35995e3f08c14d6098259211dbd2b947", "clipId": 0, "channelId": 0,
          "startBeats": 8.0, "durationBeats": 4.0, "muted": false }
      ]
    }
  ],
  "transport": {
    "tempo": 120.0,
    "numerator": 4,
    "denominator": 4,
    "loop": { "exists": true, "enabled": true, "startBeats": 0.0, "endBeats": 16.0 },
    "metronome": false
  }
}
```

Notes on fields:

- `channels[].id` — stable `ChannelId` (non-negative, unique). The loader
  advances the channel ID counter past the highest restored ID, so newly
  created channels never collide with restored ones.
- `channels[].type` — `"instrument"` or `"sampler"`. `sampleFile` is only
  meaningful for samplers.
- `channels[].plugin` — omitted when the channel never had a plugin. When a
  plugin is present in the session its live description is captured (bundle
  paths alone are ambiguous; the description identifies the specific plugin)
  and `state` is the opaque vendor blob (`getStateInformation`, base64). When a
  plugin could not be re-instantiated on load, the channel stays as a visible
  unresolved channel and keeps this entry in memory (`Channel::MissingPlugin`)
  so re-saving preserves it for recovery.
- `clips[].notes[].startBeats`/`durationBeats` — clip-local beats; pooled clip
  `startBeats` is the source header field (notes themselves use local beat
  zero).
- Instance `clipId`/`channelId` may dangle (e.g. after a pooled source was
  deleted); unresolved references are preserved as placeholders rather than
  dropped, per T03's model contract.
- `tracks[].id` and `instances[].id` are session UUIDs restored across loads
  (T12 handoff); they must be unique within their own collection in the file.
- `loop.exists == false` means no loop region at all (T16 semantics);
  `exists: true` with `enabled: false` is the set-but-disabled state.
- Track/instance UUIDs, names and colours are cosmetic/session identity; mixed
  with the stable integer IDs they survive save/load so in-session references
  remain meaningful.

## Validation rules (all enforced before commit)

- `formatVersion` must equal the current version exactly; anything else is
  rejected with an actionable error (no forward compatibility machinery).
- Integer/boolean/string types, range checks (volume 0..2, pan -1..1, tempo
  20..300, meter numerator 1..32 with denominator in {2,4,8,16}, pitch/velocity
  bounds), finiteness of all numbers, and non-empty/positive durations.
- IDs non-negative and unique per collection; track/instance UUIDs non-empty
  and unique; channel count at most `ChannelList::maxChannels` (128); plugin
  `state` must decode as base64; loop spans must satisfy
  `TransportState::validLoopRegion`.
- Malformed base64, duplicate IDs, out-of-range values, or missing sections
  reject the whole file; the live session is untouched in every failure case.

## Implementation map

- Format model + serialize/stage/write: `src/project/ProjectDocument.{h,cpp}`
- Document state (dirty tracking, New/Open/Save/Save As, two-phase load):
  `src/project/Project.{h,cpp}`
- Restore support (exact-ID channels/clips, restoreable track/instance UUIDs,
  missing-plugin recovery, description-based plugin creation):
  `src/project/ChannelList.{h,cpp}`, `src/project/ClipPool.{h,cpp}`,
  `src/project/Track.{h,cpp}`, `src/project/ClipInstance.{h,cpp}`,
  `src/project/Channel.{h,cpp}`, `src/plugins/PluginHost.{h,cpp}`
- UI actions (File menu, hotkeys Ctrl+N/O/S/Ctrl+Shift+S, discard prompts):
  `src/ui/MainContent.{h,cpp}`, `src/ui/MainWindow.{h,cpp}`, `src/Main.cpp`