---
gsd_state_version: 1.0
milestone: v1.1
milestone_name: action configuration flow
current_phase: 09
current_phase_name: configured action model and panel management
current_plan: "-"
status: roadmap ready
stopped_at: Created roadmap for milestone v1.1
last_updated: "2026-03-10T22:03:58Z"
last_activity: 2026-03-11
progress:
  total_phases: 2
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-11)

**Core value:** The device must reliably and safely execute configured local control actions—even through Wi-Fi disruption or subsystem faults—without unnecessary operator intervention.
**Current focus:** phase 9 ready for planning

## Current Position

**Current Phase:** 09
**Current Phase Name:** configured action model and panel management
**Current Plan:** -
**Total Plans in Phase:** Not planned yet
**Status:** Roadmap ready
**Last Activity:** 2026-03-11
**Last Activity Description:** Created the milestone v1.1 roadmap with two phases: Phase 9 covers the configured action model, output-binding safety, migration, and panel management surface; Phase 10 covers shared execution, schedule integration, and end-to-end verification

Phase: 1 of 2 (configured action model and panel management) - not started
Plan: -
Status: Roadmap ready
Last activity: 2026-03-11 - Created the v1.1 roadmap with two phases that replace built-in relay actions with configured relay actions shared by Actions and Schedules

Progress: [----------] 0%

## Performance Metrics

**Velocity:**
- Total plans completed: 22
- Average duration: 69.2 min
- Total execution time: 15.0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| Phase 01-foundation-refactor P01 | 3 min | 3 tasks | 15 files |
| Phase 01-foundation-refactor P02 | 6 min | 3 tasks | 9 files |
| Phase 01-foundation-refactor P03 | 7 min | 4 tasks | 4 files |

**Recent Trend:**
- Last 5 plans: Phase 06 plan 02 (4m), Phase 06 plan 03 (6 min + approved human verification), Phase 07 plan 01 (10m), Phase 07 plan 02 (13 min), Phase 07 plan 03 (6min + approved human verification)
- Trend: Recent plans remain short, while elapsed time still clusters around blocking hardware/browser verification steps.

*Updated after each plan completion*
| Phase 02-wi-fi-supervision P01 | 4 min | 3 tasks | 7 files |
| Phase 02-wi-fi-supervision P02 | 16 min | 3 tasks | 9 files |
| Phase 02-wi-fi-supervision P03 | 9 min | 3 tasks | 3 files |
| Phase 03-recovery-watchdog P01 | 5 min | 3 tasks | 11 files |
| Phase 03-recovery-watchdog P02 | 9h 23m | 3 tasks | 3 files |
| Phase 04-persistent-configuration P01 | 13 min | 3 tasks | 10 files |
| Phase 04-persistent-configuration P02 | 6 min | 3 tasks | 4 files |
| Phase 04 P03 | 4h 15m | 3 tasks | 7 files |
| Phase 05-local-control-panel P01 | 3 min | 3 tasks | 11 files |
| Phase 05-local-control-panel P02 | 5 min | 3 tasks | 11 files |
| Phase 06-action-engine-relay-control P01 | 3 min | 3 tasks | 7 files |
| Phase 06-action-engine-relay-control P02 | 4m | 3 tasks | 7 files |
| Phase 06-action-engine-relay-control P03 | 6 min (auto tasks) + approved human verification | 4 tasks | 8 files |
| Phase 07-scheduling P01 | 10m | 3 tasks | 10 files |
| Phase 07 P02 | 13 min | 4 tasks | 12 files |
| Phase 07-scheduling P03 | 6min | 4 tasks | 10 files |
| Phase 08 P01 | 22 min | 3 tasks | 19 files |
| Phase 08 P02 | 32 min | 3 tasks | 7 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Initial scope: Local-LAN HTTP panel only; no public internet exposure in v1
- Auth model: Single local operator account with simple secure sessions
- Connectivity: Wi-Fi credentials remain build-time configured in v1
- Reliability: Recovery is conservative; full restart is only for confirmed unrecoverable states
- Core function: Relay activation/deactivation is the first production action
- [Phase 1]: The top-level app context owns configuration and networking runtime state by value.
- [Phase 1]: Future subsystem homes stay header-only until their phases add real behavior.
- [Phase 1]: Build growth uses explicit source and include registration instead of source discovery.
- [Phase 01]: Bootstrap now owns Kconfig loading, interface discovery, and ready-path orchestration behind app_boot().
- [Phase 01]: Wi-Fi runtime state stays inside struct network_runtime_state and is manipulated through lifecycle APIs instead of main.c globals.
- [Phase 01]: Placeholder APP_ADMIN_USERNAME and APP_ADMIN_PASSWORD symbols keep the existing local secrets overlay buildable without editing user-specific secret files.
- [Phase 01-foundation-refactor]: Phase 1 validation now runs build-first through ./scripts/validate.sh, which delegates to ./scripts/build.sh as the canonical automated signal.
- [Phase 01-foundation-refactor]: Final Phase 1 sign-off stays a blocking hardware smoke gate using ./scripts/flash.sh and ./scripts/console.sh rather than part of the default refactor loop.
- [Phase 01-foundation-refactor]: Phase 1 completed after a human-approved hardware smoke confirmed Wi-Fi connected, DHCP IPv4 address, Reachability check passed, and APP_READY on device.
- [Phase 02-wi-fi-supervision]: Network policy now starts at network_supervisor_*() while wifi_lifecycle.* stays limited to low-level callbacks and connect primitives.
- [Phase 02-wi-fi-supervision]: Connectivity reporting uses a named enum plus structured last_failure so future panel and recovery code can read one operator-meaningful contract.
- [Phase 02-wi-fi-supervision]: DHCP-stop and IPv4-address removal callbacks only clear raw link state; the supervisor derives degraded status above them.
- [Phase 02-wi-fi-supervision]: Reuse CONFIG_APP_WIFI_TIMEOUT_MS as the bounded startup window while retries run on CONFIG_APP_WIFI_RETRY_INTERVAL_MS.
- [Phase 02-wi-fi-supervision]: Boot now continues once the supervisor reaches healthy, upstream-degraded, or degraded-retrying startup outcome instead of blocking forever on full health.
- [Phase 02-wi-fi-supervision]: Supervisor status snapshots preserve the most recent failure across recovery and expose the latest reachability result for downstream consumers.
- [Phase 02-wi-fi-supervision]: Keep ./scripts/validate.sh as the single automated entrypoint and make real-device supervision checks a blocking manual gate.
- [Phase 02-wi-fi-supervision]: Phase 2 approval requires four explicit scenarios: healthy boot, degraded-retrying, LAN-up upstream-degraded, and recovery back to healthy without reboot.
- [Phase 02-wi-fi-supervision]: Hardware sign-off records recovery plus durable last_failure visibility so later phases inherit the operator-facing contract.
- [Phase 03-recovery-watchdog]: Keep watchdog ownership inside recovery_manager and feed it only after evaluating boot/runtime liveness windows.
- [Phase 03-recovery-watchdog]: Treat LAN-up upstream degradation as acceptable runtime stability while requiring patience windows before confirmed stuck escalation.
- [Phase 03-recovery-watchdog]: Persist only the latest recovery breadcrumb via .noinit plus hwinfo reset-cause data instead of introducing a broader persistence layer.
- [Phase 03-recovery-watchdog]: Keep ./scripts/validate.sh build-only and treat real-hardware recovery/watchdog verification as the blocking Phase 3 approval gate.
- [Phase 03-recovery-watchdog]: Centralize ready, supervisor, and recovery/watchdog markers plus the device checklist in scripts/common.sh so README and CLI guidance stay aligned.
- [Phase 03-recovery-watchdog]: Record 03-02 as approved after healthy, degraded, recovery-reset, watchdog-reset, and anti-thrash scenarios were confirmed on hardware.
- [Phase 04-persistent-configuration]: Use a dedicated NVS-backed settings_storage partition inside the persistence boundary.
- [Phase 04-persistent-configuration]: Keep app_context as the single owner of the mounted persistence backend and resolved snapshot.
- [Phase 04-persistent-configuration]: Reseed only auth from build-time credentials while other sections fall back with section-local resets or safe defaults.
- [Phase 04-persistent-configuration]: Future writers save through request-shaped APIs tied to persisted_config, so callers never author raw schema-versioned NVS blobs.
- [Phase 04-persistent-configuration]: Persistence stages candidate auth/action/relay/schedule updates in RAM, validates unique IDs and schedule references, then commits only the requested sections.
- [Phase 04-persistent-configuration]: Boot logs summarize the resolved snapshot with action/schedule counts and relay intent without exposing credentials.
- [Phase 04]: Corrupt or incompatible persisted sections reset independently, while invalid auth reseeds from the configured build-time credentials.
- [Phase 04]: Boot exposes explicit per-section migration action and schema-version status instead of attempting ambiguous best-effort migration.
- [Phase 04]: Phase 4 keeps ./scripts/validate.sh as the canonical automated command and requires recorded device approval for the durability scenarios before completion.
- [Phase 05-local-control-panel]: Kept 05-01 on plain local HTTP with no TLS, websocket, filesystem, auth, or protected API routes.
- [Phase 05-local-control-panel]: Embedded authored panel assets via generate_inc_file_for_target(... --gzip) instead of generating markup in C.
- [Phase 05-local-control-panel]: Marked only PANEL-01 and PANEL-02 complete because auth/session and status requirements remain in 05-02 and 05-03.
- [Phase 05]: Phase 5 uses Zephyr's native HTTP service with linker-registered resources instead of a filesystem-backed or generated-markup panel. — This keeps the local panel transport minimal, Zephyr-native, and ready for later auth and status routes without adding a new serving stack.
- [Phase 05]: The panel shell serves authored gzip-compressed HTML and JavaScript directly from firmware while staying on local plain HTTP only. — This satisfies the authored-asset requirement while avoiding filesystem dependencies, TLS expansion, or C-generated markup in Phase 5 Wave 1.
- [Phase 05]: Boot starts the public panel shell after the network supervisor reaches a locally reachable state, including upstream-degraded connectivity. — The shell should remain available whenever the device has local LAN reachability, even if upstream internet is degraded, matching the established operator contract.
- [Phase 05]: Panel auth uses a fixed-size mutex-guarded RAM session table with opaque sid cookies so sessions survive refresh and navigation but are invalidated on reboot. — This matches the locked browser behavior while keeping trust on the firmware side and avoiding persistent tokens.
- [Phase 05]: Phase 5 exposes only the exact auth trio plus one protected aggregate /api/status endpoint; control, configuration, scheduling, and update routes stay intentionally unavailable. — A minimal exact-path API is simpler to secure now and leaves future control surfaces clearly deferred to later phases.
- [Phase 05]: Unauthorized session and status requests clear stale sid cookies so browsers recover cleanly after reboot or logout. — Clearing stale cookies avoids confusing half-authenticated browser state after the firmware invalidates all RAM sessions at boot.
- [Phase 06-action-engine-relay-control]: The board mapping for relay 0 lives behind a relay0 devicetree alias so callers stay hardware-agnostic.
- [Phase 06-action-engine-relay-control]: The relay runtime preserves remembered desired state even when boot or recovery policy forces the applied state off.
- [Phase 06-action-engine-relay-control]: Boot initializes recovery breadcrumbs before the relay service so startup policy can distinguish ordinary reboots from recovery resets.
- [Phase 06-action-engine-relay-control]: Reserve stable relay built-ins as relay0.off and relay0.on and seed them during dispatcher startup.
- [Phase 06-action-engine-relay-control]: Keep HTTP and future scheduler callers on action_id execution while the dispatcher owns relay actuation and persistence writes.
- [Phase 06-action-engine-relay-control]: Roll back relay runtime state if desired-state persistence fails so accepted commands remain durable and truthful.
- [Phase 06-action-engine-relay-control]: Keep the external panel/API surface relay-centric with one `/api/relay/desired-state` route while the dispatcher continues to own internal action execution.
- [Phase 06-action-engine-relay-control]: Show actual-versus-desired mismatch as operator visibility plus a safety note instead of treating it as an automatic command block.
- [Phase 06-action-engine-relay-control]: Keep `./scripts/validate.sh` build-first and treat browser/curl/device relay verification as the blocking Phase 6 approval gate.
- [Phase 07-scheduling]: Schedules persist as explicit UTC 5-field cron expressions and the persistence layout is bumped to version 2.
- [Phase 07-scheduling]: Enabled opposite-state same-minute overlaps are rejected before save, while same-state overlaps remain allowed.
- [Phase 07-scheduling]: Scheduler automation stays inactive until trusted time is explicitly acquired, and trust/correction events baseline the current minute, skip missed jobs, and compute only future runs.
- [Phase 07]: Trusted time comes from a scheduler-owned Zephyr SNTP helper that prefers DHCP-provided NTP and falls back to an app-configured server.
- [Phase 07]: The live scheduler runs from one delayable minute-boundary work item and recomputes from the current UTC minute instead of backfilling missed work.
- [Phase 07]: Manual and scheduled commands share one dispatcher mutex and skip already-applied desired-state writes to avoid race windows and flash churn.
- [Phase 07-scheduling]: Authenticated schedule management stays on exact /api/schedules routes with thin handlers while the scheduler and dispatcher remain the runtime owners.
- [Phase 07-scheduling]: The operator UI shows public action keys and labels instead of raw internal action IDs while still refreshing from server truth after every schedule mutation.
- [Phase 07-scheduling]: Phase 7 completion remains build-first and only closes after the documented browser/curl/device checklist is explicitly approved.
- [Phase 08]: The nRF7002DK OTA foundation uses mcuboot_secondary on MX25R64 external flash so the app keeps nearly the full internal slot.
- [Phase 08]: OTA metadata stays in a dedicated typed persistence section keyed by PERSISTENCE_NVS_ID_OTA without disturbing auth, relay, or schedule sections.
- [Phase 08]: All future update entrypoints must share ota_service staging and apply helpers so same-version and downgrade gating cannot diverge by caller.
- [Phase 08]: Kept `/api/status` compact and moved detailed OTA truth plus mutations onto the exact `/api/update` route family.
- [Phase 08]: Streaming local OTA upload validates captured `Content-Type` and `Content-Length` headers and writes chunks through `ota_service` instead of buffering firmware in RAM.
- [Phase 08]: Explicit apply requests the MCUboot test upgrade and schedules a delayed reboot so the operator receives a response before the browser session drops.
- [Quick task 01]: The embedded panel now loads Tailwind through the requested `@tailwindcss/browser@4` runtime, uses utility-only markup plus an HTML safelist for JS-rendered utility tokens, and the firmware asset pipeline Bun-minifies `main.js` before generating `main.js.gz.inc`.
- [Quick task 04]: The repo now derives the default MCUboot signing version from `app/VERSION`, so ad hoc local OTA test builds can stay minimal by bumping only `VERSION_TWEAK`.
- [Quick task 05]: The protected panel now lands on an actions-first shell at `/`, serves dedicated overview/schedules/updates routes, and keeps a future-output rail ready while multi-relay GPIO support remains deferred.

### Pending Todos

None yet.

### Blockers/Concerns

- Configurable action flow needs explicit decisions about GPIO binding validation, persistence migration for existing built-in action IDs, and how schedules behave when referenced actions are edited or removed
- Zephyr still emits non-fatal MBEDTLS Kconfig warnings during configuration, though `./scripts/validate.sh` and `./scripts/build.sh` complete successfully.
- Quick task 4 proved staging and apply for `0.0.0+1`, but the device did not return to the panel after reboot and the preferred serial console port is currently locked by another `miniterm` process.

### Quick Tasks Completed

| # | Description | Date | Commit | Status | Directory |
|---|-------------|------|--------|--------|-----------|
| 1 | Tailwind v4 utility-only panel and Bun-minified embedded panel JS | 2026-03-10 | cfc4541 | Verified | [1-make-sure-our-html-styles-are-only-tailw](./quick/1-make-sure-our-html-styles-are-only-tailw/) |
| 2 | Build, flash, and add a Playwright login smoke that verifies the first authenticated page is the dashboard. | 2026-03-10 | 25cb217 | Verified | [2-build-flash-and-add-a-playwright-login-s](./quick/2-build-flash-and-add-a-playwright-login-s/) |
| 3 | Make a dedicated login page with proper redirect-on-success using a Tailwind Plus paid HTML login flow. | 2026-03-10 | 86e469d | Partial verification | [3-make-dedicated-login-page-with-proper-re](./quick/3-make-dedicated-login-page-with-proper-re/) |
| 4 | test ota update build next version with minimal changes and update with ota | 2026-03-10 | f780aa0 | Gaps | [4-test-ota-update-build-next-version-with-](./quick/4-test-ota-update-build-next-version-with-/) |
| 5 | simplify dashboard into page navigation with actions first, fix panel flicker/reload experience, and prepare UI path for future multi-relay GPIO configuration | 2026-03-10 | 66de52f | Partial verification | [5-simplify-dashboard-into-page-navigation-](./quick/5-simplify-dashboard-into-page-navigation-/) |

## Session Continuity

Last session: 2026-03-10T09:51:40.813Z
Stopped at: Completed 08-02-PLAN.md
Resume file: None
