---
phase: 05-local-control-panel
plan: "01"
subsystem: ui
tags: [zephyr, http-server, embedded-assets, gzip, panel, local-lan]
requires:
  - phase: 02
    provides: supervisor-managed Wi-Fi connectivity status and degraded-local reachability behavior
  - phase: 03
    provides: recovery ownership and reset-cause reporting that later panel surfaces will display
  - phase: 04
    provides: boot-owned persisted config snapshot and seeded admin credential source
provides:
  - local plain-HTTP panel service with linker-registered Zephyr resources
  - gzip-embedded authored HTML and JavaScript assets served directly from firmware
  - panel shell composition in app ownership so boot starts the shell after local networking is ready
affects: [phase-05-02, phase-05-03, panel_auth, panel_status, action-engine]
tech-stack:
  added: [Zephyr HTTP server, gzip generate_inc_file_for_target asset pipeline]
  patterns: [linker-registered HTTP resources, authored firmware-served frontend assets, app-owned panel service startup]
key-files:
  created:
    - .planning/phases/05-local-control-panel/05-01-SUMMARY.md
    - app/sections-rom.ld
    - app/src/panel/panel_http.c
    - app/src/panel/assets/index.html
    - app/src/panel/assets/main.js
  modified:
    - app/CMakeLists.txt
    - app/Kconfig
    - app/prj.conf
    - app/src/app/app_config.h
    - app/src/app/app_context.h
    - app/src/app/bootstrap.c
    - app/src/panel/panel_http.h
key-decisions:
  - "Phase 5 uses Zephyr's native HTTP service with linker-registered resources instead of a filesystem-backed or generated-markup panel."
  - "The panel shell serves authored gzip-compressed HTML and JavaScript directly from firmware while staying on local plain HTTP only."
  - "Boot starts the public panel shell after the network supervisor reaches a locally reachable state, including upstream-degraded connectivity."
patterns-established:
  - "HTTP shell boundary: panel transport lives in panel_http with static resources registered at build time."
  - "Frontend assets stay authored in app/src/panel/assets and are transformed into generated gzip includes during the Zephyr build."
  - "App composition owns the panel service by value in app_context and starts it from app_boot before APP_READY."
requirements-completed: [PANEL-01, PANEL-02]
duration: 3 min
completed: 2026-03-09
---

# Phase 5 Plan 1: Panel HTTP Shell Summary

**Zephyr-native local HTTP shell with linker-registered resources and embedded gzip HTML/JS assets served directly from firmware.**

## Performance

- **Duration:** 3 min (elapsed from the first `05-01` task commit to summary creation)
- **Started:** 2026-03-09T15:36:46Z
- **Completed:** 2026-03-09T15:40:42Z
- **Tasks:** 3
- **Files modified:** 11

## Accomplishments

- Added the Phase 5 panel transport boundary with Zephyr HTTP server config, linker wiring, and a dedicated `panel_http` service.
- Embedded authored `index.html` and `main.js` assets via gzip build generation so the device serves real frontend files from firmware without C-generated markup.
- Composed the panel shell into `app_context` and `app_boot()` so the public shell starts before `APP_READY`, while preserving existing degraded-local network behavior.

## Task Commits

Each task was completed atomically:

1. **Task 1: Add the panel HTTP build surface, linker section, and local-LAN config boundary** - `011080f` (`feat`)
2. **Task 2: Embed authored HTML and JavaScript assets with a gzip build pipeline** - `53817ac` (`feat`)
3. **Task 3: Compose the panel shell into app ownership and boot readiness** - `b82669b` (`feat`)

**Plan metadata:** documented in the final completion output

## Files Created/Modified

- `app/CMakeLists.txt` - Compiles `panel_http.c`, defines the resource linker section, and generates gzip includes for the panel assets.
- `app/Kconfig` - Adds `CONFIG_APP_PANEL_PORT` as the local HTTP shell port setting.
- `app/prj.conf` - Enables the Zephyr HTTP server plus the header-capture and client settings needed for the panel shell.
- `app/sections-rom.ld` - Declares the iterable linker section for `panel_http_service` resources.
- `app/src/app/app_config.h` - Adds the panel config contract and `APP_PANEL_PORT` macro.
- `app/src/app/app_context.h` - Extends `app_context` with an owned `panel_http_server` instance.
- `app/src/app/bootstrap.c` - Loads the panel config and starts the shell during `app_boot()` before reporting `APP_READY`.
- `app/src/panel/panel_http.h` - Defines the panel HTTP server ownership shape used by app composition.
- `app/src/panel/panel_http.c` - Registers the HTTP service plus `/` and `/main.js` static resources and starts the server.
- `app/src/panel/assets/index.html` - Provides the authored Tailwind-backed panel shell document.
- `app/src/panel/assets/main.js` - Provides the authored client bootstrap for the public shell status.

## Decisions Made

- Chose Zephyr's built-in HTTP service macros and iterable resource sections as the panel serving mechanism so future auth and status routes can hang off a native subsystem boundary.
- Captured the `Cookie` header configuration in Wave 1 even though auth routes are deferred, so Wave 2 can add session routing without reworking the transport setup.
- Started the HTTP shell from `app_boot()` only after the network supervisor completes startup, ensuring the panel remains tied to local reachability rather than upstream internet health.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- `./scripts/build.sh` still emits the pre-existing MBEDTLS Kconfig warnings already tracked in project state, but the build completed successfully.

## User Setup Required

None - the public shell build path is fully automated for this plan.

## Next Phase Readiness

- `05-02` can now layer exact auth/session routes and a protected status API onto a real HTTP service instead of inventing transport scaffolding.
- The authored asset pipeline is in place for the login-first UX and dashboard work in `05-03`.
- No new blocker was introduced by `05-01`; the known MBEDTLS warnings remain out of scope.

## Self-Check: PASSED

- Verified `.planning/phases/05-local-control-panel/05-01-SUMMARY.md` exists.
- Verified prior task commits `011080f`, `53817ac`, and `b82669b` exist in git history.
