# T02: MIDI Arrangement Playback

Status: todo | Milestone: M1 | Depends on: T01, T03, T06

## Outcome

The first audible arrangement: edited MIDI clips play through their assigned instruments, alongside live audition, without stuck notes.

## Read First

- `src/core/ChannelMixer.h`, `ChannelMixer.cpp`, `TransportState.h`
- `src/project/Track.h`, `ClipInstance.h`, `Clip.h`, `Note.h`, `Channel.cpp`
- T03's unit/routing contract and T06's snapshot/ownership APIs

The mixer currently sends live MIDI only to the active channel. It has no arrangement scheduler. Note edits can invalidate vector sorting, so do not assume UI note order is a valid render event order.

## Implementation Checklist

- [ ] Compile source notes plus placement start/end into destination-tagged render events outside audio. Resolve stable IDs, sanitize timing/velocity/channel values, and ignore invalid references deterministically.
- [ ] Define clip-local-to-arrangement conversion, source/placement truncation, muted source/instance/note behavior, and zero-duration/zero-velocity policy. MIDI channel 1-16 is distinct from instrument destination ID.
- [ ] Schedule half-open block intervals `[start, end)` with a documented beat-to-sample rounding rule. Events on the next block boundary belong to the next block; no negative or out-of-range offsets.
- [ ] Aggregate all placements by destination, merge live MIDI into its audition destination, and process each plugin once per block. Clear consumed MIDI without dropping releases.
- [ ] Track active arrangement notes by destination, MIDI channel, and pitch. Choose and test an explicit policy for overlapping same-pitch voices and for collisions with live notes; ordinary MIDI cannot uniquely address duplicate same-pitch voices.
- [ ] Define deterministic ordering for simultaneous note-offs and note-ons. Clamp note ends to placement/source boundaries according to T03's policy.
- [ ] Flush affected active notes on stop, seek, mute, source/instance deletion, route change, plugin/channel removal, or snapshot replacement that invalidates them. Muted rendering must still deliver cleanup before suppressing audio.
- [ ] For M1, seeking into a sustained note waits for its next attack; document that note chasing is deferred. Do not emit orphan releases or carry stale active-note state.
- [ ] Add fake-processor/event-capture tests for scheduling and routing. Keep correctness tests independent of commercial plugins.
- [ ] Exercise the UI workflow from clip creation through placement and audible playback using the watcher; record actual observations separately from offline test results.

## Acceptance Checks

- [ ] A four-beat clip with notes at known beats produces the expected sample offsets at 120 BPM, including events exactly on block start/end.
- [ ] One source placed twice plays twice; two destinations on one track and one destination across two tracks route correctly.
- [ ] Selecting a different audition instrument does not change arrangement routing. Live keyboard playback still works while stopped and playing.
- [ ] Short notes, simultaneous chords, overlapping same-pitch notes, muted notes, placement truncation, and unsorted edits follow documented policies.
- [ ] Stop/seek/delete/mute/reassign while a long note is sounding leaves no stuck notes or invalid memory access.
- [ ] Tempo changes, sample-rate changes, multiple block sizes, and dense events remain deterministic and respect T06's capacity bounds.

## Audible Checkpoint

At completion, a user can write, place, play, stop, and edit MIDI music. Do not hold this checkpoint for loop UI or mixer polish. Transport looping, clip repetition, recording, and audio clips remain out of scope.

## Completion Record

- Changed files: pending
- Event ordering/overlap/cleanup policies: pending
- Offline verification: pending
- Watcher/manual playback observations: pending
- Unverified checks/blockers: pending
- Handoff to T04/T05: pending
