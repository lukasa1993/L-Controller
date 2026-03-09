# Phase 5: Local Control Panel - Research

**Researched:** 2026-03-09
**Status:** Ready for planning

<focus>
## Research Goal

Identify the smallest Nordic/Zephyr-native implementation shape for the local control panel that fits the current `app_context` and `bootstrap` architecture, respects the locked session-flow decisions, and keeps Phase 5 limited to local HTTP transport, authored static asset delivery, single-user login/logout/session flow, protected read-only status APIs, and the first dashboard/status views.

Phase 5 should not pull relay actuation, schedule editing, OTA workflows, HTTPS/public exposure, or multi-user account behavior into this phase.

</focus>

<findings>
## Key Findings

### Zephyr's in-tree HTTP server is the right base, but it is not zero-configuration

- The local NCS already includes Zephyr's HTTP server with static and dynamic resource support, header capture, and `http_server_start()`, which is a better fit than hand-rolling request parsing on raw sockets.
- This matches the current app shape well: `app/prj.conf` already enables networking, TCP, sockets, and entropy; `app/src/panel/panel_http.h` and `app/src/panel/panel_auth.h` already reserve the Phase 5 subsystem boundaries.
- The main integration cost is build wiring, not transport invention. The app will need panel sources added to `app/CMakeLists.txt` plus a dedicated HTTP resource linker section file for the panel service.
- Phase 5 should stay on plain HTTP/1.1 over the trusted local LAN. The SDK does support HTTPS and browser-oriented HTTP/2 paths, but that would add certificate, ALPN, and browser trust work that the roadmap explicitly does not require here.
- `CONFIG_HTTP_SERVER` is still experimental in Zephyr, but this repo already sets `CONFIG_WARN_EXPERIMENTAL=n` in `app/prj.conf`, so enabling it does not introduce a new warning-management problem.

### Build-time gzip assets fit this repo better than filesystem-backed assets

- The best match for the repo is the Zephyr sample pattern: keep authored `index.html` and `main.js` as normal source assets, gzip them at build time with `generate_inc_file_for_target(... --gzip)`, and expose them as `HTTP_RESOURCE_TYPE_STATIC` resources.
- This satisfies the project requirement to ship real HTML/JS assets rather than assembling markup as C string fragments.
- Static filesystem resources are available in the SDK, but the current app does not enable `FILE_SYSTEM`, define a mounted web volume, or carry the extra flash/layout plumbing that approach would need.
- Phase 5 only needs a small shell page and one script, so exact static resources are simpler and lower-risk than wildcard-heavy routing or filesystem-backed serving.
- Tailwind Play CDN fits this model because the device still serves authored local HTML/JS. However, the page flow must remain functional if the CDN fails to load while the network state is `NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED`.

### The SDK does not provide app auth/session middleware, so `panel_auth` must own it

- Zephyr's HTTP server provides request/response callbacks and header capture, but not login/session semantics.
- That matches the current repo structure: `panel_auth` is already reserved as a dedicated subsystem boundary, while credentials already live in `app_context.persisted_config.auth` through the Phase 4 persistence layer.
- Phase 5 should validate the single operator account directly against `app_context.persisted_config.auth` rather than inventing a second credential source.
- Session state should remain RAM-only. That is the cleanest way to keep the operator signed in across refresh/navigation while forcing re-authentication after firmware reboot.
- Existing active sessions should remain usable even if new login attempts are briefly cooled down after repeated wrong passwords.

### The locked session-flow decisions map better to cookie-backed RAM sessions than to Basic auth or client-stored tokens

- A server-issued session cookie cleanly supports the desired behavior: successful login persists until logout, refresh keeps working, reboot invalidates sessions, and multiple browsers can stay signed in concurrently.
- HTTP Basic auth is a poor fit for this phase because explicit logout and reboot-bounded invalidation are awkward once the browser caches credentials.
- A client-side bearer token in `localStorage` is also a poor fit because the device, not the browser, is supposed to remain the source of truth for session validity.
- The right Phase 5 model is a small fixed-size in-RAM session table with opaque random tokens generated from Zephyr entropy and issued as `Set-Cookie` responses.
- Repeated wrong-password attempts only need a short cooldown in `panel_auth_service`; this phase does not need account lockout, credential rotation, or takeover semantics.

### There are concrete HTTP-server constraints that should shape the route design

- Cookie-based auth requires `CONFIG_HTTP_SERVER_CAPTURE_HEADERS` and explicit capture of the `cookie` header.
- Exact path resources are sufficient for this phase: `/`, `/main.js`, `/api/auth/login`, `/api/auth/logout`, `/api/auth/session`, and `/api/status` cover the required scope.
- Wildcard resources, websocket support, compression negotiation for filesystem assets, and HTTPS service variants all add complexity without helping the Phase 5 success criteria.
- Dynamic resource handlers are single-holder while a request is in flight, so Phase 5 handlers should stay short, stateless, and snapshot-based. This argues for one-shot JSON responses and light polling, not long-polling or streaming.
- The current default HTTP header limits are small, so Phase 5 should either keep the cookie compact or increase the relevant HTTP header sizing knobs deliberately rather than discovering the issue late in manual testing.

### Most operator-visible status data already exists in the current firmware

- `network_supervisor_get_status()` already exposes the operator-facing network contract that Phase 5 should reuse instead of reaching into raw `network_runtime_state` flags directly.
- `recovery_manager_last_reset_cause()` already provides meaningful device/recovery state for the dashboard.
- `persisted_config` already gives relay reboot policy, last desired relay state, action count, and schedule count, which is enough to power the initial read-only cards without implementing control behavior.
- The OTA area does not have real runtime status yet, so the Phase 5 dashboard should show an explicit placeholder rather than fake update state.
- Because Phase 2 explicitly allows boot to continue in `NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED`, the local panel must remain useful without upstream internet. Styling may degrade if Tailwind CDN is unreachable, but login and status flow must still work.

### A few repo-level follow-ups are part of the real Phase 5 cost

- `app/prj.conf` will need HTTP/JSON settings and likely more socket/context headroom than the current `CONFIG_NET_MAX_CONTEXTS=6` leaves available.
- `app/Kconfig` should grow a small panel config surface instead of hard-coding port, session count, or cooldown numbers in C.
- `app/wifi.secrets.conf.example` currently documents Wi-Fi settings only, even though persistence/auth already depend on `CONFIG_APP_ADMIN_USERNAME` and `CONFIG_APP_ADMIN_PASSWORD`; Phase 5 should correct that.
- `scripts/validate.sh` and the checklist helpers in `scripts/common.sh` should be updated so Phase 5 keeps the established "build-first automated gate, explicit manual hardware/browser gate" pattern.

</findings>

<implementation_shape>
## Recommended Implementation Shape

### Keep panel ownership inside the existing subsystem boundaries

- Expand `app/src/panel/` rather than introducing a second web stack elsewhere.
- Recommended files:
  - `app/src/panel/panel_http.c`
  - `app/src/panel/panel_auth.c`
  - `app/src/panel/panel_status.c`
  - `app/src/panel/panel_status.h`
  - `app/src/panel/assets/index.html`
  - `app/src/panel/assets/main.js`
  - `app/sections-rom.ld`
- `panel_http.c` should own HTTP service/resource registration and the thin request callbacks that bridge into auth/status helpers.
- `panel_auth.c` should own credential validation, cooldown bookkeeping, cookie parsing/issuing, session lookup, and logout invalidation.
- `panel_status.c` should translate current app state into a stable read-only snapshot so HTTP callbacks stay small and future phases can extend status shaping without reworking routing.

### Extend app-level composition instead of inventing new globals

- Add `struct panel_auth_service panel_auth;` and `struct panel_http_server panel_http;` to `struct app_context` so panel state follows the same by-value top-level ownership model established in earlier phases.
- Extend `struct app_config` with a `panel` subsection, for example:
  - `port`
  - `max_sessions`
  - `login_failure_limit`
  - `login_cooldown_ms`
- Load those values in `app/src/app/bootstrap.c` alongside the existing Wi-Fi, reachability, recovery, and persistence config loading.
- Initialize `panel_auth` after persistence is loaded so auth always sees the active `persisted_config.auth` snapshot.
- Start the HTTP panel before `app_boot()` returns, but do not gate it on full upstream health; local panel access should remain valid in the `LAN_UP_UPSTREAM_DEGRADED` state.

### Use one public shell page plus protected JSON endpoints

- Public static resources:
  - `/` -> gzip-compressed `index.html`
  - `/main.js` -> gzip-compressed `main.js`
- Public dynamic endpoint:
  - `POST /api/auth/login`
- Protected dynamic endpoints:
  - `POST /api/auth/logout`
  - `GET /api/auth/session`
  - `GET /api/status`
- This keeps the route surface intentionally small and avoids wildcard matching.
- The HTML shell can remain public because it contains no embedded protected data. It should render a login state until `/api/auth/session` confirms an active session.
- All protected operator data stays behind the cookie-guarded JSON API.

### Prefer one aggregate status API over many micro-endpoints

- Phase 5 only needs read-only status views, and the current firmware state already lives under one `app_context`.
- A single `GET /api/status` response is simpler to secure, simpler to render from one dashboard fetch, and simpler to extend in Phases 6-8.
- Suggested payload sections:
  - `device`
    - board name
    - uptime
    - last recovery reset cause if available
    - persistence load-report summary or warnings
  - `network`
    - connectivity state
    - Wi-Fi ready/connected flags
    - IPv4 bound state
    - leased IPv4
    - reachability result
    - last failure stage/reason
  - `relay`
    - last desired state
    - reboot policy
    - `implemented: false`
  - `scheduler`
    - saved schedule count
    - enabled schedule count if the phase chooses to compute it
    - `implemented: false`
  - `update`
    - `implemented: false`
    - placeholder message for Phase 8
- This keeps Phase 5 read-only and prevents early API sprawl.

### Implement sessions as short, opaque, RAM-only records

- `panel_auth_service` should maintain a small fixed-size session table protected by a `k_mutex`.
- Each session record only needs:
  - token
  - created-at timestamp
  - last-seen timestamp if the implementation wants it
  - in-use flag
- Generate tokens with `sys_csrand_get()` and encode them into a short printable cookie value.
- Keep the cookie name short, such as `sid`, so request headers stay comfortably inside HTTP server limits.
- On successful login:
  - validate against `persisted_config.auth`
  - allocate a new session slot without evicting other active sessions
  - send `Set-Cookie: sid=...; Path=/; HttpOnly; SameSite=Strict`
- On logout:
  - remove the matching session
  - send an expired cookie
- On unauthorized protected request:
  - return `401`
  - clear any stale `sid` cookie so the browser recovers cleanly after reboot
- Because the product decision is local HTTP only, Phase 5 should not rely on TLS-only browser features such as `Secure` cookies yet. The important safeguards here are server-owned RAM sessions, `HttpOnly`, and `SameSite=Strict`.

### Keep request/response handling intentionally small

- Login should accept a small JSON body and buffer request fragments until `HTTP_SERVER_DATA_FINAL` before parsing.
- Status GET should build a snapshot and return immediately; no streaming or background-held request state.
- Logout should be a tiny protected POST with no complex body contract.
- Future relay/schedule/update routes can reuse the same auth gate and response helpers in later phases.
- Avoid shared long-lived handler state that would make the dynamic-resource single-holder behavior visible to normal multi-browser use.

### Concrete build and configuration recommendations

- `app/CMakeLists.txt`
  - add `panel_http.c`, `panel_auth.c`, and `panel_status.c`
  - add `generate_inc_file_for_target(... --gzip)` calls for `index.html` and `main.js`
  - add `zephyr_linker_sources(SECTIONS sections-rom.ld)`
  - add a `zephyr_linker_section(NAME http_resource_desc_panel_http_service ...)` entry for the panel service
- `app/sections-rom.ld`
  - define `ITERABLE_SECTION_ROM(http_resource_desc_panel_http_service, Z_LINK_ITERABLE_SUBALIGN)`
- `app/Kconfig`
  - `APP_PANEL_PORT` (default `80`)
  - `APP_PANEL_MAX_SESSIONS`
  - `APP_PANEL_LOGIN_FAILURE_LIMIT`
  - `APP_PANEL_LOGIN_COOLDOWN_MS`
- `app/prj.conf`
  - enable `CONFIG_HTTP_SERVER=y`
  - enable `CONFIG_HTTP_SERVER_CAPTURE_HEADERS=y`
  - enable `CONFIG_JSON_LIBRARY=y`
  - size `CONFIG_HTTP_SERVER_MAX_CLIENTS`, `CONFIG_HTTP_SERVER_CLIENT_BUFFER_SIZE`, and `CONFIG_HTTP_SERVER_MAX_HEADER_LEN` conservatively for a small local panel
  - increase socket/context headroom, especially `CONFIG_NET_MAX_CONTEXTS`, and likely add an explicit `CONFIG_NET_MAX_CONN`
  - keep websocket, static-FS, and TLS options off for this phase
- `app/wifi.secrets.conf.example`
  - add committed placeholders for `CONFIG_APP_ADMIN_USERNAME` and `CONFIG_APP_ADMIN_PASSWORD`

</implementation_shape>

<validation>
## Validation Architecture

### Automated validation should remain build-first

- The repo still has no first-party HTTP or browser test harness, so Phase 5 should keep `./scripts/validate.sh` as the canonical automated entrypoint.
- That automated path should remain build-focused rather than inventing a large new test framework for this phase.
- Good automated/static verification targets after implementation are:
  1. `./scripts/validate.sh`
  2. `rg -n "panel_http|panel_auth|panel_status" app/CMakeLists.txt app/src/app/app_context.h app/src/app/bootstrap.c app/src/panel`
  3. `rg -n "generate_inc_file_for_target|zephyr_linker_sources|http_resource_desc_panel_http_service" app/CMakeLists.txt app/sections-rom.ld`
  4. `rg -n "CONFIG_HTTP_SERVER|CONFIG_HTTP_SERVER_CAPTURE_HEADERS|CONFIG_JSON_LIBRARY|APP_PANEL_" app/prj.conf app/Kconfig`
- If the implementation introduces small pure helpers for cookie parsing or cooldown math, lightweight deterministic tests are acceptable, but Phase 5 should not block on establishing a full new firmware test harness.

### Blocking manual sign-off should be device + browser/curl based

- Manual validation should stay in the same real-device flow used by earlier phases: flash the board, observe the console, determine the device IP, then verify the panel from a browser and a simple HTTP client on the same LAN.
- Required Phase 5 scenarios are:
  1. unauthenticated `GET /api/status` is denied
  2. correct credentials can log in successfully
  3. refresh/navigation keep the operator signed in
  4. logout immediately revokes access to protected endpoints
  5. repeated wrong-password attempts trigger the short cooldown and then recover
  6. two browsers can stay signed in concurrently
  7. reboot invalidates the session and requires a fresh login
  8. the panel still presents local status in the `LAN_UP_UPSTREAM_DEGRADED` case even if Tailwind CDN styling is reduced
  9. relay, scheduler, and update cards remain clearly read-only or placeholder-only surfaces in this phase
- Browser validation remains necessary because this phase is explicitly about session/navigation behavior and dashboard rendering, not just API correctness.

### Validation and docs updates should follow the established repo pattern

- `scripts/common.sh` should gain Phase 5 marker and checklist printers so the phase sign-off structure matches Phase 4.
- `scripts/validate.sh` should be updated to describe the manual panel sign-off steps after the automated build path succeeds.
- `app/wifi.secrets.conf.example` or equivalent setup docs should make the admin credential requirement explicit so login validation is reproducible.
- Useful manual probes after flash are:
  - `curl -i http://<device-ip>/api/status`
  - `curl -i -c cookie.txt -H 'Content-Type: application/json' -d '{"username":"...","password":"..."}' http://<device-ip>/api/auth/login`
  - `curl -i -b cookie.txt http://<device-ip>/api/status`

</validation>

<planning_implications>
## Planning Implications

- The roadmap's three-plan split is correct and should be preserved.
- `05-01` should own the HTTP service shell, linker/CMake wiring, Kconfig/prj.conf additions, and the authored asset pipeline. It should stop once the device can serve the public shell page and local JS asset reliably.
- `05-02` should own `panel_auth` and protected API routing: login/logout/session endpoints, RAM session table, cooldown behavior, and the protected aggregate status API.
- `05-03` should own the frontend dashboard and status views plus validation/docs: login UX, session bootstrap on refresh, read-only cards, Phase 6-8 placeholders, and the `scripts/validate.sh` / `scripts/common.sh` updates needed for sign-off.
- Dependency order should stay strict:
  - `05-02` depends on `05-01`
  - `05-03` depends on `05-01` and `05-02`
- To prevent scope creep, Phase 5 should deliberately leave these out:
  - relay actuation endpoints
  - schedule create/edit/delete flows
  - OTA upload or remote-pull flows
  - HTTPS/public exposure
  - multi-user auth, password rotation, or persistent remember-me behavior
- If implementation pressure appears, keep the protected API surface to one `GET /api/status` plus the minimal auth trio. That is still enough to satisfy `AUTH-01`, `AUTH-02`, `AUTH-03`, `PANEL-01`, `PANEL-02`, and `PANEL-03` while giving later phases a clean base.
- The admin-credential example update belongs in this phase rather than later because Phase 5 is the first time those persisted credentials become an operator-facing runtime requirement.

</planning_implications>

---
*Phase: 05-local-control-panel*
*Research completed: 2026-03-09*
