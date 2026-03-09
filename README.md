# LNH Nordic

Generic controller system for `nRF7002-DK`.

## Phase 5 local panel validation workflow

Phase 5 keeps normal iteration build-first, then blocks phase approval on a separate browser/curl/device checkpoint. The automated path proves the tree still builds cleanly; the manual checkpoint proves the local auth flow, session behavior, and read-only dashboard all behave correctly on a real device.

### Automated validation remains build-first

Run the repo-owned validation entrypoint during normal development:

```sh
./scripts/validate.sh
```

- Delegates to `./scripts/build.sh`, which remains the canonical automated build signal.
- Serves as the canonical wave-level automated validation command before the blocking browser/curl/device checkpoint.
- Stays safe for frequent refactor loops because it does **not** require hardware interaction by default.
- Supports `./scripts/validate.sh --preflight` when you want `./scripts/doctor.sh` to check host tools and connected hardware first.

### Admin credentials come from the local secrets overlay

Phase 5 auth validates against the persisted single-user credential source, which reseeds from the build-time config when needed. Copy the example secrets file before building:

```sh
cp app/wifi.secrets.conf.example app/wifi.secrets.conf
```

Set these values for a reproducible local panel login flow:

- `CONFIG_APP_ADMIN_USERNAME`
- `CONFIG_APP_ADMIN_PASSWORD`

The same credentials are used for browser login and the `curl` verification flow.

### Browser, curl, and device validation is the blocking gate

Automated validation is necessary but not sufficient for Phase 5 completion. Final sign-off stays manual on real hardware and a browser on the same LAN:

```sh
./scripts/flash.sh
./scripts/console.sh
```

Use the device IP from the boot log for these baseline `curl` checks:

```sh
curl -i http://<device-ip>/api/status
curl -i -c cookie.txt -H 'Content-Type: application/json' -d '{"username":"...","password":"..."}' http://<device-ip>/api/auth/login
curl -i -b cookie.txt http://<device-ip>/api/status
```

Record these Phase 5 outcomes during the browser/curl/device run:

- `auth denial`: unauthenticated `GET /api/status` is denied before login.
- `login success`: a valid login returns a `sid` cookie and the protected status route becomes readable.
- `refresh and navigation persistence`: the panel remains signed in across refresh and in-panel navigation until logout.
- `logout revocation`: logout clears the browser cookie and protected routes become unavailable again.
- `cooldown recovery`: repeated wrong-password attempts trigger the cooldown and then recover.
- `concurrent sessions`: two browsers or browser contexts can stay signed in independently.
- `reboot invalidates session`: after a reboot, the prior browser session can no longer access protected routes until a fresh login occurs.
- `degraded styling resilience`: local status still renders during `LAN_UP_UPSTREAM_DEGRADED` conditions even if Tailwind CDN styling is reduced.
- `read-only relay, scheduler, and update surfaces`: the dashboard shows status or placeholders only, with no mutating controls in Phase 5.

Confirm the healthy-path device log reaches these ready-state markers:

- `Wi-Fi connected`
- `DHCP IPv4 address: ...`
- `Reachability check passed`
- `APP_READY`

Use this blocking device checklist before Phase 5 approval:

1. Re-run `./scripts/validate.sh` and keep the automated `./scripts/build.sh` path green.
2. Flash the latest image with `./scripts/flash.sh`.
3. Open the live device log with `./scripts/console.sh` and note the device IP.
4. Verify unauthenticated access is denied with `curl -i http://<device-ip>/api/status`.
5. Log in with the configured admin credentials, confirm a `sid` cookie is returned, and verify `curl -i -b cookie.txt http://<device-ip>/api/status` succeeds.
6. Open the panel in a browser, log in, refresh, and navigate within the Phase 5 dashboard to confirm the session remains active until logout.
7. Trigger repeated wrong-password attempts until the cooldown appears, wait for the cooldown window to expire, and confirm a valid login succeeds again.
8. Log in from a second browser or private/incognito window and confirm concurrent sessions remain usable without evicting each other.
9. Reboot the device and confirm the old browser session can no longer reach protected routes until a fresh login occurs.
10. Inspect the dashboard during normal and degraded-local conditions and confirm local status still renders even if Tailwind CDN styling is reduced.
11. Confirm relay, scheduler, and update sections remain read-only or placeholder-only in this phase.
12. Record which auth, session, cooldown, concurrent-session, reboot, degraded-styling, and read-only-surface scenarios passed or failed.

If the browser/curl/device run fails, record the exact scenario and last visible failure instead of treating the phase as complete.

## Supporting scripts

- `./scripts/validate.sh` is the canonical automated validation entrypoint for Phase 5.
- `./scripts/doctor.sh` checks host prerequisites and board visibility before build-heavy or hardware-bound work.
- `./scripts/build.sh` performs the canonical firmware build.
- `./scripts/common.sh` centralizes Phase 4 and Phase 5 markers, curl commands, and blocking device checklists.
- `./scripts/flash.sh` flashes the latest build to the device.
- `./scripts/console.sh` opens the serial console used for final panel verification.
