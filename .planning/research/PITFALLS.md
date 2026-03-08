# Pitfalls Research

**Domain:** Mission-critical embedded relay controller with local panel and OTA
**Researched:** 2026-03-08
**Confidence:** HIGH

## Critical Pitfalls

| Pitfall | Why It Hurts | Warning Signs | Prevention Strategy | Phase |
|---------|---------------|---------------|---------------------|-------|
| Continuing as a `main.c` god-object | Changes become unsafe, hard to review, and impossible to reason about under fault conditions | Merge conflicts, duplicated state, scattered globals, fragile startup edits | Refactor into explicit modules before feature growth | Phase 1 |
| Treating every Wi-Fi failure as a reboot condition | Causes reboot storms and hides whether normal reconnect logic works | Device resets during AP blips, DHCP stalls trigger full restart, uptime never stabilizes | Use a connectivity state machine, retry budgets, backoff, and escalation thresholds | Phase 2 |
| Feeding the watchdog from one central “happy path” | A stuck subsystem can be masked while the watchdog is still fed | UI hangs but device never resets, network worker deadlocks without detection | Use task watchdog channels per critical subsystem plus hardware watchdog fallback | Phase 2 |
| Mixing blocking work into network callbacks or system workqueue paths | Leads to latency spikes, deadlocks, and missed timers | Slow HTTP responses, delayed reconnects, watchdog starvation | Use dedicated workqueues and keep callbacks short/non-blocking | Phase 2 |
| Writing mutable config carelessly to flash | Power loss or frequent writes can corrupt config or wear storage early | Settings disappear after reboot, intermittent boot failures, frequent storage commits | Centralize config writes, debounce commits, version schemas, and validate on load | Phase 3 |
| Building auth as “local means safe” | Local HTTP still faces LAN threats, browser tricks, and weak-password risks | Default password stays unchanged, sessions never expire, CSRF-like surprises | Require explicit credential setup, use secure cookie rules available to HTTP, add CSRF protection, rate-limit login attempts | Phase 3 |
| Coupling UI markup to C code | Slows iteration and increases firmware risk for simple panel changes | Large string blobs in firmware code, broken escaping, poor reviewability | Keep UI as authored HTML/JS assets and embed/serve them cleanly | Phase 3 |
| Shipping OTA without rollback/confirm discipline | Failed updates can brick devices or boot-loop forever | Device dies after interrupted upload, new image never proves health, manual recovery required | Use MCUboot slots, image confirmation after healthy boot, and explicit failure handling | Phase 6 |
| Scheduling jobs without a trustworthy time strategy | Jobs fire at wrong times after reboot or network loss | Duplicate triggers, missed jobs, drift after power cycles | Define v1 time source and boot-time behavior explicitly before exposing cron UX | Phase 5 |
| Letting HTTP handlers manipulate GPIO directly | Bypasses action validation, scheduling, auditing, and safety rules | Different code paths toggle relay inconsistently | Route all mutations through a single action engine and relay service | Phase 4 |

## Security / Reliability Mistakes

| Mistake | Risk | Better Approach |
|---------|------|-----------------|
| Storing secrets or sessions in ad-hoc raw flash blobs | Corruption and migration pain | Use a single persistence subsystem with typed versioning |
| Adding remote exposure before the local control path is hardened | Premature threat surface expansion | Keep v1 LAN-only and revisit later with a fresh threat model |
| Treating OTA delivery and OTA boot safety as one concern | Update transport may work while boot safety is still broken | Separate upload/download path from image validation/boot-confirm path |
| Assuming relay control is trivial GPIO toggling | Unsafe startup states can energize hardware unexpectedly | Define safe default states and relay ownership rules first |

## “Looks Done But Isn’t” Checklist

- [ ] Wi-Fi reconnect tested against AP power cycle, DHCP delay, and DNS/reachability loss separately
- [ ] Watchdog proves recovery from a truly stuck worker, not just normal slow paths
- [ ] Config survives brownout/reboot during write
- [ ] OTA proves success, interrupted upload, bad image, and rollback behavior
- [ ] Scheduler behavior is defined for boot, missed jobs, and time resynchronization
- [ ] Relay defaults are verified on cold boot, warm boot, and failed startup
- [ ] Panel auth is tested from a real browser session flow, not just direct API calls

## Pitfall-to-Phase Mapping

| Pitfall | Phase | Verification |
|---------|-------|--------------|
| `main.c` god-object | Phase 1 | Subsystems build behind explicit module boundaries |
| Reboot-on-disconnect logic | Phase 2 | Fault injection shows reconnect before reset escalation |
| Weak watchdog design | Phase 2 | Simulated stuck workers trigger the intended escalation path |
| Unsafe config persistence | Phase 3 | Reboot during write preserves or cleanly recovers config |
| UI/C coupling | Phase 3 | Panel assets change without C string editing |
| Direct GPIO from HTTP path | Phase 4 | All command paths go through action engine APIs |
| Bad schedule/time semantics | Phase 5 | Defined tests for drift, missed jobs, and reboot behavior |
| Unsafe OTA flow | Phase 6 | Update/rollback/confirm scenarios pass on hardware |

## Sources

- https://docs.zephyrproject.org/latest/services/task_wdt/index.html
- https://docs.zephyrproject.org/latest/hardware/peripherals/watchdog.html
- https://docs.zephyrproject.org/latest/connectivity/networking/api/http_server.html
- https://docs.zephyrproject.org/latest/connectivity/networking/api/wifi.html
- https://docs.zephyrproject.org/latest/connectivity/networking/conn_mgr/main.html
- https://docs.zephyrproject.org/latest/services/storage/settings/index.html
- https://docs.zephyrproject.org/latest/services/device_mgmt/dfu.html
- https://docs.zephyrproject.org/latest/services/device_mgmt/mcumgr.html
- Project context in `.planning/PROJECT.md`

---
*Pitfalls research for: mission-critical embedded relay controller*
*Researched: 2026-03-08*
