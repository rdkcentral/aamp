# AAMP Download Infrastructure: Functional Comparison & Unification Plan

**Date:** 2026-05-01  
**Scope:** `priv_aamp.cpp::GetFile()` vs `downloader/AampCurlDownloader`

---

## 1. Architecture Summary

| Aspect | `GetFile` | `AampCurlDownloader` |
|--------|-----------|----------------------|
| Location | `priv_aamp.cpp` ~968 lines | `downloader/` ~1643 lines (downloader + store) |
| Curl handle ownership | Pooled via `CurlStore` (persistent, host-keyed) | Either takes a passed-in `CURL*` or creates its own ephemeral one |
| Buffer type | `AampGrowableBuffer*` | `std::vector<uint8_t>` in `DownloadResponse` |
| Thread safety | Relies on caller's serialization (one call at a time per curl instance) | Has internal `mCurlMutex`; `mDownloadActive` gate prevents re-entrant calls |
| Abort mechanism | `mDownloadsEnabled` + `mMediaDownloadsEnabled[]` checked in write callback via SSL callback on curl handle | `mDownloadActive = false` checked in `progress_callback`; no per-media-type disable |

---

## 2. Features in `GetFile` **Not** in `AampCurlDownloader`

1. **LL-DASH chunked injection** — The write callback (`HandleSSLWriteCallback`) detects MP4 box boundaries (`IdentifyMp4ChunkBoundary`), pushes partial chunks into the pipeline mid-download. `AampCurlDownloader::write_callback` just appends to a vector — no streaming injection.

2. **Per-media-type download abort** — `mMediaDownloadsEnabled[mediaType]` allows selectively stopping audio downloads while video continues. `CurlDownloader` only has a global `mDownloadActive` flag.

3. **ABR feedback loop** — After video segment download, `GetFile` calls `mhAbrManager.CheckAbrThresholdSize()` and `ReportDownloadComplete()` to feed bandwidth samples into ABR. `CurlDownloader` has no ABR awareness.

4. **Early abort for lowest-profile segments** — `context.earlyAbortEnabled` / `context.profileBps` to avoid aborting when already at floor bitrate.

5. **Low-bandwidth timeout disabled at lowest profile** — `lowBWTimeout` is zeroed out when `earlyAbortEnabled == false`.

6. **FOG TSB integration** — FOG-Reason header parsing, `mFogTSBEnabled` redirect detection (302), `mIsFirstRequestToFOG` handling, `fogError` output parameter.

7. **Anomaly events** — `SendAnomalyEvent()` on failure with FOG/CDN distinction; `X-Reason` and `FOG-Reason` header extraction.

8. **CMCD headers** — `CMCDGetHeaders()` appended per download; `CMCDSetNetworkMetrics()` fed back after completion.

9. **Content-Length validation** — After 200/206, checks `CURLINFO_CONTENT_LENGTH_DOWNLOAD_T` against actual buffer size; returns HTTP 416 and frees buffer on mismatch (non-gzip only).

10. **Iframe range repair** — `eAAMPConfig_RepairIframes`: strips range offset from full-object 200 response and re-parses box sizes.

11. **Harvest / recording** — `aamp_WriteFile()` to disk when harvest config is active.

12. **Net tracing instrumentation** — `#ifdef AAMP_NET_TRACE` per-request CSV logging.

13. **Profiler bucket tracking** — `profiler.ProfileBegin/End/Error(bucketType)`.

14. **Retry logic is buffer-aware** — For video, if partial data received and `downloadbps` vs `currentProfilebps` gap exceeds `BITRATE_ALLOWED_VARIATION_BAND`, retry is suppressed.

15. **Init-fragment extended retry** — Keeps retrying init segments while buffer depth > curl timeout and time budget remains.

16. **`eAAMPConfig_CurlHeader` URI parameter injection** — Appends `?param` to manifest URLs when curl header logging is enabled.

17. **Cookie recycling** — Reads `httpRespHeaders[curlInstance]` cookie and passes back via `CURLOPT_COOKIE`.

18. **Custom error codes** — `PARTIAL_FILE_START_STALL_TIMEOUT_AAMP`, `PARTIAL_FILE_CONNECTIVITY_AAMP`, `OPERATION_TIMEOUT_CONNECTIVITY_AAMP`, `PARTIAL_FILE_DOWNLOAD_TIME_EXPIRED_AAMP` — distinguishes abort cause.

19. **LL-DASH download delay tracking** — `mDownloadDelay` counter updated based on download time vs fragment duration thresholds.

20. **`ssl_callback`** — Checks `mDownloadsEnabled` during TLS handshake to abort early. `CurlDownloader`'s `updateCurlParams()` does not set `CURLOPT_SSL_CTX_FUNCTION`.

---

## 3. Features in `AampCurlDownloader` **Not** in `GetFile`

1. **HTTP 408 retry** — Explicitly handles 408 (Request Timeout) as retryable; `GetFile` only retries 5xx and timeouts.

2. **POST/DELETE request support** — `eCURL_POST`, `eCURL_DELETE` request types via `eRequestType` config.

3. **`bIgnoreResponseHeader` flag** — Can skip header callback entirely.

4. **`bNeedDownloadMetrics` gate** — Metrics collection is opt-in, reducing overhead for simple downloads.

5. **Re-entrant call protection** — `mDownloadActive` prevents double-invoke from concurrent threads without crashing.

6. **Cleaner struct-based config** — `DownloadConfig` is a value type; easy to copy, mock, and test.

7. **`GetDataString()` convenience** — Returns downloaded bytes as `std::string`.

---

## 4. Unification Plan

The goal is to drive all downloads through `AampCurlDownloader` while preserving every behavioral feature of `GetFile`, with zero regression risk. The strategy is **incremental substitution behind an adapter**, validated by existing L1 tests at each stage.

### Phase 0 — Preparatory gap-filling in `AampCurlDownloader` (no callers changed)

These are pure additions to `CurlDownloader`. Nothing in production calls the new hooks yet.

| Item | Work |
|------|------|
| **P0-1** | Add `ssl_callback` to `updateCurlParams()` — set `CURLOPT_SSL_CTX_FUNCTION` + data to enable mid-TLS abort. Needs `PrivateInstanceAAMP*` passed into config or downloader. |
| **P0-2** | Add per-media-type abort — add `std::function<bool()> isMediaEnabled` callback to `DownloadConfig`; call in `progress_callback` in addition to `mDownloadActive`. |
| **P0-3** | Add LL-DASH chunk streaming write callback — parameterize `write_callback` with an optional `onChunkReady` callback; default behavior (buffer accumulation) is unchanged. |
| **P0-4** | Add custom error codes on abort reason — map `eCURL_ABORT_REASON_*` → `PARTIAL_FILE_*` sentinel codes, exposed in `DownloadResponse`. |
| **P0-5** | **Backport HTTP 408 retry into `GetFile`** — trivial forward-port of the `CurlDownloader` retry logic into `GetFile` to eliminate this divergence before migration begins. |
| **P0-6** | Unit tests for all new `CurlDownloader` behaviour — each P0 item must have a corresponding L1 test before any production wiring proceeds. |

> **Note on P0-5:** This intentionally fixes `GetFile` first so the two are equivalent going into Phase 1, removing a potential regression vector.

---

### Phase 1 — Build a thin adapter: `GetFile` delegates to `CurlDownloader`

Create a new private method `GetFileViaDownloader()` that:

1. Populates a `DownloadConfig` from the same config reads `GetFile` uses today.
2. Calls `AampCurlDownloader::Download()`.
3. Maps `DownloadResponse` back to `GetFile`'s existing output parameters.
4. Runs all post-download processing: ABR feedback, harvest, anomaly events, profiler, CMCD metrics, iframe repair, content-length check.

`GetFile` itself becomes a thin wrapper:

```cpp
bool PrivateInstanceAAMP::GetFile(...)
{
    return GetFileViaDownloader(...);
}
```

**Key invariant:** All post-download processing (ABR, anomaly, harvest, CMCD metrics, profiler, iframe repair, content-length check) stays in `GetFile` / `GetFileViaDownloader`. It does **not** move into `CurlDownloader`. `CurlDownloader` remains focused on transport only.

**Validation:** All existing L1 tests must pass unchanged. Add new L1 tests for the adapter mapping.

---

### Phase 2 — Wire `CurlStore` pooled handles through `DownloadConfig`

`GetFile` today uses `GetCurlInstanceForURL()` which pulls from `CurlStore`. `CurlDownloader` today creates its own handle on `Initialize()`.

The adapter in Phase 1 populates `DownloadConfig::pCurl` (field already exists) with the pooled handle from `CurlStore`. `CurlDownloader` uses it as-is and does **not** call `curl_easy_cleanup` on it — the existing `mCreatedNewFd` flag already gates cleanup.

This preserves connection reuse and SSL session resumption behaviour exactly.

---

### Phase 3 — (Optional) Migrate manifest downloads to `GetFile`

`AampMPDDownloader` and HLS playlist downloaders currently call `AampCurlDownloader::Download()` directly. After Phase 1 they can be migrated to call `GetFile()` instead, giving a single code path for all downloads.

This is lower priority than the media-segment direction. Evaluate after Phase 2 is stable.

---

### Phase 4 — Extract `GetFile` post-processing into focused helpers

Once Phase 1 is stable, break the remaining orchestration logic into named private methods:

| Helper | Responsibility |
|--------|----------------|
| `BuildDownloadConfig()` | Construct `DownloadConfig` struct from AAMP configuration |
| `ApplyAbrFeedback()` | Post-download ABR reporting |
| `HandleDownloadError()` | Anomaly events, `X-Reason`/`FOG-Reason` header parsing, error code translation |
| `HarvestIfEnabled()` | Write response to disk |
| `RepairIframeIfNeeded()` | Range offset repair for iframe segments |

Each helper is independently unit-testable. `GetFile` becomes an orchestrator of ~30 lines.

---

### Phase 5 — Remove dead code

Once all callers go through the unified path and L2/L3 regression tests pass:

- The old direct `curl_easy_perform` loop in the original `GetFile` body is deleted.
- `GetFile` and `AampCurlDownloader` are now a single transport stack.

---

## 5. Risk Mitigation

| Risk | Mitigation |
|------|------------|
| LL-DASH chunk injection regression | Gated behind `lowLatencyMode` flag; L2 LL-DASH playback tests must pass before Phase 1 lands |
| ABR behaviour change | ABR feedback stays in post-processing layer; never moves into `CurlDownloader`; no change to ABR inputs |
| FOG TSB regression | FOG handling stays in adapter layer; dedicated L2 FOG tests required |
| `CurlStore` connection pooling disruption | Phase 2 passes pooled handle via existing `pCurl` field; no change to pool lifecycle |
| `mDownloadsEnabled` race during TLS | `ssl_callback` addition in P0-1 ensures abort during TLS handshake is preserved |
| Test coverage gaps | Each phase must be accompanied by passing L1 tests before merging; no phase proceeds without green CI |
