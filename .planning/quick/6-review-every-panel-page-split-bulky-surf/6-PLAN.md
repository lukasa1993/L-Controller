---
quick_task: "6"
type: quick-full
description: "Review the embedded panel, split combined workflows into dedicated routes, and redesign the UI around close-match Tailwind Plus application patterns."
files_modified:
  - app/CMakeLists.txt
  - app/src/panel/assets/index.html
  - app/src/panel/assets/login.html
  - app/src/panel/assets/main.js
  - app/src/panel/assets/panel.tailwind.css
  - app/src/panel/panel_http.c
  - tests/helpers/panel-target.js
  - tests/panel-login.spec.js
files_added:
  - app/src/panel/assets/overview.html
  - app/src/panel/assets/actions.html
  - app/src/panel/assets/action-editor.html
  - app/src/panel/assets/schedules.html
  - app/src/panel/assets/schedule-editor.html
  - app/src/panel/assets/updates.html
autonomous: true
must_haves:
  truths:
    - "The authenticated panel becomes a real multi-page experience, not one HTML shell that swaps hidden sections."
    - "Actions and schedules no longer mix list management with create/edit forms on the same page; dedicated routes exist for those workflows."
    - "The visual language shifts to a cleaner Tailwind Plus application style while staying compatible with the embedded plain HTML/JS stack."
    - "Session handling, refresh behavior, and protected-route redirects still work across every page destination."
  artifacts:
    - path: "app/src/panel/assets/actions.html"
      provides: "Tailwind Plus-inspired actions list page with dedicated navigation, summary, and CTA into the action editor route"
      contains: "data-panel-page=\"actions\"|New action|Configured actions"
    - path: "app/src/panel/assets/action-editor.html"
      provides: "Dedicated create/edit page for configured actions"
      contains: "data-panel-page=\"action-editor\"|Save action|Cancel"
    - path: "app/src/panel/assets/schedules.html"
      provides: "Dedicated schedule list page with focused status and mutation entry points"
      contains: "data-panel-page=\"schedules\"|Schedule management|New schedule"
    - path: "app/src/panel/assets/schedule-editor.html"
      provides: "Dedicated create/edit page for schedules"
      contains: "data-panel-page=\"schedule-editor\"|Save schedule|UTC"
    - path: "app/src/panel/assets/overview.html"
      provides: "Overview page with cleaner system, network, and recovery presentation"
      contains: "data-panel-page=\"overview\"|Device overview|Recovery posture"
    - path: "app/src/panel/assets/updates.html"
      provides: "Tailwind Plus-inspired update page with clearer staging, remote, and apply sections"
      contains: "data-panel-page=\"updates\"|Firmware updates|Apply staged update"
    - path: "app/src/panel/panel_http.c"
      provides: "Firmware route registration and asset serving for each dedicated page"
      contains: "\"/actions/new\"|\"/actions/edit\"|\"/schedules/new\"|\"/schedules/edit\""
    - path: "tests/panel-login.spec.js"
      provides: "Smoke coverage for login, navigation, and the new dedicated editor routes"
      contains: "/actions/new|/schedules/new|Configured actions|Schedule management"
  key_links:
    - from: "app/src/panel/panel_http.c"
      to: "app/src/panel/assets/*.html"
      via: "each protected route must serve the matching generated embedded page asset"
      pattern: "\"/overview\"|\"/actions\"|\"/actions/new\"|\"/actions/edit\"|\"/schedules\"|\"/schedules/new\"|\"/schedules/edit\"|\"/updates\""
    - from: "app/src/panel/assets/main.js"
      to: "app/src/panel/assets/*.html"
      via: "page bootstrapping and rendering depend on page-specific data attributes and mount points"
      pattern: "data-panel-page|bootstrapSession|render.*Page"
    - from: "tests/panel-login.spec.js"
      to: "app/src/panel/assets/*.html"
      via: "the smoke should verify the page markers and route flow exposed by the new templates"
      pattern: "data-panel-page|/actions/new|/schedules/new"
---

# Quick Task 6 Plan

## Objective

Turn the embedded operator panel into a cleaner Tailwind Plus-inspired multi-page application where each major workflow has its own page and the create/edit flows no longer compete with status-heavy list views.

## Tasks

### Task 1: Introduce dedicated HTML routes and embedded assets for each panel workflow
- Files: `app/CMakeLists.txt`, `app/src/panel/assets/index.html`, `app/src/panel/assets/overview.html`, `app/src/panel/assets/actions.html`, `app/src/panel/assets/action-editor.html`, `app/src/panel/assets/schedules.html`, `app/src/panel/assets/schedule-editor.html`, `app/src/panel/assets/updates.html`, `app/src/panel/assets/panel.tailwind.css`, `app/src/panel/panel_http.c`
- Action: Replace the current shared authenticated shell with route-specific HTML pages, update the CMake asset pipeline to gzip/embed each page, and register firmware routes for the dedicated action and schedule editor destinations.
- Verify:
  - `rg -n "data-panel-page|New action|New schedule|Firmware updates" app/src/panel/assets/*.html`
  - `rg -n "\"/actions/new\"|\"/actions/edit\"|\"/schedules/new\"|\"/schedules/edit\"" app/src/panel/panel_http.c`
  - `./node_modules/.bin/tailwindcss -i app/src/panel/assets/panel.tailwind.css -o /tmp/panel.css.check --minify`
- Done when: every protected workflow has a matching embedded page asset and the firmware can serve each route directly.

### Task 2: Refactor the browser runtime around page-specific rendering and dedicated edit flows
- Files: `app/src/panel/assets/main.js`
- Action: Replace hidden-view routing with page-aware bootstrapping, keep shared session/refresh helpers, and render Tailwind Plus-inspired list, detail, and editor surfaces for overview, actions, schedules, and updates. Move action/schedule edit/create flows to dedicated routes using query-driven edit state where needed.
- Verify:
  - `node --check app/src/panel/assets/main.js`
  - `rg -n "data-panel-page|renderOverviewPage|renderActionsPage|renderActionEditorPage|renderSchedulesPage|renderScheduleEditorPage|renderUpdatesPage" app/src/panel/assets/main.js`
- Done when: page loads, redirects, polling, create/edit, and navigation all work without relying on hidden sections from the old dashboard shell.

### Task 3: Refresh panel smoke coverage and run build-safe verification
- Files: `tests/helpers/panel-target.js`, `tests/panel-login.spec.js`
- Action: Update panel discovery markers and smoke tests so they validate the new page markers, login redirect behavior, and at least one dedicated action/schedule editor route. Rebuild the firmware assets to ensure the embedded page set still compiles.
- Verify:
  - `node --check tests/helpers/panel-target.js`
  - `node --check tests/panel-login.spec.js`
  - `npx playwright test tests/panel-login.spec.js --list`
  - `./scripts/build.sh`
- Done when: the updated smoke parses, the build succeeds with the new embedded pages, and the route split is reflected in automated checks.
