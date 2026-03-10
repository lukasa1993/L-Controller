---
status: passed
verified: 2026-03-10
commit: 25cb217
---

# Quick Task 2 Verification

## Goal Check

The quick task goal was met on the real device.

- The repo contains a real-device-only Playwright login smoke.
- The smoke can resolve a flashed device URL from auto-discovery or an explicit `LNH_PANEL_BASE_URL`.
- The firmware now survives boot and authenticated panel refreshes without the stack overflows and empty-body bug that initially blocked verification.
- The Playwright smoke passed against the flashed board after build and flash.

## Evidence

1. `node --check tests/helpers/panel-target.js`
   Result: passed.
   Notes: the auto-discovery helper parses cleanly and remains CLI-runnable.

2. `node --check tests/panel-login.spec.js`
   Result: passed.
   Notes: the smoke test parses cleanly.

3. `npx playwright test --list`
   Result: passed.
   Notes: Playwright registered the Chromium smoke `logs in and lands on the authenticated dashboard`.

4. `./scripts/build.sh`
   Result: passed.
   Notes: the sysbuild flow completed after the firmware fixes. Existing non-fatal MBEDTLS Kconfig warnings still appeared.

5. `./scripts/flash.sh`
   Result: passed.
   Notes: the attached board flashed successfully through J-Link serial `1050756792`.

6. `LNH_PANEL_SUBNET=192.168.19 node tests/helpers/panel-target.js`
   Result: passed.
   Notes: discovery resolved the flashed device at `http://192.168.19.86` after the `main()` stack fix.

7. `curl -sS -i -c /tmp/lnh-cookie.txt -H 'Content-Type: application/json' -d '{"username":"admin","password":"***"}' http://192.168.19.86/api/auth/login`
   Result: passed.
   Notes: login returned `200` and issued a `sid` cookie.

8. `curl -sS -i -b /tmp/lnh-cookie.txt http://192.168.19.86/api/status`
   Result: passed.
   Notes: the route returned authenticated device, network, relay, scheduler, and update JSON after the HTTP server stack increase.

9. `curl -sS -i -b /tmp/lnh-cookie.txt http://192.168.19.86/api/schedules`
   Result: passed.
   Notes: the route now returns valid JSON instead of `200` with an empty body after fixing `panel_status_render_schedule_snapshot_json()`.

10. `LNH_PANEL_BASE_URL=http://192.168.19.86 LNH_PANEL_USERNAME=admin LNH_PANEL_PASSWORD='***' npx playwright test tests/panel-login.spec.js`
    Result: passed.
    Notes: Playwright reported `1 passed`, and the login shell transitioned to the authenticated dashboard on the flashed device.
