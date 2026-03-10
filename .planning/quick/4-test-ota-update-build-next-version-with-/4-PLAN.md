---
quick_task: "4"
type: quick-full
description: "test ota update build next version with minimal changes and update with ota"
files_modified:
  - app/VERSION
  - README.md
autonomous: true
must_haves:
  truths:
    - "The default repo build produces a strictly newer MCUboot signing version than the current `0.0.0+0` baseline so the local OTA upload path can accept it as an upgrade candidate."
    - "The change stays minimal and does not alter OTA service logic, remote-update behavior, or unrelated pending panel/login edits."
    - "The repo documents how this ad hoc next-version build is intended to be used for the local upload/apply OTA test flow."
  artifacts:
    - path: "app/VERSION"
      provides: "Repo-owned application version metadata that Zephyr can feed into MCUboot signing"
      contains: "VERSION_MAJOR|VERSION_MINOR|PATCHLEVEL|VERSION_TWEAK"
    - path: "README.md"
      provides: "Operator note for creating and using the ad hoc next-version OTA test build"
      contains: "app/VERSION|OTA|signed.bin"
  key_links:
    - from: "app/VERSION"
      to: "app/src/ota/ota.c"
      via: "the OTA service compares the staged image semantic version fields and only accepts a strictly newer build"
      pattern: "ota_version_compare|build_num"
    - from: "README.md"
      to: "scripts/build.sh"
      via: "the documented local OTA test flow still relies on the existing repo build entrypoint and signed image artifact"
      pattern: "build.sh|signed.bin"
---

# Quick Task 4 Plan

## Objective

Create the smallest repo-local version bump that yields a newer signed firmware image for local OTA testing, then document how to use that build without broadening the OTA scope.

## Tasks

### Task 1: Add the minimal repo-owned next-version marker
- Files: `app/VERSION`
- Action: Add a Zephyr app version file that sets the smallest strictly newer semantic version (`0.0.0+1`) so the default MCUboot imgtool signing version stops falling back to `0.0.0+0`.
- Verify:
  - `./scripts/build.sh`
  - `rg -n "CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION|APP_VERSION_TWEAK_STRING" build/nrf7002dk_nrf5340_cpuapp/app/zephyr/include/generated/zephyr/autoconf.h build/nrf7002dk_nrf5340_cpuapp/app/zephyr/include/generated/zephyr/app_version.h`
- Done when: the build succeeds and the generated version metadata shows a newer signed image version for OTA comparison.

### Task 2: Document the local OTA test-build expectation
- Files: `README.md`
- Action: Add one narrow note explaining that this repo now uses `app/VERSION` for the ad hoc next-version local OTA test build and that the resulting signed image should be uploaded through the existing local OTA flow.
- Verify:
  - `rg -n "app/VERSION|signed.bin|OTA" README.md`
- Done when: the repo states how the next-version build is produced and where that artifact fits in the existing local OTA upload/apply steps.
