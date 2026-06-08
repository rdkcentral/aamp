# External Integrations

**Analysis Date:** 2026-06-08

## APIs & External Services

**Thunder / WPEFramework Plugin IPC:**
- Purpose: RPC communication with RDK platform plugins over local JSON-RPC websocket
- SDK/Client: WPEFramework (`WPEFrameworkCore`, `WPEFrameworkPlugins`, `WPEFrameworkProtocols` / `WPEFrameworkCOM`)
- Found via: `cmake/FindWPEFramework.cmake`
- Connection: `127.0.0.1:9998` (hardcoded in `ThunderAccess.cpp`)
- Security: Thunder security token fetched via `securityagent/SecurityTokenUtil.h` (can be disabled with `DISABLE_SECURITY_TOKEN`)
- Core wrapper: `ThunderAccess.cpp` (AAMP-level), `middleware/externals/rdk/PlayerThunderAccess.cpp` (middleware-level)
- Thunder R4 variant: `USE_THUNDER_R4` CMake flag switches library set

**SecManager Thunder Plugin (`org.rdk.SecManager.1`):**
- Purpose: DRM license acquisition and watermarking via platform security service
- Client: `middleware/externals/contentsecuritymanager/SecManagerThunder.cpp`
- Interface: `middleware/externals/contentsecuritymanager/ContentSecurityManager.h`
- Activation: `eAAMPConfig_UseSecManager` config flag
- Handles: DRM session creation, access token validation, watermark session management

**AuthService Thunder Plugin (`org.rdk.AuthService.1`):**
- Purpose: Retrieve access token for DRM license requests
- Client: `middleware/externals/contentsecuritymanager/SecManagerThunder.cpp` (line ~112)
- Callsign: `AUTH_SERVICE_CALL_SIGN = "org.rdk.AuthService.1"`
- Used by: `AampDRMLicenseManager::getAccessToken()` (`drm/AampDRMLicManager.cpp`)

**Watermark Plugin (`org.rdk.Watermark.1`):**
- Purpose: Persistent watermark rendering on-screen
- Client: `middleware/externals/contentsecuritymanager/SecManagerThunder.cpp`
- JS side: `jsbindings/PersistentWatermark/PersistentWatermarkPluginAccess.cpp`
- Activation: `CMAKE_WPEWEBKIT_WATERMARK_JSBINDINGS`

**Firebolt Device SDK:**
- Purpose: Alternative path for DRM license acquisition (non-Thunder/SecManager path)
- Activation: `eAAMPConfig_UseFireboltSDK` config flag (`AampConfig.h:228`)
- Used in: `drm/AampDRMLicManager.cpp`, `priv_aamp.cpp`, `fragmentcollector_mpd.cpp`, `fragmentcollector_hls.cpp`
- Interface: `middleware/externals/PlayerExternalsInterface.h`; conditionally uses IARM or Firebolt Device API

**FOG (Fragmenting On the Go — CDN Caching Proxy):**
- Purpose: Local CDN proxy that provides ABR and TSB (time-shift buffer) for live streams
- Protocol: Interacts via HTTP; AAMP detects FOG URLs with keyword `"tsb?"` (`AampDefine.h`)
- Response headers parsed: `Fog-Reason:`, `Fog-Recording-Id:` in `priv_aamp.cpp`
- Concurrent download limit: 4 (`FOG_MAX_CONCURRENT_DOWNLOADS`, `AampDefine.h`)

**RDK RFC / TR-181 Configuration:**
- Purpose: Remote operator configuration of AAMP at runtime via TR-181 data model
- Client: `middleware/externals/PlayerRfc.cpp` via `RFCSettings::readRFCValue()`
- AAMP config path: `Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AAMP_CFG.*`
- Base64 config path: `Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.AAMP_CFG.b64Config`
- Parsing: `AampConfig::ReadAllTR181Params()` and `AampConfig::ReadBase64TR181Param()` in `AampConfig.cpp`
- Enabled by: `PLAYER_RFC_ENABLED` compile flag

**Jira (CI Integration):**
- Purpose: Automatically post PR merge comments to Jira tickets
- Workflow: `.github/workflows/pr-merge-jiracomment.yml`
- Trigger: PR merged to `dev_sprint_25_2`
- Auth: `JIRA_BASE_URL` + `JIRA_API_TOKEN` GitHub secrets
- API: Jira REST API v2 (`/rest/api/2/issue/{key}/comment`)

## Data Storage

**Databases:**
- None — AAMP is a stateless media player library; no persistent database

**Local Time-Shift Buffer (TSB):**
- Purpose: On-device DVR buffer for live stream trick play
- Implementation: `tsb/` directory; `AampTSBSessionManager.cpp`, `AampTsbDataManager.cpp`, `AampTsbMetaDataManager.cpp`
- Storage: Local filesystem; percentage-free threshold enforced (`DEFAULT_MIN_TSB_STORAGE_FREE_PERCENTAGE = 10`)
- API surface: `tsb/api/TsbApi.h`

**Fragment Cache:**
- Purpose: In-memory cache of downloaded media segments
- Implementation: `AampCacheHandler.cpp`
- No persistence across restarts

**File Storage:**
- Developer config files: `/opt/aamp.cfg` (key=value), `/opt/aampcfg.json` (JSON)
- No cloud or network-attached file storage

**Caching:**
- In-memory only: `AampCacheHandler` for segments, curl connection pool in `downloader/AampCurlStore.cpp`

## Authentication & Identity

**DRM — OpenCDM (Open Content Decryption Module):**
- Purpose: Hardware/software DRM session management (Widevine, PlayReady)
- Implementation: `middleware/drm/ocdm/` — `OcdmBasicSessionAdapter.cpp`, `OcdmGstSessionAdapter.h`
- Interface: `middleware/drm/ocdm/opencdmsessionadapter.h`; linked against system OpenCDM
- Build flag: `CMAKE_USE_THUNDER_OCDM_API_0_2` (activates `USE_THUNDER_OCDM_API_0_2` define)
- Test stub: `test/utests/ocdm/open_cdm.h`

**DRM — ClearKey:**
- Purpose: AES-based clear-key decryption for development/testing
- Implementation: `middleware/drm/ClearKeyDrmSession.cpp`, `middleware/drm/ClearKeyDrmSession.h`

**DRM — Vanilla AES-128:**
- Purpose: HLS AES-128 segment encryption
- Always enabled: `-DAAMP_VANILLA_AES_SUPPORT` compile define (`CMakeLists.txt`)

**DRM — SecManager / Firebolt SDK (platform DRM):**
- Purpose: Platform-managed DRM license requests (Widevine, PlayReady via platform agent)
- Implementation: `middleware/externals/contentsecuritymanager/ContentSecurityManager.cpp`
- Session: `ContentSecurityManagerSession.cpp`
- Auth token: Retrieved from `org.rdk.AuthService.1` Thunder plugin; cached with mutex in `AampDRMLicenseManager::accessToken`

**IARM (Inter-Application Resource Manager) — Deprecated:**
- Purpose: Device event bus for device settings and state; being replaced by Firebolt Device API
- Implementation: `middleware/externals/rdk/PlayerExternalsRdkInterface.cpp`
- Conditional: `#ifdef IARM_MGR` guards
- Note: IARM is being deprecated in favor of DeviceSettings and Firebolt Device API per source comments

## Monitoring & Observability

**RDK Telemetry 2.0:**
- Purpose: Send telemetry events to RDK telemetry infrastructure
- Implementation: `AampTelemetry2.cpp`, `AampTelemetry2.hpp`
- Library: `libtelemetry_msgsender` (linked on non-simulator builds)
- Enabled by: `CMAKE_TELEMETRY_2_0_REQUIRED` (auto-set for Ubuntu/Darwin simulator builds)
- Always enabled on simulator builds; controlled by `AAMP_TELEMETRY_SUPPORT=1` define

**CMCD (Common Media Client Data):**
- Purpose: Embed playback metrics in CDN request headers per CTA-5004 spec
- Implementation: `AampCMCDCollector.cpp`, `AampCMCDCollector.h`
- Metrics libraries: `support/aampmetrics/` — `CMCDHeaders.h`, `AudioCMCDHeaders.h`, `VideoCMCDHeaders.h`, `ManifestCMCDHeaders.h`, `SubtitleCMCDHeaders.h`
- Active in: `AampCMCDCollector.cpp` for all HTTP segment requests

**Profiler:**
- Purpose: Internal timing instrumentation for tune/play latency analysis
- Implementation: `AampProfiler.cpp`, `AampProfiler.h`
- Output: Event-based; logged to AAMP log stream

**Error Tracking:**
- Custom: `AAMPAnomalyMessageType.h` defines anomaly categories
- No external error tracking service (e.g., no Sentry/Crashlytics)

**Logs:**
- Default: `printf` / `sd_journal_print` depending on platform
- systemd journal: `aamplogging.cpp` via `<systemd/sd-journal.h>`; enabled by `CMAKE_SYSTEMD_JOURNAL`
- EthanLog (Rialto container): `aamplogging.cpp` via `<ethanlog.h>`; enabled when `useRialtoSink` config is set (`main_aamp.cpp:171`)
- Log level: runtime-configurable via `AampLogManager::aampLoglevel`

**Network Simulation (LL-DASH development tool):**
- Purpose: Fit network persona JSON from request traces for LL-DASH testing
- Implementation: `simnet/net_persona_fitter.cpp`, `simnet/net_persona_fitter.h`
- Python counterpart: `simnet/simnet/persona_fit.py`

## CI/CD & Deployment

**Hosting:**
- RDK embedded devices (primary); deployed as shared library (`libaamp.so`)
- No cloud hosting — AAMP is a client-side library

**CI Pipeline:**
- GitHub Actions — all workflows in `.github/workflows/`
- L1 unit tests: `.github/workflows/L1-tests.yml` — runs on every PR to `develop` / `dev_sprint_25_2`
  - Container: `ghcr.io/rdkcentral/aamp-ci-image:latest`
  - Runs `test/utests/run.sh` and `middleware/test/utests/run.sh`
  - Reports: JUnit XML via `dorny/test-reporter@v1`
  - Artifacts: `ctest-results.xml` uploaded via `actions/upload-artifact@v4`
- CI Docker image build: `.github/workflows/build-ci-image.yml`
  - Triggers on push to `dev_sprint_25_2` for changes to `.github/Dockerfile.ci` or `scripts/*`
  - Publishes to `ghcr.io/rdkcentral/aamp-ci-image:latest` (GitHub Container Registry)
- FossID license scan: `.github/workflows/fossid_integration_stateless_diffscan_target_repo.yml`
  - Runs on every PR; stateless diff scan via `rdkcentral/build_tools_workflows`
  - Secrets: `FOSSID_CONTAINER_USERNAME`, `FOSSID_CONTAINER_PASSWORD`, `FOSSID_HOST_USERNAME`, `FOSSID_HOST_TOKEN`
- Jira integration: `.github/workflows/pr-merge-jiracomment.yml` — posts merge comment to Jira on PR close

## Streaming Protocols & Media Formats

**Supported Streaming Protocols:**
- HLS (HTTP Live Streaming) — `fragmentcollector_hls.cpp`
- MPEG-DASH — `fragmentcollector_mpd.cpp`, `admanager_mpd.cpp`
- Progressive MP4 — `fragmentcollector_progressive.cpp`
- OTA / HDMI-In / Video-In / Composite-In / RMF — platform shims: `ota_shim.cpp`, `hdmiin_shim.cpp`, `videoin_shim.cpp`, `compositein_shim.cpp`, `rmf_shim.cpp`

**Media Container Parsing:**
- ISOBMFF / MP4: `isobmff/isobmffbox.cpp`, `isobmff/isobmffbuffer.cpp`, `mp4demux/AampMp4Demuxer.cpp`
- MPEG-TS: `tsprocessor.cpp`, `tsDemuxer.cpp`
- DASH MPD: `dash/xml/`, `dash/mpd/`, `dash/utils/` — custom XML/MPD parser

**Ad Insertion:**
- SCTE-35 cue parsing: `scte35/AampSCTE35.cpp` — validates and parses SCTE-35 splice events
- DASH ad management: `admanager_mpd.cpp`

**Subtitle / Closed Caption:**
- WebVTT: `subtitle/webvttParser.cpp`, `subtec/subtecparser/WebvttSubtecDevParser.cpp`
- TTML: `middleware/subtec/subtecparser/TtmlSubtecParser.cpp`
- SubTec (RDK subtitle engine): `middleware/subtec/` — socket-based IPC to subtitle rendering service
- GStreamer SubTec: enabled by `CMAKE_GST_SUBTEC_ENABLED`

**ABR (Adaptive Bitrate):**
- Internal ABR: `abr/abr.cpp` — HarmonicEwmaEstimator, RollingMedianOutlierEstimator
- Legacy ABR manager: `support/aampabr/ABRManager.cpp`, `HybridABRManager.cpp`

## Webhooks & Callbacks

**Incoming:**
- None — AAMP does not expose HTTP server endpoints

**Outgoing:**
- All HTTP requests use libcurl: `downloader/AampCurlDownloader.cpp`, `downloader/AampCurlStore.cpp`
- DRM license server: HTTP POST to content-specific license URL (URL from manifest)
- SecManager/AuthService: JSON-RPC over Thunder websocket (`127.0.0.1:9998`)
- Jira REST API: POST to `{JIRA_BASE_URL}/rest/api/2/issue/{key}/comment` (CI only)

## Rialto Integration (Optional / POC)

**Rialto Media Pipeline:**
- Purpose: Containerised media pipeline alternative to direct GStreamer; used in Rialto container environments
- Client library: `libRialtoClient.so` / `RialtoClient`; found via `cmake/FindRialto.cmake`
- Interface: `IMediaPipeline.h`
- POC executables: `test/rialtoPOC/` — `rialtoPOC2`, `rialtoPOC3`
- Config: `eAAMPConfig_useRialtoSink` — enables Rialto GStreamer sink; `eAAMPConfig_useDirectRialto` — reserved (not yet implemented per `AampStreamSinkManager.cpp:142`)
- Logging: When Rialto sink enabled, log output redirected to EthanLog (`main_aamp.cpp:171`)

---

*Integration audit: 2026-06-08*
