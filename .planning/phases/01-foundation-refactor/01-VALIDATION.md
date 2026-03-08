---
phase: 1
slug: foundation-refactor
status: draft
nyquist_compliant: false
wave_0_complete: true
created: 2026-03-08
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | other — build/script smoke checks |
| **Config file** | none — existing scripts + Zephyr app config |
| **Quick run command** | `./scripts/build.sh` |
| **Full suite command** | `./scripts/build.sh` |
| **Estimated runtime** | ~180 seconds |

---

## Sampling Rate

- **After every task commit:** Run `./scripts/build.sh`
- **After every plan wave:** Run `./scripts/build.sh`
- **Before `$gsd-verify-work`:** Full automated build must be green, then complete manual hardware smoke validation
- **Max feedback latency:** 180 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 01-01-01 | 01 | 1 | PLAT-01, PLAT-03 | structure | `test -f app/src/app/app_context.h && test -f app/src/network/network_state.h` | ❌ planned | ⬜ pending |
| 01-01-02 | 01 | 1 | PLAT-01 | structure | `test -f app/src/recovery/recovery.h && test -f app/src/panel/panel_http.h && test -f app/src/relay/relay.h` | ❌ planned | ⬜ pending |
| 01-01-03 | 01 | 1 | PLAT-01 | build | `./scripts/build.sh` | ✅ existing | ⬜ pending |
| 01-02-01 | 02 | 2 | PLAT-03 | build + grep | `./scripts/build.sh && rg -n "NET_REQUEST_WIFI_CONNECT|NET_EVENT_IPV4_DHCP_BOUND" app/src/network/wifi_lifecycle.c && rg -n "app_context|network_(runtime_)?state" app/src/network/wifi_lifecycle.c app/src/app/bootstrap.c` | ❌ planned | ⬜ pending |
| 01-02-02 | 02 | 2 | PLAT-02, PLAT-03 | build + grep | `./scripts/build.sh && rg -n "getaddrinfo|APP_READY|app_boot" app/src/app/bootstrap.c app/src/network/reachability.c && rg -n "app_context|network_(runtime_)?state" app/src/app/bootstrap.c app/src/network/reachability.c` | ❌ planned | ⬜ pending |
| 01-02-03 | 02 | 2 | PLAT-02 | build + negative grep | `./scripts/build.sh && ! rg -n "K_SEM_DEFINE|net_mgmt\(|handle_wifi_|handle_ipv4_dhcp_bound|getaddrinfo|socket\(" app/src/main.c` | ✅ existing | ⬜ pending |
| 01-03-01 | 03 | 3 | PLAT-01, PLAT-02, PLAT-03 | shell | `bash -n scripts/validate.sh && test -x scripts/validate.sh` | ❌ planned | ⬜ pending |
| 01-03-02 | 03 | 3 | PLAT-02, PLAT-03 | docs | `rg -n "APP_READY|Reachability check passed|scripts/validate\.sh|flash\.sh|console\.sh" README.md` | ✅ existing | ⬜ pending |
| 01-03-03 | 03 | 3 | PLAT-01, PLAT-02, PLAT-03 | smoke | `./scripts/validate.sh` | ❌ planned | ⬜ pending |
| 01-03-04 | 03 | 3 | PLAT-02, PLAT-03 | manual checkpoint | `./scripts/flash.sh` then `./scripts/console.sh` and confirm ready-state markers | ✅ existing | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing `./scripts/build.sh` provides the baseline automated feedback loop for Phase 1
- [x] No new framework install is required before execution begins
- [ ] `scripts/validate.sh` will improve ergonomics in Wave 2, but it is not a prerequisite for starting the phase

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Refactored firmware reaches the baseline ready state on real hardware | PLAT-02, PLAT-03 | Requires Nordic board, Wi-Fi environment, DHCP/DNS, and live serial observation | Run `./scripts/flash.sh`, then `./scripts/console.sh`; confirm `Wi-Fi connected`, `DHCP IPv4 address: ...`, `Reachability check passed`, and `APP_READY`. Record the result in the Phase 1 summary. |
| Final build/flash flow still works through the repo scripts | PLAT-01, PLAT-02 | Requires attached J-Link/board and a valid local secrets overlay | After a successful `./scripts/build.sh`, run `./scripts/flash.sh` and confirm the existing toolchain + build output still deploy cleanly. |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or declared manual sign-off
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 180s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
