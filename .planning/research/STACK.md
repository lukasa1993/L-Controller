# Stack Research

**Domain:** Mission-critical embedded control firmware with operator-configured GPIO-backed relay actions
**Researched:** 2026-03-11
**Confidence:** HIGH

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| Nordic Connect SDK | v3.2.1 | Keep the current firmware, networking, HTTP, storage, and board support baseline | The repo already runs on this stack, and the milestone is a data-model and UI/API evolution rather than a platform migration |
| Zephyr GPIO + devicetree device model | Bundled with NCS v3.2.1 | Represent physical outputs behind compile-time known controllers and descriptors | Official docs center GPIO integration around devicetree-backed devices and `gpio_dt_spec`-style bindings, which supports a safe allowlisted output model better than arbitrary raw pin entry |
| Existing typed persistence layer over NVS-backed storage | Repo baseline | Persist action catalogs, relay metadata, and schedules with schema/version checks | The repo already validates typed sections on boot; this milestone needs richer action records and safe migration, not a new storage system |
| Existing Zephyr HTTP service + panel JSON contracts | Repo baseline | Serve action-management and schedule-selection APIs to the embedded panel | The panel already uses exact-path JSON routes and live refresh cycles; extending that surface is lower risk than introducing a new transport |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Existing scheduler compiler/validator | Repo baseline | Reject schedules that reference invalid actions or invalid cron expressions | Keep using it whenever action edits, deletes, or migrations can affect schedule validity |
| Existing panel asset pipeline | Repo baseline | Keep the browser UI as authored HTML/JS/CSS served from firmware | Use it for action CRUD screens and shared action-choice rendering so manual control and schedules stay aligned |
| Existing mutex-guarded action dispatcher | Repo baseline | Preserve one serialized execution path for manual and scheduled actions | Reuse it after the catalog becomes configurable so execution semantics do not fork |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `./scripts/validate.sh` | Build-first regression signal | Keep as the default automated gate while adding focused tests for persistence migration and action validation |
| Browser smoke plus device-side verification | Verify action CRUD, schedule selection, and GPIO actuation on real hardware | This milestone changes operator flow and hardware binding assumptions, so browser-only checks are not enough |

## Installation

```bash
# No new package manager dependencies are recommended for this milestone.
# Stay on the current Nordic/Zephyr toolchain and extend the existing app modules.
./scripts/validate.sh
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| Store operator-selected logical output IDs that resolve to approved GPIO bindings in firmware | Store arbitrary port/pin tuples directly from the UI | Only use raw tuples if the board support package already exposes a safe runtime-resolvable registry and you can still enforce a strict allowlist |
| Extend the current typed action catalog and schedule validation flow | Introduce a second config store just for output/action admin | Only use a separate store if action capacity or write frequency grows enough that section isolation becomes a measurable issue |
| Add generic action-management routes and keep one dispatcher | Keep `/api/relay/desired-state` as the primary control route forever | Only keep the relay-only route if the milestone is explicitly limited to one hard-wired relay and no operator-created actions |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| Arbitrary user-entered GPIO port/pin strings as the source of truth | Too easy to create invalid, unsafe, or board-specific configurations the runtime cannot trust | Persist an operator-facing action record that references a firmware-known output binding |
| Separate action lists for manual control and scheduling | UI drift and schedule breakage become almost guaranteed | Keep one persisted action catalog that both pages consume |
| Auto-seeding built-in `relay0.on` / `relay0.off` alongside the new configurable catalog | It preserves old hidden assumptions and makes migration harder to reason about | Migrate to explicit configured actions with a documented compatibility path |
| Adding a database, filesystem, or cloud service for this milestone | Completely disproportionate to an 8-action embedded admin surface | Keep the existing typed persistence model |

## Stack Patterns by Variant

**If the hardware still exposes only one real relay path:**
- Use a generic action record anyway.
- Resolve it to the single approved output binding behind the firmware boundary.

**If more GPIO-backed outputs are added later:**
- Keep the same action schema and add more approved output bindings.
- Do not change schedule or dispatcher contracts just because the binding list grows.

## Version Compatibility

| Package A | Compatible With | Notes |
|-----------|-----------------|-------|
| Nordic Connect SDK `v3.2.1` | Existing repo modules and scripts | Recommended baseline for the milestone because the repo already builds and flashes on it |
| Zephyr GPIO/devicetree APIs | Existing relay, panel, scheduler, and persistence modules | The milestone should extend current modules instead of switching subsystems |

## Sources

- [Zephyr GPIO docs](https://docs.zephyrproject.org/latest/hardware/peripherals/gpio.html) - verified the device-tree-backed GPIO model and runtime API expectations
- [Zephyr devicetree how-tos](https://docs.zephyrproject.org/latest/build/dts/howtos.html) - verified alias/device lookup patterns and compile-time device references
- [Zephyr settings docs](https://docs.zephyrproject.org/latest/services/storage/settings/index.html) - verified boot-time settings loading and commit semantics for interdependent configuration
- [Zephyr NVS docs](https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html) - verified flash-backed key/value storage behavior and write constraints
- Local repo inspection - `app/src/actions/actions.c`, `app/src/panel/panel_http.c`, `app/src/panel/panel_status.c`, `app/src/persistence/persistence.c`, `app/src/persistence/persistence_types.h`, `app/src/scheduler/scheduler.c`

---
*Stack research for: configurable relay actions*
*Researched: 2026-03-11*
