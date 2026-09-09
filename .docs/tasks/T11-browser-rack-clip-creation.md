# T11: Browser-to-Rack and Clip Creation

Status: blocked | Milestone: M1.5 | Depends on: T10

Started 2026-09-08 under explicit user authorization to continue T11 end to end
despite pending earlier manual acceptance. Preserve T10's stock-JUCE initial-testing
editor override and all dirty work. Only independent offline tests are authorized;
no application build/launch, dependency modification, staging or commit.

## Outcome

Drag an instrument from Browser into empty Channel Rack space to create a usable channel. Creating a pooled MIDI clip selects it without interrupting the workflow with Piano Roll.

## Read First

- `src/ui/sidebar/browser/PluginSection.cpp`, `BrowserSidebar.cpp`
- `src/ui/sidebar/channel/ChannelRackSidebar.cpp`
- `src/ui/sidebar/clips/ClipsSidebar.cpp`
- `src/ui/MainContent.h`, `MainContent.cpp`
- `src/project/Project.cpp`, `Channel.cpp`, `ChannelList.cpp`
- `src/ui/editor/ClipEditorWindow.cpp`, `src/plugins/PluginHost.cpp`

Existing rows accept plain plugin-path drags and replace only after successful loading. Browser double-click creates an active instrument channel; empty rack space is not a target. New Clip currently creates/selects a four-beat source and immediately opens Piano Roll.

## Implementation Checklist

- [x] Introduce explicit plugin drag payload typing and validate it at every affected consumer. Coordinate the clip payload with T12; do not classify arbitrary strings as plugins.
- [x] Make empty rack/background space, including an empty rack, a create-channel drop target. Show a labeled create preview distinct from existing-row replacement highlighting.
- [x] Load/validate before committing creation; respect the 128-channel bound, supported layouts, and quiescent plugin lifecycle. Failure or cancellation must not leave an orphan channel or change active selection.
- [x] On success, create one instrument channel, assign the loaded plugin, name it meaningfully, and make it active. Share the existing browser double-click creation path where practical.
- [x] Preserve failure-safe existing-row replacement. Make create versus replace deterministic, including row edges and add-button space; prevent bubbling from performing both operations.
- [x] Reject unsupported payloads without misleading acceptance highlights. Do not imply that unimplemented preset/sample operations work.
- [x] Change New Clip to create/select only. Keep double-click and an Edit context action as explicit editor entry points; reuse/focus an existing source editor rather than duplicating it.
- [x] Preserve editor teardown before source removal and shared-source identity. Add focused creation, drop dispatch, failure, and editor-opening regressions.

## Acceptance Checks

- [ ] Dropping a plugin into an empty or populated rack's unused area creates exactly one active channel with that plugin.
- [ ] Dropping on a row replaces only that row's plugin, with a distinct preview; failure preserves the previous plugin.
- [ ] Invalid payload, channel limit, plugin failure, and drag cancellation create no partial channel or selection change.
- [ ] New Clip adds/selects a four-beat source without opening a window, placing an instance, or assigning a destination.
- [ ] Double-click/Edit opens the correct source once; source deletion safely closes its editor.
- [ ] Watcher checks cover visible previews, empty-rack targeting, and plugin loading; unavailable runtime observations remain pending.

## Boundaries

No sampler rendering, preset implementation, or automatic timeline placement. T12 adds pooled-clip dragging; T14 completes context menus. Preserve stable channel identities and T06's safe mutation path.

## Completion Record

### Result and Files

2026-09-08: implementation and independent offline verification complete; blocked
on unobserved watcher/manual acceptance, not marked done. Earlier statuses and
T10's stock-JUCE initial-testing editor override are unchanged.

- `src/ui/DragPayload.h`: shared typed plugin encoder/decoder, explicit unknown
  rejection and preserved existing sample-file assignment payload.
- `src/ui/sidebar/browser/PluginSection.cpp`: real tree-item producer emits typed payload.
- `src/project/Project.{h,cpp}`: shared candidate-first create/replace path, stable
  target lookup, preflight bound, guarded validation/commit, private fake-loader seam.
- `src/ui/sidebar/channel/ChannelRackSidebar.{h,cpp}`: background target, distinct
  labeled feedback, deterministic row/add-button boundaries and shared load path.
- `src/ui/sidebar/clips/ClipsSidebar.{h,cpp}` and `src/ui/MainContent.cpp`: explicit
  target-safe Edit action, selection-only creation, existing-window focus/reuse.
- `tests/{CMakeLists.txt,offline_tests.cpp,README.md}`, this task and roadmap:
  actual browser/rack/clip component regressions, evidence and pending checks.

### Interaction Contract

- Plugin payload is a JUCE object `{ type: "vibedaw.plugin", path: <absolute path string> }`.
  Bare strings (including paths or numeric IDs), unknown objects, malformed fields,
  preset payloads and future `vibedaw.clip` objects are not plugin actions. No path
  existence check is done at plugin hover; the actual loader owns validation.
- Empty rack and unused content space, including the add-button area, create.
  Visible row rectangles are half-open and exclusively replace. Parent dispatch
  checks payload/capacity interest without interpreting coordinates; JUCE discovers
  accepting children first. Enter/move/drop use the shared target-local geometry
  helper and cannot preview or create over a row or outside the rack. Rows are clipped above
  the bottom button strip so a short/full rack cannot give that strip two meanings.
  This does not add rack scrolling; existing overflow navigation remains limited.
- Creation uses blue `Create instrument channel` feedback and changes the add-button
  label while hovering. Rows use green `Replace plugin: <channel>` feedback.
  Exit/cancellation and rebuild clear preview state. At 128 channels background
  rejects with no acceptance highlight; Add Channel's existing limit tooltip remains.
- Both rack creation and browser double-click use `Project::loadPlugin(path)`.
  Rack replacement uses the same method with a stable target ChannelId. Candidate
  load and mono/stereo validation precede any model commit; candidate destruction,
  creation and installation stay under AudioQuiescence::Edit. Missing targets and
  capacity are checked before loading and rechecked at commit. Replacement keeps
  channel identity/name, selection and the previous last-created-plugin setting.
  Successful creation names from the loaded plugin and selects exactly one new
  instrument channel. Failed loads show actionable feedback without orphan creation.
- Existing `sample://` file assignment to rows remains supported for existing
  absolute files and is labeled `Assign sample file (no playback)`. It does not
  create a channel, render samples, or remove an installed plugin. Presets are not
  accepted/highlighted. No unimplemented preset or sample playback action was added.
- New Clip creates/selects a four-quarter-beat pooled MIDI source only. It neither
  places an instance nor assigns a destination nor requests Piano Roll. Pool
  double-click and context Edit select/open the target source. Delayed Edit actions
  use a SafePointer to the owner and re-resolve ClipId after row rebuild/deletion.
  MainContent re-resolves the source, restores/focuses an already-open matching
  ClipId window, and creates only when absent. Existing synchronous
  `clipWillBeRemoved` closes windows before the source/grid listeners are destroyed;
  close removes registry entries and shutdown closes all remaining editors.

### Verification and Review

Only the independent test project was configured/built/run:

```sh
cmake -S tests -B /tmp/opencode/vibedaw-t06-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/opencode/vibedaw-t06-tests --target offline_tests --parallel 2
ctest --test-dir /tmp/opencode/vibedaw-t06-tests --output-on-failure
git diff --check
```

Initial configure/build and full CTest 1/1 passed (6.62 seconds), with all earlier
regressions retained. `git diff --check` passed.
`rackDropAndClipCreationTests` compiles the actual PluginTreeItem producer, rack,
rows and ClipsContent. Fake processors exercise creation, replacement, failure,
unloaded/oversized candidates, quiescent destruction, stable selection, capacity,
missing IDs, parent/row edges, add-button space, cancellation, unsupported payloads,
offscreen preview colors and preserved sample assignment. Clip tests exercise the
actual New Clip callback, selection without open requests, double-click/Edit
dispatch, source-removal ordering and delayed actions after row/source/owner deletion.
No native popup/window, audio device, installed plugin, application or watcher was
run. MainContent's window reuse/focus and teardown are source-reviewed, not native
or automated window acceptance. Callback allocation/assertion/leak probes remain.

Independent read-only review was performed by an explore agent. Its high-severity
JUCE drag-coordinate finding is fixed below; native/manual acceptance remains pending.
Local audit corrected row/add-button overlap and kept replacement from changing
the last-created-plugin setting; focused regressions pass.

### Independent Review Correction

The explore agent found that rack interest incorrectly depended on localPosition.
Stock JUCE `juce_DragAndDropContainer.cpp:291-308` asks interest using original
source details before converting to target-local coordinates. At `:143-147`, the
previous target's exit-interest recheck instead receives the new target's coordinates
(zero when no target exists). The original predicate could reject Browser-to-rack
creation or suppress exit and leave a stale create preview.

`ChannelRackContent::isInterestedInDragSource` now checks only typed payload and
channel capacity. `isCreateDropPosition` centralizes local bounds/row exclusion for
enter/move/drop. Child-first discovery preserves row priority, direct parent drop
dispatch still cannot also create on a row, and exit clears state without geometry.

The offline regression mirrors JUCE's child-to-parent hit search and event order:
interest sees Browser source coordinates outside the rack, then enter/move/drop
see actual target-local coordinates. Background-to-row-to-outside and direct
background-to-outside transitions verify row priority and preview/button cleanup,
including the exit recheck using another target's coordinates or zero. Local
row/outside commit rejection and short-rack add-button behavior remain covered.
Components are visible for hit testing but have no native peers.

With the original coordinate-dependent predicate temporarily restored, the corrected
test fixture fails `dispatchMove({20, 80}) == &rack` (CTest 0/1, 0.31 seconds).
The fix is restored; verification uses only the existing independent target's build
and CTest commands above. The reproduction additionally used
`ctest --test-dir /tmp/opencode/vibedaw-t06-tests` without verbose failure output.
Final build succeeded and full CTest 1/1 passed (6.42 seconds), retaining all earlier
regressions. `git diff --check` passed.
No application build/launch or dependency edit was performed.

### Pending Manual Checks and Handoff

1. Through the watcher, drag a real supported plugin into an empty rack, unused
   populated space and the add-button area. Observe create label/color and exactly
   one named, loaded, active channel. Cancel drags and verify no mutation.
2. Cross row edges and unused space; observe distinct replacement preview and only
   the clicked row changing. Test loading failure with the old plugin/editor open,
   and at capacity confirm background rejection but existing-row replacement works.
3. Reject bare/clip/preset payloads without misleading highlights. Confirm preserved
   sample-file assignment makes no playback promise.
4. New Clip must select a four-beat source without Piano Roll or placement. Open two
   sources explicitly, reopen each by double-click/Edit (including minimized), and
   verify correct focus without duplicates. Delete an edited shared source and close
   the app through the watcher; observe safe teardown and unresolved placements.
5. T10's native editor/restart/device risks remain unresolved and must not be mistaken
   for T11 acceptance or repaired by changing/disabling the stock dependency.

T12 stays todo pending manual acceptance or explicit authorization. Add a separate
`vibedaw.clip` payload carrying stable ClipId to the shared decoder; never use plugin
path fallback. T12/T14 own timeline edit gestures/context coverage and placement
dragging. Reuse the existing MainContent source-open path rather than opening a
second window per placement. No dependency edits, staging or commit were performed.
