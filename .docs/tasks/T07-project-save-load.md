# T07: Project Save and Load

Status: done | Milestone: M2 | Depends on: M1, M1.5

Acceptance cleared 2026-09-10: user override ("assume everything is complete.. clear
this backlog") marks this task done; its pending watcher/manual observations (native
FileChooser flows, prompts, editors across load, plugin state through a real
restart, audible workflow) are accepted by the override rather than individually
observed. Unchecked boxes in this file are cleared by the same override. See the
backlog clearance record in `.docs/ROADMAP.md`.

## Outcome

Save a musical sketch, close it, reopen it, and hear the same arrangement and instrument settings. This follows T10-T14's interaction milestone, prioritized by user feedback on 2026-09-08.

## Read First

- `src/project/Project.h`, `Project.cpp`, `Settings.h`, `Settings.cpp`
- `src/project/Channel.h`, `ChannelList.h`, `Track.h`, `ClipPool.h`, `ClipInstance.h`, `Clip.h`, `Note.h`
- `src/plugins/PluginHost.h`, `PluginHost.cpp`
- `src/ui/MainContent.cpp`, `src/ui/MainWindow.cpp`
- T10's hosted-editor lifecycle contract and T12/T14's placement/action contracts

Existing JSON settings store preferences, not sessions. JUCE plugin instances already expose `getStateInformation`/`setStateInformation`; the missing work is application-level capture, storage, and restore.

## Implementation Checklist

- [x] Introduce an explicit versioned project-file format separate from device/sidebar settings. Record schema and unit conventions in the repository; do not use application settings as the session store. (`.docs/PROJECT_FORMAT.md`, `src/project/ProjectDocument.{h,cpp}`, version 1 JSON)
- [x] Serialize stable channel IDs/order, names, plugin identity and state, channel/master controls, tracks, pooled MIDI clips/notes, instance source/destination references, tempo, time signature, and loop settings. (metronome state included as a transport preference; clip-local `loopEnabled` reserved flag serialized for completeness)
- [x] Preserve shared clip identity and ID allocation after load. Do not serialize pointers, transient selection, active-note state, or playing/recording state as a running session. (`restoreClip`/`restoreChannel` advance the monotonic counters past restored IDs; playing/recording/position/selection are excluded)
- [x] Identify the specific plugin, not only its bundle path; a bundle may contain multiple plugin descriptions. Capture/restore state using a safe plugin lifecycle outside audio, with controlled processing quiescence if necessary. (`PluginHost` retains the resolved `PluginDescription` at load; `createFromDescription` re-instantiates; capture/restore run under `AudioQuiescence::Edit`)
- [x] Store opaque plugin state losslessly. Missing plugins become visible unresolved channels that preserve their identity/blob for later recovery rather than discarding data. (base64 blobs; `Channel::MissingPlugin` recovery record; re-save round-trips it)
- [x] Parse and validate into a temporary model before replacing the current project. Reject malformed references/values/versions with an actionable error; a failed load must leave current work intact. (`ProjectDocument::stage` builds `Staged`; `Project::prepareLoad`/`commitLoad` is two-phase)
- [x] Save through a temporary file plus safe replacement and surface write failures. Do not report success until the file is safely written. (`juce::TemporaryFile` in the target directory; both write and move results checked)
- [x] Add New/Open/Save/Save As actions, a current project path/name, and dirty tracking for notes, placements, plugin state where supported, and mix changes. Prompt before discarding unsaved work. (status-bar File menu + Ctrl+N/O/S/Ctrl+Shift+S; dirty tracking rides existing model listener plumbing; `confirmAsync` prompts before New/Open and quit)
- [x] On successful load, stop/flush the old render state, safely publish the new project, and rebind UI listeners/selections. Never leave editors pointing into deleted clip storage. (commit sequence: `transport.stop()` -> quiescence Edit -> clip-editor teardown via the existing `clipWillBeRemoved` path plus explicit `closeAllClipEditors` -> clear+repopulate in place -> normal notifications rebuild panels)
- [x] Include T10 plugin-editor teardown and invalidate in-flight drags/context-menu targets on project replacement. New drag/menu mutation paths participate in dirty tracking just like numeric edits. (T10's registered-window teardown runs inside channel destruction under quiescence; T12/T14 targets re-resolve by stable ID and structural rebuilds cancel gestures; all mutations mark dirty through model notifications)
- [x] Add round-trip, corruption, missing-plugin, and failed-replacement tests. Use fake plugin state for ordinary tests; real VST restore is a separate manual check. (`projectFileTests`; fake `StatefulInstrument`)

## Acceptance Checks

- [ ] Save/reopen two channels with different plugin parameters and a shared clip placed multiple times; notes, timing, routing, mix, and loop settings match.
- [ ] Newly created objects after loading receive non-colliding IDs. Reorder still preserves routing.
- [ ] Missing plugin, invalid JSON, unsupported schema, truncated state, and unwritable destination are handled without losing the current session or recoverable state.
- [ ] Cancelled file selection or discard confirmation changes nothing. A reopened project starts stopped, without hanging notes or stale editor references.
- [ ] Plugin state survives a real restart when checked through the existing development workflow; otherwise record that check as pending.

## Boundaries

No autosave/history, sample embedding, cloud storage, or backward-compatibility machinery for hypothetical formats. Introduce migrations only after a format has actually shipped or persisted user data requires them. Audio-device preference restoration is a separate follow-up.

## Completion Record

- Changed files/schema reference: `.docs/PROJECT_FORMAT.md` documents schema v1
  and unit conventions. New: `src/project/ProjectDocument.{h,cpp}`
  (serialize/stage/writeToFile, `Staged` temp model). Changed: `Project.{h,cpp}`
  (dirty tracking, document state, two-phase load, `applyStaged` commit,
  `pluginRestorer_` seam, Project now listens to TrackList/ClipPool/Transport +
  per-track change messages), `ChannelList`/`ClipPool` (`restoreChannel`/
  `restoreClip` advancing monotonic ID counters), `Track`/`ClipInstance`
  (restoreable UUID ctor params), `Channel` (`MissingPlugin` recovery record),
  `PluginHost` (retained `PluginDescription`, `createFromDescription`),
  `Constants.h` (`.vibedaw` extension), `MainContent.{h,cpp}` (File menu button,
  hotkeys, discard prompts, editor teardown, FileChooser seam in
  `components/FileDialog.h`), `MainWindow` (project title), `Main.cpp`
  (dirty-aware quit prompt), both CMakeLists.
- Plugin lifecycle/missing-plugin policy: plugin identity is captured at load
  time (the specific `PluginDescription`, not just the bundle path) and state
  extraction/restore happen under `AudioQuiescence::Edit` on the message
  thread. On load, a plugin that cannot be re-instantiated leaves the channel
  visible and unresolved with `Channel::MissingPlugin` (description + opaque
  blob); re-saving preserves it. A corrupt base64 blob rejects the file
  outright; a present plugin receiving a (possibly truncated) blob applies it
  best-effort, matching JUCE plugin tolerance.
- Verification performed: independent offline build succeeded; full CTest 1/1
  passed (7.2 s) with all earlier suites intact; `git diff --check` passed.
  `projectFileTests` covers serialize/parse round-trips (incl. plugin identity
  and opaque state bytes), two-phase load guarantees, validation rejections
  (unsupported version, duplicate IDs, corrupt blobs, out-of-range values),
  missing-plugin recovery with blob-preserving re-save, failed saves
  (directory target, no path) keeping dirty state honest, ID non-collision and
  reorder routing after load, and dirty-tracking transitions. See
  `tests/README.md` T07 section.
- Unverified checks/blockers: watcher/manual acceptance pending for the five
  acceptance boxes (native FileChooser flows, discard prompts, real plugin
  editors on load, plugin state through a real restart, audible workflow).
  Real-VST3 restore cannot run in the offline suite (VST3 format disabled
  there); the restorer seam mirrors the default path. Metronome state is
  serialized as a transport preference (a deliberate extension of the
  checklist list). In-flight T12 drag cancellation on project replacement is
  covered by the existing structural-rebuild contract, not a new dedicated
  regression.
- Handoff to T09: schema doc, validation error catalogue, and dirty-tracking
  wiring are in place for T09 integration coverage; audio-device preference
  restoration remains the recorded follow-up.
