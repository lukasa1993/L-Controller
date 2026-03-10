# LNH Nordic

## What This Is

LNH Nordic is a mission-critical embedded control node for Nordic/Zephyr hardware that connects to a predefined Wi-Fi network and exposes a local authenticated operator panel for manual control, schedules, and OTA updates. v1.0 shipped the platform, panel, scheduling, and a single built-in relay action path; v1.1 now moves that fixed behavior into a configurable action flow where operators define relay-backed actions before they become usable.

## Core Value

The device must reliably and safely execute configured local control actions—even through Wi-Fi disruption or subsystem faults—without unnecessary operator intervention.

## Current Milestone: v1.1 Action Configuration Flow

**Goal:** Replace built-in relay-only actions with an operator-managed configuration flow where relay actions are created, named, bound to GPIO, and only then exposed for manual use and scheduling.

**Target features:**
- Relay action creation flow with operator-defined name/label and GPIO-backed hardware binding
- Validation gates so unconfigured or invalid relay actions cannot be executed or scheduled
- Actions-page management surface for configured relay actions instead of fixed built-in relay controls
- Shared action catalog across manual control and schedules so configured relay actions stay consistent everywhere
- Persistence and migration behavior that preserves safe system behavior while the action model becomes operator-configured

## Requirements

### Validated

- ✓ Device firmware builds, flashes, and runs on `nRF7002-DK` with the existing Zephyr/Nordic tooling — existing codebase
- ✓ Device can connect to a predefined Wi-Fi network using build-time configuration — existing codebase
- ✓ Device waits for DHCP/network readiness and performs a post-connect reachability check before declaring ready — existing codebase
- ✓ Repo already provides host-side bootstrap, SDK fetch, build, flash, console, and diagnostics scripts — existing codebase
- ✓ Device exposes a local authenticated HTTP panel with dedicated overview, actions, schedules, and updates routes backed by authored HTML/JavaScript assets — v1.0
- ✓ Device executes built-in relay actions through one shared dispatcher path for both manual control and scheduler automation — v1.0
- ✓ Device persists auth, relay/action, schedule, and OTA configuration with validation and safe fallback on boot — v1.0
- ✓ Device supports cron-style relay scheduling and MCUboot-backed OTA staging/apply flows — v1.0

### Active

- [ ] Operator can create a relay action by entering a name and binding it to a GPIO-backed output definition
- [ ] Relay actions remain unavailable for manual control and scheduling until their hardware configuration validates and is saved
- [ ] Panel exposes configured relay actions as managed resources on the Actions page rather than fixed built-in relay buttons
- [ ] Scheduler can target configured relay actions from the same shared action catalog used by manual control
- [ ] Operator can review, edit, enable/disable, and remove configured relay actions without corrupting persistence or schedule integrity
- [ ] Action configuration stays generic enough to support future non-relay action types without reworking the operator flow

### Out of Scope

- Multi-user accounts, roles, or credential rotation — initial panel remains intentionally single-user and straightforward
- Public internet exposure or cloud-first control plane — panel is still for local HTTP access on a trusted LAN only
- Runtime Wi-Fi onboarding UI — Wi-Fi credentials stay predefined at build time for now
- Non-relay action implementations beyond the shared configuration foundation — future integrations come after configurable relay actions are solid
- Automatic GPIO discovery or arbitrary pin probing workflows — operator-configured bindings must remain explicit and safe
- Over-engineered auth/session features that materially increase complexity without helping the embedded single-user use case — keep authentication simple and secure

## Context

- Current codebase is now a structured Nordic/Zephyr firmware application with dedicated networking, recovery, persistence, panel, action, scheduler, relay, and OTA subsystems
- v1.0 shipped a working local operator panel, scheduler, and OTA lifecycle, but the action model still assumes built-in relay IDs like `relay0.on` and `relay0.off`
- The panel and status APIs already expose a “not configured yet” relay state, which aligns with the next step of making relay actions explicit operator-managed resources
- Existing schedules persist action IDs, so this milestone must keep manual control, scheduler usage, and persistence on one shared action catalog to avoid drift
- Current relay hardware behavior is still anchored to a single GPIO-backed relay path; the milestone needs a safe configuration layer without compromising the existing recovery and startup guarantees
- Quick task 5 already shifted the authenticated landing page to an actions-first shell and prepared the UI path for future configurable action management

## Constraints

- **Tech stack**: Must stay within the Nordic/Zephyr firmware ecosystem already established in this repo — reuse the working platform and avoid unnecessary rewrites
- **Hardware**: Current target is `nRF7002-DK` and the first configurable action type is still relay/output control — architecture should scale cleanly to more hardware later
- **Connectivity**: Panel is local-LAN HTTP only — deployment assumes trusted local access, not public internet hardening
- **Reliability**: System is mission-critical and must favor deterministic behavior, layered recovery, and conservative restart escalation — accidental or noisy resets are unacceptable
- **Configuration**: Wi-Fi credentials are provided at build time initially, while action definitions become runtime-configured — keep the runtime surface focused on operator actions, not network onboarding
- **Security**: Authentication should remain simple but secure for a single embedded operator account — avoid complex account systems while still protecting the control panel
- **Maintainability**: UI assets must live as proper HTML/JS resources, not be assembled as generated strings in C — keeps firmware and frontend responsibilities separated
- **Extensibility**: Action configuration, execution, scheduling, and persistence must stay reusable so future SDK/integration work can plug into the same operator workflow
- **Safety**: Invalid or incomplete GPIO/action configuration must block execution rather than falling back to an implicit default — operator-visible truth matters more than convenience
- **Migration**: Existing devices may already hold persisted built-in relay action references and schedules — any schema change must preserve safe boot behavior and define how old data transitions

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Break `main.c` into subsystem modules before adding major features | Mission-critical behavior needs separation of concerns, testability, and clearer failure boundaries | — Pending |
| Keep panel access local-LAN-only over HTTP | Matches embedded deployment assumptions and avoids premature remote/cloud complexity | — Pending |
| Use a single local user account for panel authentication | Simplest secure model for the initial embedded admin workflow | — Pending |
| Keep Wi-Fi credentials build-time configured in v1 | Reduces runtime setup complexity while the control core is stabilized | — Pending |
| Implement conservative watchdog/recovery escalation | Full restarts are necessary for true stuck states, but must not trigger on minor faults | — Pending |
| Build OTA for both local upload and remote pull | Operators need a local service path now, while future deployments need managed update hooks | — Pending |
| Create a generic action, scheduler, and persistence foundation but ship only relay on/off first | Delivers immediate operator value without blocking future extensibility | — Pending |
| Store panel UI as dedicated HTML/JS assets using Tailwind Play CDN | Keeps frontend maintainable and avoids brittle C-generated markup | — Pending |
| Replace built-in relay actions with operator-configured relay actions in v1.1 | The next usable workflow requires naming and GPIO binding before an action appears in control or schedules | — Pending |
| Keep manual control and scheduling backed by one persisted action catalog | Operators should see the same configured actions everywhere and the scheduler must not target hidden/internal actions | — Pending |
| Hide or block unconfigured relay actions instead of exposing placeholders that still execute | Safety and operator trust require “configured” to be a real gate, not just a label | — Pending |

---
*Last updated: 2026-03-11 after milestone v1.1 initialization*
