# Project Context

## Purpose

AAMP (Advanced Adaptive Media Player), also described as the Universal Video Engine (UVE), is a native media-player engine built on GStreamer. It supports adaptive streaming and playback workflows, including HLS, DASH, and progressive media, and integrates with platform sinks, DRM systems, subtitles, and time-shift-buffer features.

The tune flow coordinates manifest and playlist processing, media and license acquisition, pipeline configuration, stream injection, and startup through the first rendered frame. Tune-time work should be judged by its effect on end-to-end time to first frame, not only by the duration of one API.

## Tech Stack

- C++17, with CMake as the build system.
- GStreamer and GStreamer app APIs for pipeline, source, seek, state, and buffer handling. The top-level CMake configuration requires GStreamer 1.18 or newer.
- `libcurl`, `libxml2`, `libdash`, OpenSSL, cJSON, UUID, and threading support.
- GoogleTest, GoogleMock, and CTest for unit and microtests.
- Platform integrations include Rialto, platform/SOC sink interfaces, OpenCDM/DRM integrations, and Subtec subtitle support.
- Middleware timing can use RDKPerf. It is enabled by default in `middleware/CMakeLists.txt`; when enabled, configuration requires both `rdk_perf.h` and `librdkperf`. Use `-DENABLE_RDKPERF=OFF` to disable it.

There is no `package.json` or Python dependency manifest at the AAMP project root; CMake and the platform setup/install scripts describe build dependencies.

## Project Conventions

### Code Style

- Follow the formatting of the surrounding C++ source. Existing code commonly uses tab indentation and a mixture of established camel-case and legacy names.
- Use the project's logging macros, such as `MW_LOG_*` in middleware and `AAMPLOG_*` in AAMP orchestration code.
- Use existing profiling helpers such as `PI_PLAYER_PROFILE_SCOPE` rather than introducing a separate timing mechanism.
- No `.clang-format`, `.clang-tidy`, or `.editorconfig` was found at the project root. A single repository-wide formatting or linting rule is therefore **[Not clearly defined in repository]**.
- Add or update focused GoogleTest/GoogleMock tests for behavior changes, especially changes to virtual interfaces and middleware APIs.

### Architecture Patterns

- Top-level AAMP logic coordinates tune state, stream abstractions, and the `StreamSink` interface. HLS, DASH/MPD, and progressive stream collectors own format-specific behavior.
- `AAMPGstPlayer` adapts AAMP's sink API to the lower-level middleware player interface. `InterfacePlayerRDK` owns the GStreamer-facing operations, including pipeline state management, seeks, and per-stream source events.
- Keep format-specific policy in the relevant stream abstraction and pipeline mechanics in middleware. A middleware optimization should receive explicit intent through its interface rather than infer an AAMP event from a generic pipeline state.
- Platform-dependent behavior is routed through sink/SOC abstractions; do not assume Rialto and non-Rialto paths behave identically.

### Testing Strategy

- The project uses CMake-built GoogleTest/GoogleMock unit tests and CTest. The microtest guides recommend fast, isolated tests, mocks for external calls, and avoiding sleeps.
- AAMP tests are under `test/utests`; middleware tests are under `middleware/test/utests`. Each has a `run.sh` helper and CMake/CTest support.
- For tune/flush changes, relevant test areas include `PrivAampTests`, `AampGstPlayer`, `InterfacePlayerTests`, and `InterfacePlayerRDKTests`.
- Update fake implementations and mocks when changing a virtual API. Test both the intended early-tune case and non-early cases such as app seek, trickplay, teardown, and non-Rialto operation where supported.
- Measure both the API timing and end-to-end `configure_to_first_frame`; verify the resulting playback position, first rendered media, and audio/video synchronization.

### Git Workflow

`CONTRIBUTING.md` describes contributing through a GitHub fork and pull request and requires the RDK Contributor License Agreement. Branch naming, commit-message format, and review labels are **[Not clearly defined in repository]**.

## Domain Context

### Tune and early-flush sequence

In the tune path in `priv_aamp.cpp`, AAMP may call `DoEarlyStreamSinkFlush(newTune, rate)`. If it returns true, AAMP obtains the stream sink and calls `Flush` before `Start()` and `Stream()`. The target position is the updated seek position for progressive media or the first PTS for other media. This early call passes `shouldTearDown=false` so a pipeline that is not yet playing or paused is not destroyed.

`DoEarlyStreamSinkFlush` answers whether a flush is needed; its return value is not, by itself, proof that the call is an early new-tune flush. Implementations can request a flush in other contexts. Use the separate `newTune` intent when strict early-tune-only behavior is required.

### Middleware `Flush` behavior

In `middleware/InterfacePlayerRDK.cpp`:

1. `Flush` updates playback rate and clears relevant underrun/timer state.
2. `SetSeekPosition(position)` saves the requested time and marks each stream as having a pending seek.
3. If a pipeline exists, the method waits up to 100 ms in `gst_element_get_state` for a usable `PLAYING` or `PAUSED` state.
4. If the state is invalid, it returns false. When `shouldTearDown` is true, it also invokes teardown; otherwise the pending per-stream seek state remains available for later processing.
5. If the pipeline is ready, `ResetGstEvents()` clears per-stream pending flags and the middleware issues a pipeline-wide seek with `GST_SEEK_FLAG_FLUSH`. If that seek fails, the middleware re-arms pending seeks.
6. Later, `SendGstEvents` applies a pending seek to each stream's source with `GST_SEEK_FLAG_FLUSH`, using the saved position. The current implementation logs a failed source seek but clears that stream's pending flag afterward; preserve this behavior deliberately or test any change to its retry/error handling.

`AAMPGstPlayer::Flush` determines `isAppSeek` from AAMP's current state and forwards the request to `InterfacePlayerRDK`. It resets AAMP buffer-control state and correction rate only when the middleware call returns true. Treat the boolean return as part of the existing API contract; do not report a deferred or incomplete pipeline-level operation as success without reviewing its callers.

### Tune-time diagnostics and evidence

- `PI_PERF configure_to_first_frame <ms>` is the documented overall tune-duration metric. `player_flush` and nested middleware scopes help explain time spent inside the player, but nested timings must not be added together as if they were exclusive.
- Middleware diagnostic flush logs include entry/exit state, elapsed time, `shouldTearDown`, `isAppSeek`, sink type, pipeline-seek outcome, and audio/video pending-seek flags. Per-source logs include media type, target position, and seek result.
- In the captured `logging_tune` run dated 2026-11-25, 68 non-teardown flushes waited about 100-103 ms, remained `READY` with `PAUSED` pending, and skipped the pipeline-level seek. The matching set of deferred source-seek logs reported success for media types 0, 1, and 2 (68 per type). This is evidence from one run, not a guarantee for every platform or tune.
- That run also contained 9 slow teardown/state-invalid exits (about 687-851 ms), which are a separate path from the non-teardown 100 ms wait. Do not mix them when evaluating an early-flush optimization.
- The diagnostic run used the Rialto sink. Do not generalize its result to non-Rialto configurations without measurement.
- See the workspace's `../logging_tune/logs/` for captured diagnostics and `../profiling_logs/tunetime breakdown.md` for earlier profiling context. Keep run-specific findings separate from repository-wide behavior.

## Important Constraints

- A flush/seek change must preserve the requested playback position and the flushing behavior needed to discard stale queued media. Validate first rendered PTS, A/V sync, rapid retunes, error handling, and stream lifecycle.
- A strict early-tune optimization must be explicitly identified as early tune. `shouldTearDown == false`, a `READY`/`PAUSED` transition, or a request to flush does not alone identify that intent.
- Keep trickplay, app-seek, teardown/recovery, and ordinary flush behavior unchanged unless separately justified and tested.
- Do not equate time spent inside `Flush` with the same reduction in end-to-end tune time; measure `configure_to_first_frame` before and after.
- GStreamer behavior and supported platform dependencies vary by build target. The Ubuntu setup guide references a GStreamer 1.16.2 environment for a prebuilt `qtdemux`, while the top-level CMake file requires GStreamer 1.18 or newer; confirm the target image and plugin requirements when building.
- The project is distributed under Apache License 2.0; preserve applicable license and copyright headers.

## External Dependencies

- GStreamer runtime and development libraries, including GStreamer app APIs and platform-specific plugins/sinks.
- `libcurl` for network transfer, `libdash` and `libxml2` for DASH/manifest handling, OpenSSL, cJSON, UUID, and POSIX threading support.
- Rialto and platform/SOC integrations for device media pipelines; OpenCDM and related DRM/license integrations for protected content.
- Subtec and related subtitle components where enabled.
- RDKPerf for optional middleware API timing.
- GoogleTest/GoogleMock for development and test builds.

Exact runtime versions and availability depend on the target platform and are **[Not clearly defined repository-wide]**.

## Additional Notes

- The primary build and platform guidance is in `README.md`, `UbuntuSetup.md`, `middleware/CMakeLists.txt`, and the install scripts.
- The tune-time CSV schema and RDKPerf notes are documented in `README.md` under “AAMP Tunetime”.
- `README.md` says this branch does not contain a first-frame timeout callback; do not expect a `first_frame_timeout` metric from this branch.
- No code changes are implied by this context document; use it as project guidance and verify current source and target-device logs for behavior-specific work.
