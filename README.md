# LNH Nordic

Generic controller system for `nRF7002-DK`.

## Phase 3 validation workflow

Phase 3 keeps fast refactor feedback build-first by default, then requires a separate device-only recovery/watchdog sign-off before the phase can be considered complete.

### Automated refactor-safe validation

Run the repo-owned validation entrypoint during normal development:

```sh
./scripts/validate.sh
```

- Delegates to `./scripts/build.sh`, which remains the canonical automated build signal.
- Serves as the canonical wave-level automated validation command before any blocking device sign-off.
- Stays safe for frequent refactor loops because it does **not** require hardware, forced degraded paths, or watchdog starvation by default.
- Supports `./scripts/validate.sh --preflight` when you want `./scripts/doctor.sh` to check host tools and connected hardware first.

### Device-only recovery/watchdog sign-off is the blocking gate

Automated validation is necessary but not sufficient for Phase 3 completion. Final sign-off stays manual on real hardware:

```sh
./scripts/flash.sh
./scripts/console.sh
```

Record these Phase 3 outcomes during the device run:

- `healthy stability`: normal boot reaches `APP_READY`, logs `Recovery watchdog armed timeout_ms=...`, and stays in `Network supervisor state=healthy` without a false recovery reset.
- `long-degraded escalation`: a long-enough degraded or stalled path eventually logs `Recovery escalation trigger=confirmed-stuck state=... stage=... reason=...` instead of hanging forever.
- `reboot breadcrumb`: the next boot reports the retained reset cause with `Previous recovery reset=confirmed-stuck hw=0x... state=... stage=... reason=...` and the recovery cooldown/stable-window policy.
- `watchdog reset`: a true watchdog-feed starvation path reboots under watchdog control and the next boot reports `Previous recovery reset=watchdog-starvation ...`.
- `anti-thrash`: after a recovery-triggered reboot, the device respects the cooldown window, avoids rapid repeat resets, and eventually logs `Recovery incident cleared after stable window ... ms` once stability is restored.

Confirm the healthy-path device log reaches all of these ready-state markers:

- `Wi-Fi connected`
- `DHCP IPv4 address: ...`
- `Reachability check passed`
- `APP_READY`

Watch for these recovery/watchdog markers during the device run:

- `Recovery watchdog armed timeout_ms=...`
- `Recovery escalation trigger=confirmed-stuck state=... stage=... reason=...`
- `Previous recovery reset=confirmed-stuck hw=0x... state=... stage=... reason=...`
- `Previous recovery reset=watchdog-starvation hw=0x... state=... stage=... reason=...`
- `Recovery cooldown active for ... ms after previous reset`
- `Recovery incident cleared after stable window ... ms`

Use this blocking device checklist before Phase 3 approval:

1. Re-run `./scripts/validate.sh` and keep the automated `./scripts/build.sh` path green.
2. Flash the latest image with `./scripts/flash.sh`.
3. Open the live device log with `./scripts/console.sh`.
4. Confirm a healthy boot reaches the ready-state markers above, arms the watchdog, and enters `Network supervisor state=healthy`.
5. Force a long-enough degraded or stalled path and confirm the recovery path eventually escalates instead of hanging forever.
6. After the recovery reset, confirm the next boot logs the retained reset cause and the recovery cooldown/stable-window policy.
7. Use the selected lab workflow to confirm a watchdog-owned reset path and record the next-boot reset cause.
8. Record whether the healthy stability, long-degraded escalation, reboot-breadcrumb, watchdog-reset, and anti-thrash scenarios passed or failed.

If the device run fails, record the exact scenario and last visible failure instead of treating the phase as complete.

## Supporting scripts

- `./scripts/validate.sh` is the canonical automated validation entrypoint for Phase 3.
- `./scripts/doctor.sh` checks host prerequisites and board visibility before build-heavy or hardware-bound work.
- `./scripts/build.sh` performs the canonical firmware build.
- `./scripts/common.sh` centralizes shared Phase 3 marker text and the blocking device checklist.
- `./scripts/flash.sh` flashes the latest build to the device.
- `./scripts/console.sh` opens the serial console used for final smoke verification.
