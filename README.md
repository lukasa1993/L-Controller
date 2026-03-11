# LNH Nordic

Generic controller system for `nRF7002-DK`.

## Phase 8 OTA lifecycle validation workflow

Phase 8 keeps normal iteration build-first, then blocks final approval on a browser, curl, and device checkpoint that proves the full OTA lifecycle on real hardware. The automated path proves the sysbuild tree still produces MCUboot artifacts cleanly; the manual checkpoint proves local upload staging, explicit apply, post-boot confirmation, rollback visibility, GitHub remote pull, and daily retry behavior.

### Automated validation remains build-first

Run the repo-owned validation entrypoint during normal development:

```sh
./scripts/validate.sh
```

- Delegates to `./scripts/build.sh`, which remains the canonical automated sysbuild signal.
- Stays safe for frequent refactor loops because it does **not** require hardware interaction by default.
- Supports `./scripts/validate.sh --preflight` when you want `./scripts/doctor.sh` to check host tools and connected hardware first.

### Ad hoc next-version OTA builds come from `app/VERSION`

The repo now derives the default MCUboot signing version from [app/VERSION](/Users/l/_DEV/LNH-Nordic/app/VERSION). For repeated local OTA tests from nearly identical code, increment `VERSION_TWEAK` before rebuilding so the generated `zephyr.signed.bin` stays strictly newer than the image already running on the device.

### Admin credentials still come from the local secrets overlay

Phase 8 OTA still depends on the Phase 5 local auth flow. Copy the example secrets file before building:

```sh
cp app/wifi.secrets.conf.example app/wifi.secrets.conf
```

Set these values for a reproducible local panel login flow:

- `CONFIG_APP_ADMIN_USERNAME`
- `CONFIG_APP_ADMIN_PASSWORD`

The same credentials are used for browser login and the `curl` verification flow.

### Playwright login smoke

The repo now includes a Playwright smoke for the dedicated `/login` flow. It is intended for the flashed device only: it starts at `/`, confirms the unauthenticated redirect to `/login`, submits the real panel login form, and only passes once the authenticated shell renders the configured-action management surface plus the protected overview, schedules, and OTA views.

Install the browser-test dependencies once:

```sh
npm install
npx playwright install chromium
```

For a flashed device on the local LAN, export the same credentials that are compiled into `app/wifi.secrets.conf`:

```sh
export LNH_PANEL_USERNAME='...'
export LNH_PANEL_PASSWORD='...'
```

Then build, flash, optionally preview the discovered URL, and run the smoke:

```sh
./scripts/build.sh
./scripts/flash.sh
node tests/helpers/panel-target.js
LNH_PANEL_BASE_URL=auto npx playwright test tests/panel-login.spec.js
```

Auto-discovery probes the active private `/24` subnet on the panel HTTP port and looks for the embedded panel shell markers. Override it when needed with:

- `LNH_PANEL_BASE_URL=http://<device-ip>`
- `LNH_PANEL_SUBNET=192.168.1`
- `LNH_PANEL_PORT=8080`

### Browser, curl, and device validation is the blocking gate

Automated validation is necessary but not sufficient for Phase 8 completion. Final sign-off stays manual on real hardware and a browser on the same LAN:

```sh
./scripts/flash.sh
./scripts/console.sh
```

Use the device IP from the boot log for these baseline OTA checks:

```sh
curl -i http://<device-ip>/api/status
curl -i -c cookie.txt -H 'Content-Type: application/json' -d '{"username":"...","password":"..."}' http://<device-ip>/api/auth/login
curl -i -b cookie.txt http://<device-ip>/api/update
curl -i -b cookie.txt -H 'Content-Type: application/octet-stream' --data-binary @build/nrf7002dk_nrf5340_cpuapp/app/zephyr/zephyr.signed.bin http://<device-ip>/api/update/upload
curl -i -b cookie.txt -X POST http://<device-ip>/api/update/apply
curl -i -b cookie.txt -X POST http://<device-ip>/api/update/clear
curl -i -b cookie.txt -X POST http://<device-ip>/api/update/remote-check
curl -i -b cookie.txt http://<device-ip>/api/status
```

Record these Phase 8 outcomes during the browser, curl, and device run:

- `local upload staging`: a newer signed image stages successfully without immediate reboot.
- `same-version rejection`: the device blocks reinstalling the running image.
- `downgrade rejection`: the device blocks older signed images before they become apply-ready.
- `explicit apply reboot`: apply reboots the device only after the operator triggers it.
- `post-boot confirmation`: the new image reaches `APP_READY`, survives the stable window, and only then becomes permanent.
- `rollback visibility`: a failed-to-confirm image reverts and the next boot shows rollback truthfully.
- `github update now`: the explicit `Update now` path checks GitHub Releases and feeds the same OTA pipeline.
- `daily retry`: a failed GitHub cycle retries on a later automatic check instead of pausing indefinitely.
- `panel truthfulness`: current version, staged or last-attempted version, last result, rollback flags, and pending warnings stay internally consistent before and after reboot.

Confirm the healthy-path device log reaches these ready-state markers:

- `Wi-Fi connected`
- `DHCP IPv4 address: ...`
- `Reachability check passed`
- `APP_READY`
- `Relay startup actual=... desired=... source=... reboot=... note=...`
- `Scheduler runtime ready schedules=... enabled=... automation=... clock=... degraded=... UTC cadence=... timeout=...ms`
- `OTA runtime ready current=... confirmed=... state=... remote=lukasa1993/L-Controller every=...h`

Use this blocking device checklist before Phase 8 approval:

1. Re-run `./scripts/validate.sh` and keep the automated `./scripts/build.sh` path green.
2. Flash the latest image with `./scripts/flash.sh`.
3. Open the live device log with `./scripts/console.sh`, note the device IP, and confirm the ready-state, relay, scheduler, and OTA markers appear.
4. Log in through the dedicated `/login` route and confirm the dashboard update surface shows current version, staged version truth, last result, and pending warnings before testing.
5. Upload a newer signed firmware image locally and confirm it stages successfully without rebooting until the explicit apply action is used.
6. Attempt one same-version upload and one older-version upload, and confirm both are rejected with clear operator feedback and no unsafe staged image remains.
7. Trigger apply for the newer staged image, confirm the browser disconnects during reboot, and confirm a fresh login is required after the device returns.
8. After reboot, confirm the new firmware reaches `APP_READY`, remains healthy through the stable window, and only then becomes confirmed.
9. Exercise one rollback path by preventing confirmation or using a known-bad image, then confirm the device returns to the previous firmware and the next boot surfaces rollback or failed-attempt truth clearly.
10. Use the explicit `Update now` action while upstream internet is available and confirm the device checks GitHub Releases, selects the latest stable eligible artifact, and stages or applies it through the same safety rules as local upload.
11. Force or simulate one failed GitHub update cycle, then allow the next daily check window and confirm the device retries instead of pausing remote updates permanently.
12. Refresh the panel or API before staging, after staging, after apply, and after confirmation or rollback, and confirm the OTA fields remain internally consistent and operator-meaningful.
13. Record which local upload staging, same-version rejection, downgrade rejection, explicit apply reboot, post-boot confirmation, rollback visibility, GitHub update-now, daily retry, and panel truthfulness scenarios passed or failed.

If the browser, curl, or device run fails, record the exact scenario and last visible failure instead of treating the phase as complete.

## Supporting scripts

- `./scripts/validate.sh` is the canonical automated validation entrypoint for Phase 8.
- `./scripts/doctor.sh` checks host prerequisites and board visibility before build-heavy or hardware-bound work.
- `./scripts/build.sh` performs the canonical firmware build.
- `./scripts/common.sh` centralizes Phase 4 through Phase 8 markers, curl commands, and blocking device checklists.
- `./scripts/flash.sh` flashes the latest build to the device.
- `./scripts/console.sh` opens the serial console used for final OTA verification.
