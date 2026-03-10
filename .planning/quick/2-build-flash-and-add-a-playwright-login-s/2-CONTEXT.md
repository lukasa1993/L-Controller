# Quick Task 2: Build, flash, and add a Playwright login smoke that verifies the first authenticated page is the dashboard. - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Task Boundary

Keep the existing firmware build and flash flow intact, then add Playwright coverage that logs into the live local panel and confirms the first authenticated surface is the dashboard rather than the login shell.

</domain>

<decisions>
## Implementation Decisions

### Device targeting
- The Playwright flow should auto-detect the flashed device instead of requiring a manually supplied LAN URL.
- Auto-detection should stay local-LAN only and derive the target from the host's active private subnet plus the configured panel port.

### Build and flash packaging
- Do not add a new one-command repo wrapper script for this task.
- Keep `./scripts/build.sh` and `./scripts/flash.sh` as the manual hardware steps that precede the Playwright smoke.

### Browser assertion depth
- Treat success as a real authenticated dashboard transition, not just a successful login response.
- The smoke should verify that the login shell is hidden, the dashboard becomes visible, and stable authenticated dashboard markers render from protected device status.

### Claude's Discretion
- Keep admin credentials out of source control and inject them through environment variables when running Playwright.
- Use a focused device-discovery helper rather than coupling Playwright to an interactive serial-console session.
- Prefer a lightweight smoke test and supporting utilities over a broader end-to-end suite.

</decisions>

<specifics>
## Specific Ideas

- Probe the active private subnet on the configured panel port and identify the correct device by checking for the panel shell markers.
- Assert post-login markers that are unlikely to be false positives, such as the authenticated session chip and at least one dashboard card title rendered from protected state.
- Document the exact environment variables and manual sequence: validate or build, flash, wait for the device to boot, then run Playwright.

</specifics>
