# Deferred Items

## 2026-03-08 — Plan 02-03 completion checks

- `app/src/network/network_supervisor.c:260` still triggers `warning: unused variable 'wifi_config'` during `./scripts/validate.sh`.
  - Observed during final Phase 2 validation.
  - Scope: pre-existing implementation warning, not caused by the Task 3 continuation.
  - Follow-up: remove the unused local or restore its intended usage in a future cleanup pass.

- Zephyr configuration still emits the known non-fatal MBEDTLS Kconfig warnings during `./scripts/validate.sh`.
  - Observed during final Phase 2 validation.
  - Scope: already-known warning path that does not block the build.
  - Follow-up: revisit the MBEDTLS RNG configuration in a future cleanup or security-focused pass if the warning becomes actionable.
