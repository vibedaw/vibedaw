# T03 Model Regression Cases

Captured 2026-09-08 alongside the model for integration into T06's test-only seam.
These remain specifications. Core model cases are now implemented and passing in
`tests/offline_tests.cpp`; see `tests/README.md` and T06's completion record for
executed coverage and gaps. Watcher UI cases below remain unverified. Do not build
or launch the application to run the independent offline target.

## Identity and Routing

1. Add channels A and B to `ChannelList`. Capture `getId()` for both. Place source S
   on track X twice, targeting A and B; place S on track Y targeting A. Move A from
   index 0 to 1. All three `getChannelId()` values must be unchanged and
   `getChannelById(aId)` must still return A, while `getChannel(0)` returns B.
2. In a `Project`, select A by index, reorder it, then remove a different channel.
   `getActiveChannelId()` stays A's ID and `getActiveChannel()` follows its index.
   The project listener receives the updated index (including for the live-MIDI
   bridge). Remove A: active ID/index become -1, not the adjacent instrument.
   Invalid index selection must also clear selection rather than select index 0.
3. Remove A and add C; `getChannelById(aId)` is null, C has a new ID, and existing
   placements still contain aId. Clear channels and add D; neither old ID resolves.
   Test both a missing ID and `InvalidChannelId`. No mixer bus assignment is involved.
4. Add source S, capture its ID, remove it, and add T. Then clear the pool and add U.
   T and U have distinct new IDs; S's ID never resolves to either. Existing placements
   remain present, with their original ID/start/duration/destination, as unresolved
   placeholders. Repeated removal and empty clear are harmless.
5. Register a `ClipPool::Listener`. During `clipWillBeRemoved(sId)`, S still resolves;
   during `clipRemoved(sId)` it does not. Clear emits both events for every source.
   Listener teardown must happen before its owner/pool is destroyed.

## Musical Timing

1. Pool `MidiClip(0, 4)` and create `ClipInstance(sId, aId, 0, 4)`. At 120 BPM,
   `TransportState::setPositionInBeats(4)` gives `getPosition() == 2` seconds;
   setting two seconds gives four quarter-note beats. At 60 BPM the same four beats
   give four seconds. Source and placement start/duration never change with tempo.
   Test 90 BPM with tolerance, rather than integer-only conversion expectations.
2. `containsTime` on placement [8, 12) is true at 8 and false at 12;
   `overlapsRange(12, 16)` is false. Notes also use half-open time containment.
3. Source length 4, placement [8, 14), note [0, 1): the intended render interval is
   [8, 9). Note [3.5, 5) becomes [11.5, 12). [12, 14) is silent. With placement
   duration 2, all material ends by beat 10. A note starting exactly at either the
   source or placement end is omitted. These are future T02 scheduler assertions;
   T03 only establishes the contract and uses it for the actual-note preview.
4. Repeat case 3 with nonzero `MidiClip::startTime` and `loopEnabled == true`.
   Neither offsets nor repeats source-local notes. Only the placement positions
   the source; source beat zero is the placement origin.
5. AudioClip length 2 remains two seconds, regardless of tempo. The MIDI placement
   action must reject AudioClip and PatternClip instead of interpreting their
   lengths as playable MIDI beats. Loop bounds are quarter-note beats at all meters.
6. Grid denominator 4 yields 1 beat; 16 yields 0.25 beat; 32 yields 0.125 beat;
   triplet 6 yields 2/3 beat and 24 yields 1/6 beat. Check minimum zoom (10 pixels
   per beat): drawing terminates even when subdivision spacing is less than a pixel.

## Validation and Notification

1. `Track::addClipInstance` ignores null and invalid instances: negative start,
   zero/negative duration, NaN/infinity, overflowing end, invalid source ID.
   A missing positive source/destination ID is permitted as an unresolved reference.
   Invalid start/duration setters leave the previous value unchanged.
2. `ClipPool::addClip` returns `InvalidClipId` for null or invalid source timing.
   Invalid source timing setters preserve prior values. Audio timing remains seconds.
3. Attach `juce::ChangeListener` to a Track and drain the message queue in the future
   test harness after each operation. Add/remove/clear and instance start/duration,
   source/destination, mute, and selection setters invalidate the Track. Notifications
   are asynchronous/coalesced: do not assert one callback per setter. Track removal
   is detected through TrackList, not a callback on the destroyed Track.
4. Attach Clip and ClipPool listeners to S. Note add/update/remove/clear and source
   length/name/colour/mute/loop edits notify synchronously. The pool conservatively
   sends `clipChanged` for all pooled sources on a source edit. Selection is UI-only.
5. Before note add/erase/clear changes vector storage, all Clip listeners receive
   `notesInvalidated()`. Grids clear selection, hover, and drag pointers immediately.
   `updateNote(pointer, replacement)` preserves storage and notifies afterwards;
   invalid replacement timing is rejected. `getNotes()` and `findNoteAt()` expose
   only const access. Notes are in insertion order, not guaranteed time order.
6. Place S twice, update a note through `updateNote`, and verify both placements
   still resolve the same edited source. Remove one placement: source note content
   and the other placement survive. Source edits do not resize existing placements.
7. With a JUCE message manager and a ChannelList listener, mutate an owned Channel
   using each of `setName`, `setMuted`, `setPlugin` (including null), `setSampleFile`,
   `setColour`, `setVolume`, `setPan`, and `setMixerTrackId`. After draining messages,
   `channelChanged` identifies that same live Channel with the updated values and
   runs on the message thread. Multiple setters may coalesce; never require one
   callback per setter. Direct setter use must work without sidebar reselection.
8. Queue a Channel change, reorder it before dispatch, then drain: the callback
   identifies the same stable ID at its new index. Queue another change and remove
   that Channel before dispatch: no late callback may expose the destroyed Channel.
   Repeat with clear and list destruction, then drain. A surviving or newly added
   Channel must still notify exactly through its current owning list.
9. Drain pending model notifications, then exercise `prepareToPlay`, `processBlock`
   (muted and unmuted, with no plugin), and `releaseResources` in the future seam.
   These paths and their meter writes must emit no Channel change notification.
   All model setters are message-thread-only, not a realtime automation API.

## Watcher UI Checks

1. Empty project: Place/Move/Delete/Assign/Mute are disabled as appropriate, and the
   strip explains missing selections. Add a track, instrument, and four-beat MIDI
   source through the UI. Set start 0 and Place: span is b0 to b4, not four seconds.
2. Click placement, enter a different start, Move; click another instrument, Assign;
   toggle Mute; Delete. Only the selected placement changes. Source selection in
   Clips remains independent of placement selection. Negative, empty, malformed,
   NaN, and infinite start text must not create or move anything.
3. Place on an existing and a newly added track; scroll vertically/horizontally;
   verify header/lane alignment, beat-zero label, destination names/IDs, selected
   outline, mute dimming, actual note preview, and updated scrollbar extent.
4. Open S in two editors. Select/hover/drag a note in one; add or delete notes in
   the other. The first editor cancels invalidated interactions without touching
   stale pointers. Right-click and double-click deletion callbacks use note copies.
5. Delete Source with its editors open: both close before the source is destroyed;
   placements show missing-source placeholders and remain selectable/deletable.
   New sources cannot make them resolve again. Repeat via `clearClips` in the seam.
6. Channel reorder/removal require the model seam until channel-management UI exists.
   Verify active rack highlight, placement assignments, missing-destination label,
   and ability to reassign a missing destination. Source/channel absence means silence
   in the future renderer, never fallback to the active instrument.
7. Check note entry at top/bottom key boundaries and after viewport scroll. Keyboard,
   note grid, and ruler must agree. Default viewport starts around pitch 79 downwards,
   rather than mapping visible rows above pitch 127. Resize editor and timeline and
   check narrow/popped-out panels for usable controls.
8. With a valid source/instrument selected, left-click each track's name, blank
   header background, and colour strip. Header/lane selection and the placement
   strip must follow that track, and Place must target it. Right-click the header
   must not change selection. Clicking the M/S child controls must still invoke
   only their existing toggle callbacks, without invoking header selection.
9. Change a placement destination's mute/name via model setters: after message
   dispatch, lane dimming/name and the active destination status must refresh.
   Drop a sample onto the active instrument, then a plugin: Place/Assign validity,
   unresolved rendering, rack text, and active status must refresh without clicking
   the channel again. Active ID and existing placement destinations must not change.
   Test a non-active destination too: its lane updates without changing audition
   selection. Plugin-loading checks remain watcher/manual, not plugin-free tests.

## T06 Ownership Boundary

All mutation APIs and callbacks above are message-thread model APIs, not audio-safe
containers or callbacks. Listeners may observe changes or detach views, but must not
reentrantly mutate model collections during notifications. T06 must snapshot/copy,
subscribe to collection lifetimes as well as content edits, and publish lifetime-safe
render data. T02 must validate references, sort events, apply source/placement bounds
and mute, and never use the active audition channel as an arrangement fallback.
