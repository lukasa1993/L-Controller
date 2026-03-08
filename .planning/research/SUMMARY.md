# Project Research Summary

**Project:** LNH Nordic
**Domain:** Mission-critical Nordic/Zephyr embedded relay controller with local web operations
**Researched:** 2026-03-08
**Confidence:** HIGH

## Executive Summary

This project is best treated as a serious embedded control product, not as a “Wi-Fi demo plus a small web server.” The official Zephyr/Nordic path supports the major building blocks already needed here: Wi-Fi lifecycle management, connectivity monitoring, HTTP serving, persistent settings, watchdog supervision, and bootloader-backed firmware updates. That means the biggest architectural risk is not missing platform capability — it is keeping too much logic coupled inside a single `main.c` and then layering mission-critical behavior on top of that.

The recommended approach is to first establish clean subsystem boundaries and conservative supervision, then add the local control plane, then wire the first real action (relay on/off), then add scheduling, and finally harden OTA. OTA should rely on MCUboot/sysbuild and image-slot discipline, while the web panel should use authored HTML/JS assets embedded or served as proper files rather than generated markup in C.

## Key Findings

### Recommended Stack

Use official Zephyr/Nordic building blocks instead of custom infrastructure.

**Core technologies:**
- Zephyr `wifi_mgmt`, `net_mgmt`, and Connection Manager — Wi-Fi/IP lifecycle and event-driven supervision
- Zephyr HTTP Server — local panel, static assets, and API endpoints
- Settings subsystem with NVS backend — conservative persistent config storage
- Task Watchdog with hardware watchdog fallback — layered supervision and reset escalation
- MCUboot + sysbuild + DFU APIs — safe firmware update foundation
- MCUmgr over UDP plus custom HTTP upload handling — remote management/update transport and operator upload path
- Devicetree GPIO abstractions — safe relay control boundaries

### Expected Features

**Must have (table stakes):**
- Local authenticated admin panel
- Robust Wi-Fi supervision and reconnection
- Manual relay control
- Persistent config storage
- Scheduled actions
- Safe OTA path

**Should have (competitive / structural):**
- Generic action engine
- Event/audit visibility
- Future-ready integration framework

**Defer (v2+):**
- Multi-user access control
- Public/cloud remote management
- Rich third-party SDK integrations

### Architecture Approach

The brownfield repo should become a small composition root plus isolated services: config store, network supervisor, recovery manager, HTTP/auth layer, action engine, scheduler, relay driver, OTA service, and telemetry. All control mutations should flow through the action engine; all health decisions should flow through the recovery manager.

**Major components:**
1. `app_core` — startup ordering and wiring
2. `net_supervisor` + `recovery_manager` — connectivity and fault handling
3. `http_service` + `auth_service` — local operator interface
4. `action_engine` + `relay_driver` — first business function
5. `scheduler_service` — cron-style automation
6. `ota_service` — image staging and swap flow

### Critical Pitfalls

1. **Growing the system inside `main.c`** — split modules before feature growth
2. **Rebooting on transient Wi-Fi issues** — use retry budgets and escalation thresholds
3. **Weak watchdog feeding strategy** — supervise critical subsystems individually
4. **Unsafe config writes** — centralize persistence and validate schemas
5. **Unsafe OTA boot path** — require slot-based updates and image confirmation

## Implications for Roadmap

Based on research, the likely safe phase structure is:

### Phase 1: Foundation Refactor
**Rationale:** architecture risk is currently the biggest blocker
**Delivers:** module boundaries, typed config model, relay abstraction, service skeletons
**Avoids:** `main.c` sprawl

### Phase 2: Network Supervision & Recovery
**Rationale:** mission-critical behavior starts with fault handling, not the UI
**Delivers:** Wi-Fi state machine, reachability policy, watchdog/escalation model
**Uses:** Wi-Fi management, Connection Manager, task watchdog

### Phase 3: Local Control Plane
**Rationale:** panel/auth/config need stable system boundaries underneath
**Delivers:** persistent settings, single-user auth, HTTP panel shell, status endpoints
**Uses:** settings subsystem, HTTP server, embedded UI assets

### Phase 4: Action Engine & Relay Control
**Rationale:** first real product function should land on top of the control plane
**Delivers:** validated relay on/off action path from UI/API to hardware
**Implements:** action engine + relay driver

### Phase 5: Scheduling
**Rationale:** scheduling depends on persistent config and action abstractions
**Delivers:** cron-style jobs triggering relay actions with defined time semantics
**Avoids:** hidden time drift / missed-job ambiguity

### Phase 6: OTA Lifecycle
**Rationale:** safest once the system is structured enough to test boot/update health clearly
**Delivers:** MCUboot/sysbuild integration, upload/pull staging, confirm/rollback flow
**Uses:** DFU APIs and slot-based boot discipline

### Phase Ordering Rationale

- Subsystem boundaries come before feature breadth.
- Recovery and connectivity come before operator UX.
- Scheduler comes after action abstractions and persistence exist.
- OTA comes after structure is stable enough to validate the boot/update lifecycle seriously.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 5:** time source strategy, missed-job semantics, and clock sync behavior
- **Phase 6:** exact remote-pull OTA transport and packaging policy

Phases with standard patterns:
- **Phase 2:** Wi-Fi supervision and task watchdog architecture
- **Phase 3:** HTTP server + settings-backed local panel pattern

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Strong official Zephyr coverage for the needed subsystems |
| Features | MEDIUM | Feature expectations are partly domain inference and user-driven |
| Architecture | HIGH | Brownfield refactor path is clear and platform-aligned |
| Pitfalls | HIGH | Risks are concrete and directly tied to the requested system behavior |

**Overall confidence:** HIGH

### Gaps to Address

- Exact v1 time source and cron semantics for schedules
- Exact remote-pull OTA transport/policy and artifact format
- Whether UI assets stay embedded-only or later move to filesystem-backed static serving

## Sources

### Primary
- https://docs.zephyrproject.org/latest/connectivity/networking/api/http_server.html
- https://docs.zephyrproject.org/latest/connectivity/networking/api/wifi.html
- https://docs.zephyrproject.org/latest/connectivity/networking/conn_mgr/main.html
- https://docs.zephyrproject.org/latest/services/storage/settings/index.html
- https://docs.zephyrproject.org/latest/services/task_wdt/index.html
- https://docs.zephyrproject.org/latest/services/device_mgmt/dfu.html
- https://docs.zephyrproject.org/latest/services/device_mgmt/mcumgr.html
- https://docs.zephyrproject.org/latest/build/sysbuild/index.html

### Project context
- `.planning/PROJECT.md`
- `.planning/codebase/ARCHITECTURE.md`
- `.planning/codebase/STACK.md`

---
*Research completed: 2026-03-08*
*Ready for roadmap: yes*
