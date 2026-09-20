# VibeDAW Roadmap

## Current Handoff

| Task | Status | Scope |
| --- | --- | --- |
| [T21 MIDI Recording](tasks/T21-midi-recording.md) | initial implementation wrapped; manual testing deferred | Shared recorder, expressive MIDI, clip workspace and four recording modes; advanced requirements deferred |

The user requested wrapping up and deferring the harder parts. T21's completion
record defines the delivered initial scope and limitations. Offline tests pass;
hardware/plugin/native acceptance remains in [TOTEST](TOTEST.md). No further
implementation task is active. The original full design is not claimed complete.

## Previous Work Closed

The previous implementation workstream is closed; testing is deferred, not
declared observed or passed. [T15 mixer routing](tasks/T15-mixer-routing-model.md)
and [T20 UI overhaul](tasks/T20-ui-overhaul.md) are implementation-complete with
manual acceptance deferred. Their existing completion records remain intact.

- [Testing handoff](TOTEST.md): grouped manual checks and known defects to revisit.
- [Full roadmap archive](ROADMAP_ARCHIVE.md): unchanged prior roadmap, including
  its uncommitted contents, decisions, verification records and acceptance history.
  It stays in this directory so its relative task links still resolve.
- September 10, 2026 acceptance was a user override, not evidence that every
  runtime check was observed. Earlier actual observations remain distinct in
  the archive and testing handoff; that override did not accept later T15 work.
- T10's editor-originated plugin restart bypass of audio quiescence remains an
  open safety defect. Closing the old workstream does not fix or waive it away.

## Working Rules

Keep implementation, offline verification, manual observation and user acceptance
separate. Record results only when performed or reported. Do not build or launch
the application; the watcher owns that. Future implementation may use the
independent offline test project, with native/device checks tracked separately.

Audio recording, sample playback, automation, sends/effects, bus chains, general
undo/redo and advanced clip transforms remain future work, not completed features
or prerequisites for T21. T21's local replacement recovery is not general undo.
