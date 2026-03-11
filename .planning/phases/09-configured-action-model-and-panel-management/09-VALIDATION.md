---
phase: 9
slug: configured-action-model-and-panel-management
status: approved
nyquist_compliant: true
wave_0_complete: true
created: 2026-03-11
---

# Phase 9 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | other — build-first Zephyr firmware validation plus blocking browser, curl, and device sign-off for the configured-action panel flow |
| **Config file** | none — existing repo scripts plus the current Playwright harness under `tests/` |
| **Quick run command** | `./scripts/build.sh` |
| **Full suite command** | `./scripts/validate.sh` |
| **Estimated runtime** | ~90 seconds |

---

## Sampling Rate

- **After every task commit:** Run `./scripts/build.sh`
- **After every plan wave:** Run `./scripts/validate.sh`
- **Before `$gsd-verify-work`:** Full automated validation must be green, then complete the blocking browser, curl, and reboot scenarios for configured-action management
- **Max feedback latency:** 90 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 09-01-01 | 01 | 1 | BIND-01, BIND-02 | structural | `rg -n "display_name|output_key|command|enabled|action_id" app/src/persistence/persistence_types.h app/src/actions/actions.h app/src/actions/actions.c` | ❌ planned | ⬜ pending |
| 09-01-02 | 01 | 1 | MIGR-01, MIGR-03 | structural | `rg -n "relay0\\.on|relay0\\.off|legacy|migration|normalize|built-in" app/src/actions/actions.c app/src/persistence/persistence.c app/src/app/bootstrap.c` | ❌ planned | ⬜ pending |
| 09-01-03 | 01 | 1 | BIND-01, BIND-02, MIGR-03 | build + structural | `./scripts/build.sh && rg -n "relay_outputs|label|binding|schedule|legacy|persistence_store_save_actions" app/boards/nrf7002dk_nrf5340_cpuapp.overlay app/src/relay/relay.c app/src/actions/actions.c app/src/persistence/persistence.c app/src/scheduler/scheduler.c` | ✅ existing | ⬜ pending |
| 09-02-01 | 02 | 2 | ACT-04 | API surface | `rg -n "/api/actions|action snapshot|configured action|Ready|Disabled|Needs attention" app/src/panel/panel_http.c app/src/panel/panel_status.c` | ❌ planned | ⬜ pending |
| 09-02-02 | 02 | 2 | ACT-03, BIND-01, BIND-02 | API validation | `rg -n "operator-safe|normalize|collision|binding|invalid-action|invalid-binding|display name" app/src/panel/panel_http.c app/src/actions/actions.c` | ❌ planned | ⬜ pending |
| 09-02-03 | 02 | 2 | ACT-05, MIGR-03 | build + runtime wiring | `./scripts/build.sh && rg -n "persistence_store_save_actions|reload|configured action|enabled state|usability" app/src/panel/panel_http.c app/src/actions/actions.c app/src/persistence/persistence.c` | ✅ existing | ⬜ pending |
| 09-03-01 | 03 | 3 | ACT-03, ACT-04 | UI surface | `rg -n "No configured actions|Create action|Edit action|Needs attention|Ready|Disabled" app/src/panel/assets/main.js app/src/panel/assets/index.html` | ❌ planned | ⬜ pending |
| 09-03-02 | 03 | 3 | ACT-04, ACT-05 | UI/API integration | `rg -n "/api/actions|renderActions|action catalog|output summary|enabled" app/src/panel/assets/main.js` | ❌ planned | ⬜ pending |
| 09-03-03 | 03 | 3 | MIGR-03 | structural + build | `./scripts/build.sh && rg -n "/api/relay/desired-state|relay-on|relay-off|actionChoices" app/src/panel/assets/main.js app/src/panel/panel_http.c app/src/panel/panel_status.c tests` | ✅ existing | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing script-based validation remains the canonical automated signal
- [x] No new test framework is required before Phase 9 execution begins
- [x] If browser automation expands, it should reuse the existing Playwright harness and `tests/helpers/panel-target.js` device-discovery flow
- [x] Final approval remains blocked on browser, curl, and reboot-backed validation that the build-only path cannot prove

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Clean boot no longer exposes fake-ready relay controls or hidden built-in action truth | MIGR-01, MIGR-03 | Requires live device boot, panel rendering, and persisted catalog inspection | Boot the device with no configured actions, authenticate, open the Actions page, and confirm the page shows the create-first empty state instead of executable relay toggles or built-in action placeholders. |
| Authenticated operator can create a valid configured relay action that survives reboot | ACT-03, ACT-04, BIND-01 | Requires live panel mutation, generated ID behavior, persistence, and reboot | Create a relay action through the Actions page, confirm the API or UI reports a generated operator-safe ID and `Ready` status, reboot the device, and confirm the same record returns unchanged. |
| Invalid or unsafe binding requests are rejected before the action becomes usable | BIND-02 | Requires forged request coverage against the authenticated API, not just the happy-path UI | Send an authenticated create or update request with an unsupported output binding, then confirm the API rejects it and no `Ready` action record is created from that request. |
| Operator can edit name, binding, and enabled state and see truthful catalog status afterward | ACT-05, ACT-04 | Requires live mutation, refresh, and usability rendering | Edit an existing action, toggle enabled state, change the approved binding, refresh the page, and confirm the catalog shows the updated name, output summary, and `Ready` or `Disabled` status correctly. |
| Legacy built-in action records are handled explicitly without hidden fallback execution | MIGR-01, MIGR-03 | Requires persisted legacy data and boot-time migration observation | Start from a device snapshot that still contains legacy built-in action records, boot the new firmware, and confirm boot logs or panel truth show explicit handling while the firmware does not silently restore `relay0.on` or `relay0.off` as the primary configured-action model. |
| Existing schedules that still reference legacy built-in IDs survive Phase 9 boot and validation | MIGR-03 | Requires persisted legacy schedules plus reboot-backed validation | Boot Phase 9 firmware against a persisted schedule table that still references legacy relay built-in IDs, then confirm the schedule section is preserved rather than reset while those references remain compatibility-only behavior. |
| Schedules remain honest and deferred until Phase 10 | MIGR-03 | Requires checking cross-surface truth after the action catalog exists | After Phase 9 implementation, open the Schedules surface and confirm it does not claim configured actions are selectable or fully integrated before Phase 10 lands that shared-catalog path. |

*If none: "All phase behaviors have automated verification."*

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or existing Wave 0 infrastructure
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all missing framework dependencies
- [x] No watch-mode flags
- [x] Feedback latency < 90s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved on 2026-03-11
