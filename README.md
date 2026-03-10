# LNH Nordic

Generic controller system for `nRF7002-DK`.

## Phase 6 relay control validation workflow

Phase 6 keeps normal iteration build-first, then blocks phase approval on a separate browser/curl/device checkpoint. The automated path proves the tree still builds cleanly; the manual checkpoint proves live relay control and safe-state behavior on a real device.

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

Phase 6 relay control still depends on the Phase 5 local auth flow, which validates against the persisted single-user credential source and reseeds from the build-time config when needed. Copy the example secrets file before building:

```sh
cp app/wifi.secrets.conf.example app/wifi.secrets.conf
```

Set these values for a reproducible local panel login flow:

- `CONFIG_APP_ADMIN_USERNAME`
- `CONFIG_APP_ADMIN_PASSWORD`

The same credentials are used for browser login and the `curl` verification flow.

### Browser, curl, and device validation is the blocking gate

Automated validation is necessary but not sufficient for Phase 6 completion. Final sign-off stays manual on real hardware and a browser on the same LAN:

```sh
./scripts/flash.sh
./scripts/console.sh
```

Use the device IP from the boot log for these baseline `curl` checks:

```sh
curl -i http://<device-ip>/api/status
curl -i -c cookie.txt -H 'Content-Type: application/json' -d '{"username":"...","password":"..."}' http://<device-ip>/api/auth/login
curl -i -b cookie.txt http://<device-ip>/api/status
curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{"desiredState":true}' http://<device-ip>/api/relay/desired-state
curl -i -b cookie.txt http://<device-ip>/api/status
curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{"desiredState":false}' http://<device-ip>/api/relay/desired-state
curl -i -b cookie.txt http://<device-ip>/api/status
```

Record these Phase 6 outcomes during the browser/curl/device run:

- `relay on`: the relay turns on through the authenticated route and the physical relay, `GET /api/status`, and the panel agree on actual on plus desired on.
- `relay off`: the relay turns off through the authenticated route and the physical relay, `GET /api/status`, and the panel agree on actual off plus desired off.
- `curl parity`: the Phase 6 relay `curl` flow matches what the browser control reports.
- `degraded local control`: relay control still works while the device is locally reachable in `LAN_UP_UPSTREAM_DEGRADED`.
- `normal reboot policy`: an ordinary reboot applies the configured normal-boot relay policy.
- `recovery-forced relay off`: a recovery-triggered reboot forces the relay off until a fresh command is issued.
- `actual-versus-desired mismatch visibility`: the panel clearly shows actual and remembered desired state together when policy keeps them different.
- `pending or blocked feedback`: the toggle locks and explains pending, blocked, or failure cases inline without a routine confirmation dialog.

Confirm the healthy-path device log reaches these ready-state markers:

- `Wi-Fi connected`
- `DHCP IPv4 address: ...`
- `Reachability check passed`
- `APP_READY`
- `Relay startup actual=... desired=... source=... reboot=... note=...`

Use this blocking device checklist before Phase 6 approval:

1. Re-run `./scripts/validate.sh` and keep the automated `./scripts/build.sh` path green.
2. Flash the latest image with `./scripts/flash.sh`.
3. Open the live device log with `./scripts/console.sh`, note the device IP, and confirm the ready-state markers plus the relay startup log appear.
4. Log in with the configured admin credentials and confirm protected access still works before testing relay control.
5. Use the Phase 6 relay `curl` route to drive the relay on and confirm the physical relay, `GET /api/status`, and the live panel state all agree on actual on plus desired on.
6. Use the same relay `curl` route to drive the relay off and confirm the physical relay, `GET /api/status`, and the live panel state all agree on actual off plus desired off.
7. Open the panel in a browser, toggle the relay on and off, and confirm the control is one tap, uses no routine confirmation dialog, and shows inline pending or failure feedback while locked.
8. Exercise the local-LAN degraded path, confirm `LAN_UP_UPSTREAM_DEGRADED`, and verify authenticated relay control still works while the device remains locally reachable.
9. Reboot the device normally and confirm the relay plus status payload follow the configured normal-boot policy.
10. Trigger the recovery-reset or equivalent confirmed-fault path and confirm the next boot forces the relay off until a fresh command is issued.
11. Confirm the panel makes actual versus remembered desired state visible when safety policy keeps the applied relay off and shows the relevant safety note.
12. Record which relay on, relay off, curl parity, degraded-local control, normal reboot policy, recovery-forced off, mismatch visibility, and pending or blocked feedback scenarios passed or failed.

If the browser/curl/device run fails, record the exact scenario and last visible failure instead of treating the phase as complete.

## Supporting scripts

- `./scripts/validate.sh` is the canonical automated validation entrypoint for Phase 6.
- `./scripts/doctor.sh` checks host prerequisites and board visibility before build-heavy or hardware-bound work.
- `./scripts/build.sh` performs the canonical firmware build.
- `./scripts/common.sh` centralizes Phase 4, Phase 5, and Phase 6 markers, curl commands, and blocking device checklists.
- `./scripts/flash.sh` flashes the latest build to the device.
- `./scripts/console.sh` opens the serial console used for final relay-control verification.
