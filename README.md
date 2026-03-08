# LNH Nordic

Generic controller system for `nRF7002-DK`.

## Phase 1 validation workflow

Phase 1 keeps fast refactor feedback build-first by default, then requires a separate hardware smoke sign-off before the phase can be considered complete.

### Automated refactor-safe validation

Run the repo-owned validation entrypoint during normal development:

```sh
./scripts/validate.sh
```

- Delegates to `./scripts/build.sh`, which remains the canonical automated build signal.
- Stays safe for frequent refactor loops because it does **not** require hardware by default.
- Supports `./scripts/validate.sh --preflight` when you want `./scripts/doctor.sh` to check host tools and connected hardware first.

### Hardware smoke is the blocking sign-off gate

Automated validation is necessary but not sufficient for Phase 1 completion. Final sign-off stays manual on real hardware:

```sh
./scripts/flash.sh
./scripts/console.sh
```

Confirm the device log reaches all of these ready-state markers:

- `Wi-Fi connected`
- `DHCP IPv4 address: ...`
- `Reachability check passed`
- `APP_READY`

If the smoke run fails, record the exact stage that failed instead of treating the phase as complete.

## Supporting scripts

- `./scripts/doctor.sh` checks host prerequisites and board visibility before build-heavy or hardware-bound work.
- `./scripts/build.sh` performs the canonical firmware build.
- `./scripts/flash.sh` flashes the latest build to the device.
- `./scripts/console.sh` opens the serial console used for final smoke verification.
