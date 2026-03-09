---
phase: 5
slug: local-control-panel
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-03-09
---

# Phase 5 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | other — build-first Zephyr validation plus browser/curl sign-off |
| **Config file** | none — existing scripts + app config |
| **Quick run command** | `bash -n scripts/validate.sh scripts/common.sh scripts/build.sh` |
| **Full suite command** | `./scripts/validate.sh` |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run `bash -n scripts/validate.sh scripts/common.sh scripts/build.sh`
- **After every plan wave:** Run `./scripts/validate.sh`
- **Before `$gsd-verify-work`:** Full automated validation must be green, then complete browser/curl/device scenarios for auth flow, protected routes, and dashboard status rendering
- **Max feedback latency:** 30 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 05-01-01 | 01 | 1 | PANEL-01 | build wiring | `rg -n "CONFIG_HTTP_SERVER|CONFIG_HTTP_SERVER_CAPTURE_HEADERS|APP_PANEL_" app/Kconfig app/prj.conf && test -f app/src/panel/panel_http.c` | ❌ planned | ⬜ pending |
| 05-01-02 | 01 | 1 | PANEL-01, PANEL-02 | asset pipeline | `test -f app/src/panel/assets/index.html && test -f app/src/panel/assets/main.js && rg -n "generate_inc_file_for_target|zephyr_linker_sources|sections-rom\.ld" app/CMakeLists.txt` | ❌ planned | ⬜ pending |
| 05-01-03 | 01 | 1 | PANEL-01 | integration | `rg -n "panel_http_server_|APP_READY|panel_http" app/src/app/bootstrap.c app/src/panel/panel_http.c app/src/panel/panel_http.h && test -f app/sections-rom.ld` | ❌ planned | ⬜ pending |
| 05-02-01 | 02 | 2 | AUTH-01, AUTH-03 | auth core | `test -f app/src/panel/panel_auth.c && rg -n "session|cooldown|persisted_config\.auth|entropy|token" app/src/panel/panel_auth.c app/src/panel/panel_auth.h` | ❌ planned | ⬜ pending |
| 05-02-02 | 02 | 2 | AUTH-01, AUTH-02, AUTH-03 | route protection | `rg -n "login|logout|session|Set-Cookie|Cookie|Path=/|HTTP_SERVER_REGISTER_HEADER_CAPTURE" app/src/panel/panel_http.c app/src/panel/panel_auth.c app/src/panel/panel_auth.h && ! rg -n "/api/(relay|schedule|update|config)" app/src/panel/panel_http.c` | ❌ planned | ⬜ pending |
| 05-02-03 | 02 | 2 | AUTH-02, PANEL-03 | status API | `test -f app/src/panel/panel_status.c && rg -n "api/status|network_state|recovery|persisted_config|relay|schedule|update" app/src/panel/panel_http.c app/src/panel/panel_status.c app/src/panel/panel_status.h` | ❌ planned | ⬜ pending |
| 05-03-01 | 03 | 3 | PANEL-01, PANEL-02, PANEL-03 | frontend | `rg -n "Tailwind|cdn\.tailwindcss\.com|login|dashboard|status" app/src/panel/assets/index.html app/src/panel/assets/main.js` | ❌ planned | ⬜ pending |
| 05-03-02 | 03 | 3 | AUTH-03, PANEL-03 | UX flow | `rg -n "logout|session|fetch\('/api/status|LAN_UP_UPSTREAM_DEGRADED|placeholder" app/src/panel/assets/main.js app/src/panel/assets/index.html` | ❌ planned | ⬜ pending |
| 05-03-03 | 03 | 3 | AUTH-01, AUTH-02, AUTH-03, PANEL-01, PANEL-02, PANEL-03 | docs + shell | `bash -n scripts/validate.sh scripts/common.sh scripts/build.sh && rg -n "Phase 5|panel|login|logout|cookie|status|browser|curl|CONFIG_APP_ADMIN_USERNAME|CONFIG_APP_ADMIN_PASSWORD" README.md scripts/validate.sh scripts/common.sh app/wifi.secrets.conf.example` | ✅ existing | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing script-based validation remains the canonical automated signal for the phase
- [x] No new external test framework is required before execution begins
- [x] Each planned task has either an automated structural/build check or explicit manual sign-off coverage
- [x] Manual browser/curl scenarios are called out up front for session behavior that cannot be proven with static checks alone

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Unauthenticated access is denied to protected routes, while future control/configuration/scheduling/update endpoints remain intentionally unavailable in Phase 5 | AUTH-02 | Requires a flashed device and real HTTP requests against the running firmware | Run `./scripts/flash.sh`, determine the device IP from `./scripts/console.sh`, then verify `curl -i http://<device-ip>/api/status` returns an auth denial before login and confirm Phase 5 does not expose mutating relay/schedule/update/config endpoints. |
| Correct login persists across refresh/navigation until explicit logout | AUTH-01, AUTH-03 | Browser session semantics and cookie handling need real navigation behavior | Open the panel in a browser, log in, refresh, navigate between status views, and confirm the session remains active until the logout action is used. |
| Wrong-password retries enter and recover from cooldown | AUTH-01 | Cooldown timing and operator messaging are runtime UX behaviors | Submit repeated wrong passwords until cooldown triggers, confirm login is temporarily rejected, wait for cooldown expiry, then confirm valid login succeeds. |
| Concurrent browsers stay signed in independently | AUTH-03 | Requires two separate browser contexts or devices on the LAN | Log in from two browsers or one browser plus one private/incognito session, confirm both remain usable, then log out from one and confirm the other still behaves according to the intended contract. |
| Device reboot invalidates active sessions | AUTH-03 | Requires session state to be tested across firmware restart | Log in, confirm protected access works, reboot the device, then verify the old browser session no longer reaches protected routes without logging in again. |
| Dashboard reflects local device/network state even when Tailwind CDN is degraded | PANEL-02, PANEL-03 | Needs a real device plus a network path where the CDN is unavailable but the panel is still reachable | With the panel running, simulate or observe `LAN_UP_UPSTREAM_DEGRADED`, load the panel, and confirm local status data still renders clearly even if Tailwind styling is reduced. |
| Dashboard remains read-only for relay, scheduler, and update surfaces in this phase | PANEL-03 | Requires real rendered UI review to avoid accidental control affordances | Inspect the dashboard after login and confirm relay, scheduler, and update sections are status-only or explicit placeholders with no Phase 6-8 mutating controls. |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or declared manual sign-off
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 30s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
