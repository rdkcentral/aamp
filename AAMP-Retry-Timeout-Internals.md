<!--
If not stated otherwise in this file or this component's license file the
following copyright and licenses apply:

Copyright 2026 RDK Management

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# AAMP Download Retry & Timeout Internals

This document describes how AAMP handles retries and timeouts for media
segment and manifest downloads, covering both HTTP error codes and curl-level
failures.

---

## Architecture: Two Download Paths

AAMP uses two distinct download paths with separate retry logic:

| Path | Used for |
|---|---|
| `PrivateInstanceAAMP::GetFile()` in `priv_aamp.cpp` | Segments, init fragments, playlists, and manifests (HLS and DASH foreground) |
| `AampCurlDownloader::Download()` in `downloader/AampCurlDownloader.cpp` | DASH manifest background downloads via `AampMPDDownloader` |

---

## Default Curl Timeouts

All defaults are defined in `AampDefine.h`.

| Constant | Value | Applied to |
|---|---|---|
| `DEFAULT_CURL_TIMEOUT` | **5 s** | General `CURLOPT_TIMEOUT` baseline |
| `DEFAULT_CURL_CONNECTTIMEOUT` | **3 s** | `CURLOPT_CONNECTTIMEOUT` — TCP connection phase only |
| `CURL_FRAGMENT_DL_TIMEOUT` | **10 s** | Media segment downloads |
| `DEFAULT_PLAYLIST_DL_TIMEOUT` | **10 s** | HLS sub-manifest / playlist downloads |
| `EAS_CURL_TIMEOUT` | **3 s** | Emergency Alert System manifest |
| `EAS_CURL_CONNECTTIMEOUT` | **2 s** | Emergency Alert System connection |
| `DEFAULT_DRM_NETWORK_TIMEOUT` | **5 s** | DRM license network requests |

Per-tune overrides are applied via `PrivateInstanceAAMP::SetCurlTimeout()`:
- `mManifestTimeoutMs` — from app config; default `-1` (use curl default)
- `mNetworkTimeoutMs` — used as the retry-window timeout for video segments

### Progress-Callback Abort Reasons

Beyond `CURLOPT_TIMEOUT`, the progress callback (`aamp_progress_callback`) enforces three additional in-flight abort conditions, tracked as `CurlAbortReason`:

| Abort reason | Config key | Default | Notes |
|---|---|---|---|
| First-byte timeout (`eCURL_ABORT_REASON_START_TIMEDOUT`) | `eAAMPConfig_CurlDownloadStartTimeout` | 0 (disabled) | Always disabled for playlists and manifests |
| Stall detection (`eCURL_ABORT_REASON_STALL_TIMEDOUT`) | `eAAMPConfig_CurlStallTimeout` | 10,000 ms (`DEFAULT_STALL_DETECTION_TIMEOUT`) | No bytes received for this duration |
| Low-bandwidth (`eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT`) | `eAAMPConfig_CurlDownloadLowBWTimeout` | `NetworkTimeout × LOW_BW_TIMEOUT_FACTOR` (floor: `DEFAULT_LOW_BW_TIMEOUT`) | Projected total download time exceeds network timeout |

All three abort reasons interrupt the transfer. In `GetFile()`, curl typically reports the immediate result as `CURLE_ABORTED_BY_CALLBACK`; AAMP later maps the associated timeout-style `http_code` to `CURLE_OPERATION_TIMEDOUT` (and may then further sub-classify it, for example to 130-133). Note that `LOW_BANDWIDTH_TIMEDOUT` **does not trigger a retry** (see below); it drives ABR profile rampdown instead.

---

## Timeout Sub-Classification

When `CURLE_OPERATION_TIMEDOUT` is returned, AAMP inspects curl timing metrics (via `GetCurlTimeoutFailureReason()` in `AampUtils.cpp`) to determine the root cause:

| Code | `CURLINFO_*` condition | Meaning |
|---|---|---|
| `eCURL_TIMEOUT_DNS` | `CURLINFO_NAMELOOKUP_TIME == 0` | Timed out before DNS resolved |
| `eCURL_TIMEOUT_CONNECT` | `CURLINFO_CONNECT_TIME == 0` | DNS resolved but TCP connect timed out |
| `eCURL_TIMEOUT_DATA` | Both non-zero | Timed out during data transfer |

`IsCurlTimeoutFailure()` returns `true` for `CURLE_OPERATION_TIMEDOUT`,
`eCURL_TIMEOUT_DNS`, and `eCURL_TIMEOUT_CONNECT`. Because
`eCURL_TIMEOUT_DATA` shares the same numeric value as
`CURLE_OPERATION_TIMEDOUT`, the function also treats it as a timeout
failure, but it cannot distinguish data-transfer timeouts as a separate
case based on that value alone.

---

## Internal Error Codes for Partial Downloads

`GetFile()` maps certain curl abort conditions to internal pseudo-HTTP codes (defined in `priv_aamp.h`) for finer-grained error reporting upstream:

| Code | Value | Meaning |
|---|---|---|
| `PARTIAL_FILE_CONNECTIVITY_AAMP` | 130 | Partial download with connectivity failure |
| `PARTIAL_FILE_DOWNLOAD_TIME_EXPIRED_AAMP` | 131 | Partial download with time expiry |
| `OPERATION_TIMEOUT_CONNECTIVITY_AAMP` | 132 | Full timeout with connectivity failure |
| `PARTIAL_FILE_START_STALL_TIMEOUT_AAMP` | 133 | No data (or partial) due to stall/start timeout |

These codes are in the range `[130, 133]` and are checked by `streamabstraction.cpp` and `priv_aamp.cpp` to distinguish connectivity failures from data-transfer failures.

---

## Retry Logic: `GetFile()` Path

### Attempt Budget

```
maxDownloadAttempt = 1 + retryCount
```

| Media type | Retry count | Default max attempts |
|---|---|---|
| Init fragments (`INIT_VIDEO/AUDIO/SUBTITLE/AUX_AUDIO/IFRAME`) | `eAAMPConfig_InitFragmentRetryCount` (configurable) | Configurable |
| All others (segments, manifests, playlists) | `DEFAULT_DOWNLOAD_RETRY_COUNT` = **1** | **2** |

### HTTP Error Handling

| HTTP status | Retry behaviour | Wait between retries |
|---|---|---|
| `200`, `204`, `206` | Success — no retry | — |
| `408` | At least 1 retry always (attempt budget raised to minimum 1 if it was 0) | `eAAMPConfig_Http5XXRetryWaitInterval` (default **1,000 ms**) |
| `5xx` (excluding `502`) | Retry while `downloadAttempt < maxDownloadAttempt` | `eAAMPConfig_Http5XXRetryWaitInterval` (default **1,000 ms**) |
| `502` | Retry up to `DEFAULT_FRAGMENT_DOWNLOAD_502_RETRY_COUNT` = **1** time (independent of the normal attempt budget) | `eAAMPConfig_Http5XXRetryWaitInterval` (default **1,000 ms**) |
| Other `4xx` | No retry | — |

### Curl / Timeout Error Handling

| Condition | Media type | Retry behaviour |
|---|---|---|
| `CURLE_COULDNT_CONNECT` or `IsCurlTimeoutFailure()` | Manifest, playlist, audio | Always retry (`loopAgain = true`) |
| `CURLE_COULDNT_CONNECT` or `IsCurlTimeoutFailure()` | Init fragment | Retry; extends `maxDownloadAttempt` while `bufferDuration × 1000 > curlDownloadTimeoutMS` and still within the `maxInitDownloadTimeMS` window |
| `CURLE_COULDNT_CONNECT` or `IsCurlTimeoutFailure()` | Video/audio segment | Retry only if `bufferDuration > curlDownloadTimeout + fragmentDuration`; if video has partial data and bandwidth is far below current ABR profile, retry is suppressed |
| `eCURL_ABORT_REASON_LOW_BANDWIDTH_TIMEDOUT` | Any | **No retry** — caller drives ABR rampdown |
| `CURLE_FILE_COULDNT_READ_FILE` | Any | Translated to HTTP `404`; no retry |

`CURLE_PARTIAL_FILE` is also flagged in `HttpRequestEnd` logs with a connection-status marker `(0)`/`(1)` indicating whether any bytes were successfully sent before the failure.

---

## Retry Logic: `AampCurlDownloader` Path (DASH Background Manifest)

Used by `AampMPDDownloader` for background DASH MPD polling. Simpler loop in `AampCurlDownloader::Download()`:

| Error class | Max retries | Retry wait |
|---|---|---|
| HTTP `408` | 1 (forced minimum) | `iDownloadRetryWaitMs` (default **1,000 ms**) |
| HTTP `502` | `iDownload502RetryCount` — **10** for manifests (`MANIFEST_DOWNLOAD_502_RETRY_COUNT`), **1** for fragments (`DEFAULT_FRAGMENT_DOWNLOAD_502_RETRY_COUNT`) | `MIN_DELAY_BETWEEN_MANIFEST_UPDATE_FOR_502_MS` = **1,000 ms** |
| Other HTTP `5xx` | `iDownloadRetryCount` = `DEFAULT_DOWNLOAD_RETRY_COUNT` = **1** | `iDownloadRetryWaitMs` (default **1,000 ms**) |
| `CURLE_COULDNT_CONNECT` or timeout | `numRetriesAllowed` (same as above) | — |

The much higher 502 retry count for manifests (10 vs 1) reflects that manifest `502` errors are typically transient CDN propagation issues that resolve within seconds.

---

## Upper-Layer Failure Thresholds

After all per-download retries are exhausted, the track-level download loop continues but increments failure counters. When thresholds are breached, AAMP fires a tune failure event and stops playback:

| Counter | Threshold constant | Default | Action |
|---|---|---|---|
| `segDLFailCount` (media segments) | `eAAMPConfig_FragmentDownloadFailThreshold` | `MAX_SEG_DOWNLOAD_FAIL_COUNT` = **10** | `SendDownloadErrorEvent(AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE)` |
| `manifestDLFailCount` (HLS sub-manifest) | `MAX_MANIFEST_DOWNLOAD_RETRY` | **3** (`fragmentcollector_hls.h`) | `SendDownloadErrorEvent(AAMP_TUNE_MANIFEST_REQ_FAILED)` |
| MPD manifest failures | `MAX_MANIFEST_DOWNLOAD_RETRY_MPD` | **2** (`fragmentcollector_mpd.h`) | Same error event |
| Ad segment failures | `MAX_AD_SEG_DOWNLOAD_FAIL_COUNT` | **2** (`AampDefine.h`) | Ad playback failure event |

---

## Key Source Files

| File | Role |
|---|---|
| `priv_aamp.cpp` — `GetFile()` | Primary segment/manifest download + retry loop |
| `downloader/AampCurlDownloader.cpp` — `Download()` | DASH background manifest download + retry loop |
| `AampMPDDownloader.cpp` | DASH manifest orchestration; calls `AampCurlDownloader` |
| `AampUtils.cpp` — `GetCurlTimeoutFailureReason()`, `IsCurlTimeoutFailure()` | Timeout sub-classification |
| `fragmentcollector_hls.cpp` | HLS sub-manifest failure counting (`manifestDLFailCount`) |
| `fragmentcollector_mpd.cpp` | DASH manifest failure counting |
| `AampDefine.h` | All default timeout and retry constants |
| `priv_aamp.h` | Internal partial-download error codes (130–133) |
