# Deferred Items

## 2026-03-08

- `./scripts/build.sh` currently aborts during Kconfig validation because `app/wifi.secrets.conf` assigns `APP_ADMIN_USERNAME` and `APP_ADMIN_PASSWORD`, but `app/Kconfig` does not define those symbols. This plan did not modify either file, so the blocker remains out of scope for `01-01`.
