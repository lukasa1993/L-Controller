# Architecture Research

**Domain:** Mission-critical Nordic/Zephyr control device with local admin panel, OTA, persistence, and scheduling
**Researched:** 2026-03-08
**Confidence:** HIGH

## Recommended Architecture

### Major Components

| Component | Responsibility | Notes |
|-----------|----------------|-------|
| `app_core` / bootstrap | Minimal boot orchestration and subsystem startup ordering | `main.c` should become a thin composition root only |
| `config_store` | Load, validate, commit, and version persistent settings | Wrap Zephyr Settings so higher layers never touch raw keys directly |
| `net_supervisor` | Wi-Fi association, IP readiness, reachability policy, retry budgets | Central owner of connectivity state machine |
| `recovery_manager` | Task watchdog channels, hardware watchdog fallback, escalation policy | Decides when to degrade, restart subsystems, or reset device |
| `http_service` | Start/stop local HTTP server and route requests | Own static asset registration and API handler wiring |
| `auth_service` | Single-user credential verification and session management | Keep scope intentionally small but isolated |
| `action_engine` | Validate and execute action definitions | Must be generic even if only relay on/off exists at first |
| `relay_driver` | Board-specific relay control abstraction | Owns GPIO mapping, default states, and safety checks |
| `scheduler_service` | Time-based triggering of actions | Separates cron-like decisions from action execution |
| `ota_service` | Handle upload/pull orchestration, staging, validation, and reboot-to-swap | Uses DFU/MCUboot primitives instead of inventing update semantics |
| `telemetry` / `event_log` | Structured logs, health summaries, operator-visible status | Critical for mission-critical diagnosis |

### Boundary Rules

- `main.c` initializes modules and hands off; it does not contain business logic.
- HTTP handlers never toggle GPIO directly; they submit requests to `action_engine`.
- Scheduler never knows hardware details; it triggers actions by ID/type.
- Recovery/watchdog logic never depends on HTTP code paths staying healthy.
- OTA logic is isolated from panel presentation; the UI calls APIs, not bootloader internals.
- Persistent config is mediated through typed domain models, not ad-hoc string keys scattered across modules.

## Data / Control Flow

```text
Boot
  -> load static/build-time config
  -> initialize logging + config_store
  -> initialize relay_driver to safe defaults
  -> initialize recovery_manager
  -> start net_supervisor
  -> once network policy allows, start http_service and ota_service

Operator browser
  -> HTTP server
  -> auth_service
  -> API handler
  -> action_engine / scheduler_service / config_store / ota_service
  -> relay_driver or DFU staging

Background supervision
  -> net_supervisor updates health state
  -> scheduler_service dispatches due actions
  -> recovery_manager monitors task channels and escalation thresholds
  -> telemetry/event_log records state transitions and failures
```

## Brownfield Refactor Strategy

### Refactor shape

1. Keep `main.c` as a temporary bootstrap entrypoint.
2. Extract pure modules first: config model, relay abstraction, action contracts, scheduler model.
3. Extract service modules next: Wi-Fi/network supervision, recovery/watchdog, HTTP/auth, OTA.
4. Replace cross-module globals with explicit context structs and narrow APIs.
5. Add tests around the modules that contain policy/state-machine logic before wiring everything together.

### Suggested Top-Level Layout

```text
app/
  include/app/
    app_core.h
    config/
    net/
    recovery/
    http/
    auth/
    actions/
    scheduler/
    ota/
    relay/
  src/
    main.c
    app_core.c
    config/
    net/
    recovery/
    http/
    auth/
    actions/
    scheduler/
    ota/
    relay/
  ui/
    index.html
    app.js
    styles/
```

### UI Asset Strategy

- Author the panel as normal frontend files under `app/ui/`.
- Build/compress/embed those files as Zephyr HTTP static resources for v1.
- Treat the UI bundle as part of the firmware artifact, even though the source is proper HTML/JS.

## Suggested Build Order

1. **Foundation split** — module boundaries, typed config, relay abstraction, common error model.
2. **Network + recovery** — Wi-Fi state machine, Connection Manager events, watchdog strategy.
3. **Persistence + auth + panel shell** — config store, login/session flow, status UI shell.
4. **Action engine + relay command path** — manual relay control from API/UI.
5. **Scheduler** — persistent scheduled jobs triggering relay actions.
6. **OTA** — MCUboot/sysbuild, upload/pull staging, health-confirm flow.
7. **Operational hardening** — event log, failure injection, polish, and validation.

## Why This Order

- It removes `main.c` risk before complexity explodes.
- It establishes supervision before adding control-plane features that depend on uptime.
- It ensures API/UI work sits on typed persistence and auth boundaries instead of ad-hoc globals.
- It keeps OTA late enough that the system is structured, but early enough to shape partitioning and build flow before release.

## Sources

- https://docs.zephyrproject.org/latest/connectivity/networking/api/http_server.html
- https://docs.zephyrproject.org/latest/connectivity/networking/api/wifi.html
- https://docs.zephyrproject.org/latest/connectivity/networking/conn_mgr/main.html
- https://docs.zephyrproject.org/latest/services/storage/settings/index.html
- https://docs.zephyrproject.org/latest/services/task_wdt/index.html
- https://docs.zephyrproject.org/latest/services/device_mgmt/dfu.html
- https://docs.zephyrproject.org/latest/services/device_mgmt/mcumgr.html
- `.planning/PROJECT.md`
- `.planning/codebase/ARCHITECTURE.md`

---
*Architecture research for: mission-critical Nordic/Zephyr control device*
*Researched: 2026-03-08*
