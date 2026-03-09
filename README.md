# LNH Nordic

Generic controller system for `nRF7002-DK`.

## Phase 4 persistence validation workflow

Phase 4 keeps normal iteration build-first, then blocks phase approval on a separate real-device persistence checkpoint. The automated path proves the tree still builds cleanly; the hardware checkpoint proves reboot durability, supported power loss behavior, corrupt-section fallback, auth reseed, and relay reboot-policy behavior.

### Automated validation remains build-first

Run the repo-owned validation entrypoint during normal development:

```sh
./scripts/validate.sh
```

- Delegates to `./scripts/build.sh`, which remains the canonical automated build signal.
- Serves as the canonical wave-level automated validation command before the blocking device durability checkpoint.
- Stays safe for frequent refactor loops because it does **not** require hardware or storage mutation by default.
- Supports `./scripts/validate.sh --preflight` when you want `./scripts/doctor.sh` to check host tools and connected hardware first.

### Device durability checkpoint is the blocking gate

Automated validation is necessary but not sufficient for Phase 4 completion. Final sign-off stays manual on real hardware:

```sh
./scripts/flash.sh
./scripts/console.sh
```

Record these Phase 4 outcomes during the device run:

- `clean-device defaults`: first boot logs persistence initialization, starts with blank safe action/schedule data, and reseeds the configured admin credentials when auth is missing.
- `reboot persistence`: saved auth, relay/action, and schedule data survive reboot and reload on the next boot.
- `supported power loss durability`: within the supported write model, a reboot or power loss after a completed save does not leave ambiguous state.
- `section corruption isolation`: corrupt or incompatible relay/action or schedule data resets only the broken section while healthy sections remain intact.
- `auth reseed`: missing or corrupt auth data restores the configured single-operator credential source without wiping healthy sections.
- `relay reboot policy`: the stored relay desired state and configured reboot policy are both reflected after the next boot.

Confirm the healthy-path device log reaches these ready-state markers:

- `Wi-Fi connected`
- `DHCP IPv4 address: ...`
- `Reachability check passed`
- `APP_READY`

Watch for these persistence markers during boot and reload:

- `Persistence snapshot ready (layout v..., actions=..., schedules=..., relay desired=..., relay reboot=...)`
- `Persistence auth: ...`
- `Persistence actions: ...`
- `Persistence relay: ...`
- `Persistence schedule: ...`

Use this blocking device checklist before Phase 4 approval:

This hardware gate is blocking for phase approval. Do not mark Phase 4 complete until a real-device run records each scenario below.

1. Re-run `./scripts/validate.sh` and keep the automated `./scripts/build.sh` path green.
2. Flash the latest image with `./scripts/flash.sh`.
3. Open the live device log with `./scripts/console.sh`.
4. Confirm a clean device logs persistence initialization, starts with blank safe action/schedule defaults, and reseeds the configured admin credentials when the auth section is absent or corrupt.
5. Save representative auth, relay/action, and schedule data through the implemented persistence entrypoints.
6. Reboot the device and confirm the saved values reload on the next boot together with the persistence snapshot and per-section status logs.
7. Repeat the durability check with the supported power loss or power-cycle lab flow after a completed save and confirm the device does not report ambiguous half-written state.
8. Corrupt or remove exactly one persisted section and confirm only that section resets or reseeds while healthy sections still report loaded.
9. Persist a non-default relay last desired state and reboot policy, reboot again, and confirm next-boot logs or observed relay behavior match the stored policy.
10. Record whether the clean-device defaults, reboot persistence, supported power loss durability, section corruption isolation, auth reseed, and relay reboot policy scenarios passed or failed.

If the device run fails, record the exact scenario and last visible failure instead of treating the phase as complete.

## Supporting scripts

- `./scripts/validate.sh` is the canonical automated validation entrypoint for Phase 4.
- `./scripts/doctor.sh` checks host prerequisites and board visibility before build-heavy or hardware-bound work.
- `./scripts/build.sh` performs the canonical firmware build.
- `./scripts/common.sh` centralizes Phase 4 persistence markers and the blocking device checklist.
- `./scripts/flash.sh` flashes the latest build to the device.
- `./scripts/console.sh` opens the serial console used for final durability verification.
