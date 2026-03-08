# Stack Research

**Domain:** Mission-critical Nordic/Zephyr embedded relay controller with local HTTP admin panel, OTA, persistence, and scheduling
**Researched:** 2026-03-08
**Confidence:** HIGH

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| Nordic Connect SDK | 3.2.x line already used in repo | Primary firmware platform | Matches the working brownfield codebase and keeps Wi-Fi, build, and board support aligned with the existing project |
| Zephyr networking (`wifi_mgmt`, `net_mgmt`, Connection Manager) | Zephyr 4.2.x line via current NCS | Wi-Fi association, IP lifecycle, connectivity monitoring | Official Zephyr path for Wi-Fi-capable applications; reduces custom link-state boilerplate and gives clear connectivity events |
| Zephyr HTTP Server | Zephyr 4.2.x line | Local HTTP admin panel, static asset serving, API endpoints | Official server supports static resources, static filesystem resources, and dynamic handlers without inventing a custom socket server |
| Settings subsystem + NVS backend | Zephyr 4.2.x line | Persistent configuration/state | Official settings API is the standard way to persist key-value configuration; NVS is the conservative choice for non-filesystem storage |
| Task Watchdog + hardware watchdog fallback | Zephyr 4.2.x line | Conservative recovery supervision | Official pattern for supervising multiple critical execution paths while still escalating to hardware reset if the scheduler or software watchdog fails |
| MCUboot + sysbuild + DFU image APIs | Zephyr 4.2.x line | Safe firmware upgrade foundation | Official upgrade path for Zephyr devices; gives image-slot discipline, rollback/confirm semantics, and build-time integration |
| MCUmgr over UDP plus custom HTTP upload path | Zephyr 4.2.x line | Remote management/update path and local operator upload path | MCUmgr is the official remote-management stack over IP, while the panel upload can write to a secondary image using DFU APIs |
| Devicetree-driven GPIO (`gpio_dt_spec`) | Zephyr 4.2.x line | Relay control abstraction | Keeps hardware bindings declarative and portable instead of scattering pin numbers through application logic |

### Supporting Libraries / Subsystems

| Library / Subsystem | Version | Purpose | When to Use |
|---------------------|---------|---------|-------------|
| LittleFS | Zephyr 4.2.x line | Optional mutable file storage | Use only if you truly need writable filesystem-backed assets or large mutable blobs; not required for core config |
| `k_work` / `k_work_delayable` on dedicated workqueues | Zephyr 4.2.x line | Deferred work, retries, scheduling engine internals | Use for action dispatch, reconnect backoff, and scheduled execution instead of blocking callbacks |
| Flash map / image partitions | Zephyr 4.2.x line | OTA slot layout and storage boundaries | Required once MCUboot and persistent storage partitions are introduced |
| Zephyr JSON helpers or minimal fixed-schema parsing | Zephyr 4.2.x line | HTTP request/response payloads | Use only for a constrained control API; keep payload schemas small and deterministic |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `west build --sysbuild` | Build app + MCUboot together | Needed once OTA bootloader support is added |
| `twister` | Unit/integration test execution | Use for subsystem tests once modules exist; especially useful for scheduler/action logic |
| Existing `scripts/*.sh` wrappers | Local developer workflow | Preserve these wrappers, but evolve them to support sysbuild, image packaging, and update artifacts |

## Installation / Enablement Notes

```bash
# Build with MCUboot once OTA work starts
west build -b nrf7002dk/nrf5340/cpuapp --sysbuild app

# Typical follow-on targets
west flash
west debug
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| Settings + NVS | Settings + ZMS | Consider ZMS only if you later need storage behavior that clearly benefits from it and you validate it thoroughly on target |
| Build-authored HTML/JS assets embedded as static resources | Static filesystem resources via LittleFS | Use static filesystem resources if you need a writable/mounted web asset volume; otherwise embedded gzip assets are simpler and safer |
| MCUmgr over UDP for network management | Fully custom remote update protocol | Only choose custom transport if MCUmgr cannot satisfy operational requirements after proof-of-concept validation |
| Dedicated supervisor modules around Zephyr networking | Monolithic event handling in `main.c` | Only acceptable for throwaway demos, not for a mission-critical controller |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| Legacy connectivity APIs such as `net_context` in application code | Zephyr explicitly marks the legacy connectivity API as something applications should not use | Socket APIs, `wifi_mgmt`, `net_mgmt`, and Connection Manager |
| FCB as the default new settings backend | Zephyr guidance favors NVS (and now NVS/ZMS) for non-filesystem settings storage | Settings + NVS |
| HTML assembled as C string fragments | Hard to maintain, review, test, and compress; violates the desired frontend separation | Author real `.html` / `.js` files and embed or serve them as assets |
| Reboot-on-any-network-error logic | Creates recovery loops and masks root causes | Explicit Wi-Fi state machine with retry budgets and only escalate on confirmed stuck states |
| Writing update payloads into the active image without bootloader discipline | High brick risk | Secondary slot + MCUboot + image confirmation |

## Stack Patterns by Variant

**If UI assets are effectively immutable between firmware releases:**
- Author UI files normally in a dedicated frontend directory
- Gzip/embed them as Zephyr HTTP static resources at build time
- Prefer this for v1 because it is simpler and more tamper-resistant

**If UI assets must be writable or replaced independently:**
- Serve them from a mounted filesystem via `HTTP_RESOURCE_TYPE_STATIC_FS`
- Accept the extra storage/mount/corruption surface area
- Use only after the simpler embedded-asset path is proven

**If remote fleet-style updates become operationally important:**
- Keep MCUboot as the image authority
- Reuse DFU image-writing APIs underneath either MCUmgr or a controlled pull client
- Do not invent a second incompatible image format

## Version Compatibility

| Package A | Compatible With | Notes |
|-----------|-----------------|-------|
| Current repo NCS 3.2.1 workspace | Zephyr 4.2.x line in the workspace | Already present in the brownfield repo according to codebase mapping |
| Sysbuild | MCUboot | Officially supported path for building bootloader + application together |
| Zephyr HTTP Server | Static resources / static filesystem resources | Both are first-class official server resource types |
| Settings subsystem | NVS / ZMS backends | Official docs currently recommend NVS and ZMS for non-filesystem storage |

## Sources

- https://docs.zephyrproject.org/latest/connectivity/networking/api/http_server.html — HTTP server resource model, static/static-FS resources
- https://docs.zephyrproject.org/latest/connectivity/networking/api/wifi.html — Wi-Fi management API
- https://docs.zephyrproject.org/latest/connectivity/networking/conn_mgr/main.html — Connection Manager overview
- https://docs.zephyrproject.org/apidoc/latest/group__net__mgmt.html — L4 connectivity events
- https://docs.zephyrproject.org/latest/services/storage/settings/index.html — settings subsystem, recommended backends
- https://docs.zephyrproject.org/latest/services/storage/nvs/nvs.html — NVS storage details
- https://docs.zephyrproject.org/latest/services/task_wdt/index.html — task watchdog + hardware fallback
- https://docs.zephyrproject.org/latest/hardware/peripherals/watchdog.html — hardware watchdog API
- https://docs.zephyrproject.org/latest/services/device_mgmt/dfu.html — DFU image handling
- https://docs.zephyrproject.org/latest/services/device_mgmt/mcumgr.html — MCUmgr transports and capabilities
- https://docs.zephyrproject.org/latest/build/sysbuild/index.html — sysbuild integration
- Repo codebase map in `.planning/codebase/ARCHITECTURE.md` and `.planning/codebase/STACK.md`

---
*Stack research for: mission-critical Nordic/Zephyr embedded relay controller*
*Researched: 2026-03-08*
