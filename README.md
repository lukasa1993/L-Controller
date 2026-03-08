# LNH Nordic

Generic controller system for `nRF7002-DK`.

## Phase 2 validation workflow

Phase 2 keeps fast refactor feedback build-first by default, then requires a separate device-only supervision sign-off before the phase can be considered complete.

### Automated refactor-safe validation

Run the repo-owned validation entrypoint during normal development:

```sh
./scripts/validate.sh
```

- Delegates to `./scripts/build.sh`, which remains the canonical automated build signal.
- Serves as the canonical wave-level automated validation command before any blocking device sign-off.
- Stays safe for frequent refactor loops because it does **not** require hardware or forced Wi-Fi failures by default.
- Supports `./scripts/validate.sh --preflight` when you want `./scripts/doctor.sh` to check host tools and connected hardware first.

### Device-only supervisor sign-off is the blocking gate

Automated validation is necessary but not sufficient for Phase 2 completion. Final sign-off stays manual on real hardware:

```sh
./scripts/flash.sh
./scripts/console.sh
```

Record these supervision outcomes during the device run:

- `healthy`: normal boot reaches `Network supervisor state=healthy` together with the ready-state markers below.
- `degraded-retrying`: a disrupted boot or Wi-Fi loss reaches `Network supervisor state=degraded-retrying` instead of hanging forever.
- `upstream-degraded`: Wi-Fi and DHCP stay up while the configured reachability path fails, producing `Network supervisor state=lan-up-upstream-degraded`.
- `recovery`: restoring the failed path returns the device to `Network supervisor state=healthy` and logs `Network recovered to healthy; last failure=... reason=...`.

Confirm the healthy-path device log reaches all of these ready-state markers:

- `Wi-Fi connected`
- `DHCP IPv4 address: ...`
- `Reachability check passed`
- `APP_READY`

Use this blocking device checklist before Phase 2 approval:

1. Re-run `./scripts/validate.sh` and keep the automated `./scripts/build.sh` path green.
2. Flash the latest image with `./scripts/flash.sh`.
3. Open the live device log with `./scripts/console.sh`.
4. Confirm a normal boot reaches the healthy markers above and `Network supervisor state=healthy`.
5. Break the Wi-Fi or boot dependency path and confirm `Network supervisor state=degraded-retrying` instead of an indefinite hang.
6. Keep Wi-Fi and DHCP up while breaking the configured reachability target, then confirm `Network supervisor state=lan-up-upstream-degraded`.
7. Restore the failed path and confirm `Network recovered to healthy; last failure=... reason=...` followed by `Network supervisor state=healthy`.
8. Record which healthy, degraded-retrying, upstream-degraded, and recovery scenarios passed or failed.

If the device run fails, record the exact scenario and last visible failure instead of treating the phase as complete.

## Supporting scripts

- `./scripts/validate.sh` is the canonical automated validation entrypoint for Phase 2.
- `./scripts/doctor.sh` checks host prerequisites and board visibility before build-heavy or hardware-bound work.
- `./scripts/build.sh` performs the canonical firmware build.
- `./scripts/flash.sh` flashes the latest build to the device.
- `./scripts/console.sh` opens the serial console used for final smoke verification.
