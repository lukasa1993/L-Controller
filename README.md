# LNH Nordic

Generic controller system for `nRF7002-DK`.

## Phase 7 scheduling validation workflow

Phase 7 keeps normal iteration build-first, then blocks phase approval on a separate browser, curl, and device checkpoint. The automated path proves the tree still builds cleanly; the manual checkpoint proves trusted-time gating, schedule lifecycle management, reboot semantics, and live scheduled execution on a real device.

### Automated validation remains build-first

Run the repo-owned validation entrypoint during normal development:

```sh
./scripts/validate.sh
```

- Delegates to `./scripts/build.sh`, which remains the canonical automated build signal.
- Serves as the canonical wave-level automated validation command before the blocking browser, curl, and device checkpoint.
- Stays safe for frequent refactor loops because it does **not** require hardware interaction by default.
- Supports `./scripts/validate.sh --preflight` when you want `./scripts/doctor.sh` to check host tools and connected hardware first.

### Admin credentials come from the local secrets overlay

Phase 7 scheduling still depends on the Phase 5 local auth flow, which validates against the persisted single-user credential source and reseeds from the build-time config when needed. Copy the example secrets file before building:

```sh
cp app/wifi.secrets.conf.example app/wifi.secrets.conf
```

Set these values for a reproducible local panel login flow:

- `CONFIG_APP_ADMIN_USERNAME`
- `CONFIG_APP_ADMIN_PASSWORD`

The same credentials are used for browser login and the `curl` verification flow.

### Browser, curl, and device validation is the blocking gate

Automated validation is necessary but not sufficient for Phase 7 completion. Final sign-off stays manual on real hardware and a browser on the same LAN:

```sh
./scripts/flash.sh
./scripts/console.sh
```

Use the device IP from the boot log for these baseline `curl` checks:

```sh
curl -i http://<device-ip>/api/status
curl -i -c cookie.txt -H 'Content-Type: application/json' -d '{"username":"...","password":"..."}' http://<device-ip>/api/auth/login
curl -i -b cookie.txt http://<device-ip>/api/schedules
curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{"scheduleId":"weekday-open","cronExpression":"*/15 6-18 * * 1-5","actionKey":"relay-on","enabled":true}' http://<device-ip>/api/schedules/create
curl -i -b cookie.txt http://<device-ip>/api/schedules
curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{"scheduleId":"weekday-open","cronExpression":"0 7 * * 1-5","actionKey":"relay-on","enabled":true}' http://<device-ip>/api/schedules/update
curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{"scheduleId":"weekday-open","enabled":false}' http://<device-ip>/api/schedules/set-enabled
curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{"scheduleId":"weekday-open","enabled":true}' http://<device-ip>/api/schedules/set-enabled
curl -i -b cookie.txt -H 'Content-Type: application/json' -d '{"scheduleId":"weekday-open"}' http://<device-ip>/api/schedules/delete
```

Record these Phase 7 outcomes during the browser, curl, and device run:

- `untrusted scheduler gating`: scheduling stays inactive until the device trusts UTC time.
- `initial sync reset then degrade`: one failed initial time-sync attempt causes one recovery reset, then scheduling comes back degraded instead of reboot-looping.
- `schedule create`, `schedule edit`, `schedule enable`, `schedule disable`, `schedule delete`: the operator can manage the full schedule lifecycle through the panel or exact routes.
- `manual versus scheduled interaction`: a manual relay command does not disarm the next scheduled run.
- `scheduled execution parity`: the physical relay, `GET /api/status`, and scheduler surface agree on the scheduled action outcome.
- `reboot persistence with list re-check`: saved scheduler state survives reboot and reappears in `GET /api/schedules`.
- `missed job skip after trust restore`: missed runs are skipped rather than replayed after reboot or time recovery.
- `time correction future-only recompute`: major clock correction recomputes only future runs.
- `conflict rejection`: opposite-state schedules targeting the same UTC minute are rejected before save.
- `recent problem visibility`: degraded-time or rejected-save scenarios remain visible in recent scheduler problems.

Confirm the healthy-path device log reaches these ready-state markers:

- `Wi-Fi connected`
- `DHCP IPv4 address: ...`
- `Reachability check passed`
- `APP_READY`
- `Relay startup actual=... desired=... source=... reboot=... note=...`
- `Scheduler runtime ready schedules=... enabled=... automation=... clock=... degraded=... UTC cadence=... timeout=...ms`
- `Scheduler recent problems=... clock=... degraded=...`

Use this blocking device checklist before Phase 7 approval:

1. Re-run `./scripts/validate.sh` and keep the automated `./scripts/build.sh` path green.
2. Flash the latest image with `./scripts/flash.sh`.
3. Open the live device log with `./scripts/console.sh`, note the device IP, and confirm the ready-state, relay, and scheduler markers appear.
4. Before trusted time is available, confirm the scheduler reports untrusted or syncing state while manual relay control still remains available.
5. Deliberately exercise one failed initial trusted-time acquisition path and confirm the device performs one recovery reset, then comes back with scheduling inactive and visibly degraded instead of reboot-looping.
6. Log in through the panel or curl and confirm `GET /api/schedules` returns action choices, runtime summary, recent problems, and the current schedule list without exposing raw internal action IDs.
7. Create a near-future schedule and confirm the next run appears immediately in the panel or `GET /api/schedules` output.
8. Edit that schedule, then disable and re-enable it, and confirm each mutation is reflected in the list and next-run display.
9. Delete one schedule and confirm it disappears cleanly without disturbing unrelated schedules.
10. Create a near-future relay schedule, issue a manual relay command before the due minute, and confirm the next scheduled run still executes normally.
11. Wait for the due minute and confirm the physical relay, `GET /api/status`, and scheduler last-result or next-run fields all agree that execution came from the scheduler path.
12. Reboot the device before one scheduled minute passes, then reopen the scheduler view or `GET /api/schedules` and confirm edited, disabled, enabled, and deleted schedule state is still correct.
13. Restore trusted time after that reboot and confirm the missed run is skipped rather than replayed automatically.
14. Force or simulate a major clock correction and confirm the scheduler recomputes only future runs without backfilling earlier missed minutes.
15. Attempt to save two enabled schedules that command opposite relay states in the same minute and confirm the save is rejected with operator-visible feedback.
16. Confirm recent scheduler problems remain visible after rejected saves, untrusted time, or degraded-time scenarios.
17. Record which scheduler gating, reset-then-degrade, create, edit, enable, disable, delete, manual-versus-scheduled, scheduled execution, reboot persistence, missed-job skip, time-correction recompute, conflict rejection, and recent-problem visibility scenarios passed or failed.

If the browser, curl, or device run fails, record the exact scenario and last visible failure instead of treating the phase as complete.

## Supporting scripts

- `./scripts/validate.sh` is the canonical automated validation entrypoint for Phase 7.
- `./scripts/doctor.sh` checks host prerequisites and board visibility before build-heavy or hardware-bound work.
- `./scripts/build.sh` performs the canonical firmware build.
- `./scripts/common.sh` centralizes Phase 4 through Phase 7 markers, curl commands, and blocking device checklists.
- `./scripts/flash.sh` flashes the latest build to the device.
- `./scripts/console.sh` opens the serial console used for final scheduler and relay verification.
