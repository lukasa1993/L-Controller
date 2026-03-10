# Deferred Items

- Pre-existing Zephyr MBEDTLS Kconfig warnings still appear during `./scripts/build.sh`; they did not block Phase 06-01 and were left unchanged.
- Pre-existing compiler warning in `app/src/network/network_supervisor.c` (`unused variable 'wifi_config'`) still appears during builds and is unrelated to the relay-runtime work.
