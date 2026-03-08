# Phase 2: Wi-Fi Supervision - Context

**Gathered:** 2026-03-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Move Wi-Fi lifecycle ownership into a dedicated supervisor that owns boot-time connection attempts, ongoing reconnect behavior, and precise connectivity state publication. This phase keeps build-time Wi-Fi credentials, clarifies how network health should be interpreted, and prepares status for later recovery and panel work without adding new operator-facing capabilities beyond supervision.

</domain>

<decisions>
## Implementation Decisions

### Ready-state meaning
- Full health means the device is associated to Wi-Fi, has DHCP IPv4, and the configured reachability target succeeds.
- If boot does not reach full health within a bounded startup window, the device should enter an explicit degraded state and continue supervising in the background rather than blocking forever.
- Wi-Fi plus DHCP with a failed reachability check should be exposed as a distinct LAN-up / upstream-degraded condition, not collapsed into either fully healthy or generic offline.
- The reachability target represents a configured dependency path for the deployment, not generic internet access and not a throwaway diagnostic hint.

### Reconnect posture
- After losing health, Phase 2 should keep recovering on its own rather than giving up or escalating early.
- Retry cadence should stay steady rather than using aggressive retry bursts followed by slower backoff waves.
- Once the full healthy path returns, the supervisor should clear degraded state immediately and treat the network as recovered.
- Phase 2 should keep retrying through prolonged outages instead of introducing an early “stuck” classification before the dedicated Recovery & Watchdog phase exists.

### Status contract
- Downstream consumers should receive a named connectivity state model rather than having to infer meaning from raw flags alone.
- The named states should clearly distinguish at least: not-ready, connecting, healthy, LAN-up / upstream-degraded, and disconnected or degraded-retrying conditions.
- The last network failure should be stored as structured stage plus reason, not only as a human summary.
- The most recent failure should remain visible until a newer failure replaces it, even after current health returns.

### Claude's Discretion
- Exact labels and wire format for the named connectivity states, as long as they preserve the distinctions above.
- Exact durations for the bounded startup window and steady retry interval.
- Exact shape of the structured last-failure payload, as long as it captures stage plus reason for downstream consumers.
- Whether the configured reachability target itself is exposed in status surfaces or only consumed internally through config.

</decisions>

<specifics>
## Specific Ideas

- No external product references were given; prefer straightforward, operator-meaningful naming and behavior.
- The future panel should be able to explain why the device is not fully healthy without requiring the operator to read serial logs.
- Avoid clever retry-wave behavior; patience and clarity matter more than aggressive flapping recovery.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `app/src/network/wifi_lifecycle.c` and `app/src/network/wifi_lifecycle.h` already own Wi-Fi-ready callbacks, connect requests, DHCP waiting, and disconnect handling; they are the natural seam for supervisor-owned behavior.
- `app/src/network/network_state.h` already centralizes mutable connectivity state and is the safest home for richer supervisor status.
- `app/src/network/reachability.c` already separates post-connect reachability from Wi-Fi association, which supports the chosen LAN-up versus upstream-degraded distinction.
- `app/src/app/bootstrap.c` is the existing orchestration seam where the current one-shot bring-up flow can be replaced by supervisor-owned startup behavior.
- `app/src/app/app_config.h` and `app/src/app/app_context.h` already provide typed Wi-Fi and reachability config plus shared runtime context.
- `scripts/validate.sh`, `scripts/build.sh`, `scripts/common.sh`, and `README.md` already define the canonical build-first and hardware-smoke validation path for later Phase 2 regression coverage.

### Established Patterns
- `main.c` is already a composition root, so policy and long-lived network supervision should stay behind bootstrap and subsystem boundaries.
- Short `net_mgmt` callbacks update shared state while semaphores linearize boot-time waiting; Phase 2 should extend that ownership model rather than re-centralizing logic.
- Negative-errno returns and `LOG_INF` / `LOG_WRN` / `LOG_ERR` logging are established runtime signaling patterns.
- Build-time Wi-Fi configuration through Kconfig plus the local secrets overlay is a locked project decision and remains the Phase 2 input model.
- Some generated codebase maps still describe the pre-Phase-1 single-file layout; for Phase 2 research and planning, the live source tree is the source of truth.

### Integration Points
- `app_boot()` is the natural seam for swapping the current connect-once boot path with supervisor-owned startup and reconnect orchestration.
- `app_context.network_state` is the best state carrier for future recovery, panel, and auth consumers.
- `app/src/recovery/recovery.h`, `app/src/panel/panel_http.h`, and `app/src/panel/panel_auth.h` already reserve future subsystem boundaries that will consume the current state plus last failure contract.
- `./scripts/validate.sh` should remain the default regression entrypoint, with real-device smoke later checking the expected healthy and degraded markers.

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---
*Phase: 02-wi-fi-supervision*
*Context gathered: 2026-03-08*
