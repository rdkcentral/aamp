# Codebase Concerns

**Analysis Date:** 2026-06-08

## Tech Debt

**God-class PrivateInstanceAAMP:**
- Issue: `PrivateInstanceAAMP` is a 19,000-line monolith across `priv_aamp.h` (4,356 lines) and `priv_aamp.cpp` (15,056 lines). It handles DRM, buffering, network, event dispatch, playback state, ABR, TSB, subtitles, and more in a single class.
- Files: `priv_aamp.h`, `priv_aamp.cpp`
- Impact: Near-impossible to reason about thread safety across the entire surface; every feature addition worsens the problem; compilation is slow.
- Fix approach: Incremental extraction of cohesive responsibilities (e.g., DRM, buffer management, position tracking) into smaller focused classes that `PrivateInstanceAAMP` delegates to.

**Large fragment collector files:**
- Issue: Core stream processing files are extremely large: `fragmentcollector_mpd.cpp` at 14,696 lines, `fragmentcollector_hls.cpp` at 7,603 lines, `streamabstraction.cpp` at 4,731 lines.
- Files: `fragmentcollector_mpd.cpp`, `fragmentcollector_hls.cpp`, `streamabstraction.cpp`
- Impact: Difficult to review, test, and maintain; makes onboarding slow; increases merge conflict surface.
- Fix approach: Extract discrete responsibilities (MPD period handling, timeline processing, segment template resolution, ABR interaction) into separate translation units.

**Misspelled public config key (API-breaking fix):**
- Issue: Config key `"mpdStichingSupport"` is a permanent public API spelling mistake (missing 't' in "Stitching"). Changing it would break existing caller configs.
- Files: `AampConfig.cpp:356`, `AampConfig.h:210`, `priv_aamp.cpp:14360`, `AampMPDDownloader.h:77`
- Impact: Any external caller using the correctly-spelled string will silently fail.
- Fix approach: Add the corrected key `"mpdStitchingSupport"` as an alias while keeping the original; deprecate original.

**Deprecated legacy event listener retained indefinitely:**
- Issue: `AAMPEventListener` is explicitly marked `LEGACY` and "to be deprecated" since at least the introduction of `AAMPEventObjectListener`, but both still coexist with active conversion code.
- Files: `AampEventListener.h:53-55`, `AampEventListener.cpp:263`, `AampEvent.h:256`
- Impact: Dual code paths for every event dispatch; increased test surface; confusion about which path new code should use.
- Fix approach: Audit all callers of `AAMPEventListener::Event(AAMPEvent&)`, migrate to `AAMPEventObjectListener`, remove the conversion layer in `AampEventListener.cpp`.

**Deprecated `aamp_GetCurrentTimeMS()` still widely used:**
- Issue: `aamp_GetCurrentTimeMS()` is documented in `AampUtils.h` with `//TODO: Use NOW_STEADY_TS_MS/NOW_SYSTEM_TS_MS instead` but is called ~60 times throughout the codebase.
- Files: `AampUtils.h:108`, and ~60 call sites across production code.
- Impact: Inconsistent time source usage; the `NOW_STEADY_TS_MS` macro is the preferred monotonic source.
- Fix approach: Replace each call site with the appropriate macro; remove the function.

**Duplicate `cJSON_GetNumberValue` workaround:**
- Issue: Both `AampJsonObject.cpp:468` and `middleware/drm/DrmJsonObject.cpp:468` and `middleware/playerJsonObject/PlayerJsonObject.cpp:467` contain the same `// TODO: replace with cJSON_GetNumberValue(strObj)` comment, indicating a workaround was copy-pasted rather than shared.
- Files: `AampJsonObject.cpp:468`, `middleware/drm/DrmJsonObject.cpp:468`, `middleware/playerJsonObject/PlayerJsonObject.cpp:467`
- Impact: Three independent code paths doing the same thing; fixing the underlying issue requires three edits.
- Fix approach: Consolidate into a shared JSON utility; update `cJSON` version if `cJSON_GetNumberValue` is now available.

**Unresolved CMakeLists public header decision:**
- Issue: `CMakeLists.txt:693` contains `#TODO: Decide which are the actual public headers` above the install target, meaning internal implementation headers may be inadvertently installed.
- Files: `CMakeLists.txt:693-703`
- Impact: Consumers may take dependencies on private headers that could change without notice.
- Fix approach: Audit installed headers against the public API; remove internal headers from the install target.

**Incomplete TSB text track buffer control:**
- Issue: `AampTrackWorkerManager.cpp:241` notes that text track injection is excluded from proper buffer control logic with a temporary exemption.
- Files: `AampTrackWorkerManager.cpp:241-242`
- Impact: Buffer management for subtitle/text tracks is uncontrolled, which can cause memory pressure during long-duration playback.
- Fix approach: Apply the same `AampBufferControl` logic to text track workers that is used for audio/video.

**`volatile std::atomic` misuse:**
- Issue: `priv_aamp.h:1038` declares `volatile std::atomic<long long> mPausePositionMilliseconds`. `volatile` is redundant (and potentially harmful) on `std::atomic` — `std::atomic` already provides sequential consistency.
- Files: `priv_aamp.h:1038`
- Impact: May suppress compiler optimisations without providing additional safety; signals a misunderstanding of atomics.
- Fix approach: Remove `volatile`; use `std::atomic<long long>` directly.

## Known Bugs

**Ad Manager period offset negative value bug (test-documented):**
- Symptoms: In `admanager_mpd.cpp`, `(int)(currPeriodDuration - abObj.endPeriodOffset)` can produce a negative result that currently causes a test to pass for the wrong reason.
- Files: `admanager_mpd.cpp`, `test/utests/tests/AdManagerMPDTests/FunctionalTests.cpp:2345`
- Trigger: Ad break placement when the content period duration is shorter than the ad's `endPeriodOffset`.
- Workaround: None; the test masks the bug.

**`MediaStreamContext::CacheFragmentData()` Phase 2 stub always returns false:**
- Symptoms: Any call to `CacheFragmentData()` logs a WARN and returns `false`. This is a Phase 2 stub pending Phase 3 implementation.
- Files: `MediaStreamContext.cpp:277-283`
- Trigger: Any code path that invokes unified fragment caching.
- Workaround: Callers must not rely on this method for correctness.

**Possible fragment cache memory leak in `MediaTrack`:**
- Symptoms: `streamabstraction.cpp:1741` logs `"fragment.ptr[…] already set - possible memory leak"` when `initialize` is true but the cached fragment slot already holds data.
- Files: `streamabstraction.cpp:1741`
- Trigger: Under specific retune or seek conditions where a cached slot is reused without being cleared first.
- Workaround: Data is cleared after the log, but the prior allocation is not explicitly accounted for.

**Floating point epsilon `FLOATING_POINT_EPSILON = 0.1` acknowledged as too large:**
- Symptoms: In `fragmentcollector_mpd.cpp:6019`, a comment explicitly states "0.1 seems too big an epsilon" for a seek-point comparison that can reach values like `-0.088400000000000006`.
- Files: `fragmentcollector_mpd.cpp:6019-6022`, `AampUtils.h:63`, `isobmff/isobmffprocessor.cpp:29`
- Trigger: Subtitle init during seeks near period boundaries.
- Workaround: Current code does not return early when it should, potentially initialising the subtitle parser at a wrong position.

**OpenCDM `signalled` flag race condition (acknowledged):**
- Symptoms: `opencdmsessionadapter.h:46` documents `// TODO: added to handle the events fired before calling wait, need to recheck` for a `bool signalled` field used to handle events arriving before `wait()`.
- Files: `middleware/drm/ocdm/opencdmsessionadapter.h:46`
- Trigger: DRM key response arrives before the session thread reaches its wait state.

**Subtitle parser uses position instead of PTS:**
- Symptoms: `priv_aamp.cpp:14520` notes subtitle clock update is not implemented for the subtitle parser which uses position rather than PTS, causing potential sync drift.
- Files: `priv_aamp.cpp:14520`
- Trigger: Live stream or trick play scenarios where position and PTS diverge.

**DASH MPD `SegmentList` merging not implemented:**
- Symptoms: Two locations in `MPDModel.cpp` skip SegmentList merging with "not sure how to do it, ignore now."
- Files: `dash/mpd/MPDModel.cpp:1396`, `dash/mpd/MPDModel.cpp:2077`
- Trigger: MPD manifests that use SegmentList at both `AdaptationSet` and `Representation` level.
- Workaround: Only SegmentList from the leaf level is used; inherited SegmentLists are silently dropped.

**PlayReady multi-key support not implemented:**
- Symptoms: `PlayReadyHelper.cpp:126,143` notes only the first `keyId` is taken from multi-key content.
- Files: `middleware/drm/helper/PlayReadyHelper.cpp:126,143`
- Trigger: PlayReady-encrypted content with multiple key IDs (common in multi-period DASH).

**Hardcoded aspect dimensions in SecManager session:**
- Symptoms: `SecManagerThunder.cpp:166-168` hardcodes `width=1920, height=1080` with a `// TODO: Remove hardcoded values` note.
- Files: `middleware/externals/contentsecuritymanager/SecManagerThunder.cpp:166-168`
- Trigger: Content security sessions on non-1080p output devices.

## Security Considerations

**SSL peer verification disabled by default:**
- Risk: `eAAMPConfig_SslVerifyPeer` defaults to `false` (see `AampConfig.cpp:271`), meaning both `CURLOPT_SSL_VERIFYHOST` and `CURLOPT_SSL_VERIFYPEER` are set to 0 for all downloads unless the application explicitly enables it.
- Files: `AampConfig.cpp:271`, `priv_aamp.cpp:4550-4553`, `downloader/AampCurlDownloader.cpp:617-620`
- Current mitigation: Application can call `PlayerInstanceAAMP::SetSslVerifyPeerConfig(true)`.
- Recommendations: Change the default to `true`; require explicit opt-out rather than opt-in for TLS verification.

**WideVine KID URL workaround parsed from URL query string:**
- Risk: DRM behaviour (which key location is used) is determined by whether the URL contains the literal string `"WideVineKIDWorkaround"`. A malicious URL could trigger or suppress this path.
- Files: `priv_aamp.cpp:13430-13442`, `AampConfig.h:176`
- Current mitigation: Operator configuration only; URL is typically controlled by the CDN.
- Recommendations: Move DRM configuration to a secure out-of-band channel rather than parsing the content URL.

**Session token stored in plain `std::string`:**
- Risk: `mSessionToken` in `PrivateInstanceAAMP` is a plain `std::string` member with no secure memory management (not zeroed on destruction).
- Files: `priv_aamp.h:1140`
- Current mitigation: None.
- Recommendations: Use a secure string type that zeroes memory on destruction, or ensure the token lifetime is as short as possible.

**Harvest/dump debug features in production binary:**
- Risk: `eAAMPConfig_HarvestConfig` / `eAAMPConfig_HarvestPath` allow writing raw media segments and manifests to the filesystem at runtime; `ENABLE_DUMP` (commented-out macro) would dump DRM PSSH data.
- Files: `priv_aamp.cpp:5245-5254`, `priv_aamp.cpp:13449`, `AampConfig.cpp:205,395-396`
- Current mitigation: Harvest config defaults to 0/empty so it is off by default; `ENABLE_DUMP` is commented out.
- Recommendations: Guard harvest features behind a compile-time `AAMP_DEVELOPER_BUILD` flag; never ship with `ENABLE_DUMP` enabled.

## Performance Bottlenecks

**`recursive_mutex` on hot download path:**
- Problem: `PrivateInstanceAAMP::mLock` is a `std::recursive_mutex` acquired/released hundreds of times per second on the download path (manifest refresh, fragment injection).
- Files: `priv_aamp.cpp` (dozens of `lock_guard<recursive_mutex>` sites), `priv_aamp.h`
- Cause: Recursive mutex is required because the same thread re-enters the lock; indicates that the locking strategy was not designed upfront.
- Improvement path: Audit re-entrant paths; replace recursive sections with proper RAII unlock-then-relock patterns, then downgrade to `std::mutex`.

**`usleep` on network persona TTFB path:**
- Problem: `priv_aamp.cpp:4701,4743` use raw `usleep()` in the curl write callback to simulate network latency, blocking the curl thread in 50ms chunks.
- Files: `priv_aamp.cpp:4680-4744`
- Cause: Network persona simulation (test-only feature) directly throttles the real download thread.
- Improvement path: Network persona should operate at a higher level (e.g., post-download sleep) rather than blocking curl's I/O thread.

**`~60` call sites using `aamp_GetCurrentTimeMS` (system clock):**
- Problem: The function uses `std::chrono::system_clock` which can jump on wall-clock adjustments. The preferred `NOW_STEADY_TS_MS` uses `steady_clock`.
- Files: `AampUtils.h:108` and ~60 callers across production sources.
- Cause: Historical API predates the macro alternatives.
- Improvement path: Systematically replace with `NOW_STEADY_TS_MS` or `NOW_SYSTEM_TS_MS` as appropriate for each call site.

## Fragile Areas

**HLS discontinuity handling (`FIXME!` marked):**
- Files: `fragmentcollector_hls.cpp:2724`, `fragmentcollector_hls.cpp:2756`, `fragmentcollector_hls.cpp:4094`, `fragmentcollector_hls.cpp:5887`
- Why fragile: Four distinct sites in the HLS discontinuity logic are explicitly marked `FIXME!` without elaboration, indicating known correctness gaps.
- Safe modification: Any change to HLS discontinuity index construction or `CheckDiscontinuityAroundPlaytarget` must be validated against the full suite at `test/utests/tests/StreamAbstractionAAMP_HLS/FunctionalTests.cpp`.
- Test coverage: Partial — functional tests exist but the FIXME sites suggest gaps.

**Known Coverity data race conditions (annotated but not resolved):**
- Files: `priv_aamp.cpp:2234,2246,2312,2330` (CID:306170), `fragmentcollector_mpd.cpp:1289` (CID:328774), `fragmentcollector_mpd.cpp:9690` (CID:190371), `fragmentcollector_hls.cpp:2958` (CID:335490)
- Why fragile: Coverity flagged these as data race conditions; they are annotated but the underlying race has not been fixed. CID:306170 involves `mIsPeriodChangeMarked` read/write across threads.
- Safe modification: Do not add new unsynchronised accesses near these areas; ensure any refactoring adds proper locking.
- Test coverage: None targeting these specific races.

**Progressive playback collector is largely unfinished:**
- Files: `fragmentcollector_progressive.cpp:48-67`
- Why fragile: The file header documents ~15 unimplemented TODOs including: valid duration reporting, progress events, ABR ramp-down signalling, pause/play testing, trickplay (must return error), profiling, and user-agent configuration.
- Safe modification: Do not enable progressive playback in production without addressing the TODO list; changes here are unlikely to have test coverage.
- Test coverage: Not detected — no unit tests for `StreamAbstractionAAMP_Progressive`.

**WebVTT parser: CUE ID and cue settings not parsed:**
- Files: `subtitle/webvttParser.cpp:226,278`
- Why fragile: The WebVTT parser silently ignores CUE IDs and cue settings (positioning, alignment), so any content that depends on these will display incorrectly without errors.
- Safe modification: Adding parsing here requires understanding the downstream `vttCue.h` data model.

**RMF shim is a stub for CC and audio track selection:**
- Files: `rmf_shim.cpp:257,266,283`
- Why fragile: Three RMF features (audio track selection ×2, closed captions) are explicit placeholders. Callers that rely on RMF providing these capabilities will silently receive no result.
- Test coverage: Not detected in `test/` tree.

**DRM license acquisition race condition (acknowledged, not fixed):**
- Files: `AampDRMLicPreFetcher.cpp:433`
- Why fragile: The TODO explicitly acknowledges an unresolved race between license acquisition and adding items to the fetch queue.
- Safe modification: Changes to `mLicenseAcquisitionMutex` lock scope must account for this.

**`AampTrackWorkerManager` text track exclusion:**
- Files: `AampTrackWorkerManager.cpp:241`
- Why fragile: The text track is excluded from the "wait for worker job completion" logic with a TODO. This creates a window where the main thread may proceed while a subtitle job is still in progress.

## Scaling Limits

**Single global `gpGlobalConfig` singleton:**
- Current capacity: One `AampConfig` instance shared across all `PlayerInstanceAAMP` instances in a process.
- Limit: Per-player configuration overrides work via a layered precedence system, but global defaults are shared state. Race conditions are possible if multiple players are created concurrently and modify global config.
- Files: `main_aamp.cpp:45,109-142`, `AampConfig.h:782`
- Scaling path: Make each player hold its own fully-resolved config snapshot at construction time.

**`DrmInterface::mInstance` static singleton:**
- Current capacity: One DRM interface per process.
- Limit: Prevents concurrent multi-DRM or multi-player configurations that need independent DRM interfaces.
- Files: `drm/DrmInterface.h:76`, `drm/DrmInterface.cpp:200,207`
- Scaling path: Refactor to a factory pattern; scope DRM interface lifetime to the player instance.

## Dependencies at Risk

**Deprecated DRM systems retained in enum:**
- Risk: `DrmSystems.h` retains `eDRM_CONSEC_agnostic` (deprecated), `eDRM_Adobe_Access` (fully deprecated), and `eDRM_Vanilla_AES` (fully deprecated) in the live enum.
- Files: `middleware/drm/DrmSystems.h:38-40`
- Impact: Any switch-case over `DrmSystems` that lacks a `default:` handler will silently succeed with no-op for these values; expanding the enum is error-prone.
- Migration plan: Remove deprecated entries after auditing all switch statements; use a versioned API if backward compatibility is required.

**Thunder OCDM API 0.2 compatibility shim:**
- Risk: `USE_THUNDER_OCDM_API_0_2` ifdefs appear in 11 locations across the OCDM adapter, indicating support for an older API version is maintained in parallel.
- Files: `middleware/drm/ocdm/opencdmsessionadapter.cpp`, `middleware/drm/ocdm/opencdmsessionadapter.h`, `middleware/drm/ocdm/OcdmGstSessionAdapter.cpp`
- Impact: Each OCDM API call has two code paths; drift between them is an ongoing maintenance burden.
- Migration plan: Establish a minimum supported Thunder OCDM version; remove the `USE_THUNDER_OCDM_API_0_2` branches once old platforms are retired.

## Missing Critical Features

**Progressive playback ABR not implemented:**
- Problem: `fragmentcollector_progressive.cpp:61-62` notes that available bitrates are undefined and trickplay must return an error. There is no ABR ramp-down when bandwidth is insufficient.
- Blocks: Reliable progressive MP4/MP3 playback under poor network conditions.

**PlayReady multi-key content not supported:**
- Problem: `PlayReadyHelper.cpp:126,143` — only the first `keyId` is extracted. Multi-period DASH with per-period keys will fail to decrypt periods after the first.
- Blocks: PlayReady-encrypted multi-period live streams.

**Closed captions for RMF streams not implemented:**
- Problem: `rmf_shim.cpp:283` — CC for RMF is a placeholder.
- Blocks: Accessibility compliance for RMF-sourced content.

**Unified fragment cache (`CacheFragmentData`) not implemented:**
- Problem: `MediaStreamContext::CacheFragmentData()` (Phase 2 stub) always returns `false`. Phase 3 implementation is pending.
- Files: `MediaStreamContext.cpp:277-283`
- Blocks: The architectural goal of unified fragment caching across HLS/DASH/progressive.

## Test Coverage Gaps

**No tests for `StreamAbstractionAAMP_Progressive`:**
- What's not tested: Pause/play, progress events, bandwidth throttling, trickplay error responses, duration reporting.
- Files: `fragmentcollector_progressive.cpp` (no corresponding test file found under `test/utests/`)
- Risk: Any regression in progressive playback goes undetected.
- Priority: High

**Tests directly access private members via FIXME violations:**
- What's not tested (correctly): `PlayerInstanceAAMPTestsMain.cpp` and `PauseOnPlaybackTests.cpp` access `mPlayerInstance->aamp` directly, bypassing encapsulation with explicit `FIXME` comments.
- Files: `test/utests/tests/PlayerInstanceAAMP/PlayerInstanceAAMPTestsMain.cpp:492,1314,1416,1424,1432,1441`, `test/utests/tests/PlayerInstanceAAMP/PauseOnPlaybackTests.cpp:51,102,119,136`
- Risk: Tests may not reflect real behaviour if the private interface changes; they would silently continue to compile via friend or public exposure.
- Priority: Medium

**LocalTSB test has commented-out mock setup:**
- What's not tested: `LocalTSBTests.cpp:60` has `//mPrivateInstanceAAMP->mStreamSink = g_mockAampGstPlayer; //TODO fix` — the GStreamer sink mock is not wired up, leaving TSB playback pipeline interactions untested.
- Files: `test/utests/tests/PrivateInstanceAAMP/LocalTSBTests.cpp:60`
- Risk: TSB sink interactions are not covered.
- Priority: Medium

**Ad manager period offset negative value bug is masked by a test:**
- What's not tested: The correct behaviour when `currPeriodDuration < endPeriodOffset` — the test currently passes because of the bug, not despite it.
- Files: `test/utests/tests/AdManagerMPDTests/FunctionalTests.cpp:2345`
- Risk: Fixing the bug could break the test; the intended correct value for `endPeriodOffset` is not asserted.
- Priority: High

**MPD FunctionalTests contain multiple RAII TODO refactors:**
- What's not tested: Cleanup paths in XML reader tests are not RAII-safe — leaks will occur if a test assertion fails mid-test.
- Files: `test/utests/tests/StreamAbstractionAAMP_MPD/FunctionalTests.cpp:3348,3414,3477,3543`
- Risk: Test memory leaks can mask real issues in CI memory-check runs.
- Priority: Low

---

*Concerns audit: 2026-06-08*
