# Quick Task 5: simplify dashboard into page navigation with actions first, fix panel flicker/reload experience, and prepare UI path for future multi-relay GPIO configuration - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Task Boundary

Simplify the local control panel so the primary action controls are immediately visible and usable, replace the long single-dashboard experience with focused page navigation, and remove the current flicker/reload behavior that makes action clicks feel broken.

This quick task is intentionally UI-first. Do not expand the firmware persistence, relay runtime, or API model to support multiple configurable GPIO relays in this slice. Keep the panel architecture ready for that future work, but defer the actual multi-relay data-model change.

</domain>

<decisions>
## Implementation Decisions

### Scope Cut
- Deliver the panel UX cleanup now and explicitly defer configurable multi-relay GPIO support to a follow-up task or phase.

### Navigation Model
- Replace the long single-page dashboard with page-level navigation for a compact overview plus focused `Actions`, `Schedules`, and `Updates` surfaces.
- Keep device and network summary information lightweight and persistent enough that operators do not need to scroll past it to reach controls.

### Action Feedback
- Use optimistic local feedback for action presses so controls react immediately, then reconcile quietly with refreshed device truth after the API returns.

### Claude's Discretion
- Reuse the current panel visual language and the existing Tailwind Plus-inspired login direction instead of pulling in broad new paid references unless a specific additional block becomes necessary during implementation.
- Favor targeted DOM updates over whole-surface rerenders whenever possible to reduce flicker and preserve interaction continuity.

</decisions>

<specifics>
## Specific Ideas

- Put the primary action controls at the top of the protected experience.
- Make navigation obvious enough that `Actions`, `Schedules`, and `Updates` feel like separate destinations instead of stacked cards.
- Preserve firmware truth as the source of record even when the UI responds optimistically.
- Leave clear room in the information architecture for future configurable multi-relay GPIO management.

</specifics>
