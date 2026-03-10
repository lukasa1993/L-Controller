# Architecture Research

**Domain:** Configurable GPIO-backed relay actions inside an existing Nordic/Zephyr control node
**Researched:** 2026-03-11
**Confidence:** HIGH

## Standard Architecture

### System Overview

```text
┌─────────────────────────────────────────────────────────────┐
│                 Panel/UI and Authenticated API             │
├─────────────────────────────────────────────────────────────┤
│  Actions page CRUD   Schedule form choices   Status JSON   │
└──────────────┬──────────────────────┬───────────────────────┘
               │                      │
┌──────────────▼──────────────────────▼───────────────────────┐
│                 Action Catalog Boundary                    │
├─────────────────────────────────────────────────────────────┤
│  Action CRUD service   Public metadata serializer          │
│  Schedule reference checks   Migration adapter             │
└──────────────┬──────────────────────┬───────────────────────┘
               │                      │
┌──────────────▼───────────────┐  ┌───▼───────────────────────┐
│ Output Registry (new)        │  │ Scheduler (modified)      │
│ devicetree-backed bindings   │  │ validates action refs     │
│ logical output IDs only      │  │ compiles dynamic choices  │
└──────────────┬───────────────┘  └───┬───────────────────────┘
               │                      │
┌──────────────▼──────────────────────▼───────────────────────┐
│        Persistence + Reload/Validation Boundary            │
├─────────────────────────────────────────────────────────────┤
│  staged save requests   schema migration   boot reset      │
└──────────────┬──────────────────────────────────────────────┘
               │
┌──────────────▼──────────────────────────────────────────────┐
│         Relay Runtime / Execution / Recovery Policy        │
└─────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|------------------------|
| Panel action-management surface | Create, edit, delete, enable/disable, and display configured relay actions | New authenticated `/api/actions*` routes plus `main.js` action CRUD UI |
| Action catalog service | Own persisted action records, validation, migration, and public metadata | Extend `app/src/actions/` beyond fixed `relay0.on/off` helpers |
| Output registry | Map operator-selectable logical bindings to compile-time devicetree-backed GPIO descriptors | New module that exposes allowed outputs and resolves to `gpio_dt_spec` / device pointers |
| Scheduler integration | Consume dynamic action choices and keep schedule references valid | Extend schedule snapshot JSON and mutation validation around real action records |
| Persistence boundary | Atomically validate actions together with schedules and schema changes | Reuse `persistence_store_save_*()` staged save request pattern |
| Relay/output execution path | Apply configured action intent safely at runtime | Keep actual GPIO actuation behind the relay/output runtime service |

## Recommended Project Structure

```text
app/src/
├── actions/          # Catalog CRUD, migration, public metadata, execution lookup
├── outputs/          # New logical output registry backed by devicetree-known bindings
├── panel/            # Action CRUD routes, status JSON, and Actions/Schedules UI wiring
├── persistence/      # Schema/version changes and staged validation across actions/schedules
├── scheduler/        # Dynamic action choice generation and action-reference validation
└── relay/            # Safe actuation/runtime ownership for relay-backed outputs
```

### Structure Rationale

- **`actions/`:** Already owns lookup and execution; it should become the source of truth for configured actions instead of fixed built-ins.
- **`outputs/`:** Separates "what hardware bindings exist on this board" from "which operator-defined actions use them".
- **`panel/`:** Already owns route-level validation and JSON contracts, so action CRUD belongs here with thin handlers.
- **`persistence/`:** Already stages candidate snapshots and validates cross-section invariants, which is exactly what action/schedule coupling needs.
- **`scheduler/`:** Must stop assuming `relay-on` / `relay-off` are the only legal public choices.

## Architectural Patterns

### Pattern 1: Compile-Time Hardware Registry, Runtime Logical Selection

**What:** Keep physical output capabilities in devicetree-backed descriptors, but let the operator choose among those allowed bindings at runtime.
**When to use:** When hardware is board-defined but the operator should choose which output an action maps to.
**Trade-offs:** Safer and easier to validate than arbitrary raw pins, but less flexible than unconstrained runtime GPIO entry.

### Pattern 2: Single Action Catalog with Derived Public Metadata

**What:** Store one canonical action record and derive UI-facing keys/labels/choices from it for both manual control and scheduling.
**When to use:** When multiple surfaces must expose the same action set with no drift.
**Trade-offs:** Requires migration away from hard-coded public keys, but removes duplicated choice lists.

### Pattern 3: Stage-Validate-Commit Cross-Section Mutations

**What:** Build candidate action/schedule state in RAM, validate referential integrity, then persist and reload only if the full snapshot is consistent.
**When to use:** Whenever action edits can invalidate schedules or boot-time state.
**Trade-offs:** More code up front, but avoids partially-applied admin mutations.

## Data Flow

### Action Create / Update

```text
Operator form
    ↓
/api/actions/create or /api/actions/update
    ↓
Action payload validation (IDs, labels, binding choice, enabled state)
    ↓
Candidate action catalog + schedule dependency validation
    ↓
Persistence save + runtime reload
    ↓
Updated Actions page and Schedule choices
```

### Schedule Create / Update

```text
Schedule form
    ↓
Action choice selected from dynamic catalog
    ↓
/api/schedules/create or /api/schedules/update
    ↓
Resolve chosen public action to canonical action ID
    ↓
Persist schedule table + scheduler recompilation
```

### Manual Execution

```text
Configured action button on Actions page
    ↓
/api/actions/execute or equivalent action-aware command route
    ↓
Lookup canonical action
    ↓
Resolve output binding
    ↓
Relay/output runtime apply + persisted desired-state update if needed
```

## Anti-Patterns

### Anti-Pattern 1: Raw GPIO as Operator Truth

**What people do:** Store controller names, pins, and flags directly as freeform config entered by the operator.
**Why it's wrong:** It bypasses the board's devicetree-defined capabilities and makes validation, migration, and safety much harder.
**Do this instead:** Persist logical output IDs that resolve to a compile-time registry of known-safe bindings.

### Anti-Pattern 2: Separate Catalogs for Actions and Schedules

**What people do:** Keep manual control choices in one place and schedule choices in another.
**Why it's wrong:** Labels, enablement, and visibility drift immediately, and schedule bugs become hard to diagnose.
**Do this instead:** Serialize both surfaces from the same action catalog.

### Anti-Pattern 3: Mutation Without Schedule Impact Checks

**What people do:** Delete or repurpose actions without checking who depends on them.
**Why it's wrong:** Saved schedules silently become invalid or change meaning.
**Do this instead:** Validate references before commit and force explicit remediation.

## Integration Points

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| `panel` <-> `actions` | Thin CRUD/execute API calls | Panel should not know relay internals or built-in IDs |
| `actions` <-> `outputs` | Logical binding resolution | Keeps hardware lookup separate from operator metadata |
| `actions` <-> `persistence` | Candidate catalog save requests | Best place for migration and uniqueness validation |
| `persistence` <-> `scheduler` | Cross-validation on load/save | Action edits must preserve schedule referential integrity |
| `actions` <-> `relay` | Execution intent only | Relay/output service should stay the actuation owner |

## Sources

- Local repo analysis of `app/src/actions/actions.c`, `app/src/panel/panel_http.c`, `app/src/panel/panel_status.c`, `app/src/panel/assets/main.js`, `app/src/persistence/persistence.c`, and `app/src/scheduler/scheduler.c`
- [Zephyr GPIO API](https://docs.zephyrproject.org/latest/hardware/peripherals/gpio.html)
- [Zephyr Devicetree how-tos](https://docs.zephyrproject.org/latest/build/dts/howtos.html)
- [Zephyr Settings](https://docs.zephyrproject.org/latest/services/storage/settings/index.html)
- [Zephyr NVS](https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html)

---
*Architecture research for: configurable GPIO-backed relay actions*
*Researched: 2026-03-11*
