---
quick_task: "5"
type: quick-full
description: "Simplify the dashboard into page navigation with actions first, reduce panel flicker/reload churn, and leave a clean UI path for future multi-relay work."
files_modified:
  - app/src/panel/assets/index.html
  - app/src/panel/assets/main.js
  - app/src/panel/panel_http.c
  - tests/helpers/panel-target.js
  - tests/panel-login.spec.js
autonomous: true
must_haves:
  truths:
    - "The protected panel lands authenticated users on an `Actions`-first shell at `/`, with persistent navigation to `Overview`, `Schedules`, and `Updates`."
    - "Manual action clicks provide immediate optimistic feedback and no longer depend on a whole-dashboard redraw before the operator sees progress, while the device APIs remain the source of truth."
    - "The `Actions` page includes a clear secondary rail for output topology and future expansion so additional GPIO-backed outputs can be introduced later without redesigning the shell."
    - "Scheduler and OTA flows still work behind their dedicated views, and the existing login/discovery smoke continues to identify and validate the protected shell."
  artifacts:
    - path: "app/src/panel/assets/index.html"
      provides: "Authenticated shell with top navigation, summary chrome, dedicated view mounts, and an actions-first landing layout"
      contains: "data-panel-nav|data-panel-view|Actions|Schedules|Updates"
    - path: "app/src/panel/assets/main.js"
      provides: "Route-aware panel rendering, optimistic action handling, targeted refresh logic, and a future-ready output topology rail"
      contains: "setActiveView|renderOverviewSurface|renderActionsSurface|renderActionsInfoCard|state.activeView"
    - path: "app/src/panel/panel_http.c"
      provides: "Protected shell routes for the page-style panel destinations"
      contains: "\"/overview\"|\"/schedules\"|\"/updates\""
    - path: "tests/panel-login.spec.js"
      provides: "Smoke coverage for the authenticated shell after redirect-driven login and navigation"
      contains: "Actions|Schedules|Updates|data-panel-nav|dashboard-view"
  key_links:
    - from: "app/src/panel/assets/index.html"
      to: "app/src/panel/assets/main.js"
      via: "the shell's nav triggers and view mounts must line up with the browser runtime's route-aware rendering functions"
      pattern: "data-panel-nav|data-panel-view|setActiveView|render.*Surface"
    - from: "app/src/panel/panel_http.c"
      to: "app/src/panel/assets/main.js"
      via: "the firmware must serve the same authenticated shell on the page routes that the browser runtime activates without full reloads"
      pattern: "\"/overview\"|\"/schedules\"|\"/updates\"|setActiveView|history.pushState"
    - from: "tests/helpers/panel-target.js"
      to: "app/src/panel/assets/index.html"
      via: "panel auto-discovery still depends on stable authenticated shell markers after the protected page is reorganized"
      pattern: "LNH Nordic Mission Console|data-panel-page=\"dashboard\"|data-panel-nav"
---

# Quick Task 5 Plan

## Objective

Turn the protected panel into a simpler, action-first experience with real page-level navigation, while removing the current redraw behavior that makes clicks feel slow and visually unstable.

## Tasks

### Task 1: Replace the long dashboard shell with page navigation and an actions-first layout
- Files: `app/src/panel/assets/index.html`, `app/src/panel/assets/main.js`, `app/src/panel/panel_http.c`
- Action: Rework the authenticated shell so `/` is the default `Actions` landing view, add persistent top chrome plus navigation for `Overview`, `Actions`, `Schedules`, and `Updates`, and serve the same authenticated shell on the page-style routes needed for direct navigation.
- Verify:
  - `rg -n "data-panel-nav|data-panel-view|Overview|Actions|Schedules|Updates" app/src/panel/assets/index.html app/src/panel/assets/main.js`
  - `rg -n "\"/overview\"|\"/schedules\"|\"/updates\"" app/src/panel/panel_http.c`
  - `node --check app/src/panel/assets/main.js`
- Done when: the shell exposes focused destinations, `/` clearly lands on the `Actions` experience, and refresh/logout/session status remain available without forcing users to scroll through unrelated cards.

### Task 2: Refactor client rendering and action handling to reduce flicker
- Files: `app/src/panel/assets/main.js`
- Action: Split the browser runtime into route-aware render helpers, keep only the relevant surface updating on refresh, add optimistic relay feedback so button presses react immediately before the device truth sync completes, and add a future-output rail that explains where additional GPIO-backed outputs will appear later.
- Verify:
  - `node --check app/src/panel/assets/main.js`
  - `rg -n "setActiveView|renderOverviewSurface|renderActionsSurface|refreshDashboard|handleRelayToggle" app/src/panel/assets/main.js`
  - Browser smoke on the real panel: log in, confirm `/` opens the `Actions` view, switch to `Schedules` and `Updates` without a document reload, return to `Actions`, and verify a relay button shows immediate pending/active feedback before the refresh settles.
- Done when: relay interaction no longer feels like a full-page reload, the current view stays stable across polling and mutations, the future multi-output rail is visible on `Actions`, and scheduler/update flows still re-render correctly inside their own surfaces.

### Task 3: Update panel shell smoke coverage and run build-safe verification
- Files: `tests/helpers/panel-target.js`, `tests/panel-login.spec.js`
- Action: Update shell markers and smoke assertions so discovery and login verification match the new authenticated navigation shell and action-first landing experience.
- Verify:
  - `node --check tests/helpers/panel-target.js`
  - `node --check tests/panel-login.spec.js`
  - `npx playwright test tests/panel-login.spec.js`
  - `./scripts/build.sh`
- Done when: static verification passes, the smoke exercises the new shell after login, and the firmware build still embeds the updated panel assets successfully.
