# T15: Mixer Routing Model

Status: backlog | Milestone: Later (post-M2) | Depends on: M2 (workstream order)

## Outcome

An approved behavior/decision document for the relationship between Channel Rack instruments and the mixer, followed by scoped implementation tasks if changes are agreed. The mixer currently mirrors instrument channels 1:1; the desired replacement, if any, is undecided. This is discovery, not delivery of a routing feature.

T15 does not block T12. Keep its current per-instance instrument routing and cross-track move behavior unchanged while this decision is pending.

## Read First

- T03's routing/identity contract, T04's mixer completion record, and T06's audio boundary/cleanup contract
- T07's persistence contract and actual shipped format, if any, when discovery starts
- T12/T14's placement and destination interaction contracts
- `src/project/Channel.h`, `ChannelList.h`, `Track.h`, `ClipInstance.h`, `Project.h`
- `src/core/ChannelMixer.h`, `ChannelMixer.cpp`, `MixerState.h`
- `src/ui/panels/MixerPanel.cpp`, `src/ui/mixer/MixerStrip.cpp`, `src/ui/sidebar/channel/ChannelRackSidebar.cpp`

## Discovery Checklist

- [ ] Re-check the current model and document three distinct concepts: instrument channels host sound generators; arrangement tracks hold placements; mixer strips/buses control or combine audio. Identify where the current implementation couples these concepts, without assuming they must become separate objects.
- [ ] Gather concrete user workflows: should multiple instruments feed one mixer destination, should destinations exist independently of instruments, and what should creating, selecting, reordering, or deleting either object do?
- [ ] Compare retaining 1:1 instrument strips, adding shared buses behind those strips, and independent assignable mixer destinations. Record signal-flow examples, UI consequences, complexity, and tradeoffs; do not presume FL Studio's architecture or that matching it is the goal.
- [ ] Distinguish MIDI destination selection on each clip instance from downstream instrument-audio routing. Explain whether each option preserves that separation; any proposed change to per-instance routing needs explicit agreement, not an incidental remap through track or strip order.
- [ ] Resolve the chosen behavior for defaults, many-to-one summing (if desired), destination assignment, master output, gain/pan/mute/solo, meters, live audition, missing destinations, and deletion. Record explicit exclusions rather than leaving implementation to infer behavior.
- [ ] Treat insert effects, pre/post-fader sends, bus chains/feedback restrictions, and multi-output instruments as scope questions. Determine whether they constrain the initial model or remain deferred; this task promises none of these features.
- [ ] Identify stable-ID/reference requirements and preserve existing session sound and routing as applicable. Review actual persistence at implementation time; plan migrations only for a shipped format, not hypothetical compatibility layers.
- [ ] Document implications for T06's bounded audio handoff, processor ownership/process-once rules, safe graph changes, plugin/editor lifecycle, and note/tail cleanup on reroute, removal, mute, or project replacement. Separate audio suppression from MIDI note ownership; do not bypass the existing safety boundary.
- [ ] Present the options and recommended behavior to the user, record approval or unresolved questions in a linked decision document, and create concrete follow-on implementation/test tasks only for the agreed scope.

## Acceptance Checks

- [ ] A linked, user-approved decision document defines the chosen model, alternatives rejected and why, concrete signal-flow/UI examples, defaults, lifecycle behavior, and explicit non-goals.
- [ ] The decision clearly distinguishes instrument channels, arrangement tracks, and mixer strips/buses, and states whether/how many-to-one routing and independent destinations are desired.
- [ ] Stable identities, existing-session preservation as applicable, audio boundary/cleanup obligations, and shipped-format migration needs are recorded without speculative compatibility work.
- [ ] Effects, sends, and multi-output implications have explicit scope decisions, not implied feature commitments.
- [ ] Follow-on implementation tasks cover the approved changes, dependencies, and focused offline/manual acceptance scenarios; if retaining the current model, record why no routing implementation is needed and any agreed documentation/UI follow-ups.
- [ ] T12 remains independent and unchanged. Completion claims an approved decision and actionable follow-ups, not implemented remapping or audible feature verification.

## Boundaries

Docs/discovery only. No mixer remapping, graph rewrite, new routing controls, persistence migration, or changes to existing routing contracts in this task. Do not adopt another DAW's architecture by assumption. Agree concrete behavior before implementation; unresolved user choices remain blockers to this task's approval, not to T12.

## Completion Record

- Changed files: pending
- Decision document/user approval: pending
- Chosen model, alternatives, and scope exclusions: pending
- Identity/session/audio-boundary implications: pending
- Verification performed: pending
- Unresolved questions/blockers: pending
- Follow-on task links: pending
