# Requirements: LNH Nordic

**Defined:** 2026-03-11
**Core Value:** The device must reliably and safely execute configured local control actions even through Wi-Fi disruption or subsystem faults without unnecessary operator intervention.

## v1.1 Requirements

Requirements for milestone v1.1 Action Configuration Flow.

### Action Management

- [ ] **ACT-03**: Authenticated operator can create a relay action with a unique operator-safe action ID and display name
- [ ] **ACT-04**: Authenticated operator can view configured relay actions with name, enabled state, binding summary, and usability status
- [ ] **ACT-05**: Authenticated operator can edit an existing relay action's name, output binding, and enabled state
- [ ] **ACT-06**: Authenticated operator can delete a relay action only through a flow that preserves schedule integrity
- [ ] **ACT-07**: Actions page manual control executes selected configured actions rather than fixed built-in relay commands
- [ ] **ACT-08**: Device exposes one shared configured action catalog to both Actions and Schedules surfaces

### Output Binding and Safety

- [ ] **BIND-01**: Authenticated operator can bind a relay action to an approved GPIO-backed output definition known to firmware
- [ ] **BIND-02**: Device rejects relay actions with invalid or unsafe output bindings before they become usable
- [ ] **BIND-03**: Unconfigured, invalid, or disabled relay actions are not executable from the Actions page
- [ ] **BIND-04**: Unconfigured, invalid, or disabled relay actions are not selectable for new or edited schedules

### Scheduling Integration

- [ ] **SCHED-05**: Schedule create and edit flows list configured relay actions from the shared action catalog
- [ ] **SCHED-06**: Device blocks or explicitly remediates schedule changes that would leave a schedule referencing a missing or unusable action
- [ ] **SCHED-07**: Scheduled executions continue to dispatch through the same canonical action ID path used by manual execution

### Migration and Persistence

- [ ] **MIGR-01**: Device safely handles persisted legacy `relay0.on` and `relay0.off` action references during migration to configured actions
- [ ] **MIGR-02**: Device safely handles persisted schedule references to legacy built-in relay actions during migration
- [ ] **MIGR-03**: Action configuration changes persist across reboot and boot-time validation never causes hidden fallback execution of removed built-in actions

## Future Requirements

### Hardware Expansion

- **RELAY-04**: Device can manage multiple relay channels
- **RELAY-05**: Device can assign schedules and actions to specific relay channels independently

### Broader Action Types

- **ACT-09**: Device can execute non-relay actions through future SDK or integration adapters
- **ACT-10**: Operator can configure non-relay action types through the same shared action-management flow

### Action Operations

- **OBS-01**: Operator can see which schedules depend on each configured action before editing or deleting it
- **OBS-02**: Operator can trigger a safe test action or diagnostic preview for a configured output

## Out of Scope

| Feature | Reason |
|---------|--------|
| Arbitrary raw GPIO discovery or free-form pin entry from the panel | Firmware must keep hardware bindings explicit and safe |
| Non-relay action implementations | This milestone only establishes the shared configuration flow |
| Bulk action templates or import/export | Not required to prove the core configurable-action model |
| Public internet or cloud-backed action management | Panel remains local-LAN only |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| ACT-03 | Phase 9 | Pending |
| ACT-04 | Phase 9 | Pending |
| ACT-05 | Phase 9 | Pending |
| ACT-06 | Phase 10 | Pending |
| ACT-07 | Phase 10 | Pending |
| ACT-08 | Phase 10 | Pending |
| BIND-01 | Phase 9 | Pending |
| BIND-02 | Phase 9 | Pending |
| BIND-03 | Phase 10 | Pending |
| BIND-04 | Phase 10 | Pending |
| SCHED-05 | Phase 10 | Pending |
| SCHED-06 | Phase 10 | Pending |
| SCHED-07 | Phase 10 | Pending |
| MIGR-01 | Phase 9 | Pending |
| MIGR-02 | Phase 10 | Pending |
| MIGR-03 | Phase 9 | Pending |

**Coverage:**
- v1.1 requirements: 16 total
- Mapped to phases: 16
- Unmapped: 0

---
*Requirements defined: 2026-03-11*
*Last updated: 2026-03-11 after milestone v1.1 definition*
