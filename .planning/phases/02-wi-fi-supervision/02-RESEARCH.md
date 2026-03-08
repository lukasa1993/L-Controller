# Phase 2: Wi-Fi Supervision - Research

**Researched:** 2026-03-08
**Status:** Ready for planning

<focus>
## Research Goal

Determine the cleanest way to replace the current one-shot Wi-Fi bring-up path with a supervisor-owned lifecycle that keeps build-time credentials, reconnects steadily in the background, distinguishes LAN reachability from upstream health, and publishes precise connectivity state plus the last network failure for later consumers.

</focus>

<findings>
## Key Findings

### Phase 1 already created the right seams for a dedicated supervisor

- `app/src/app/bootstrap.c` already centralizes startup orchestration through `app_boot()` instead of spreading network policy across `main.c`.
- `app/src/network/wifi_lifecycle.c` already owns the low-level Wi-Fi callback registration, connect/disconnect event handling, DHCP-bound wait path, and connect request submission.
- `app/src/network/reachability.c` already isolates the post-association reachability probe, which is the exact seam needed for the required LAN-up versus upstream-degraded distinction.
- `app/src/app/app_context.h` and `app/src/network/network_state.h` already provide a shared runtime state carrier that future recovery and panel work can consume.
- `app/src/main.c` is now only a composition root plus idle loop, so Phase 2 can add long-lived supervision without another structural refactor first.

### The pinned Nordic/Zephyr stack already exposes the event surface Phase 2 needs

- The repo already uses `register_wifi_ready_callback()` from Nordic's Wi-Fi ready library, so the supervisor can observe when Wi-Fi hardware becomes usable before attempting association.
- Zephyr exposes `NET_EVENT_WIFI_CONNECT_RESULT` and `NET_EVENT_WIFI_DISCONNECT_RESULT`, which cover association success/failure and runtime disconnects.
- The pinned `net_event.h` also exposes `NET_EVENT_IPV4_DHCP_BOUND`, `NET_EVENT_IPV4_DHCP_STOP`, and `NET_EVENT_IPV4_ADDR_DEL`, so DHCP loss can be treated separately from Wi-Fi link loss.
- The Wi-Fi ready callback runs on the NET_MGMT thread, so callbacks must stay lightweight: record state, update semaphores, and schedule follow-up work instead of doing reachability probes or retry sleeps inline.

### The cleanest Phase 2 shape is a true supervisor module above the existing lifecycle helpers

The most incremental design is:

1. Keep `wifi_lifecycle` as the low-level adapter for Wi-Fi events, connect requests, DHCP-edge tracking, and interface status logging.
2. Keep `reachability_check_host()` as the probe implementation for the configured dependency target.
3. Add a dedicated `network_supervisor` module that owns policy: startup timing, reconnect cadence, reachability decisions, named connectivity states, and last-failure publication.
4. Change `app_boot()` so it initializes and starts the supervisor, then waits only for a bounded startup outcome instead of performing the whole network lifecycle itself.

This is safer than replacing the existing implementation with Zephyr Connection Manager in the same phase. The repo already has working Wi-Fi seams; Phase 2 should build on those seams rather than swapping to a different connectivity framework while the product behavior is still being defined.

### Boot semantics must shift from blocking bring-up to bounded startup supervision

- The current flow blocks in `app_boot()` until Wi-Fi association, DHCP, and reachability all succeed, then returns `APP_READY`.
- Phase 2 requires a different contract: the supervisor should try to achieve full health during a bounded startup window, but if that does not happen it must enter an explicit degraded state and continue supervising in the background instead of returning a fatal error for transient network problems.
- Fatal returns should stay limited to true initialization errors such as invalid config, no Wi-Fi interface, or callback-registration failure.
- The existing `CONFIG_APP_WIFI_TIMEOUT_MS` is the natural startup deadline to reuse for this bounded boot outcome.

### Connectivity state must become a first-class contract instead of raw booleans

The current runtime state is useful for internal coordination, but it is too implicit for later consumers. Phase 2 should add an explicit status contract to `struct network_runtime_state`, for example:

- `NETWORK_CONNECTIVITY_NOT_READY`
- `NETWORK_CONNECTIVITY_CONNECTING`
- `NETWORK_CONNECTIVITY_HEALTHY`
- `NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED`
- `NETWORK_CONNECTIVITY_DEGRADED_RETRYING`

That named state should coexist with raw details such as Wi-Fi readiness, association, IPv4 lease, and reachability result. The explicit state is what future panel and recovery code should read first.

### Last-failure publication should be structured and persistent across recovery

Phase 2 should add a structured last-failure payload instead of relying on one disconnect status integer. The payload should preserve at least:

- failure stage (`ready`, `connect`, `dhcp`, `reachability`, `disconnect`)
- reason code (`int` return/status value)
- whether a failure is currently recorded

The most recent failure should remain visible after recovery until a newer failure replaces it. That matches the phase context and gives later operator surfaces something durable to explain.

### Reconnect policy should use steady scheduled work, not nested blocking loops or reboot logic

- The current code has a one-shot `wifi_lifecycle_connect_once()` plus blocking sem waits; there is no background owner after boot succeeds or fails.
- Phase 2 should introduce a steady reconnect cadence through a `k_work_delayable`-owned retry path or an equivalent supervisor worker. That keeps retry behavior explicit and avoids sleeping inside callbacks.
- Disconnect, DHCP-loss, and reachability-failure transitions should schedule the same supervisor recovery path instead of creating separate policy branches.
- This phase should not introduce reboot escalation or “permanently stuck” classification. The roadmap reserves those concerns for the dedicated Recovery & Watchdog phase.

### Reachability should be supervisor-owned and re-checked after link recovery

- The existing `reachability_check_host()` implementation is already a good probe for “full health” because it validates the configured upstream dependency path, not just local association.
- Phase 2 should run that probe after DHCP success and after recovery events, so the system can distinguish between local LAN availability and upstream failure.
- `LAN_UP_UPSTREAM_DEGRADED` should be reachable both during boot and after runtime disruptions.
- Reachability probing must run outside the NET_MGMT callback thread.

### Existing runtime ownership needs a few targeted upgrades for supervision

`struct network_runtime_state` currently stores semaphores, raw flags, and a single disconnect status. Phase 2 likely needs to grow it with:

- named connectivity state
- structured last-failure record
- startup outcome semaphore or equivalent gate
- delayable work item(s) for retry and possibly reachability re-checks
- retry cadence and startup deadline bookkeeping
- optional light synchronization around status snapshots if multiple execution contexts start reading/writing richer state

The current `wifi_ready_state` singleton inside `wifi_lifecycle.c` is acceptable for the repo's single-interface design, but the ownership should stay explicit and should not silently grow into a general hidden-global pattern.

</findings>

<recommendation>
## Recommended Implementation Shape

### Recommended module responsibilities

- `app/src/network/wifi_lifecycle.{h,c}`
  - keep low-level Wi-Fi ready, connect, disconnect, DHCP, and interface-status handling
  - add any missing IPv4-loss event coverage needed by the supervisor
  - avoid embedding retry cadence or startup-policy logic here

- `app/src/network/network_supervisor.{h,c}`
  - own supervisor lifecycle start/stop hooks
  - own bounded startup supervision
  - own scheduled reconnect attempts
  - own reachability-state transitions and last-failure updates
  - expose status helpers for future downstream consumers

- `app/src/network/network_state.h`
  - define the named connectivity-state enum
  - define the structured failure payload
  - hold supervisor runtime fields needed across callbacks and scheduled work

- `app/src/app/bootstrap.c`
  - load config as today
  - initialize/start supervisor instead of performing a full connect-and-wait sequence directly
  - treat degraded startup as a valid running mode rather than a fatal boot error

### Recommended configuration posture

- Keep SSID, PSK, security mode, and reachability host as build-time configuration.
- Reuse `CONFIG_APP_WIFI_TIMEOUT_MS` as the bounded startup window.
- Add one explicit build-time retry cadence symbol (for example `CONFIG_APP_WIFI_RETRY_INTERVAL_MS`) instead of hard-coding a magic reconnect delay in C.
- Do not add runtime provisioning, credential storage, or operator-tunable networking policy yet.

### Recommended state/failure model

Use a small, operator-meaningful status surface rather than a broad diagnostic blob. A good minimum is:

- current named connectivity state
- Wi-Fi ready/connected flags
- IPv4 bound flag and leased address
- last reachability result
- last failure stage and reason

That is enough for Phase 2 and keeps Phase 5's future panel work simple.

</recommendation>

<validation>
## Validation Architecture

### Existing validation assets to reuse

- `./scripts/validate.sh` is already the repo-owned fast validation entrypoint.
- `./scripts/build.sh` remains the canonical automated build signal.
- `./scripts/doctor.sh` remains useful as an optional environment/device preflight before hardware-heavy checks.
- `./scripts/flash.sh` and `./scripts/console.sh` already define the real-device smoke path.
- `README.md` already documents the current ready markers and build-first/manual-gate split.

### Recommended Phase 2 validation strategy

- Keep automated feedback build-first through `./scripts/validate.sh`; Phase 2 still does not justify introducing a separate host test framework.
- Add targeted static checks inside plan tasks so each plan can prove the supervisor files, state enums, and integration points exist without waiting for manual hardware access.
- Reserve device-level validation for the network behaviors that cannot be proven by build output alone.

The minimum manual scenarios Phase 2 should validate are:

1. **Healthy boot path** — device reaches full healthy state and logs the expected success markers.
2. **Boot while AP or credentials path is unavailable** — startup window expires into explicit degraded-retrying status instead of hanging forever.
3. **Runtime disconnect recovery** — AP power cycle or forced disconnect transitions to retrying and later back to healthy without reboot.
4. **Upstream degradation with LAN still up** — Wi-Fi plus DHCP stays up while the reachability target fails, producing the distinct upstream-degraded state.
5. **Recovery from upstream degradation** — once the reachability target returns, state clears back to healthy and last failure remains visible until replaced.

### Nyquist implications

- Every implementation task should have a fast automated build/static verification command.
- The phase can stay Nyquist-viable without a new unit-test framework by combining frequent build validation with explicit manual network-failure checkpoints.
- Plan 3 should concentrate the board-level scenario coverage and documentation updates so the earlier plans can keep tight build feedback loops.

</validation>

<planning_implications>
## Planning Implications

- The roadmap's planned three-plan split is correct and should be preserved.
- Plan 1 should establish the supervisor contract, explicit connectivity-state model, and the event/state ownership boundaries.
- Plan 2 should implement the actual retry cadence, bounded startup outcome, reachability-state transitions, and structured last-failure reporting.
- Plan 3 should update the validation flow and documentation around healthy, degraded, and recovery scenarios, with manual device checkpoints for the network cases automation cannot prove.
- Plan 2 should depend on Plan 1.
- Plan 3 should depend on Plans 1 and 2 because validation needs the real supervisor behavior and final status markers.

</planning_implications>

---
*Phase: 02-wi-fi-supervision*
*Research completed: 2026-03-08*
