# LNH Nordic

## What This Is

LNH Nordic is a mission-critical embedded control node for Nordic/Zephyr hardware that connects to a predefined Wi-Fi network and exposes a local operator panel for controlling and scheduling device actions. The current repo already proves Wi-Fi bring-up on `nRF7002-DK`; this project evolves it into a production-structured controller with robust recovery, OTA updates, persistent configuration, and an extensible action framework starting with relay control.

## Core Value

The device must reliably and safely execute configured local control actions—even through Wi-Fi disruption or subsystem faults—without unnecessary operator intervention.

## Requirements

### Validated

- ✓ Device firmware builds, flashes, and runs on `nRF7002-DK` with the existing Zephyr/Nordic tooling — existing codebase
- ✓ Device can connect to a predefined Wi-Fi network using build-time configuration — existing codebase
- ✓ Device waits for DHCP/network readiness and performs a post-connect reachability check before declaring ready — existing codebase
- ✓ Repo already provides host-side bootstrap, SDK fetch, build, flash, console, and diagnostics scripts — existing codebase

### Active

- [ ] Replace the single-file firmware structure with a modular architecture that separates Wi-Fi, recovery/watchdog, relay control, scheduling, persistence, OTA, HTTP server, authentication, and UI asset delivery
- [ ] Make Wi-Fi management extremely robust, with bounded retries, clear failure states, and conservative self-recovery that escalates to full restart only for confirmed unrecoverable conditions
- [ ] Expose a local-LAN-only authenticated HTTP control panel using proper static HTML/JavaScript assets, styled via Tailwind Play CDN, rather than UI generation in C
- [ ] Support OTA update flows for both authenticated local upload and remote pull
- [ ] Persist operator configuration for the single local user, relay/action definitions, schedules, and future SDK/integration settings
- [ ] Implement a local cron-style scheduler and a generic action framework, with single relay activation/deactivation as the first production action
- [ ] Keep the system simple enough to operate on embedded hardware while applying mission-critical precautions around fault isolation, recovery, and secure defaults

### Out of Scope

- Multi-user accounts, roles, or credential rotation — initial panel is intentionally single-user and straightforward
- Public internet exposure or cloud-first control plane — panel is for local HTTP access on a trusted LAN only
- Runtime Wi-Fi onboarding UI — Wi-Fi credentials stay predefined at build time for the first release
- Non-relay action implementations beyond the extensible framework — future integrations come after the core stack is stable
- Over-engineered auth/session features that materially increase complexity without helping the embedded single-user use case — keep authentication simple and secure

## Context

- Current codebase is a brownfield Nordic/Zephyr firmware repo centered on `app/src/main.c` plus shell automation scripts
- Existing firmware already proves the core networking path: locate the Wi-Fi interface, connect with configured credentials, wait for DHCP, run a TCP reachability check, and signal readiness
- The repo currently lacks production structure for multiple subsystems; runtime behavior is concentrated in one translation unit, which is risky for maintainability and fault isolation
- This work shifts the project from a Wi-Fi bring-up prototype into a mission-critical embedded controller with clear subsystem boundaries, local web operations, OTA lifecycle management, and scheduled control behavior
- The first end-to-end business function is relay activation/deactivation. Other action types and SDK-backed integrations will be added only after the core platform is solid

## Constraints

- **Tech stack**: Must stay within the Nordic/Zephyr firmware ecosystem already established in this repo — reuse the working platform and avoid unnecessary rewrites
- **Hardware**: Current target is `nRF7002-DK` with one implemented relay path first — architecture should scale cleanly to more hardware later
- **Connectivity**: Panel is local-LAN HTTP only — deployment assumes trusted local access, not public internet hardening
- **Reliability**: System is mission-critical and must favor deterministic behavior, layered recovery, and conservative restart escalation — accidental or noisy resets are unacceptable
- **Configuration**: Wi-Fi credentials are provided at build time initially — simplifies early deployment and reduces runtime configuration surface
- **Security**: Authentication should remain simple but secure for a single embedded operator account — avoid complex account systems while still protecting the control panel
- **Maintainability**: UI assets must live as proper HTML/JS resources, not be assembled as generated strings in C — keeps firmware and frontend responsibilities separated
- **Extensibility**: Action execution, scheduling, and persistence must be designed as reusable subsystems from day one — future SDK/integration work depends on this foundation

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

---
*Last updated: 2026-03-08 after initialization*
