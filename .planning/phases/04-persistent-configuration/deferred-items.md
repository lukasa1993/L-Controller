# Deferred Items

## 2026-03-09

- `./scripts/build.sh` emits pre-existing Kconfig warnings for `MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG_ALLOW_NON_CSPRNG` and `MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG`. They are outside the 04-01 persistence scope and were not changed by this plan.
