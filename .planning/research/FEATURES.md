# Feature Research

**Domain:** Mission-critical embedded relay controller with local authenticated web panel
**Researched:** 2026-03-08
**Confidence:** MEDIUM

## Feature Landscape

### Table Stakes (Users Expect These)

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Local authenticated admin panel | Operators expect a safe way to control and inspect the device | MEDIUM | Single-user auth is enough for v1, but session handling still needs discipline |
| Manual relay control | Core product behavior; device is not useful without it | LOW | Must support explicit on/off with safe default state handling |
| Persistent configuration | Operators expect settings and schedules to survive reboot | MEDIUM | Split static build-time config from mutable runtime config |
| Robust Wi-Fi reconnect | A networked controller is expected to recover from normal AP issues | MEDIUM | Must tolerate drops without rebooting on every disconnect |
| Health / status visibility | Operators need to know if Wi-Fi, scheduler, auth, and relay subsystems are healthy | MEDIUM | Include connectivity state, last error, and uptime-ish signals |
| OTA update path with rollback discipline | Embedded networked products are expected to be maintainable in the field | HIGH | Safe update mechanics matter more than update frequency |
| Scheduled actions | Once relay control exists, timer/cron behavior is a natural expectation | MEDIUM | Requires a clear time source strategy |
| Safe recovery behavior | Mission-critical operators expect the system to fail controlled, not chaotically | HIGH | Watchdogs, supervision, and recovery thresholds are part of the product, not just implementation detail |

### Differentiators (Competitive Advantage)

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Generic action engine | Lets the product grow beyond relay control without re-architecting | HIGH | Important structurally even if only one action is implemented initially |
| Presets / scenes | Makes repeated operational workflows fast | MEDIUM | Naturally builds on action abstractions |
| Remote-pull OTA policies | Reduces manual operator effort in managed deployments | HIGH | Introduce only after basic safe OTA works |
| Future SDK / integration framework | Enables downstream value without touching the relay core each time | HIGH | Should be architected early but filled in later |
| Event/audit log | Improves trust and troubleshooting | MEDIUM | Helpful for mission-critical operations and post-failure diagnosis |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Public internet exposure of the panel | Feels convenient for remote access | Expands the threat model dramatically and conflicts with the local HTTP deployment assumption | Keep v1 local-LAN-only |
| Reboot on every repeated Wi-Fi issue | Feels like a quick reliability fix | Causes reboot storms and hides whether the network layer can self-heal | Use retry budgets and explicit escalation rules |
| Multi-user roles and account administration in v1 | Sounds more “complete” | Adds large auth complexity without helping the single-operator use case | Single local admin account first |
| User-authored scripting language in the device | Seems flexible | Large safety, validation, and persistence burden on constrained hardware | Structured action types with fixed schemas |
| Runtime-generated HTML in C | Seems easy early on | Becomes unmaintainable and blocks frontend iteration | Author real HTML/JS assets and embed/serve them |

## Feature Dependencies

```text
Authenticated panel
    └──requires──> persistent config
                         └──supports──> schedules
                                           └──triggers──> action engine
                                                             └──calls──> relay driver

OTA updates
    └──requires──> MCUboot / image-slot layout

Remote-pull OTA
    └──requires──> robust networking supervisor

Audit / event log
    └──enhances──> recovery diagnosis and operator trust
```

### Dependency Notes

- **Authenticated panel requires persistent config:** credentials, session secrets, and settings must survive reboot safely.
- **Schedules require persistent config and a time strategy:** otherwise jobs vanish or execute unpredictably after reset.
- **Action engine requires relay driver abstraction:** hardware control should not be implemented directly inside HTTP handlers or schedulers.
- **OTA requires bootloader discipline:** update delivery is useless unless the boot path can validate, swap, and recover safely.

## MVP Definition

### Launch With (v1)

- [ ] Modular firmware structure with clear subsystem boundaries — foundation for all future work
- [ ] Robust Wi-Fi supervision and conservative recovery — core reliability requirement
- [ ] Local authenticated panel with authored HTML/JS assets — operational control surface
- [ ] Single relay activation/deactivation action — first business function
- [ ] Persistent configuration store — required for auth, actions, and schedules
- [ ] Local scheduler for relay actions — core automation use case
- [ ] OTA path (local upload + remote pull-capable architecture) — maintainability requirement

### Add After Validation (v1.x)

- [ ] More relay channels — add after the first channel architecture proves out
- [ ] Event/audit history views — add once the basic control flow is stable
- [ ] Action presets / scenes — add once relay/action primitives are trustworthy
- [ ] AP fallback or local provisioning mode — only if field operations prove it is needed

### Future Consideration (v2+)

- [ ] Third-party SDK / integration actions — defer until core action engine is stable
- [ ] Multi-user or role-based auth — defer unless operator reality demands it
- [ ] Public/cloud remote management — defer until the local-only product is proven and threat model is revisited

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Modular subsystem split | HIGH | MEDIUM | P1 |
| Wi-Fi supervisor | HIGH | MEDIUM | P1 |
| Authenticated local panel | HIGH | MEDIUM | P1 |
| Relay on/off action | HIGH | LOW | P1 |
| Persistent config | HIGH | MEDIUM | P1 |
| Scheduler | HIGH | MEDIUM | P1 |
| OTA foundation | HIGH | HIGH | P1 |
| Event log | MEDIUM | MEDIUM | P2 |
| Presets / scenes | MEDIUM | MEDIUM | P2 |
| SDK integrations | MEDIUM | HIGH | P3 |

## Sources

- User-stated product goals in `.planning/PROJECT.md`
- Existing brownfield repo capabilities from `.planning/codebase/ARCHITECTURE.md`
- Official Zephyr docs for HTTP server, Wi-Fi management, settings, watchdog, and DFU, used to ground what the platform naturally supports

---
*Feature research for: mission-critical embedded relay controller*
*Researched: 2026-03-08*
