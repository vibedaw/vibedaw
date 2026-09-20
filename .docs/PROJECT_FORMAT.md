# VibeDAW Project File Format

Reference for the versioned project document introduced by T07. This format is
separate from `~/.config/vibedaw/settings.json` (device/sidebar preferences);
projects are user-chosen `.vibedaw` files. Current version: **3**
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
  "formatVersion": 3,
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
  "mixerChannels": [
    {
      "id": 3,
      "name": "Keys",
      "volume": 0.9,
      "pan": 0.0,
      "muted": false,
      "solo": false,
      "colour": "ff6a6aff"
    }
  ],
  "master": { "gain": 1.0, "muted": false },
  "clips": [
    {
      "id": 0, "type": "midi", "name": "Sketch", "colour": "ff6ad94a",
      "startBeats": 0.0, "durationBeats": 4.0, "loopEnabled": false,
      "notes": [
        { "pitch": 60, "startBeats": 0.0, "durationBeats": 1.0, "velocity": 100, "channel": 1 }
      ],
      "expressionEvents": [
        { "beat": 0.0, "status": 176, "data1": 64, "data2": 127 },
        { "beat": 0.5, "status": 224, "data1": 0, "data2": 64 },
        { "beat": 1.0, "status": 176, "data1": 64, "data2": 0 }
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
- `channels[].mixerTrackId` is an active destination in v2/v3: `-1` means direct
  Master; otherwise it must reference an existing `mixerChannels[].id`. New
  instruments default to direct Master. Multiple instruments may share the same
  mixer destination. Per-instrument volume, pan, mute and solo remain separate
  controls and are preserved; routing does not move them onto the destination.
- `mixerChannels` is a required independent collection, not a mirror of
  instruments or arrangement tracks. Each entry has a stable immutable
  `MixerChannelId`, name, volume, pan, mute, solo and colour. Mixer IDs use a
  separate namespace from instrument IDs and are never indexes. Every mixer
  channel feeds Master; there are no bus chains, effects, sends or sidechains.
- New projects start with one empty mixer channel named `Mixer 1`. Removing a
  destination routes its instruments directly to Master before deletion. The
  final mixer channel may be removed: `"mixerChannels": []` saves and reloads as
  zero destinations, without inserting a default. Clearing instruments does not
  clear mixer channels.
- Mixer additions do not reuse deleted IDs during the owner's lifetime. Exact-ID
  project restore advances the allocation counter past restored IDs without
  resetting it. Allocation counters themselves are not serialized.
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
- MIDI `clips[].expressionEvents` is required in v3, including when empty.
  Each event stores a clip-local `beat` and exact raw MIDI `status`, `data1`,
  `data2` integers. CC status is `0xB0..0xBF` (176..191); pitch bend status is
  `0xE0..0xEF` (224..239). The low status nibble encodes MIDI channel minus one;
  no separate channel field is stored. Both data bytes are 0..127. For CC,
  `data1` is the controller number and `data2` its value. For pitch bend, `data1`
  is the low seven bits and `data2` the high seven bits (0..16383, centre 8192).
  Other MIDI message types, including notes, are not expression events.
- Expression arrays are in nondecreasing beat order. Array order at equal beats
  is significant and preserved, including duplicate messages, so controller
  sequences are never reordered by status/channel/value. Model insertion and
  replacement sort stably by beat; the file reader rejects descending times.
  Events beyond the source duration are retained (like notes after trimming),
  not clamped or discarded. Scheduling/playback belongs to render snapshots.
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

## Version migrations

The reader accepts v1, v2 and v3; the writer always emits v3. V1/v2 MIDI clips
stage with empty expression arrays; any unknown `expressionEvents` property in
those older versions is ignored, not interpreted as v3 data. Existing note
validation still applies. V2 mixer routing and controls are unchanged in v3.

In v1, `channels[].mixerTrackId` was an inert saved integer. Loading v1 resets
every instrument destination to direct Master (`-1`), regardless of that saved
value, and seeds one empty default mixer channel (ID `0`, name `Mixer 1`, unity
volume, centre pan, mute/solo off). Per-instrument volume, pan, mute and solo,
Master settings, plugin state and arrangement data remain unchanged. This
prevents old metadata from silently changing the audible routing. V1 does not
require a `mixerChannels` section.

## Validation rules (all enforced before commit)

- `formatVersion` must be `1`, `2` or `3`; anything else is rejected with an actionable
  error (no forward compatibility machinery).
- Integer/boolean/string types, range checks (volume 0..2, pan -1..1, tempo
  20..300, meter numerator 1..32 with denominator in {2,4,8,16}, pitch/velocity
  bounds), finiteness of all numbers, and non-empty/positive durations.
- IDs non-negative and unique per collection; track/instance UUIDs non-empty
  and unique; channel count at most `ChannelList::maxChannels` (128); plugin
  `state` must decode as base64; loop spans must satisfy
  `TransportState::validLoopRegion`.
- Instrument and mixer IDs must be in `0..INT_MAX-1`; integer parsing rejects
  overflow before narrowing. Mixer count is bounded independently by
  `ChannelList::maxMixerChannels` (128). Duplicate mixer IDs, missing v2/v3 mixer
  destinations, destination values other than `-1` or a declared mixer ID,
  non-finite/out-of-range mixer controls and malformed mixer entries reject the
  entire file before session mutation. Shared destination references are valid.
- Each MIDI clip is bounded independently to `MidiClip::maxNotes` (65536) notes
  in all versions and `MidiClip::maxExpressionEvents` (65536) expression events
  in v3. These storage limits do not guarantee a particular render-block event
  budget. Expression beats must be finite and in
  `0..TransportState::maxPositionBeats` (1e9), inclusive. Status must encode CC
  or pitch bend on channel 1..16, and both data bytes must be integers in
  0..127. Missing fields/arrays, malformed event objects, fractional/overflowing
  MIDI bytes, unsupported statuses, excessive counts and descending event beats
  reject the entire v3 document. Note times retain their existing finite,
  nonnegative start / positive duration / finite end validation.
- Malformed base64, duplicate IDs, out-of-range values, or missing sections
  reject the whole file; the live session is untouched in every failure case.
  `ProjectDocument::stage` also leaves its caller's previous `Staged` value
  unchanged on failure, assigning the new value only after full validation.

## Implementation map

- Format model + serialize/stage/write: `src/project/ProjectDocument.{h,cpp}`
- Expression source model: `MidiExpressionEvent` and
  `MidiClip::{getExpressionEvents,addExpressionEvent,setExpressionEvents,clearExpressionEvents}`
  in `src/project/Clip.{h,cpp}`. Successful expression mutations notify the
  existing clip listeners/dirty tracking; clearing an already empty array is a
  no-op. `replaceContent(notes, expressionEvents)` validates both before replacing
  either and emits one note-pointer invalidation followed by one clip change.
  It does not change source duration or provide cross-thread synchronization.
  Cloning preserves both collections. Load application uses
  `ClipData::expressionEvents` alongside `ClipData::notes`.
- Document state (dirty tracking, New/Open/Save/Save As, two-phase load):
  `src/project/Project.{h,cpp}`
- Restore support (exact-ID channels/clips, restoreable track/instance UUIDs,
  missing-plugin recovery, description-based plugin creation):
  `src/project/ChannelList.{h,cpp}`, `src/project/ClipPool.{h,cpp}`,
  `src/project/Track.{h,cpp}`, `src/project/ClipInstance.{h,cpp}`,
  `src/project/Channel.{h,cpp}`, `src/plugins/PluginHost.{h,cpp}`
- Independent mixer controls/meters: `src/core/MixerState.h`; ownership,
  exact-ID restore and validated routing: `src/project/ChannelList.{h,cpp}`.
  On load, `Project` clears/repopulates mixers before restoring instruments,
  under `AudioQuiescence::Edit`, without swapping the model owner objects.
- UI actions (File menu, hotkeys Ctrl+N/O/S/Ctrl+Shift+S, discard prompts):
  `src/ui/MainContent.{h,cpp}`, `src/ui/MainWindow.{h,cpp}`, `src/Main.cpp`
