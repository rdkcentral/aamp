# Technology Stack

**Analysis Date:** 2026-06-08

## Languages

**Primary:**
- C++17 — Core media player engine, all production runtime code
  - Standard enforced via `set(CMAKE_CXX_STANDARD 17)` and `CMAKE_CXX_STANDARD_REQUIRED ON` in `CMakeLists.txt`
  - Extensions disabled (`CMAKE_CXX_EXTENSIONS OFF`)

**Secondary:**
- Kotlin — Android/KMP CLI wrapper (`kotlin/aampcli/main.kt`); built only when `CMAKE_BUILD_KOTLIN_ENABLED`
- JavaScript — UVE/AAMPMediaPlayer JS bindings (`jsbindings/`); runs inside WPEWebKit/JavaScriptCore engine
- Python 3 — Build tooling, test log parsing (`test/utests/parse_gtest_log.py`), meson build system
- Bash — All install and dependency scripts (`scripts/`)
- CMake — Build system; minimum version 3.5 (unit tests require 3.18)

**Objective-C++:**
- macOS builds use `-x objective-c++` flag; set via `OS_CXX_FLAGS` in `CMakeLists.txt`

## Runtime

**Environment:**
- Native C++ shared library (`libaamp.so` / `libaamp.dylib`) — deployed embedded on RDK devices
- CLI test harness (`aamp-cli`) — used for simulator/developer builds on macOS and Ubuntu

**Supported Platforms:**
- RDK/Linux (embedded — primary target)
- Ubuntu 20.04+ (simulator builds; `CMAKE_PLATFORM_UBUNTU`)
- macOS / Darwin (Xcode/Homebrew; `CMAKE_SYSTEM_NAME STREQUAL Darwin`)
- Raspberry Pi (via `CMAKE_SOC_PLATFORM_RPI`)

**Package Manager (macOS):**
- Homebrew — installs all native dependencies; see `scripts/install_dependencies.sh`

**Package Manager (Ubuntu):**
- apt — installs native packages; meson installed via pip3 when system version < 1.4.0

## Frameworks

**Core Media Pipeline:**
- GStreamer 1.x (>= 1.18.0) — video/audio pipeline; `gstreamer-1.0`, `gstreamer-app-1.0`, `gstreamer-video-1.0`
  - Checked via `pkg_check_modules` in `CMakeLists.txt`

**JavaScript Runtime (platform-dependent):**
- JavaScriptCore (macOS) — `JavaScriptCore.framework`; found via `find_library(JSCORE_FRAMEWORK JavaScriptCore)`
- `javascriptcoregtk-4.1` / `javascriptcoregtk-4.0` (Ubuntu) — system WebKitGTK JavaScriptCore
- WPE WebKit (`wpe-webkit-1.1` / `wpe-webkit-1.0`) — on RDK embedded targets

**Testing:**
- GoogleTest + GoogleMock — `pkg_check_modules(GTEST REQUIRED gtest)` / `(GMOCK REQUIRED gmock)`; installed via `scripts/install_gtest.sh`
- CTest — test orchestration (`include(CTest)` in `test/utests/CMakeLists.txt`)
- Address Sanitizer — enabled on non-Linux builds in unit tests; optional on Ubuntu via `SANITIZER_ENABLED`

**Build:**
- CMake (>= 3.5 main build; >= 3.18 for unit tests) — `CMakeLists.txt`
- Meson + Ninja — used for building libdash and GStreamer dependencies (`scripts/install_libdash.sh`)
- Xcode — supported on macOS via `XCODE_GENERATE_SCHEME` properties; Xcode schemas created by `xcode_define_schema()`
- Docker — CI image defined in `.github/Dockerfile.ci`; base: `ubuntu:22.04`

## Key Dependencies

**Critical (always required):**
- `libcurl` (>= 8.5 on macOS, any on Linux) — all HTTP/HTTPS segment and manifest downloads; `downloader/AampCurlDownloader.cpp`
- `openssl` — TLS for curl and DRM license requests; `pkg_check_modules(OPENSSL REQUIRED openssl)`
- `libxml-2.0` — DASH MPD parsing; `dash/xml/`, `dash/mpd/`
- `libdash` — DASH manifest/segment library; built from source via `scripts/install_libdash.sh`
- `libcjson` — JSON config and telemetry parsing; `AampJsonObject.cpp`, `AampTelemetry2.cpp`
- `uuid` — Session/request UUID generation; `uuid/uuid.h` used in `AampCMCDCollector.cpp`
- `readline` — aamp-cli interactive shell; linked via `-lreadline`
- `glib-2.0` — GLib event loop primitives; required on Darwin builds

**Infrastructure:**
- `libatomic` — 16-byte atomic ops for `ABRManager::PersistBandwidthData` on Linux/x86_64; linked via `-latomic`
- `libdl` — Dynamic library loading on Linux; excluded on Darwin (part of libSystem)

**Optional (build-flag gated):**
- `libsystemd` — systemd journal logging; enabled by `CMAKE_SYSTEMD_JOURNAL`; `aamplogging.cpp`, `jsbindings/jsutils.cpp`
- `ethanlog` — Container/Rialto logging; enabled by `CMAKE_USE_ETHAN_LOG`; `aamplogging.cpp`, `middleware/playerLogManager/PlayerLogManager.cpp`
- `libtelemetry_msgsender` — RDK Telemetry 2.0; enabled by `CMAKE_TELEMETRY_2_0_REQUIRED`; `AampTelemetry2.cpp`
- `RialtoClient` — Rialto media pipeline sink; found via `cmake/FindRialto.cmake`; optional POC build
- WPEFramework (Thunder) — RDK plugin IPC; found via `cmake/FindWPEFramework.cmake`; used for DRM and device API access
- OpenCDM — Open Content Decryption Module; found in `middleware/drm/ocdm/`; `CMAKE_USE_THUNDER_OCDM_API_0_2`
- OpenGL / GLUT / GLEW — On-screen rendering for simulator builds; platform-specific

**Test-only:**
- GoogleTest / GoogleMock — L1 unit tests in `test/utests/` and `middleware/test/utests/`
- GStreamer (test) — required even in test builds to resolve link dependencies

## Configuration

**Runtime Configuration Priority (lowest to highest):**
1. AAMP defaults in source code (`AampConfig.cpp`)
2. Operator/RFC settings — read via TR-181 (`Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AAMP_CFG.*`); `AampConfig::ReadAllTR181Params()`
3. Stream-provided overrides
4. Application settings passed via UVE API
5. Developer config files: `/opt/aamp.cfg`, `/opt/aampcfg.json`

**Key Config Headers:**
- `AampConfig.h` — all `eAAMPConfig_*` enum values; 200+ configuration options
- `AampDefine.h` — compile-time numeric constants (buffer sizes, thresholds)

**Build-time CMake Feature Flags:**
- `CMAKE_PLATFORM_UBUNTU` — Ubuntu simulator build
- `CMAKE_WPEWEBKIT_JSBINDINGS` — enable JS binding library
- `CMAKE_SYSTEMD_JOURNAL` — redirect logs to systemd
- `CMAKE_USE_ETHAN_LOG` — redirect logs to EthanLog (Rialto container)
- `CMAKE_TELEMETRY_2_0_REQUIRED` — enable RDK telemetry 2.0
- `CMAKE_EXTERNAL_PLAYER_INTERFACE_DEPENDENCIES` — use externally-built middleware libs
- `CMAKE_INBUILT_AAMP_DEPENDENCIES` — build ABR and metrics support libs inline
- `CMAKE_GST_SUBTEC_ENABLED` — enable GStreamer-based subtitle rendering
- `CMAKE_BUILD_KOTLIN_ENABLED` — build Kotlin/JNI bridge
- `SANITIZER_ENABLED` — AddressSanitizer on Ubuntu builds
- `UTEST_ENABLED` — include unit tests in build
- `DISABLE_SECURITY_TOKEN` — skip Thunder security token in externals

**Environment Variables (CI):**
- `LOCAL_DEPS_BUILD_DIR` — path to locally built dependencies (default `/opt/local_deps` in CI)
- `PKG_CONFIG_PATH` — extended to include `$LOCAL_DEPS_BUILD_DIR/lib/pkgconfig`

## Platform Requirements

**Development (macOS):**
- Homebrew with: `git`, `glib`, `cmake`, `openssl@3`, `libxml2`, `ossp-uuid`, `cjson`, `gstreamer`, `gst-plugins-base`, `meson`, `ninja`, `lcov`, `jq`, `curl`
- Xcode command-line tools (`xcrun`, `xcodebuild`)
- macOS deployment target: 26.0 (`CMAKE_OSX_DEPLOYMENT_TARGET`)

**Development (Ubuntu):**
- Ubuntu 20.04+ LTS
- Packages: see `scripts/install_dependencies.sh` → `install_pkgs_linux_fn()`
- Key packages: `libgstreamer1.0-dev`, `libcurl4-openssl-dev`, `libssl-dev`, `libxml2-dev`, `libcjson-dev`, `libreadline-dev`, `libjavascriptcoregtk-4.1-dev`
- Meson >= 1.4.0

**Production (RDK embedded):**
- RDK Linux (ARM/x86)
- GStreamer 1.18+ available on device
- Thunder/WPEFramework runtime for DRM and plugin IPC
- OpenCDM for hardware DRM sessions

---

*Stack analysis: 2026-06-08*
