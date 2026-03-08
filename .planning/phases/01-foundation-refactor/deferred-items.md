# Deferred Items

## 2026-03-08

- Resolved in `01-02`: `./scripts/build.sh` no longer aborts on `APP_ADMIN_USERNAME` and `APP_ADMIN_PASSWORD` because `app/Kconfig` now defines placeholder symbols for the existing local secrets overlay. The mismatch was discovered in `01-01` and fixed as a build-unblocking deviation while verifying `01-02`.
