# T05: Loop and Metronome

Status: todo | Milestone: M1 | Depends on: T01, T02, T04

## Outcome

A user can set a musical loop and play along with an audible, sample-timed click. This closes M1.

## Read First

- `src/core/TransportState.h`, `TransportState.cpp`, `ChannelMixer.h`, `ChannelMixer.cpp`
- `src/ui/TransportComponent.cpp`, `src/ui/timeline/TimeRuler.cpp`
- T01's block timing contract, T02's note cleanup, T04's master stage

Existing loop logic wraps only after a full block and drops overshoot. Metronome currently has only UI/state; click rendering must be implemented, not just enabled.

## Implementation Checklist

- [ ] Add usable loop-start/end editing in beats/bars, plus a visible loop region. Numeric fields are enough; do not require drag handles. Validate finite nonnegative start and end greater than start.
- [ ] Split transport/scheduler traversal into segments at exact loop boundaries while keeping MIDI offsets relative to the original block. Preserve overshoot and support more than one wrap in a block within an explicit minimum-loop/capacity policy.
- [ ] Do not process plugins separately per timeline segment unless the render contract requires it; aggregate scheduled events into the original block where possible.
- [ ] At each wrap, release notes from the old segment and trigger new attacks deterministically. Avoid duplicated events at loop start/end. Sustains crossing loop end are cut for M1, not implicitly tied.
- [ ] Define enabling/disabling/changing the loop while playing, including a cursor outside the new range. Apply changes at a safe block boundary and use the same discontinuity/cleanup rules as seek.
- [ ] Generate a short bounded click without file loading or callback allocation. Schedule it at sample offsets and keep its tail across blocks.
- [ ] Use quarter-note model units, with one click per notated denominator beat (`4 / denominator` quarter notes) and an accent every numerator beats. For M1, 6/8 is six eighth-note clicks, not compound dotted-quarter grouping.
- [ ] Click only while playing and enabled, including with no instrument channels. Inject before master gain/mute; channel mute/solo does not suppress it. Disable clears any pending click tail.
- [ ] Test wrap segmentation, note ordering, click offsets/tails, tempo/time-signature changes, and loop edits with offline rendering.

## Acceptance Checks

- [ ] A four-bar loop repeats with no drift at different tempos, sample rates, and buffer sizes.
- [ ] A boundary inside a block, exactly at its end, and multiple boundaries in one block preserve elapsed samples without duplicate/missing attacks.
- [ ] Held notes at loop end are released; repeated passes, seeks, and loop edits do not accumulate voices.
- [ ] Clicks are sample-timed, accented correctly in 4/4, 3/4, and 6/8, and aligned with notes/playhead after wrap or seek.
- [ ] Disabled/stopped metronome is silent; enabled metronome works with zero channels and obeys master mute/gain.
- [ ] Complete the roadmap's M1 release checklist and record any remaining blocker rather than declaring the milestone finished.

## Completion Record

- Changed files: pending
- Loop edit/minimum-length/click policies: pending
- Offline verification: pending
- Watcher/manual M1 workflow observations: pending
- Unverified checks/blockers: pending
- M1 release checklist result: pending
