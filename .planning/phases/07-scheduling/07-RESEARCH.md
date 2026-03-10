# Phase 7: Scheduling - Research

**Researched:** 2026-03-10
**Status:** Ready for planning

<focus>
## Research Goal

Identify the smallest repo-aligned implementation shape that adds persistent local scheduling for relay actions with deterministic trusted-clock, reboot, and time-resynchronization behavior.

Phase 7 should reuse the Phase 6 action dispatcher, keep all execution local to the device, expose schedule management through the authenticated local panel and API, and leave behind a scheduler boundary that later phases can extend without reopening relay or panel fundamentals.

Phase 7 should not pull in multi-relay orchestration, timezone-management UI, cloud scheduling, a generalized notification/event-log system, or a broader action editor.

</focus>

<findings>
## Key Findings

### The Phase 6 action path is already the mandatory execution seam

- `app/src/actions/actions.h` already defines `ACTION_DISPATCH_SOURCE_SCHEDULER`, and `app/src/actions/actions.c` already maps that source through the shared dispatcher and into relay runtime status.
- `action_dispatcher_execute()` already does the two things scheduling must keep: it actuates through `relay_service_apply_command()` and persists the remembered desired relay state through `persistence_store_save_relay()`.
- That means scheduled jobs should not call relay code directly, should not write relay persistence directly, and should not invent a second execution path.
- Planning implication: Phase 7 should treat scheduler execution as “select schedule -> resolve action -> call dispatcher with scheduler source -> record result,” nothing more direct.

### The persistence layer already owns schedule durability, but the current schema is not full-cron capable

- `app/src/persistence/persistence_types.h` currently stores schedules as `schedule_id`, `action_id`, `enabled`, `days_of_week_mask`, and `minute_of_day`.
- `app/src/persistence/persistence.c` already enforces section-local load/save, unique `schedule_id` values, and schedule-to-action reference validation.
- The hidden constraint is schema migration: the repo uses one shared `APP_PERSISTENCE_LAYOUT_VERSION` across auth, actions, relay, and schedule sections, and `persistence_read_blob()` expects exact struct sizes.
- Any size change to `struct persisted_schedule` therefore forces one of two paths:
  1. a whole-layout version bump that resets all non-auth sections on upgrade, or
  2. a more invasive per-section migration/refactor before full-cron can land.
- The good news is that this is the cheapest phase to take a one-time layout bump. There is no shipped schedule data yet, the action catalog is still just reserved built-ins, and there is no operator-facing reboot-policy editor to preserve.
- Planning implication: the simplest safe Phase 7 path is a one-time persistence layout bump and a new schedule schema, not a section-versioning refactor.

### There is no live scheduler runtime yet; bootstrap and app context need new ownership

- `app/src/scheduler/scheduler.h` is only a stub with `struct scheduler_service { struct app_context *app_context; }` and one init prototype.
- `app/src/app/app_context.h` does not yet contain a scheduler service.
- `app/src/app/bootstrap.c` initializes persistence, recovery, relay, actions, panel auth, network supervision, and panel HTTP, but it never initializes a scheduler runtime or a trusted clock.
- `app/src/main.c` just boots once and then sleeps forever, while background policy code in `network_supervisor` and `recovery_manager` uses `k_work_delayable` work instead of polling loops or dedicated foreground threads.
- Planning implication: Phase 7 should follow the existing house pattern and add a real scheduler service with delayable work, runtime state, and status getters, rather than building a minute-poll loop in `main()`.

### Time trust is the real Phase 7 design problem, not cron syntax

- The current firmware has no trusted wall-clock subsystem. Existing runtime timing is uptime-based (`k_uptime_get()`) inside networking and recovery code.
- The vendored SDK already includes the building blocks Phase 7 needs:
  - Zephyr SNTP (`.ncs/v3.2.1/zephyr/include/zephyr/net/sntp.h`) for one-shot network time queries.
  - Zephyr time utilities (`.ncs/v3.2.1/zephyr/doc/kernel/timeutil.rst`) including `timeutil_timegm64()` for UTC calendar conversion.
  - Nordic’s `date_time` library (`.ncs/v3.2.1/nrf/include/date_time.h`) which can maintain UTC and set `CLOCK_REALTIME`.
- The Nordic `date_time` library is informative, but it is not a perfect fit for this repo as-is:
  - it hard-codes public NTP servers in `.ncs/v3.2.1/nrf/lib/date_time/date_time_ntp.c`
  - it allows only one application event handler
  - its documentation and auto-update story are LTE-centric even though this product is Wi-Fi only
- Planning implication: Phase 7 is better served by an app-owned trusted-clock helper inside `app/src/scheduler/` that uses Zephyr SNTP/time APIs directly, keeps server/timeout choices under app control, and exposes exactly the scheduler semantics this repo needs.

### Timezone and “real cron” semantics are still unresolved product decisions

- The repo has no timezone configuration surface in `app/Kconfig`, `app_config`, persistence, or panel UX.
- The NTP path gives UTC. The Nordic `date_time` library can expose local time only when it has a valid timezone, which the Wi-Fi-only path does not currently provide.
- That means there is no existing mechanism for device-local wall time, DST behavior, or per-schedule timezone handling.
- Planning implication: the smallest safe Phase 7 choice is to define schedule authoring and status in UTC and label it clearly in the panel/API. If local-time cron semantics are required, the phase grows into a timezone configuration feature as well.
- Separate from timezone, “full 5-field cron” still needs an explicit grammar decision:
  - support `*`, ranges, lists, and steps
  - no seconds field
  - no macros like `@daily`
  - explicitly document day-of-month/day-of-week semantics instead of leaving them implicit
- Planning implication: lock UTC-vs-local and cron grammar before plan decomposition, because those decisions affect persistence, validation, status rendering, and operator expectations.

### Recovery integration is the cleanest way to satisfy “restart, then degrade” clock behavior

- The Phase 7 context explicitly says the device should restart when it cannot establish a correct clock, and then stay degraded instead of running automation with uncertain time.
- The current recovery API only records `confirmed-stuck` and `watchdog-starvation` reset triggers.
- `recovery_manager_last_reset_cause()` already preserves retained reset breadcrumbs across reboot, and bootstrap already logs them.
- Planning implication: Phase 7 should extend recovery with a dedicated clock-trust reset trigger rather than calling `sys_reboot()` directly from scheduler code.
- Recommended behavior:
  1. boot reaches network-ready state
  2. scheduler attempts initial time sync
  3. if sync fails and this is the first such boot, request a recovery reset with a clock-specific trigger
  4. on the next boot, detect that retained trigger and come up with scheduling inactive/degraded instead of reboot-looping
- This preserves operator-visible breadcrumbs and matches the existing recovery posture.

### The current panel and API only have room for scheduler summary, not schedule management

- `app/src/panel/panel_status.c` currently returns only `implemented=false`, schedule counts, enabled counts, and a placeholder string under `scheduler`.
- `app/src/panel/assets/main.js` and `app/src/panel/assets/index.html` still render a Phase 7 placeholder card, not a dedicated scheduling view.
- `PANEL_STATUS_RESPONSE_BODY_LEN` is 2048 bytes. That is enough for summary state, but it is the wrong shape for a list of schedules plus recent scheduler problems.
- `panel_http.c` follows an exact-path HTTP resource pattern with fixed per-route contexts and bounded request/response buffers.
- Planning implication: keep `/api/status` as a compact scheduler summary and add dedicated authenticated schedule endpoints rather than trying to stuff CRUD/list/detail payloads into the status route.
- Because the current route style is exact-path rather than wildcard or path-parameter heavy, the cleanest Phase 7 route shape is also exact-path.

### Raw internal action IDs are not ready for direct operator exposure

- `struct persisted_action` only stores `action_id`, `enabled`, and `relay_on`; there is no friendly action name or description in persistence.
- The Phase 7 context explicitly says operators should select friendly named actions and should not see raw internal action IDs.
- Planning implication: Phase 7 should add a server-side mapping layer that exposes public action choices such as a stable action key and label, while still persisting internal `action_id` references underneath.
- With today’s built-ins, the public choices can stay relay-centric (`relay-on`, `relay-off`) even if the internal IDs remain `relay0.on` and `relay0.off`.

### Concurrency and flash-wear are the two hidden runtime risks

- Today, `action_dispatcher_execute()` has no execution lock. That was tolerable when only manual relay commands existed; it becomes much riskier once panel/manual and scheduler sources can both command the relay.
- Scheduler CRUD will also introduce mutable compiled runtime state that the current repo does not yet own anywhere.
- Planning implication: Phase 7 should serialize action execution with a mutex inside the dispatcher or its immediate caller, and the scheduler service should own its own mutex-protected compiled snapshot and history buffer.
- Flash wear is the other hidden constraint: because Phase 6 deliberately persists relay desired state on every accepted action, a frequently repeating schedule would write NVS repeatedly.
- Planning implication: before high-frequency scheduling is added, the shared action path needs a no-op fast path that skips relay persistence when the requested desired state already matches the remembered desired state. That preserves the “same action path” rule while removing avoidable writes.

### Conflict rejection belongs in the scheduler domain layer, not only in the UI

- The Phase 7 context requires opposite relay states that would fire in the same minute to be rejected before they become durable.
- The current repo shape helps here: there are only 8 schedules max and the first action model still maps to one relay boolean (`relay_on`).
- Planning implication: conflict detection should live in a scheduler-domain validation step that runs before persistence save, using compiled cron matchers plus action outcomes.
- The UI should surface those validation errors, but it should not be the source of truth.
- Identical-state overlaps are a separate product choice. The context only forbids opposite-state collisions, so the smallest Phase 7 scope is to reject conflicting on/off overlaps and allow same-state overlap unless planning chooses to tighten it.

</findings>

<implementation_shape>
## Recommended Implementation Shape

### Take a one-time persistence layout bump and replace the schedule shape now

- Bump `APP_PERSISTENCE_LAYOUT_VERSION` once for Phase 7 and document that current non-auth persisted sections reset on first boot after upgrade.
- Replace the weekday-plus-minute schedule contract with a full-cron-oriented schedule record.
- Recommended durable shape:
  - opaque `schedule_id`
  - internal `action_id`
  - `enabled`
  - canonical 5-field cron expression string in UTC
- Do not add a schedule display name in Phase 7 unless planning proves it necessary. The action label plus cron expression are enough for the first serious scheduling surface, and skipping a name field keeps the persistence change smaller.
- Keep the public operator/API surface free of raw internal IDs by mapping public action keys to internal `action_id` values in the scheduler or panel layer.

### Compile persisted schedule records into a scheduler-owned runtime snapshot

- Keep `persisted_config.schedule` as the durable source of truth.
- Add a scheduler-owned RAM snapshot that compiles each cron expression into fixed matcher structures, for example field bitsets for minute/hour/day-of-month/month/day-of-week.
- Keep all runtime-only state out of persistence:
  - compiled matchers
  - next due minute
  - trusted-clock state
  - last execution or skip result
  - recent problem history ring buffer
- After any successful schedule save, reload the scheduler’s compiled snapshot from the just-written durable config and re-arm the next due work item.
- Do not let panel code or action code reach directly into compiled scheduler internals; expose summary getters and mutation APIs instead.

### Keep time synchronization inside the scheduler module and use Zephyr-native clock primitives

- Implement a small trusted-clock helper under `app/src/scheduler/` rather than introducing a broad new public subsystem.
- Recommended backend:
  - use Zephyr SNTP for initial and periodic sync
  - set `CLOCK_REALTIME` / system time from the obtained UTC value
  - use `gmtime_r()` plus `timeutil_timegm64()` for matching and next-run calculations
- Keep time semantics explicit and minute-granular:
  - schedules evaluate in UTC
  - automation does not fire until time is trusted
  - missed runs are skipped, never replayed
  - large forward or backward clock corrections cancel the old next-run timer and recompute from corrected time
- Define a minute-resolution correction threshold so tiny SNTP jitter does not create unnecessary reschedules. Anything that changes schedule resolution behavior should be treated as a correction event.

### Extend recovery once, then make degraded scheduling visible instead of silent

- Add a clock-specific recovery reset trigger and carry it through retained reset breadcrumbs.
- Recommended boot semantics:
  1. initialize scheduler runtime after persistence and actions are ready
  2. once networking is up enough to attempt time sync, perform initial sync
  3. if initial sync fails on a normal boot, request one recovery reset
  4. if the next boot shows the same retained trigger, start the panel normally but leave scheduling inactive and visibly degraded
- This keeps the local panel usable for operators while honoring the “restart, then degrade” requirement.

### Drive execution from one delayable work item and keep all callers serialized

- Add one `k_work_delayable` in the scheduler service that always targets the earliest next due enabled schedule.
- When the work fires:
  - read trusted UTC time
  - normalize to the current minute
  - evaluate all enabled schedules that match that minute
  - dispatch each matching action through `action_dispatcher_execute(..., ACTION_DISPATCH_SOURCE_SCHEDULER, ...)`
  - record execution or skip outcomes in the scheduler history buffer
  - compute and arm the next due minute
- Protect action execution with a mutex so scheduled and manual commands cannot interleave unpredictably.
- Add duplicate-fire protection for backward time adjustments or repeated same-minute work runs. With only 8 schedules, a simple per-minute token plus an 8-bit executed mask is sufficient.

### Keep the external API exact-path, JSON, and operator-friendly

- Preserve the exact-path route style already used by `panel_http.c`.
- Recommended scheduler routes:
  - `GET /api/schedules` — list schedules, action choices, runtime summary, recent problems
  - `POST /api/schedules/create`
  - `POST /api/schedules/update`
  - `POST /api/schedules/delete`
  - `POST /api/schedules/set-enabled`
- Keep `/api/status` as the dashboard summary only. Additive scheduler summary fields there should include:
  - whether the runtime is implemented
  - whether automation is currently active
  - whether time is trusted or degraded
  - saved and enabled counts
  - next run summary
  - last outcome summary
- Use `json_obj_parse()` descriptors for schedule mutation payloads. The relay route’s manual boolean parsing is too small and too brittle for cron strings, opaque IDs, and action keys.

### Build a dedicated scheduling view inside the existing asset strategy

- Keep the panel as embedded authored HTML and JavaScript. The repo already has the right asset pipeline in `app/CMakeLists.txt`.
- The simplest UI shape is still a single-page app, but it now needs a real scheduling section or view rather than a placeholder card.
- Recommended Phase 7 browser behavior:
  - dashboard keeps a compact scheduler summary card
  - operator can open a dedicated schedule-management surface on the same page
  - the schedule-management surface shows schedule list, action choices, cron editor, enable/disable controls, and recent scheduler problems
  - all times are labeled UTC in v1
- Keep the UI relay-centric externally: action choices should read like relay outcomes, not raw action-engine jargon.

### Keep Phase 7 intentionally narrow

- Support numeric 5-field cron with `*`, lists, ranges, and steps.
- Do not add seconds, macros, nicknames, or per-schedule timezone handling.
- Keep recent scheduler problems in a RAM ring buffer of 10 entries instead of inventing a broader persisted notification system.
- Keep scheduling bound to the current single-relay action model. Multi-relay orchestration stays out of scope.

</implementation_shape>

<validation>
## Validation Architecture

### Automated validation should remain build-first but add scheduler-structure gates

- Keep `./scripts/build.sh` as the fastest automated firmware signal.
- Keep `./scripts/validate.sh` as the canonical repo-owned validation entrypoint.
- Extend the existing shell-static checks already used in prior phases:
  - `bash -n scripts/validate.sh scripts/common.sh scripts/build.sh`
- Add plan-level structural checks for:
  - scheduler sources added to `app/CMakeLists.txt`
  - scheduler ownership added to `app_context` and bootstrap
  - scheduler execution using `ACTION_DISPATCH_SOURCE_SCHEDULER`
  - dedicated schedule routes present in `panel_http.c`
  - `/api/status` exposing additive scheduler summary instead of placeholder-only fields
  - negative checks proving schedule handlers do not call relay APIs directly
  - persistence writes flowing through schedule save APIs rather than ad-hoc blob writes
- Extend `scripts/common.sh` with Phase 7 helper printers following the Phase 4/5/6 pattern:
  - ready-state markers
  - schedule CRUD curl commands
  - scheduler scenario labels
  - device/browser checklist

### If the phase can afford one narrow deterministic seam, spend it on cron and next-run logic

- The repo still has no first-party test framework, so the default validation posture remains build-first plus hardware sign-off.
- Even so, Phase 7 has more pure policy logic than prior phases: cron parsing, next-run calculation, conflict rejection, duplicate-fire suppression, and time-resync behavior.
- If planning is willing to add one new deterministic seam, make it a tiny scheduler-domain test target for:
  - cron parse acceptance and rejection
  - next-run calculation
  - conflict detection
  - minute-deduplication after backward time correction
- If the repo deliberately avoids introducing tests in this phase, then the manual validation matrix must explicitly cover those semantics on-device.

### Blocking manual sign-off should prove CRUD, execution, and time semantics separately

Minimum scenario set:

1. **Create schedule** — An authenticated operator can create a valid schedule from browser and curl/API paths, and it appears in the dedicated schedule list and dashboard summary.
2. **Edit schedule** — A saved schedule can be edited in place without creating duplicate records or losing enable state unexpectedly.
3. **Enable/disable/delete** — The operator can disable, re-enable, and delete a schedule, and each mutation persists across reboot.
4. **Conflict rejection** — Opposite relay-state schedules that would fire in the same minute are rejected before save with a clear operator-visible error.
5. **Trusted-time gate** — With no trusted clock, manual relay control still works, but automation remains inactive and visibly degraded.
6. **Restart then degrade** — Initial clock acquisition failure triggers one reset; the next boot comes up with scheduling inactive/degraded rather than entering a reboot loop.
7. **Scheduled execution path** — When a due minute arrives with trusted time, the scheduler fires through the shared dispatcher, the relay status source becomes `scheduler`, and the latest scheduler result is visible in status/history.
8. **Missed-job semantics** — After reboot or time-unavailable downtime, the device skips missed runs instead of replaying backlog when time becomes trusted again.
9. **Time correction semantics** — A deliberate forward or backward clock correction recomputes future runs without double-firing a schedule in the same UTC minute.
10. **Manual-vs-scheduled interaction** — A manual relay command does not disarm scheduling; the next due scheduled run still executes, and concurrent manual/scheduled commands serialize cleanly.
11. **LAN-only degraded operation** — In `LAN_UP_UPSTREAM_DEGRADED`, the panel remains reachable. If time was already trusted, future schedules still execute locally; if time was never trusted, scheduler remains degraded and visible while manual control still works.
12. **Flash-churn protection** — Repeated schedules that request the already-remembered desired relay state do not generate unnecessary persistence churn.

### Docs and repo workflows should follow the same pattern as Phase 6

- Update `README.md` so a clean checkout can reproduce the schedule CRUD and validation flow.
- Update `scripts/validate.sh` messaging so the automated build step and the blocking browser/curl/device scheduler sign-off are clearly separated.
- Keep scenario-level recording in phase summaries or validation docs instead of declaring Phase 7 complete on build success alone.

</validation>

<planning_implications>
## Planning Implications

### Recommended plan split

- `07-01` should lock the data and runtime contract:
  - persistence layout bump
  - new persisted schedule shape
  - cron grammar decision
  - UTC/time-trust semantics
  - scheduler service ownership in app context and bootstrap
  - dispatcher serialization and no-op persistence optimization
- `07-02` should land the live scheduler engine:
  - trusted-clock acquisition
  - recovery-triggered restart-then-degrade path
  - compiled matcher cache
  - next-run calculation
  - due-job dispatch
  - conflict validation
  - recent-problem history
- `07-03` should expose the operator surface and validation workflow:
  - exact-path schedule CRUD routes
  - action-choice mapping and labels
  - dedicated scheduling UI/view
  - status summary fields
  - `scripts/common.sh`, `scripts/validate.sh`, and `README.md` updates
  - blocking browser/curl/device validation

### Decisions that should be made before planning starts

- Are Phase 7 schedules explicitly UTC in v1?
- Is a one-time persistence layout bump acceptable now?
- What exact 5-field cron grammar is supported?
- What are the day-of-month/day-of-week semantics?
- What public action keys and labels should the operator/API see?
- What delta counts as a “major” time correction for scheduler recomputation and history logging?
- Are same-state overlaps allowed, or only opposite-state overlaps rejected?

### Things Phase 7 should explicitly not absorb

- timezone configuration UI or DST policy management
- multi-relay schedule assignment
- a general action-management editor
- a broader persisted event log or notification system
- cloud or remote scheduling dependencies
- OTA/update workflow changes

</planning_implications>
