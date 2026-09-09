# T13: Unified Sidebar Tab Rail

Status: done | Milestone: M1.5 | Depends on: T12 (workstream order, not a technical dependency)

## Outcome

Collapsed Browser and Channel Rack become clearly labeled tabs in one compact left-side rail. Neither leaves a blank column, and either can be reopened naturally without disturbing the other's state.

## Read First

- `src/ui/sidebar/Sidebar.h`, `Sidebar.cpp`
- `src/ui/sidebar/SidebarContainer.h`, `SidebarContainer.cpp`
- `src/ui/sidebar/SidebarTab.h`, `SidebarTab.cpp`
- `src/ui/MainContent.cpp`

Current layout reserves 28 pixels per collapsed sidebar and offsets tabs separately. `updateTabs()` can retain stale sidebar bindings when the collapsed set changes. Right-tab positioning uses remembered expanded width and can place the Clips tab outside its collapsed container. Re-check these source findings before editing.

## Implementation Checklist

- [x] Reserve one compact rail per side only when that side has collapsed panels. Expanded Browser/Channel Rack remain adjacent columns; collapsed tabs stack within the same rail, not separate horizontal slots.
- [x] Give every tab a legible identity, tooltip, and clear hover/focus state consistent with the existing design. Header close means collapse; clicking its tab restores that panel independently.
- [x] Bind tabs to stable sidebar identities, rebuilding/rebinding when membership changes rather than matching only the number of tabs.
- [x] Lay out left and right tabs within actual rail bounds; remove the right-side expanded-width offset defect.
- [x] Preserve each panel's remembered expanded width and existing shortcuts. Derive central layout from actual expanded widths plus at most one rail per side.
- [x] Define narrow-window arbitration so the central workspace and reopen controls remain reachable. Temporarily constrained widths must not overwrite remembered user widths.
- [x] Keep collapse/expand layout changes coherent; avoid transient blank columns or stale hit areas. Do not add animation unless it improves rather than obscures the layout.
- [x] Add component/layout regressions for membership, targeting, width accounting, repeated toggle cycles, resize, and right-side collapse.

## Acceptance Checks

- [x] Collapse Browser then Channel Rack, and repeat in reverse order: both occupy one rail with distinct reachable tabs and no blank reserved panel area.
- [x] Reopen either panel while the other stays collapsed; each tab always operates the correct panel, including equal-count membership changes.
- [x] Repeated collapse/reopen restores widths without accumulating offsets or shrinking preferences.
- [x] Collapsed right-side Clips remains visible and clickable inside its container.
- [x] Narrow-window resize and keyboard toggles keep reopen controls reachable and center bounds valid.
- [x] Watcher observations confirm the visual transitions and hit areas; tests alone are not claimed as visual acceptance.

All acceptance checks accepted by the user on 2026-09-08 after observing the
running watcher. The user noted feedback to be raised later; it is not yet
recorded and may amend this record or feed T14.

## Boundaries

No general docking rewrite, panel-system redesign, or complete sidebar-settings persistence. Preserve existing visual language and independent expanded panels; a rail is not a new mutually exclusive tabbed editor.

## Completion Record

- Changed files: `src/ui/sidebar/Sidebar.h` (icon symbol), `SidebarContainer.h/.cpp` (rail layout, identity-stable tab rebinding, constrained-width API), `SidebarTab.h/.cpp` (IconButton glyph tab, tooltip, testable `activate()`), `src/ui/components/IconButton.h` (tooltip support), `src/ui/MainContent.cpp` (narrow-window arbitration in `updateLayout`), sidebar factories (distinct UTF-8 glyphs: Browser, Channel Rack, Clips), `tests/CMakeLists.txt` (adds SidebarContainer/SidebarTab/BrowserSidebar/PresetSection/SampleSection), `tests/offline_tests.cpp` (`sidebarRailTests`), `tests/README.md`.
- Rail/width/narrow-window contract: one 28px rail per side whenever at least one sidebar on that side is collapsed; collapsed tabs stack vertically inside it (no per-sidebar columns); total width = expanded widths + rail. `constrainTo(available)` clamps expanded sidebars proportionally for display only (integer floor division; remainder goes to the center); sidebar `width_`/`preferredWidth_` never mutate; `constrainTo` fires no container-listener notifications, so parent layout cannot reenter. `updateLayout` and `getDisplayedWidth` derive clamped widths from the same budget formula, keeping container size and child bounds consistent across reentrant `resized()` calls.
- Verification performed: independent offline build succeeded; full CTest 1/1 passed (6.76 seconds) with all T03-T12 suites, allocation probes and assertion/leak diagnostics intact; `git diff --check` clean. Regressions cover rail geometry, equal-count tab rebinding, right-side offset fix, narrow-window clamping/restoration, width memory across toggle cycles, resize-notification coherence and sidebar-removal tab cleanup.
- Unverified checks/blockers: none outstanding. Watcher/native acceptance was
  observed and accepted by the user on 2026-09-08. User feedback on the
  implementation was deferred by the user and may still arrive; treat it as
  potential amendments rather than reopened acceptance.
- Handoff to T14: tab activation is `SidebarTab::activate()`; tab glyphs are set in the sidebar factories; toolbar placement controls are untouched. T14 may add context menus on tabs/rails without new layout dependencies.
