# Pitfalls Research

**Domain:** Brownfield configurable relay action management in an embedded local panel
**Researched:** 2026-03-11
**Confidence:** HIGH

## Critical Pitfalls

### Pitfall 1: Treating raw GPIO input as safe operator truth

**What goes wrong:**
The panel stores port/pin data the firmware cannot safely resolve or should never drive, so actions look configured but fail or behave dangerously on hardware.

**Why it happens:**
It feels simpler to let the browser submit "GPIO 0.13" directly and skip a firmware-owned binding model.

**How to avoid:**
Persist actions against approved output bindings defined by firmware, then validate every action against that binding registry before exposing it.

**Warning signs:**
Action records contain raw GPIO text, or the runtime has to guess which Zephyr device maps to a user-entered string.

**Phase to address:**
Phase 9 - data model and migration foundation

---

### Pitfall 2: Keeping manual control and schedules on separate action lists

**What goes wrong:**
The Actions page shows one set of names while the scheduler form shows another, and deleting or renaming an action breaks only one surface.

**Why it happens:**
Teams optimize each page independently and leave the shared catalog for later.

**How to avoid:**
Make one persisted action catalog the source of truth and feed both manual and scheduled flows from it.

**Warning signs:**
`panel_status` builds `actionChoices` from hard-coded JSON while the Actions page uses separate rules.

**Phase to address:**
Phase 10 - panel/API surface

---

### Pitfall 3: Editing or deleting actions without schedule impact handling

**What goes wrong:**
Schedules keep stale `action_id` references and fail only when a minute becomes due.

**Why it happens:**
Action CRUD looks like a panel concern, so schedule integrity is forgotten.

**How to avoid:**
Revalidate schedules after every action mutation, reject destructive changes when schedules still depend on an action, or migrate them explicitly.

**Warning signs:**
Action delete/update code paths do not call schedule validation or scheduler reload.

**Phase to address:**
Phase 9 and Phase 10

---

### Pitfall 4: Leaving built-in relay actions alive after the new model ships

**What goes wrong:**
The firmware silently reseeds `relay0.on` and `relay0.off`, so the UI never truly becomes configuration-driven and migration stays half-finished.

**Why it happens:**
Keeping legacy paths feels safer than choosing a clean migration boundary.

**How to avoid:**
Decide whether legacy IDs are migrated once, translated compatibly, or explicitly invalidated - then remove unconditional reseeding from steady-state behavior.

**Warning signs:**
Action init still auto-adds built-ins no matter what the stored catalog contains.

**Phase to address:**
Phase 9 - data model and migration foundation

---

### Pitfall 5: UI placeholders that hide the real backend contract

**What goes wrong:**
The UI says "not configured yet," but a hidden relay-only route still exists and continues to bypass the future action model.

**Why it happens:**
The placeholder is added as UX debt while the backend keeps its old exact route.

**How to avoid:**
Move manual execution to the catalog model as part of the same milestone, or clearly mark the relay-only route as migration-only and remove it from the panel.

**Warning signs:**
`/api/relay/desired-state` remains the only manual-control endpoint after configurable actions are introduced.

**Phase to address:**
Phase 10 - panel/API surface

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Reuse `relay_on` boolean inside `persisted_action` for every future action type | Fastest way to ship relay CRUD | Forces another schema rewrite when non-relay actions arrive | Only acceptable if paired with a clearly versioned migration plan |
| Rewrite the whole action catalog for every no-op update | Simplifies persistence code | Extra flash churn and harder diffing of real changes | Acceptable at current tiny scale if writes stay bounded and validated |
| Keep schedule forms defaulting to `relay-on` | Minimal JS changes | Breaks as soon as actions become configurable | Never after this milestone starts |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Panel -> actions | Sending public labels instead of stable keys/IDs | Send stable public keys and resolve to canonical `action_id` in firmware |
| Actions -> scheduler | Mutating actions without reloading scheduler | Save the catalog, then revalidate/reload schedules before reporting success |
| Persistence -> boot | Accepting stale schema silently | Keep the current typed version checks and define the migration outcome clearly |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Full snapshot refresh after every tiny mutation | UI feels chatty and firmware work spikes | Accept the current refresh loop for now, but keep mutation endpoints efficient and bounded | Mostly acceptable at current scale of 8 actions / 8 schedules |
| Excess flash writes from repeated action edits/no-ops | Wear grows faster than expected | Detect unchanged saves and avoid writing on no-op edits where possible | Becomes meaningful when operators tune actions frequently |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Exposing unauthenticated action CRUD routes | Local attackers can rewire automation behavior | Keep the same authenticated route discipline as schedules and OTA |
| Allowing unsafe action IDs or labels into JSON/HTML without validation | Broken UX and possible injection bugs | Reuse strict operator-safe ID rules and escape UI output consistently |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Showing only raw internal IDs | Operators cannot tell what they are scheduling | Show label, state, and binding summary everywhere |
| Letting delete succeed while silently removing schedule targets | Automation appears "saved" but stops working later | Block delete or force an explicit operator decision about dependent schedules |

## "Looks Done But Isn't" Checklist

- [ ] **Action creation:** Verify the saved action actually appears on the Actions page and in schedule choices.
- [ ] **Action deletion:** Verify dependent schedules are blocked, migrated, or removed intentionally.
- [ ] **Migration:** Verify old persisted built-in relay IDs do not survive as hidden duplicate behavior.
- [ ] **Manual execution:** Verify there is no UI path that still bypasses the configured action catalog.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Broken migration | HIGH | Boot into safe defaults, preserve diagnostics, and force operator reconfiguration instead of driving unknown hardware |
| Invalid action edit | MEDIUM | Reject the save, keep prior catalog intact, and show the blocking reason in the panel |
| Stale schedule references | MEDIUM | Reload scheduler, mark affected schedules invalid/disabled, and expose the failure in the schedule surface |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Unsafe raw GPIO configuration | Phase 9 | CRUD tests and hardware checks prove only approved bindings can be saved/executed |
| Split manual/schedule catalogs | Phase 10 | One action appears identically in Actions and Schedules |
| Stale schedule references | Phase 10 | Delete/edit flows show explicit behavior for dependent schedules |
| Legacy built-ins lingering | Phase 9 | Boot and migration tests prove built-ins are migrated or removed intentionally |
| Relay-only bypass route | Phase 10 | Browser flow uses configured actions, not hidden relay toggles |

## Sources

- [Zephyr GPIO docs](https://docs.zephyrproject.org/latest/hardware/peripherals/gpio.html)
- [Zephyr settings docs](https://docs.zephyrproject.org/latest/services/storage/settings/index.html)
- [Zephyr NVS docs](https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html)
- Local repo inspection - `app/src/actions/actions.c`, `app/src/panel/panel_http.c`, `app/src/panel/panel_status.c`, `app/src/panel/assets/main.js`, `app/src/persistence/persistence.c`, `app/src/scheduler/scheduler.c`

---
*Pitfalls research for: configurable relay actions*
*Researched: 2026-03-11*
