# Quick Task 6: Review every panel page, split bulky surfaces into more dedicated routes where it improves the workflow, and redesign the panel with close-match Tailwind Plus paid UI patterns in a clean app style. - Context

**Gathered:** 2026-03-11
**Status:** Ready for planning

<domain>
## Task Boundary

Review the embedded panel end to end, identify where the current routes still hide multiple workflows inside one surface, split those areas into more dedicated destinations where it materially improves the operator flow, and redesign the pages around close-match Tailwind Plus paid application UI patterns.

Keep this within the existing embedded plain HTML/JS panel architecture. Do not introduce a frontend framework or expand the firmware API scope beyond the routes and client behavior needed to support the new page structure.

</domain>

<decisions>
## Implementation Decisions

### Tailwind Plus Fidelity
- Use Tailwind Plus paid application UI blocks as the primary visual reference and adapt them closely to the embedded panel.

### Route Strategy
- Split the current panel more aggressively than the existing shared shell when it improves real workflows.
- The most likely low-risk splits are the combined create/edit management forms inside the Actions and Schedules surfaces, with any further route expansion justified by workflow clarity rather than novelty.

### Visual Direction
- Shift the authenticated panel away from the current mission-console styling toward a cleaner Tailwind Plus app/settings feel with stronger hierarchy, calmer surfaces, and clearer page framing.

### Claude's Discretion
- Reuse the existing login route unless Tailwind Plus references expose a materially better flow for the current firmware constraints.
- Keep overview, actions, schedules, and updates grounded in the current firmware truth model even when page structure changes.
- Prefer route and template changes that keep Playwright smoke coverage practical and do not require backend API contract changes.

</decisions>

<specifics>
## Specific Ideas

- Treat the authenticated panel as a real multi-page application instead of a single shell that swaps hidden sections.
- Give create/edit work its own destination where forms currently compete with list and status content.
- Keep navigation, session state, and refresh behavior consistent across all page destinations.
- Use Tailwind Plus application-shell, table, stats, description-list, and form-layout patterns as the main design vocabulary.

</specifics>
