# T07: Project Save and Load

Status: backlog | Milestone: M2 | Depends on: M1

## Outcome

Save a musical sketch, close it, reopen it, and hear the same arrangement and instrument settings. This is the first post-M1 priority.

## Read First

- `src/project/Project.h`, `Project.cpp`, `Settings.h`, `Settings.cpp`
- `src/project/Channel.h`, `ChannelList.h`, `Track.h`, `ClipPool.h`, `ClipInstance.h`, `Clip.h`, `Note.h`
- `src/plugins/PluginHost.h`, `PluginHost.cpp`
- `src/ui/MainContent.cpp`, `src/ui/MainWindow.cpp`

Existing JSON settings store preferences, not sessions. JUCE plugin instances already expose `getStateInformation`/`setStateInformation`; the missing work is application-level capture, storage, and restore.

## Implementation Checklist

- [ ] Introduce an explicit versioned project-file format separate from device/sidebar settings. Record schema and unit conventions in the repository; do not use application settings as the session store.
- [ ] Serialize stable channel IDs/order, names, plugin identity and state, channel/master controls, tracks, pooled MIDI clips/notes, instance source/destination references, tempo, time signature, and loop settings.
- [ ] Preserve shared clip identity and ID allocation after load. Do not serialize pointers, transient selection, active-note state, or playing/recording state as a running session.
- [ ] Identify the specific plugin, not only its bundle path; a bundle may contain multiple plugin descriptions. Capture/restore state using a safe plugin lifecycle outside audio, with controlled processing quiescence if necessary.
- [ ] Store opaque plugin state losslessly. Missing plugins become visible unresolved channels that preserve their identity/blob for later recovery rather than discarding data.
- [ ] Parse and validate into a temporary model before replacing the current project. Reject malformed references/values/versions with an actionable error; a failed load must leave current work intact.
- [ ] Save through a temporary file plus safe replacement and surface write failures. Do not report success until the file is safely written.
- [ ] Add New/Open/Save/Save As actions, a current project path/name, and dirty tracking for notes, placements, plugin state where supported, and mix changes. Prompt before discarding unsaved work.
- [ ] On successful load, stop/flush the old render state, safely publish the new project, and rebind UI listeners/selections. Never leave editors pointing into deleted clip storage.
- [ ] Add round-trip, corruption, missing-plugin, and failed-replacement tests. Use fake plugin state for ordinary tests; real VST restore is a separate manual check.

## Acceptance Checks

- [ ] Save/reopen two channels with different plugin parameters and a shared clip placed multiple times; notes, timing, routing, mix, and loop settings match.
- [ ] Newly created objects after loading receive non-colliding IDs. Reorder still preserves routing.
- [ ] Missing plugin, invalid JSON, unsupported schema, truncated state, and unwritable destination are handled without losing the current session or recoverable state.
- [ ] Cancelled file selection or discard confirmation changes nothing. A reopened project starts stopped, without hanging notes or stale editor references.
- [ ] Plugin state survives a real restart when checked through the existing development workflow; otherwise record that check as pending.

## Boundaries

No autosave/history, sample embedding, cloud storage, or backward-compatibility machinery for hypothetical formats. Introduce migrations only after a format has actually shipped or persisted user data requires them. Audio-device preference restoration is a separate follow-up.

## Completion Record

- Changed files/schema reference: pending
- Plugin lifecycle/missing-plugin policy: pending
- Verification performed: pending
- Unverified checks/blockers: pending
- Handoff to T09: pending
