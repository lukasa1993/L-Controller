# Phase 7: Scheduling - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Add persistent local scheduling for relay actions with deterministic clock, reboot, and resynchronization behavior. Phase 7 must let the operator manage schedules through the authenticated local panel/API and execute scheduled relay actions through the same shared action path as manual control. It does not add multi-relay expansion, remote dependencies, OTA/update workflows, or a generalized notification system.

</domain>

<decisions>
## Implementation Decisions

### Clock trust and degraded behavior
- Scheduled automation must not fire until the device has a correct/trusted clock.
- If the device cannot establish a correct clock, it should recover by restarting and then stay in a degraded state rather than silently continuing automation with uncertain time.
- Missed scheduled runs are skipped rather than replayed or backfilled after reboot or time unavailability.
- After a major clock correction, future runs should be recomputed from the corrected time instead of preserving the old cadence.

### Schedule authoring model
- Phase 7 should expose real full 5-field cron scheduling rather than a simplified weekday/time-only rule editor.
- Schedule management should live in a dedicated scheduling view, not only as an expanded dashboard card.
- Operators should select friendly named actions in the scheduler UI/API surface; raw internal action IDs stay hidden.
- Schedule editing should feel like schedule management for a serious controller, not like lightweight dashboard toggles.

### Manual control and conflict semantics
- Manual relay commands take effect immediately but do not disarm or pause scheduling; the next matching scheduled run still executes.
- Two schedules that would command conflicting relay states at the same minute should be rejected as invalid instead of resolved by hidden precedence.
- Operator-facing scheduling status should prominently show the next run, the last executed or skipped result, and whether automation is currently active.
- Phase 7 should store recent scheduler problems in a small fixed history buffer of 10 slots for now instead of introducing a broader notification system in this phase.

### Claude's Discretion
- Exact wording, layout, and information density of the dedicated scheduling view.
- Exact friendly action labels for the existing relay actions and how action names are sourced for future action types.
- Exact representation of degraded clock state, restart messaging, and how the scheduler history buffer appears in the panel/API.
- Exact internal persistence/runtime design needed to support full 5-field cron while preserving deterministic behavior and the existing safety posture.

### Locked Semantics
- Phase 7 schedules are authored, stored, rendered, and executed in UTC in v1. The UI/API must label UTC clearly; timezone and DST configuration remain out of scope.
- Supported cron grammar is numeric 5-field `minute hour day-of-month month day-of-week` at minute resolution with `*`, comma-separated lists, ranges, and step values. Phase 7 does not add a seconds field, month/day names, or macro forms such as `@hourly`.
- Day-of-month and day-of-week follow standard cron OR semantics when both are restricted.
- A major clock correction is any backward jump or any forward adjustment that changes the normalized current UTC minute used by scheduler matching. Sub-minute jitter that stays within the same normalized minute is not treated as a correction event.
- Same-state overlaps are allowed. Opposite-state overlaps in the same minute are rejected before save.
- Public action choices are `relay-on` and `relay-off`, labeled `Relay On` and `Relay Off`, and they map internally to the stable `relay0.on` and `relay0.off` action IDs.
- If initial trusted-time acquisition fails on a normal boot, Phase 7 performs one recovery reset and then comes up with scheduling inactive and degraded on the next boot rather than looping or automating with uncertain time.

</decisions>

<specifics>
## Specific Ideas

- “If we don’t have correct clock it must restart and go into degraded state.”
- Full cron power matters more than a simplified weekly-only editor for this phase.
- Scheduling should feel like a first-class operator workflow with its own surface, not a small bolt-on card.
- A notification system can come later; for now, keep a stored recent-problems list with 10 slots.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `app/src/actions/actions.h` and `app/src/actions/actions.c` already define `ACTION_DISPATCH_SOURCE_SCHEDULER` and keep manual/future scheduled execution behind the shared action dispatcher.
- `app/src/persistence/persistence_types.h` and `app/src/persistence/persistence.c` already persist a separate schedule section, validate schedule-to-action references, and enforce unique `schedule_id` values.
- `app/src/panel/panel_http.c`, `app/src/panel/panel_status.c`, and `app/src/panel/assets/main.js` already provide the authenticated panel shell plus a scheduler placeholder surface that can expand into a dedicated scheduling experience.
- `app/src/scheduler/scheduler.h` already reserves a scheduler service boundary, even though the implementation is still a stub.

### Established Patterns
- Prior phases locked the panel to authenticated local-LAN HTTP only, with RAM-only sessions and relay-centric operator language.
- Manual panel relay control already uses the shared action path and future scheduling must keep that same contract.
- Persistence is sectioned and validated before write, so schedule edits should continue to save atomically and reject invalid references or ambiguous records up front.
- Current runtime timing is uptime-based in networking, recovery, and auth flows; there is not yet a trusted wall-clock subsystem for scheduler semantics.

### Integration Points
- The current persisted schedule contract only stores `schedule_id`, `action_id`, `enabled`, `days_of_week_mask`, and `minute_of_day`, so Phase 7 planning must decide how full 5-field cron expands or replaces that persisted shape.
- `app/src/app/bootstrap.c` initializes persistence, relay, actions, auth, network, and panel, but does not yet initialize or expose a live scheduler runtime; Phase 7 must wire scheduler state into bootstrap and shared app context.
- `panel_status_render_json()` and the dashboard JS currently report only schedule counts and a placeholder message; Phase 7 can reuse those status channels for next-run, last-result, degraded-time, and recent-problem visibility.
- Conflict rejection should happen before schedules become durable, matching the existing persistence and validation posture from earlier phases.

</code_context>

<deferred>
## Deferred Ideas

- A broader operator notification/alerting system is explicitly deferred; Phase 7 only needs a small stored recent-problems history.
- Multi-relay scheduling, per-relay orchestration, and richer automation beyond the first relay action stay in later phases.
- External/cloud scheduling dependencies or internet-hosted automation remain out of scope; Phase 7 stays fully local.
- OTA/update workflows remain Phase 8 work.

</deferred>

---
*Phase: 07-scheduling*
*Context gathered: 2026-03-10*
