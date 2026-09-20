# T15: Mixer Routing Model

Status: done (implementation complete; manual acceptance deferred to [TOTEST](../TOTEST.md)) | Milestone: Later (post-M2) | Depends on: M2

## Approved Direction

The user explicitly rejected one instrument per mixer channel, selected one
initial mixer channel with add/remove buttons and direct-Master instrument
defaults, and authorized fixing this now. This extends the original discovery-only
task to the scoped implementation below. No external dependency sources are edited.

- Arrangement tracks hold placements; each placement still targets an instrument
  by `ChannelId`. Track moves, source editing and live audition are unchanged.
- Instruments host plugins and choose one audio output: Master or an independent
  mixer channel. Multiple instruments can feed the same mixer channel.
- Mixer channels exist independently: new projects have one, adding instruments
  creates none, and deleting the last mixer channel is allowed. Empty mixer
  channels and empty mixer collections are saved faithfully.
- Mixer Add Channel and Remove Selected buttons manage destinations. Strip
  selection is mixer-local; right-click Rename labels the destination. Rack rows
  expose Output buttons and an Audio Output context submenu.
- Removing a mixer channel reroutes its instruments to Master. Removing an
  instrument does not remove a mixer channel. Maximum 128 of each, independently.
- Mixer gain (0..2), linear stereo balance (-1..1), mute, additive solo and meters
  apply after summing assigned instrument audio. Muting wins over solo. Mono
  output ignores balance. Master gain/mute and metering remain post-sum.
- Mixer mute/solo affects audio only, not MIDI note ownership. Mixer solo excludes
  non-solo destinations and direct-Master instruments; metronome bypasses solo.
  Existing instrument gain/pan/mute/solo stays upstream (including instrument
  note suppression), preserving old project sound and rack controls.
- Bus chains, effects, sends, sidechain inputs, multi-output instruments and
  routing automation are explicitly deferred, not implied by this implementation.

Retaining paired instrument strips, or adding group buses behind mandatory paired
strips, was rejected because neither provides the requested independent mixer.

## Safety And Persistence

- Stable mixer IDs are separate from instrument IDs and array indexes. Ordinary
  additions never reuse removed IDs; delayed UI actions re-resolve both endpoints.
- Destination buffers are preallocated for the bounded mixer capacity. Each
  instrument still processes once per block; no routing-time callback allocation.
- Mixer structure and assignments change under quiescence. The audio-only
  `PreserveVoices` edit mode keeps held notes and queued MIDI across skipped
  callbacks; rendering/transport briefly pauses. Any nested destructive edit
  upgrades the whole outer edit to reset-on-rejection. Pending panic/overflow is
  never cleared by a preserving edit. Plugin lifecycles retain their existing gate.
- V2 project files store independent destinations, controls and output IDs. V1
  remains loadable: its formerly inert mixerTrackId is ignored, instrument mix
  controls remain intact, outputs go to Master, and one empty mixer is seeded.
- Invalid V2 destinations, IDs, duplicate IDs, counts and controls fail staging
  before live mutation. Save/load retains the existing in-place model owners.

## Completion Record

- Implemented: model/ownership and route validation, preallocated audio summing,
  independent mixer UI and rack outputs, dirty tracking, V2 persistence and V1
  migration. Changes are in `src/core`, `src/project`, `src/ui`, internal docs
  and tests only; JUCE/dependency source is unchanged.
- Offline build: `cmake -S tests -B /tmp/opencode/vibedaw-routing-tests -DCMAKE_BUILD_TYPE=Debug`
  then `cmake --build /tmp/opencode/vibedaw-routing-tests --target offline_tests --parallel 4`.
- Verification: full `ctest --test-dir /tmp/opencode/vibedaw-routing-tests --output-on-failure`
  passed (1/1, 11.80 seconds); `git diff --check` passed.
- Coverage: independent lifecycle/IDs/capacity, sample-exact summed controls/meters,
  mono/direct-Master/solo/cancellation, held notes through rerouting/removal,
  real callback rejection with queued MIDI and nested destructive edits, process
  once/no callback C++ allocation, UI actions/pop-out layout/stale targets,
  V2 round trip/zero destinations, V1 migration and malformed-load atomicity.
- Independent read-only review found an engine panic on audio-only edits; fixed
  with preserving quiescence and real-callback regressions. Follow-up review
  found no substantive issues. Tests do not certify external plugins or OS scheduling.
- No application build/launch, dependency edits, staging or commit performed.

## Remaining Acceptance

- [ ] In the running app, verify new project has one mixer channel plus Master;
  adding instruments leaves that count unchanged and their outputs read Master.
- [ ] Add/rename a mixer channel, assign two instruments to it, and hear combined
  fader/pan/mute/solo behavior with truthful meters and unaffected audition selection.
- [ ] Reroute held notes; remove an assigned mixer channel (including the last),
  confirming audio returns to Master without stuck notes or instrument deletion.
- [ ] Check native output menus, narrow-window scrolling and mixer pop-out/dock.
- [ ] Save/reopen with real plugin state and shared routes; open an existing V1
  sketch and confirm its sound is retained. Bus chains/sidechain remain deferred.
