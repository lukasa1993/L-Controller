# Quick Task 3: Make a dedicated login page with proper redirect-on-success using a Tailwind Plus HTML sign-in flow. - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Task Boundary

Replace the in-dashboard login shell with a dedicated embedded `/login` page, keep the panel dashboard on `/`, and make the browser redirect cleanly between those pages based on session state.

</domain>

<decisions>
## Implementation Decisions

### Login layout reference
- Use the Tailwind Plus Application UI sign-in "Split screen" HTML pattern as the layout reference.
- Adapt it to the existing panel voice, Tailwind browser runtime, and embedded-firmware constraints instead of copying the stock marketing copy or remote image usage directly.

### Redirect contract
- Treat `/` as the protected dashboard entry and `/login` as the only login surface.
- When the dashboard shell detects no valid session, redirect to `/login?next=<safe-local-path>`.
- After a successful login, sanitize `next` and navigate back to that local path, defaulting to `/`.

### Asset and runtime shape
- Keep one shared `main.js` so session bootstrap, login submit, logout, and expired-session handling all stay in one place.
- Add a dedicated `login.html` asset plus a new firmware HTTP resource for `/login` instead of trying to emulate a separate page inside the existing dashboard HTML.

### Claude's Discretion
- Keep the static `/` route public and let the client-side session bootstrap handle the redirect, rather than converting the root shell into a server-side auth gate.
- Preserve the existing dashboard visual language on `/` and focus the Tailwind Plus adaptation on the new login page.
- Update the Playwright smoke to prove the full `/` -> `/login` -> `/` redirect path against the real device.

</decisions>

<specifics>
## Specific Ideas

- Store one short-lived flash message in `sessionStorage` so logout and expired-session redirects can explain why the browser landed on `/login`.
- Add a `data-panel-page` marker to both HTML entrypoints so the shared script can choose page-specific bootstrap behavior safely.
- Update panel auto-discovery markers so they keep probing `/` successfully after the login form moves off the dashboard HTML.

</specifics>
