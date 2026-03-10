# Feature Research

**Domain:** Configurable relay action management for a local embedded control panel
**Researched:** 2026-03-11
**Confidence:** HIGH

## Feature Landscape

### Table Stakes (Users Expect These)

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Action CRUD | An operator-configured action flow is not real unless actions can be created, reviewed, updated, and removed | MEDIUM | Needs safe ID rules, label/name support, and persistence-backed mutations |
| Validation-before-use | Users expect unconfigured or invalid outputs to stay unavailable instead of failing at execution time | MEDIUM | Must be enforced in backend routes and reflected in panel status |
| Shared catalog across manual control and schedules | If an action exists, operators expect it to appear consistently anywhere it can be used | MEDIUM | Current repo splits on fixed public keys; this must move to dynamic catalog-derived choices |
| Enable/disable and visibility state | Operators need to temporarily remove actions from use without deleting history/configuration | LOW | Schedules must respect the same enablement state |
| Dependency-aware delete/edit behavior | Deleting or changing an action must not silently break schedules | HIGH | Requires schedule reference checks, warnings, and possibly block-or-disable policies |
| Clear hardware summary in UI | A named action is not enough; operators need to know which GPIO-backed output it controls | LOW | Keep the display operator-safe and avoid raw internal-only wiring jargon where possible |

### Differentiators (Competitive Advantage)

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Migration assistant for built-in relay actions | Lets existing devices move from `relay0.on/off` to configured actions without surprise | HIGH | Strong differentiator because it avoids "factory reset to use the new model" pressure |
| Usage visibility | Shows whether an action is referenced by one or more schedules before edit/delete | MEDIUM | Makes schedule safety tangible in the admin flow |
| Action health/status badge | Explains why an action is blocked, unavailable, or hidden | LOW | Keeps "not configured yet" honest and operator-visible |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Arbitrary raw pin entry for any controller/pin | Feels flexible | Hard to validate safely across boards, easy to misconfigure, and misaligned with Zephyr's devicetree-oriented hardware model | Persist a logical binding selected from a compile-time output registry |
| Multi-type action DSL in the first configurable milestone | Sounds future-proof | Delays the concrete relay flow and explodes validation surface too early | Make the data model extensible, but ship only configurable relay actions now |
| Silent schedule repair when an action is deleted | Feels convenient | Can change automation semantics without operator consent | Block delete, require reassignment, or disable affected schedules with an explicit warning |
| Separate "manual outputs" and "schedule outputs" inventories | Seems simpler per page | Causes drift and inconsistent operator truth | One action catalog exposed differently per surface |

## Feature Dependencies

```text
Configured output registry
    └──requires──> Action catalog CRUD
                         ├──requires──> Validation-before-use
                         ├──requires──> Shared panel/API serialization
                         └──enables──> Schedule action selection

Dependency-aware delete/edit
    └──requires──> Schedule reference inspection

Migration from built-ins
    └──conflicts with──> Silent boot-time reseeding of fixed actions
```

### Dependency Notes

- **Action catalog CRUD requires configured output registry:** action records need a safe hardware binding target before they can be executed or scheduled.
- **Schedule action selection requires shared serialization:** the schedule form should consume the same action metadata that the Actions page shows.
- **Delete/edit safety requires schedule reference inspection:** otherwise an operator can create orphaned or behavior-shifted schedules accidentally.
- **Migration conflicts with built-in reseeding:** the backend cannot both auto-seed fixed relay actions and claim that configured actions are the only usable actions.

## MVP Definition

### Launch With (v1.1)

- [ ] Create, list, edit, enable/disable, and delete configured relay actions
- [ ] Persist action records with safe validation and migration behavior
- [ ] Show configured relay actions on the Actions page instead of fixed built-in buttons
- [ ] Make configured actions selectable in schedule create/edit forms
- [ ] Block execution and scheduling when an action is unconfigured, invalid, or disabled
- [ ] Protect schedules from silent breakage when referenced actions change

### Add After Validation (v1.x)

- [ ] Action usage counters / "used by schedules" hints
- [ ] Explicit operator test action for a configured output
- [ ] Better migration or import helpers for moving from legacy built-ins

### Future Consideration (v2+)

- [ ] Multiple relay/output channels per board
- [ ] Non-relay action types and integration-backed actions
- [ ] Bulk action templates or grouped actions

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Action CRUD | HIGH | MEDIUM | P1 |
| Validation-before-use | HIGH | MEDIUM | P1 |
| Shared action catalog for Actions + Schedules | HIGH | MEDIUM | P1 |
| Dependency-aware delete/edit behavior | HIGH | HIGH | P1 |
| Migration from built-ins | MEDIUM | HIGH | P2 |
| Usage visibility | MEDIUM | MEDIUM | P2 |
| Action health/status badge | MEDIUM | LOW | P2 |
| Multi-type action support | LOW | HIGH | P3 |

## Sources

- User-requested milestone scope from conversation
- Local repo analysis of `app/src/actions`, `app/src/panel`, `app/src/scheduler`, and `app/src/persistence`
- [Zephyr GPIO API](https://docs.zephyrproject.org/latest/hardware/peripherals/gpio.html) and [Devicetree how-tos](https://docs.zephyrproject.org/latest/build/dts/howtos.html) for the hardware-binding constraints that shape the feature set

---
*Feature research for: configurable relay action management*
*Researched: 2026-03-11*
