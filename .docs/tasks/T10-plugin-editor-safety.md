# T10: Restore Plugin Editing Safely

Status: in_progress | Milestone: M1.5 | Depends on: T06 implementation; runtime acceptance retained

Started 2026-09-08 under explicit user request to resume from T10. Earlier runtime
acceptance blocks remain unchanged; only the independent offline target is permitted.

## Current Override (2026-09-08)

The user explicitly requested: "just roll back the juice change and put the plugin
button back, it was working well enough for intiial testing". This supersedes the
fail-closed implementation and handoff below, which are retained as historical
investigation records. T10 remains in_progress, not fully safe or fully accepted.

- Restored root `FetchContent_MakeAvailable(JUCE)` at the original 7.0.12 pin and
  CMake 3.16 minimum. Offline tests use the existing unmodified checkout directly.
- Removed the unfinished private-copy CMake patcher, HostRestartLatch, adapter source
  checks, private restart API calls, Channel restart polling and AudioEngine's new
  device-service timer. Prior device preparation and voice-reset paths are retained.
- PluginHost now forwards actual hasEditor/createEditor calls under short edit
  guards, refuses a second active editor owner and focuses/reuses its registered
  window. Registered windows close before plugin reload or host destruction;
  user close clears editor ownership under an edit guard.
- The visible Plugin button resolves the active channel by stable ID. The rack's
  context action resolves its clicked ID through a lifetime-checked owner. Empty,
  unloaded and genuinely editorless targets remain disabled with explanations.
- Offline tests now cover enabled capabilities/selection and a synthetic editor
  component with no native peer, rather than requiring universal editor denial.

### Remaining Risk

Stock JUCE 7.0.12 can perform private editor-originated restart mutations before
notifying the host. Guards around probing, opening and closing do not cover later
editor actions or autonomous plugin restarts. Concurrent processing may block,
layout/MIDI changes are not reconciled by a host restart service, and device versus
message-thread lock ordering remains unresolved. Stopping transport is not audio
quiescence. This limitation is explicitly tolerated for initial testing only.
Native focus/reopen, real-plugin configuration, replacement/deletion/shutdown with
open editors, active MIDI/restarts and device changes still require observation.
No application build or launch is authorized, and none is claimed.

### Rollback Verification

Executed for this rollback request:

```sh
cmake -S tests -B /tmp/opencode/vibedaw-t06-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/vibedaw-t06-tests --target offline_tests --parallel 2
ctest --test-dir /tmp/opencode/vibedaw-t06-tests --output-on-failure
git diff --check
```

Configure/build succeeded. The initial run exposed stale fake-processor expectations:
it reported hasEditor=true but returned null, which stock createEditorIfNeeded
asserts against. Removed those inconsistent creation calls from capability-only
tests; actual successful creation/duplicate-owner rejection remains covered by
EditorInstrument. Rebuilt and reran: full CTest 1/1 passed (6.36 seconds), including
prior T03/T06/T01/T02/T04/T05 suites and callback allocation checks. Diff whitespace
check passed. Source audit found no remaining private restart APIs, latch includes,
patch macros/imports or adapter test registration in application/test code.

The existing JUCE source checkout is clean and reports exact tag 7.0.12.
Generated private copies from the cancelled attempt are not imported by either
CMake project; no unknown/generated files were deleted.
Root CMakeLists, AudioEngine.{h,cpp} and Channel.{h,cpp} have no diff against HEAD
after removing only the unfinished integration. Prior roadmap/T07-T14 planning
edits and T01-T06 implementation are preserved. MainContent wiring and native
window focus/close paths were source-reviewed, not exercised in a native window.
No application configure/build/launch, native VST/device run, staging or commit.

## Historical Plan and Investigation (Superseded)

## Outcome

Configure the selected instrument through a visible Plugin button and channel context action without reopening the audio-lifecycle safety gap. This is the next implementation task, not a claim that M1 has passed runtime acceptance.

## Read First

- `T06-safe-audio-boundary.md`, especially its editor restriction and ownership contract
- `src/plugins/PluginHost.h`, `PluginHost.cpp`, `PluginWindow.h`, `PluginWindow.cpp`
- `src/ui/MainContent.h`, `MainContent.cpp`
- `src/ui/sidebar/channel/ChannelRackSidebar.cpp`
- `src/core/AudioBoundary.h`, `src/project/Channel.cpp`
- JUCE's actual hosted editor/restart implementation used by this checkout

The status-bar Plugin button remains disabled, `openPluginWindow()` explains the
current target's restriction, and `PluginHost::hasEditor/createEditor` deny access.
The restriction is intentional: editor-originated lifecycle/restart callbacks
bypass quiescence. Restoring only the UI would be incorrect.

## Implementation Checklist

- [x] Trace editor creation, parameter changes, restart requests, preparation, replacement, and destruction through JUCE and our host. Record affected threads and the concrete safe interception strategy before enabling editors.
- [ ] Route or defer lifecycle mutations through T06's ownership boundary without waiting, allocating, destroying objects, or invoking UI from our audio callback. Stopping transport alone is not quiescence.
- [x] If the hosted restart path cannot be safely controlled, retain the restriction and record a concrete blocker rather than bypassing it.
- [ ] Restore a visible Plugin button reflecting the selected channel, with an explanatory disabled state for no plugin/no supported editor. Add the equivalent channel context action, shared with T14.
- [ ] Bind editor windows to stable channel/plugin lifetime, not reorderable indexes or current selection. Reopening an already-open editor should focus it rather than create duplicate windows.
- [ ] Close/detach editors safely before replacement, channel deletion, project replacement, and shutdown. Keep selection changes from retargeting an existing window.
- [x] Add synthetic lifecycle/ownership and button binding regressions; retain separate real-plugin watcher checks. Coverage is of denial and existing guarded destruction, not enabled editor restart behavior.

## Acceptance Checks

- [ ] Open, configure, close, and reopen the selected instrument's editor; parameter changes affect the intended plugin.
- [ ] Switching/reordering channels does not retarget windows or configure the wrong instrument.
- [ ] Restart requests and open/close while playback/live MIDI are active do not cause unsafe access, deadlock, or stranded notes; any controlled silence is documented.
- [ ] Replacement, load failure, channel deletion, and shutdown with open editors leave no stale references or windows.
- [x] Offline lifecycle regressions pass; real VST behavior explicitly remains pending. This does not satisfy the enabled-editor acceptance checks above.

## Boundaries

No general plugin-host redesign, effects chain, or promise that arbitrary third-party code is real-time safe. Do not relax T06's callback contract to restore a button. T07 owns persisted plugin state.

## Completion Record

Independent review follow-up: added a MainContent-owned `juce::TooltipWindow`
so the Plugin button's selected-channel restriction explanation can actually be
displayed. Source-reviewed only; visible hover behavior remains a watcher check.

### Result and Changed Files

2026-09-08: completed the feasible fail-closed portion, not editor restoration.
T10 was marked in_progress before implementation and is now blocked on an actual
adapter integration as well as native acceptance. No JUCE dependency was edited.

- `src/ui/components/PluginButton.h`: actual message-thread Project-listening button,
  labelled Plugin, always disabled, with stable-ID selected-target lookup and reasons
  for no selection, no loaded instance, and restart restriction. No retained host or
  channel pointer, and no hosted capability query. Destruction removes its listener.
- `src/ui/MainContent.{h,cpp}`: use that button and the same target-specific reason
  for direct `openPluginWindow()` calls. No editor is constructed by this action.
- `src/ui/sidebar/channel/ChannelRackSidebar.{h,cpp}`: right-click builds a disabled
  Open Plugin Editor entry with the clicked channel's explanation. Right-click does
  not change audition selection. Menu data owns text, not a plugin callback; it can
  outlive row rebuild/deletion without performing a stale action. Enabled action and
  native-window focus/reuse remain deliberately unimplemented.
- `src/plugins/PluginHost.cpp`: document why even hasEditor must remain fail-closed;
  existing false/null enforcement is unchanged.
- `tests/{CMakeLists.txt,offline_tests.cpp,README.md}`: compile the real rack/sidebar
  in the independent target, exercise the actual PluginButton and menu data, extend
  synthetic capability/destruction probes, and document verification limits.
- `.docs/ROADMAP.md` and this task: authorization, lifecycle findings, partial result,
  remaining implementation blocker and handoff. Earlier task records/statuses and
  unrelated workspace changes are preserved.

### Lifecycle Findings

Source references below are in the existing `build/_deps/juce-src/modules/`
checkout, pinned by the root CMakeLists to JUCE 7.0.12. Format references abbreviate
`juce_audio_processors/format_types/juce_VST3PluginFormat.cpp`.

1. Load/ownership: PluginHost loads under AudioQuiescence::Edit, checks initial
   mono/stereo channel totals, and owns the adapter instance. Channel::setPlugin
   replaces it under the same recursive writer gate, clearing delivery/reset state
   and preparing a new instance if the channel is prepared. Both browser load paths
   construct a candidate first; candidate failure preserves the installed instance.
   Direct PluginHost::loadPlugin instead clears its own old instance before loading.
2. Capability/creation: Format:2996-3012 and :3527 onward call controller createView
   even from hasEditor unless an active editor already exists. createEditor makes a
   VST3PluginWindow around that view. Thus a UI capability refresh is not a passive
   read and cannot be enabled independently. T10 never calls either hosted API.
3. Parameters: Format:3680-3702 performs editor edits through setValueNotifyingHost;
   VST3Parameter::setValue at :2282-2286 updates cached values and pushes JUCE's
   parameter dispatcher. Plugin-originated restart requests are a separate path,
   not something a parameter listener can safely intercept before mutation.
4. Restart timing: Format:3719-3727 asserts the message thread and forwards flags to
   ComponentRestarter. `juce_VST3Common.h:1791-1830` ORs flags atomically, executes
   immediately on the message thread, and uses AsyncUpdater for other threads.
   This corrects any assumption that all editor restarts are deferred. Off-thread
   requests are tolerated by JUCE but trigger an assertion and may post a message;
   arbitrary plugin requests from processBlock are not proven real-time safe.
5. Private restart handler: Format:3738-3771 resets for kReloadComponent, releases
   and prepares for kIoChanged, updates latency/MIDI mapping/parameter values, then
   notifies AudioProcessorListeners. These calls target the internal adapter, not
   our PluginHost overrides. A host listener is too late. Mappings (:2516-2523),
   prepare/release (:2562-2639), reset (:3040-3051), and processBlock (:2662-2669)
   use the same blocking SpinLock. A concurrent audio call can spin for the entire
   lifecycle operation. Stopping transport does not stop live device processing.
6. Preparation: JUCE prepareToPlay requires the message thread, takes a
   MessageManagerLock and the process spinlock, changes buses and allocates mappings.
   Host-triggered preparation is gated, but AudioEngine::audioDeviceAboutToStart
   calls prepare from device lifecycle without an explicit message-thread marshal.
   A future restart gate must also resolve this lock order: a device writer holding
   quiescence while waiting for the message manager can conflict with a UI restart
   waiting for that writer. Do not simply insert another writer lock and assume
   the existing device path is safe. This is source-observed risk, not a reproduced
   native deadlock.
7. Destruction: PluginHost::closeWindows deletes registered wrappers under its edit
   guard before releasing/deleting the instance. Wrapper destruction clears its
   editor before unregistering; user close currently deletes the wrapper directly
   without a guard, safe only because hosted editors are denied. JUCE native editor
   destruction (:1528-1548) calls removed/setFrame(nullptr), editorBeingDeleted and
   releases the view. Adapter cleanup (:2398-2432) runs on the message thread,
   asserts no active editor, releases resources, disconnects controllers and clears
   the component handler. ComponentRestarter cancels pending AsyncUpdater work in
   its destructor. None of this is a tested enabled-window lifetime in our host.
8. Shutdown/project: Main destroys the main UI, detaches audio/mixer, then destroys
   Project/channels/hosts, closing any registered wrapper before its instance dies.
   Project::shutdown currently saves settings only; it is not a project replacement
   operation. T07 must supply explicit teardown for future project load. Selection
   and reorder do not move host objects, but existing public PluginWindow creation
   has no singleton/focus policy. T10 does not claim that missing behavior is done.

### Exact Blocker and Required Solution

The stock JUCE 7.0.12 public host API has no before/after restart hook. The private
VST3HostContext and ComponentRestarter own the mutation. ExtensionsVisitor
(Format:2481-2506) exposes IComponent/preset/ARA operations, not that handler or
the separate edit-controller instance. Replacing a component handler through a
guessed interface is not a general solution and would risk losing JUCE's parameter,
gesture and context-menu handling. A wrapper override cannot intercept internal
virtual calls on the adapter. Polling, suspendProcessing, or a guard around editor
creation alone leaves the rest of the window lifetime/restart path uncovered.

The concrete next implementation is a maintained, version-pinned JUCE adapter hook
(or an upstream equivalent), before **all** restart mutations and before acquiring
the adapter process lock. Non-message-thread requests must only latch bounded flags,
with non-audio servicing, not acquire AudioQuiescence::Edit from audio. On the
message thread, close admission, drain an admitted block, apply restart under a
defined device/message-thread lock order, revalidate mono/stereo buses and prepared
rate/block size, reconcile Channel note/reset state, and reopen admission. Invalid
layouts must leave the destination silent, not pass oversized layouts to stereo
scratch. Coalesced flags and queued callbacks must be invalidated at instance
destruction. Guard native creation/probing/close as well, then add one window per
host with focus-on-reopen. Never hold a writer lock for an entire editor lifetime.

That adapter/device-lifecycle integration is not supplied here: it is beyond a
safe application-side interception with the checkout's public API. Editing only
the generated build dependency would not be a reproducible solution. A maintained
hook also needs an independent adapter-enabled synthetic regression target; the
currently permitted target compiles VST3 out and cannot validate such a patch.
Access therefore remains disabled at every existing UI/host/wrapper entry point.
This restriction removes editor-driven exposure, not a guarantee that plugins can
never issue autonomous restart requests while their editor is closed.

### Verification

Executed only the documented independent commands:

```sh
cmake -S tests -B /tmp/opencode/vibedaw-t06-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/vibedaw-t06-tests --target offline_tests --parallel 2
ctest --test-dir /tmp/opencode/vibedaw-t06-tests --output-on-failure
git diff --check
```

Configure/build succeeded; full CTest 1/1 passed with earlier T03/T06/T01/T02/T04/T05
suites retained. The new `pluginEditorRestrictionTests` checks empty/selected/loaded
states, coalesced plugin/name updates, stable selection through reorder, target
menu data independent of selection and after row/channel destruction, repeated
denied editor API calls even when prepared/playing, failed candidate load preserving
the host, replacement/deletion/project destruction under quiescence and off audio,
and listener teardown before a queued model update. All synthetic hosted capability
and creation call counters remain zero. Existing callback allocation probes and
assertion/leak failure criteria remain enabled. Source review covered MainContent
integration (not compiled into this target), JUCE lifecycle and shutdown ordering.

These tests do **not** simulate a working JUCE restart interceptor or open an editor.
No application configure/build/launch, watcher observation, native popup/window,
audio device, installed VST test, sanitizer run, staging or commit was performed.
Manual open/configure/reopen, intended-plugin parameter changes, active MIDI/restart,
focus/reorder, replacement/failure/deletion and shutdown acceptance all remain
pending and cannot pass until the adapter blocker is resolved. Native layout and
disabled-tooltip/menu presentation also remain unobserved.

### Handoff

- T11 remains todo with its T10 dependency visible; this turn does not silently
  authorize proceeding past the implementation blocker. Its browser/drop changes
  must preserve candidate-before-replacement and editor denial.
- T14 can reuse the target-specific reason and ChannelRow menu builder. Do not turn
  entry 1 into an enabled callback until the lifecycle hook and stable window
  ownership are implemented; delayed actions must re-resolve lifetime-safe targets.
- T07 must close editors before swapping project/channel ownership and cancel
  per-instance pending restart work. Settings-only shutdown is not that contract.
- Next action for T10: implement and offline-test the maintained adapter hook plus
  device preparation lock ordering, then restore enabled UI/window ownership and
  collect watcher/native VST observations. Do not mark done on synthetic denial
  tests alone. Earlier runtime acceptance statuses are unchanged.
