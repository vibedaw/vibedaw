# T01: Audio-Clocked Transport

Status: todo | Milestone: M1 | Depends on: T06

## Outcome

Audio owns playback time. Play, stop, seek, and tempo commands produce coherent block timing even if the UI stalls.

## Read First

- `src/core/TransportState.h`, `TransportState.cpp`
- `src/core/ChannelMixer.h`, `ChannelMixer.cpp`
- `src/core/AudioEngine.cpp`, `src/Main.cpp`
- `src/ui/MainContent.h`, `MainContent.cpp`, `TransportComponent.cpp`
- `src/ui/timeline/TimeRuler.cpp`, `TimelineContent.cpp`

Transport currently lives in `MainContent`; its timer advances seconds at 30 Hz. The existing audio-style `processBlock` has no caller, and its loop path can synchronously notify UI listeners. Do not just call it from audio unchanged.

## Implementation Checklist

- [ ] Move authoritative playback state to an engine-owned lifetime, using T06's command/publication contract. Keep a UI-facing facade if useful, not a second clock.
- [ ] Capture block-start position, sample rate, tempo, time signature, playing state, and discontinuity revision once per block. Specify scheduling-before-advancement order for T02.
- [ ] Advance from rendered samples only while playing; preserve fractional musical position without long-term rounding drift. A tempo change preserves the musical position rather than preserving elapsed seconds.
- [ ] Remove wall-clock advancement from `MainContent::timerCallback`; UI polls published state and repaints on the message thread.
- [ ] Define controls: play resumes; stop holds position and requests note cleanup; rewind seeks to zero; seek works stopped or playing and publishes a discontinuity. Preserve or deliberately document any changed shipped control semantics.
- [ ] Feed transport into the mixer entry point that already runs on audio; avoid installing a second device callback or advancing time twice.
- [ ] Add a visible arrangement playhead and basic ruler seek or beat-position input. Convert presentation seconds using the shared timing contract.
- [ ] Validate tempo/time signature/seek positions; handle prepare/release and sample-rate changes coherently. Keep recording disabled or explicitly unavailable until recording exists.
- [ ] Add deterministic transport tests using sample counts, not wall-clock sleeps.

## Acceptance Checks

- [ ] At 48 kHz and 120 BPM, rendering 96,000 samples from beat zero advances exactly four quarter-note beats within a documented numerical tolerance.
- [ ] Different block sizes and a final partial block produce the same final position.
- [ ] Stopped blocks do not advance. Play/stop/seek commands apply at the documented boundary and generate cleanup/discontinuity signals for T02.
- [ ] A tempo change preserves beat position and changes subsequent advancement; a sample-rate change produces no position jump.
- [ ] UI stalls do not alter progression. The playhead reflects audio-published state, not an independent prediction clock.
- [ ] No UI/listener callbacks execute on audio and no second transport writer remains.

## Boundaries

No clip rendering yet. Establish discontinuity hooks now; T02 consumes them for note cleanup. Loop splitting and click rendering belong to T05. Do not enable the old overshoot-discarding loop logic as an interim implementation.

## Completion Record

- Changed files: pending
- Block timing/control API: pending
- Verification performed: pending
- Unverified checks/blockers: pending
- Handoff to T02/T05: pending
