---
phase: 6
slug: action-engine-relay-control
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-03-10
---

# Phase 6 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | other — build-first Zephyr validation plus blocking browser/curl/device relay safety sign-off |
| **Config file** | none — existing scripts, board overlay, and app config |
| **Quick run command** | `./scripts/build.sh` |
| **Full suite command** | `./scripts/validate.sh` |
| **Estimated runtime** | ~25 seconds |

---

## Sampling Rate

- **After every task commit:** Run `./scripts/build.sh`
- **After every plan wave:** Run `./scripts/validate.sh`
- **Before `$gsd-verify-work`:** Full automated validation must be green, then complete browser/curl/device relay control and safety scenarios on real hardware
- **Max feedback latency:** 45 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 06-01-01 | 01 | 1 | RELAY-03 | build wiring | `rg -n "relay\.c|relay_service" app/CMakeLists.txt && rg -n "CONFIG_GPIO=y" app/prj.conf && test -f app/boards/nrf7002dk_nrf5340_cpuapp.overlay && rg -n "relay0|gpios" app/boards/nrf7002dk_nrf5340_cpuapp.overlay` | ❌ planned | ⬜ pending |
| 06-01-02 | 01 | 1 | RELAY-03 | runtime policy | `test -f app/src/relay/relay.c && rg -n "reboot_policy|recovery|desired|actual|source|safe|gpio" app/src/relay/relay.c app/src/relay/relay.h` | ❌ planned | ⬜ pending |
| 06-01-03 | 01 | 1 | RELAY-03 | integration | `./scripts/build.sh && rg -n "relay_service_|persisted_config\.relay|recovery_manager_last_reset_cause|panel_http_server_init" app/src/app/bootstrap.c app/src/app/app_context.h app/src/relay/relay.c app/src/relay/relay.h` | ❌ planned | ⬜ pending |
| 06-02-01 | 02 | 2 | ACT-01 | dispatcher contract | `test -f app/src/actions/actions.c && rg -n "action_dispatcher_|action_id|source|execute|result" app/src/actions/actions.c app/src/actions/actions.h` | ❌ planned | ⬜ pending |
| 06-02-02 | 02 | 2 | ACT-01, ACT-02 | catalog seeding | `rg -n "relay.*on|relay.*off|built-?in|action_id|persisted_config\.actions|persistence_store_save_actions" app/src/actions/actions.c app/src/actions/actions.h app/src/persistence/persistence.h` | ❌ planned | ⬜ pending |
| 06-02-03 | 02 | 2 | ACT-02 | shared execution path | `./scripts/build.sh && rg -n "action_dispatcher_execute|enabled|last_desired_state|source|relay_service_" app/src/actions/actions.c app/src/actions/actions.h app/src/relay/relay.c app/src/relay/relay.h && ! rg -n "gpio_pin_(configure|set)" app/src/panel/panel_http.c` | ❌ planned | ⬜ pending |
| 06-03-01 | 03 | 3 | RELAY-01, RELAY-02, ACT-02 | API route | `rg -n "/api/relay|desired-state|action_dispatcher_|actual|desired|source|safety|blocked|pending" app/src/panel/panel_http.c app/src/panel/panel_status.c app/src/panel/panel_status.h && ! rg -n "/api/(schedule|update|config)" app/src/panel/panel_http.c` | ❌ planned | ⬜ pending |
| 06-03-02 | 03 | 3 | RELAY-01, RELAY-02 | frontend | `rg -n "relay|toggle|desired|actual|source|safety|pending|blocked" app/src/panel/assets/index.html app/src/panel/assets/main.js && ! rg -n "confirm\(" app/src/panel/assets/main.js` | ❌ planned | ⬜ pending |
| 06-03-03 | 03 | 3 | RELAY-01, RELAY-02, RELAY-03 | docs + validation | `bash -n scripts/validate.sh scripts/common.sh scripts/build.sh && rg -n "Phase 6|relay|curl|desired|actual|recovery|safe|toggle" README.md scripts/validate.sh scripts/common.sh` | ✅ existing | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing script-based validation remains the canonical automated signal
- [x] No new external test framework is required before execution begins
- [x] Phase 6 tasks pair firmware changes with build or structural checks, and docs or script tasks keep shell-static checks
- [x] Final approval remains blocked on real device, browser, and curl confirmation of actuator and safety semantics

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Browser toggle turns the first relay on and the panel reports the new actual, desired, and source fields coherently | RELAY-01 | Requires flashed firmware, a real actuator path, and browser interaction against the live device | Run `./scripts/flash.sh`, log in through the panel, toggle the relay on once, then confirm the physical outcome together with `GET /api/status` and the rendered card state. |
| Browser toggle turns the first relay off and the panel reflects the new live state without a confirmation dialog | RELAY-02 | Requires real runtime UI behavior plus physical observation of the output | From the authenticated panel, toggle the relay off, confirm no routine confirm dialog appears, and verify the panel plus the physical relay both settle to off. |
| Equivalent curl commands can drive relay on and off through the authenticated control route | RELAY-01, RELAY-02 | Requires device IP, real auth cookie handling, and the live HTTP surface | Use the Phase 5 login curl flow to obtain a `sid` cookie, invoke the Phase 6 relay route to drive on and off, and confirm the device plus status payload match each command. |
| Local relay control still works while upstream reachability is degraded but LAN access is healthy | RELAY-01, RELAY-02 | Requires a real network condition where the panel is reachable locally but the reachability probe is degraded | With the device in `LAN_UP_UPSTREAM_DEGRADED`, log in locally, send a relay command, and confirm the command succeeds and the panel remains usable on the LAN. |
| Ordinary reboot obeys the configured normal-boot relay policy | RELAY-03 | Needs a real reboot and observable relay outcome after boot | Set up the intended remembered desired state, reboot the device normally, and confirm the relay and status payload match the configured normal-boot policy. |
| Recovery-triggered reboot or confirmed fault recovery forces the relay off until a fresh command is issued | RELAY-03 | Requires a real recovery-reset path that build output cannot simulate fully | Trigger the recovery-reset scenario defined for the device, confirm the next boot forces the relay off, and verify the panel shows that a fresh command is now required. |
| Panel makes actual-versus-desired mismatch visible when safety policy keeps the applied relay state off | RELAY-03 | Requires observing runtime UI text against a live safety override case | Create a case where the remembered desired state is on but boot or recovery policy forces the relay off, then confirm the relay card shows both values plus the safety note clearly. |
| Toggle locks and explains pending, blocked, or failure cases inline | RELAY-01, RELAY-02 | In-flight state and blocked feedback are runtime UX behaviors | Exercise a pending or blocked control case on the running device and confirm the toggle becomes non-interactive until the outcome is known and the inline reason is visible. |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or declared manual sign-off
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 45s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved on 2026-03-10
