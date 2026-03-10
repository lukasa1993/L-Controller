# Deferred Items

- Pre-existing sysbuild Kconfig warnings around `MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG*` still appear during `./scripts/build.sh`. They did not block the MCUboot/sysbuild OTA foundation and were left out of scope for `08-01`.
- Pre-existing compiler warning in `app/src/network/network_supervisor.c` (`unused variable 'wifi_config'`) still appears during builds. This warning is unrelated to the OTA changes and was not modified here.
