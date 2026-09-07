# T06: Safe Audio Boundary

Status: todo | Milestone: M1 | Depends on: T03

## Outcome

One documented, bounded control/data handoff lets transport, arrangement editing, and mixer controls interact with audio safely. This is a prerequisite for playback, not end-of-milestone cleanup.

## Read First

- `src/Main.cpp`, `src/core/AudioEngine.h`, `AudioEngine.cpp`
- `src/core/ChannelMixer.h`, `ChannelMixer.cpp`, `TransportState.h`, `TransportState.cpp`
- `src/project/Channel.h`, `Channel.cpp`, `ChannelList.cpp`, `Project.cpp`
- `src/plugins/PluginHost.h`, `PluginHost.cpp`
- `CMakeLists.txt`, `justfile`

Current risks include callback logging and temporary buffers, concurrent channel-vector mutation, plugin replacement during processing, ordinary cross-thread control fields, and synchronous transport listeners. Adding a mutex around the entire audio callback is not the intended fix.

## Implementation Checklist

- [ ] Write an ownership table for UI model, render snapshot, channel/plugin lifetime, transport commands, meters, and position. Specify producer/consumer threads and safe reclamation.
- [ ] Publish immutable/precompiled arrangement data outside the audio callback. Audio acquires it only at a block boundary. Include a revision/change signal for note cleanup when sources/routes disappear.
- [ ] Ensure old snapshots and plugin instances are reclaimed outside audio. An atomic pointer or shared pointer alone is not proof of safe lifetime or real-time-safe destruction.
- [ ] Use a bounded command/state handoff for transport and coherent control updates. Define queue overflow/coalescing behavior; stop/panic commands must not be silently lost. Independent meters can use atomic publication.
- [ ] Make channel add/remove/reorder and plugin replacement safe. Prefer a minimal controlled handoff; if graph swapping is too broad, visibly disallow/defer structural edits until processing is safely quiescent. Stopping transport alone does not stop live audio callbacks.
- [ ] Preallocate/reuse channel scratch and MIDI buffers during preparation. Define a bounded event-capacity policy with observable overflow handling outside audio; do not silently grow buffers inside the callback.
- [ ] Remove `LOG_INFO` and other synchronous logging from mixer/channel processing and any called event paths. Audit locks, file I/O, listener/UI callbacks, allocations, and destruction reachable from our callback.
- [ ] Preserve live input and note releases when changing the audition destination. Keep enough routing state to release held notes on their original instrument or deliberately panic that instrument.
- [ ] Add a small independently runnable offline test target using JUCE's existing facilities or another justified minimal option. Do not add a framework dependency by default. Register model/timing tests without requiring GUI launch, hardware, or installed VSTs.
- [ ] Document the test-only command and integrate T03's model cases. Do not build or launch the application to validate this task.

## Acceptance Checks

- [ ] The running render path never reads UI-mutated clip/note/placement containers.
- [ ] No callback logging, per-block scratch-buffer allocation, UI notification, or last-owner plugin/snapshot destruction occurs in our audio path.
- [ ] Add/remove/reorder/plugin replacement while audio is active is safe or clearly blocked until a safe handoff is possible.
- [ ] Audition destination changes with held notes do not strand voices. Empty channel lists and removed active destinations are valid states.
- [ ] Oversized blocks, preparation/sample-rate changes, command saturation, and dense MIDI have explicit bounded behavior.
- [ ] Offline tests can exercise render/control math without launching VibeDAW; any unavailable execution is recorded.

## Boundaries

Do not build a general-purpose engine framework. Keep scheduling in or close to `ChannelMixer`, extracting only the pure timing/event logic needed for deterministic tests. Third-party plugins may have internal real-time limitations; this task covers our ownership and processing code.

## Completion Record

- Changed files: pending
- Ownership table and handoff APIs: pending
- Capacity/overflow and structural-edit policy: pending
- Test-only command and results: pending
- Unverified checks/blockers: pending
- Handoff to T01/T02/T04: pending
