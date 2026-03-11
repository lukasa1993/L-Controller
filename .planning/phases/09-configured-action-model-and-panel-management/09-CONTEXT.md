# Phase 9: Configured Action Model and Panel Management - Context

**Gathered:** 2026-03-11
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace fixed built-in relay action assumptions with a configured relay-action catalog and an authenticated Actions-page management surface. This phase defines how relay actions are represented, named, bound to approved outputs, shown in the panel, and migrated away from the built-in relay model. It does not yet make those configured actions executable from the Actions page, selectable in schedule create/edit flows, or safe to delete while preserving schedule integrity; those integrations stay in Phase 10.

</domain>

<decisions>
## Implementation Decisions

### Action record shape
- One saved relay action represents one executable command, not an output container with implicit child commands.
- Operators create command-shaped actions such as separate "Relay 0 On" and "Relay 0 Off" style records rather than one parent output that owns both behaviors.
- The operator enters a friendly action name and the device generates the stable operator-safe action ID from that name.
- A valid saved action becomes enabled immediately by default rather than saving into a disabled holding state.
- The Actions page should read as a flat action catalog, not a grouped output tree.

### Binding selection and catalog truth
- Action create and edit flows should present only firmware-approved output choices in a picker; raw GPIO or free-form binding entry should not appear in the panel.
- Each saved action should show its bound output plus the command meaning together in the catalog, such as output label plus the action behavior.
- Catalog usability language should stay short and operator-facing: `Ready`, `Disabled`, and `Needs attention`.
- If no configured actions exist yet, the Actions page should lead with a create-first empty state rather than a long explanatory or reference-first screen.

### Migration posture
- Phase 9 should make a clean break from legacy built-in relay actions instead of importing them into the new catalog.
- Old built-in action records should be treated as removable legacy data, not as migrated first-class configured actions.
- The panel should not show a migration badge, warning, or one-time notice for dropped legacy built-in actions.
- Explicit migration handling still needs to exist in firmware behavior so boot never falls back to hidden built-in execution, but that explicitness should not depend on user-facing legacy UI.

### Claude's Discretion
- Exact action-ID normalization rules and collision handling, as long as IDs are generated from the friendly name and remain operator-safe.
- Exact create and edit layout, card styling, and row density for the flat Actions catalog.
- Exact short copy for `Ready`, `Disabled`, and `Needs attention`, including when a reason appears inline versus inside edit/detail affordances.
- Exact approved-output labels and whether small hardware hints appear in detail views, as long as the main picker stays friendly and non-technical.

</decisions>

<specifics>
## Specific Ideas

- The configured-action model should feel command-centric: one saved record equals one thing the device can eventually execute.
- Friendly names drive the operator experience; stable IDs matter for durability and cross-surface references, but they should be device-generated rather than hand-authored in normal use.
- The Actions page should feel like a real management catalog, not a placeholder for future multi-output topology.
- The user wants a clean break from legacy built-in actions: remove old relay junk and do not surface legacy migration messaging in the panel.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `app/src/actions/actions.c` and `app/src/actions/actions.h`: existing dispatcher, public key/label helpers, and the current built-in relay assumptions that Phase 9 needs to replace with configured-action catalog truth.
- `app/src/persistence/persistence_types.h` and `app/src/persistence/persistence.c`: current persisted action and schedule contracts, sectioned save/load APIs, atomic validation posture, and layout-version migration/reset behavior.
- `app/src/panel/panel_http.c`: authenticated exact-route HTTP patterns plus the existing persistence-save-and-runtime-reload mutation flow used by schedule handlers.
- `app/src/panel/panel_status.c`: the current status and schedule snapshot serializers, including the existing `outputsConfigured` gate and hard-coded action-choice serialization.
- `app/src/panel/assets/main.js`: reusable shell, navigation, fetch, render, and empty-state patterns for the Actions surface.
- `app/src/app/app_context.h` and `app/src/app/bootstrap.c`: shared runtime wiring for persistence, actions, relay, scheduler, and panel.
- `tests/panel-login.spec.js` and `tests/helpers/panel-target.js`: existing browser smoke harness that can extend to Phase 9 panel verification later.

### Established Patterns
- Operator-facing UI must stay truthful and avoid fake-ready controls or roadmap-style placeholders once a surface claims a capability.
- Internal action IDs should stay out of normal operator workflows; panel surfaces prefer friendly labels and exact, operator-readable states.
- Persistence saves are sectioned, validated before write, and atomic at the request level; incomplete or malformed drafts should not become durable records.
- The codebase favors authenticated exact-route JSON APIs layered onto the existing panel shell rather than ad hoc endpoints or browser-held source-of-truth state.
- Current runtime behavior is still single-relay and command-centric: one action ID resolves to one relay boolean command today.

### Integration Points
- The persisted action catalog and persisted schedule table already share canonical `action_id` references, so Phase 9 catalog choices directly shape Phase 10 schedule integration.
- `action_dispatcher_execute()` already runs through canonical action IDs, which aligns with the decision that one configured relay action equals one executable command.
- `panel_status_outputs_configured()` and `panel_http_outputs_configured()` are currently stubbed false, so Phase 9 has a clear seam for replacing placeholder gating with real configured-action truth.
- Schedule snapshot `actionChoices` are currently hard-coded to `relay-on` and `relay-off`; Phase 9 can establish the future catalog serialization path even though schedule create/edit adoption remains Phase 10 work.
- Boot currently auto-seeds built-in relay actions, so Phase 9 planning must explicitly remove that dependency without introducing hidden fallback execution.

</code_context>

<deferred>
## Deferred Ideas

- Making configured relay actions executable from the Actions page belongs to Phase 10.
- Feeding the new configured-action catalog into schedule create/edit flows belongs to Phase 10.
- Delete flows, dependency preservation, and repair of schedule references to removed or mutated actions belong to Phase 10.
- Multi-output expansion, richer hardware topology, and non-relay action types remain future work beyond this phase.
- Advanced binding entry, raw GPIO authoring, or deeply technical hardware fields in the main panel flow stay out of scope.

</deferred>

---
*Phase: 09-configured-action-model-and-panel-management*
*Context gathered: 2026-03-11*
