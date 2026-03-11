---
quick_task: "7"
type: quick-full
description: "Fix panel HTTP navigation stalls by isolating requests and adding a configurable 10s timeout"
files_modified:
  - app/Kconfig
  - app/prj.conf
  - app/src/app/app_config.h
  - app/src/app/bootstrap.c
  - app/src/panel/assets/action-editor.html
  - app/src/panel/assets/index.html
  - app/src/panel/assets/login.html
  - app/src/panel/assets/main.js
  - app/src/panel/assets/overview.html
  - app/src/panel/assets/schedule-editor.html
  - app/src/panel/assets/schedules.html
  - app/src/panel/assets/updates.html
  - app/src/panel/panel_http.c
files_added:
  - .planning/quick/7-fix-panel-http-navigation-stalls-by-isol/7-CONTEXT.md
must_haves:
  truths:
    - "The panel HTTP stack no longer relies on one mutable singleton route buffer per endpoint for active requests."
    - "Panel request timeout ownership is explicit in tracked app configuration, with a default of 10 seconds."
    - "Zephyr HTTP inactivity timeout follows the panel-owned timeout setting through tracked repo configuration."
    - "Existing panel routes and response contracts remain compatible with the current embedded UI."
  artifacts:
    - path: "app/src/panel/panel_http.c"
      provides: "Per-client request isolation for dynamic panel handlers and startup logging for the configured timeout"
      contains: "CONFIG_HTTP_SERVER_MAX_CLIENTS|request timeout|route slot"
    - path: "app/Kconfig"
      provides: "Panel-owned configurable request timeout setting"
      contains: "APP_PANEL_REQUEST_TIMEOUT|10"
    - path: "app/prj.conf"
      provides: "Explicit Zephyr HTTP server timeout wiring for the panel build"
      contains: "CONFIG_HTTP_SERVER_CLIENT_INACTIVITY_TIMEOUT"
    - path: "app/src/app/app_config.h"
      provides: "Application config contract for panel timeout ownership"
      contains: "APP_PANEL_REQUEST_TIMEOUT"
  key_links:
    - from: "app/Kconfig"
      to: "app/prj.conf"
      via: "tracked configuration must make the panel timeout visible to the Zephyr HTTP server timeout symbol"
      pattern: "APP_PANEL_REQUEST_TIMEOUT|HTTP_SERVER_CLIENT_INACTIVITY_TIMEOUT"
    - from: "app/src/app/bootstrap.c"
      to: "app/src/panel/panel_http.c"
      via: "panel server init consumes the app-owned timeout configuration"
      pattern: "request_timeout|panel_http_server_init"
    - from: "app/src/panel/panel_http.c"
      to: "app/src/panel/assets/main.js"
      via: "server behavior changes must preserve the current panel route and payload contract used by the browser runtime"
      pattern: "/api/status|/api/actions|/api/schedules|/api/update"
---

# Quick Task 7 Plan

## Objective

Make the embedded panel HTTP handling resilient during navigation by isolating active request state per client and by making the default 10-second request timeout explicit and configurable in tracked project configuration.

## Tasks

### Task 1: Add panel-owned timeout configuration and wire it into tracked build settings
- Files: `app/Kconfig`, `app/prj.conf`, `app/src/app/app_config.h`, `app/src/app/bootstrap.c`
- Action: Introduce an app-level panel request timeout setting with a 10-second default, expose it through the app config contract, and make the Zephyr HTTP inactivity timeout follow the tracked panel setting instead of relying on an implicit upstream default.
- Verify:
  - `rg -n "APP_PANEL_REQUEST_TIMEOUT|HTTP_SERVER_CLIENT_INACTIVITY_TIMEOUT" app/Kconfig app/prj.conf app/src/app/app_config.h app/src/app/bootstrap.c`
- Done when: the panel timeout is explicitly owned by the app config and the build config clearly sets the HTTP inactivity timeout from tracked repo files.

### Task 2: Replace singleton panel route buffers with per-client request isolation
- Files: `app/src/panel/panel_http.c`
- Action: Refactor dynamic route contexts so active clients no longer share one mutable request/response buffer per endpoint, while preserving the existing handlers, payload contracts, and response helpers.
- Verify:
  - `rg -n "CONFIG_HTTP_SERVER_MAX_CLIENTS|route slot|request timeout" app/src/panel/panel_http.c`
  - `sed -n '1,260p' app/src/panel/panel_http.c`
- Done when: panel handlers use per-client route state rather than one singleton buffer per endpoint, and server startup reports the configured timeout.

### Task 3: Run focused verification and document the quick-task outcome
- Files: `.planning/STATE.md`, `.planning/quick/7-fix-panel-http-navigation-stalls-by-isol/7-SUMMARY.md`, `.planning/quick/7-fix-panel-http-navigation-stalls-by-isol/7-VERIFICATION.md`
- Action: Run targeted config/syntax/build checks for the modified firmware files, summarize the implemented fix, record verification status, and append the quick-task entry to `STATE.md`.
- Verify:
  - `./scripts/build.sh`
  - `git diff --stat`
- Done when: the tracked verification signal is recorded, quick-task docs are complete, and `STATE.md` lists this task in the quick-task completion table.
