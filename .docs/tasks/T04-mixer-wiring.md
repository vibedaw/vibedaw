# T04: Mixer Wiring

Status: todo | Milestone: M1 | Depends on: T02, T06

## Outcome

The mixer shows real instrument channels and controls their sound. Master controls and meters reflect the actual summed output.

## Read First

- `src/ui/MainContent.cpp`, `src/ui/panels/MixerPanel.h`, `MixerPanel.cpp`
- `src/ui/mixer/MixerStrip.cpp`, `MasterStrip.cpp`, `LevelMeter.cpp`
- `src/core/ChannelMixer.h`, `ChannelMixer.cpp`
- `src/project/Channel.h`, `Channel.cpp`, `ChannelList.cpp`

`MixerPanel` is constructed without project/control wiring and defaults to decorative strips/sends. Its meter update reads channel zero as master. Channel audio has gain/pan/mute but no solo or real master stage.

## Implementation Checklist

- [ ] Populate strips from actual channels, including names, colours, current values, and stable IDs. Handle add/remove/reorder and zero channels without fabricating instruments.
- [ ] Connect gain/pan/mute and audition selection through T06's handoff. Ensure strip selection actually invokes its callback and model-to-UI refresh does not feed changes back.
- [ ] Align model/UI gain ranges and labels. Use a continuous pan law, including exactly centred pan; cover mono output safely.
- [ ] Implement channel solo with explicit precedence: a muted channel remains silent even when soloed; if any channel is soloed, only unmuted soloed channels are audible. Multiple solos are allowed.
- [ ] Preserve note cleanup and processor lifecycle when muted/solo-suppressed. Do not simply skip a plugin and withhold note-offs. Define unmute behavior for already-held notes consistently with T02.
- [ ] Add master gain/mute after channel summing. Make a defined injection point for T05's metronome before master gain/mute.
- [ ] Publish per-channel and true post-master stereo meter levels from audio. Fix mute/stale-meter behavior and block-size-dependent decay; avoid reading channel zero as master.
- [ ] Hide/disable unsupported sends and bus controls rather than inventing a new bus system. Remove fake fixed-channel assumptions.
- [ ] Test gain/pan/mute/solo/master with fake processors and offline buffers.

## Acceptance Checks

- [ ] Changing a fader audibly affects only its instrument; master gain affects the complete mix. Rebuilding strips preserves values.
- [ ] Channel reorder/removal does not change which instrument a strip controls. External channel changes appear in the mixer.
- [ ] Single/multiple solo and mute combinations follow the stated policy without hanging voices on transitions.
- [ ] Two independent channel outputs produce an accurately summed master signal and truthful meters. Empty/muted states decay to silence.
- [ ] Pan changes continuously through zero; mono/stereo buffers and meter calculations remain finite.
- [ ] Mixer remains usable in a narrow window and with enough channels to require scrolling.

## Completion Record

- Changed files: pending
- Gain/pan/mute/solo/master policies: pending
- Verification performed: pending
- Unverified checks/blockers: pending
- Handoff to T05/T07: pending
