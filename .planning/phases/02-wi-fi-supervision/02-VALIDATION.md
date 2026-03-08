---
phase: 2
slug: wi-fi-supervision
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-03-08
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | other — fast static checks plus build/script smoke |
| **Config file** | none — existing scripts + Zephyr app config |
| **Quick run command** | `bash -n scripts/validate.sh scripts/common.sh` |
| **Full suite command** | `./scripts/validate.sh` |
| **Estimated runtime** | ~10 seconds |

---

## Sampling Rate

- **After every task commit:** Run `bash -n scripts/validate.sh scripts/common.sh`
- **After every plan wave:** Run `./scripts/validate.sh`
- **Before `$gsd-verify-work`:** Full automated validation must be green, then complete manual device scenarios for healthy, degraded, and recovered network states
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | NET-01, NET-03 | structure | `test -f app/src/network/network_supervisor.h && rg -n "NETWORK_CONNECTIVITY_|last_failure|failure_stage|network_supervisor" app/src/network/network_state.h app/src/network/network_supervisor.h` | ❌ planned | ⬜ pending |
| 02-01-02 | 01 | 1 | NET-01, NET-03 | integration | `test -f app/src/network/network_supervisor.c && rg -n "network_supervisor_" app/src/app/bootstrap.c app/src/network/network_supervisor.c && ! rg -n "wifi_lifecycle_wait_for_connection_and_ipv4|reachability_check_host" app/src/app/bootstrap.c` | ❌ planned | ⬜ pending |
| 02-01-03 | 01 | 1 | NET-01, NET-03 | event coverage | `rg -n "NET_EVENT_IPV4_DHCP_STOP|NET_EVENT_IPV4_ADDR_DEL|NET_EVENT_WIFI_DISCONNECT_RESULT" app/src/network/wifi_lifecycle.c && rg -n "wifi_lifecycle_|network_supervisor_" app/src/network/wifi_lifecycle.h app/src/network/network_supervisor.c` | ❌ planned | ⬜ pending |
| 02-02-01 | 02 | 2 | NET-01, NET-02 | policy | `rg -n "APP_WIFI_RETRY_INTERVAL_MS" app/Kconfig && rg -n "startup|retry|k_work_delayable" app/src/app/app_config.h app/src/network/network_supervisor.c app/src/network/network_supervisor.h` | ❌ planned | ⬜ pending |
| 02-02-02 | 02 | 2 | NET-02, NET-03 | state reporting | `rg -n "LAN_UP_UPSTREAM_DEGRADED|last_failure" app/src/network/network_state.h && rg -n "reachability|healthy|degraded|failure" app/src/network/network_supervisor.c app/src/network/reachability.c` | ❌ planned | ⬜ pending |
| 02-02-03 | 02 | 2 | NET-01, NET-02, NET-03 | scope guard | `! rg -n "sys_reboot|watchdog|wdt" app/src/app/bootstrap.c app/src/network/network_supervisor.c && rg -n "retry|degraded|healthy|failure" app/src/network/network_supervisor.c` | ❌ planned | ⬜ pending |
| 02-03-01 | 03 | 3 | NET-01, NET-02, NET-03 | docs | `bash -n scripts/validate.sh scripts/common.sh && rg -n "healthy|degraded|retry|upstream|validate\.sh|flash\.sh|console\.sh" README.md scripts/common.sh scripts/validate.sh` | ✅ existing | ⬜ pending |
| 02-03-02 | 03 | 3 | NET-01, NET-02, NET-03 | docs + shell | `bash -n scripts/validate.sh scripts/common.sh && rg -n "build\.sh|healthy|degraded|recovery|upstream" README.md scripts/validate.sh scripts/common.sh` | ✅ existing | ⬜ pending |
| 02-03-03 | 03 | 3 | NET-02, NET-03 | manual checkpoint | `./scripts/flash.sh` then `./scripts/console.sh` and confirm healthy, degraded-retrying, upstream-degraded, and recovery markers` | ✅ existing | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing shell and documentation surfaces already support fast automated verification without adding a new framework
- [x] No `MISSING` automated references remain in the phase task map
- [x] Existing `./scripts/validate.sh` remains the canonical wave-level automated validation command
- [x] No new framework install is required before execution begins

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Device reaches the healthy state on normal boot | NET-01, NET-03 | Requires real Wi-Fi, DHCP, DNS, and serial observation on Nordic hardware | Run `./scripts/flash.sh`, then `./scripts/console.sh`; confirm the Phase 2 healthy-state markers and record the final state plus last-failure visibility behavior in the summary. |
| Device does not hang forever when boot cannot reach full health | NET-01, NET-02 | Requires a real AP outage or equivalent environment failure during boot | Boot with the AP unavailable or dependency path intentionally broken; confirm the startup window expires into an explicit degraded/retrying state and background supervision continues. |
| Device recovers from runtime Wi-Fi loss without reboot | NET-02 | Requires live network disruption during a running firmware session | While the device is running, force AP loss or disconnect, confirm the state transitions to retrying, then restore connectivity and confirm it returns to healthy without full device restart. |
| Device distinguishes LAN-up from upstream-degraded | NET-03 | Requires a network environment where Wi-Fi and DHCP stay up while the configured reachability target fails | Keep Wi-Fi association and DHCP alive, break the configured reachability target path, confirm the distinct upstream-degraded state, then restore the path and confirm healthy state returns while last failure remains visible until replaced. |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or declared manual sign-off
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 30s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
