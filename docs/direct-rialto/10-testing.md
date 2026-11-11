# Direct Rialto - Testing

## Purpose

This page explains how direct-rialto is tested: the L1 (unit) suites and the seams that make them possible, how to build and run them, the Rialto simulator that stands in for a real server, the L2 integration tests that drive `aamp-cli`, which components have no dedicated test, and what the test set cannot currently show. Read it before changing direct-rialto code, so that you add tests in the right place and run the right suites.

## Source files

| File | Role |
| --- | --- |
| `test/utests/tests/AampRialtoPlayerTests/` | 334 tests in eight topic files plus a shared fixture; the player with real collaborators and mocked Rialto |
| `test/utests/tests/AampRialto*Tests/`, `AampPlayerStateMachineTests/`, `AampDrmBridgeTests/`, `PrivateInstanceAAMPNotifiableTests/`, `RialtoMediaKey*Tests/` | One gtest executable per direct-rialto class (see the inventory) |
| `test/utests/fakes/FakeAampRialto*.cpp`, `FakeRialtoFactories.cpp`, `FakeRialtoSessionCreator.cpp`, `FakeAampDrmBridge.cpp`, `FakeAampPlayerStateMachine.cpp`, `FakePrivateInstanceAAMPNotifiable.cpp` | Link-time replacements in the shared `fakes` library |
| `test/utests/mocks/MockAampRialtoMediaSource.h`, `MockIRialtoControlBackend.h`, `MockRialtoMediaKeys.h`, `MockIStreamSinkNotifiable.h`, `MockDrmBridge.h`, `MockIMediaPipeline*.h`, `MockIControlFactory.h`, `MockGLib.h` | gmock objects |
| `test/utests/ReadMe.md`, `test/utests/run.sh`, `.github/instructions/l1-*.instructions.md` | Build and run workflow |
| `test/rialto/RialtoSimulator.cpp`, `test/rialto/CMakeLists.txt`, `test/rialto/README.md` | In-process Rialto simulator |
| `aamp_test_internal/test/l2test/AAMP-RIALTO-10000_Tune`, `10001_Tune_HLS`, `10002_TrickModes` | L2 suites dedicated to direct-rialto |
| `aamp_test_internal/test/l2test/framework.py`, `run_l2_aamp.py`, `README.md` | L2 framework |
| `aamp_test_internal/test/Rialto/README.md`, `startmanager.sh` | Real Rialto server manager for manual runs (rialto-gstreamer oriented README) |

## Test levels at a glance

[[diagram:direct-rialto-test-architecture-component]]

*L1 replaces everything around one class; L2 runs the real `aamp-cli` against either the simulator or a real Rialto server.*

| Level | What is real | What is faked or simulated | Where it runs |
| --- | --- | --- | --- |
| L1 | The class under test (its `.cpp` is compiled into the test executable) and, in the player suite, its real helpers | Rialto API (`IMediaPipeline` mocks), AAMP core (`PrivateInstanceAAMP`, `AampConfig`), GLib timers, DRM | `test/utests`, in CI (`.github/workflows/L1-tests.yml`) |
| L2 simulator | All AAMP code including all of direct-rialto, real HTTP streams | The Rialto client library (`RialtoSimulator`): no decode, no DRM | `aamp_test_internal/test/l2test` (internal repo) |
| L2 real Rialto | AAMP plus a real Rialto client and server | Nothing; needs a running server manager | Manual, `--aamp_video` |

## L1 unit tests

### Structure and inventory

Each direct-rialto class has its own directory under `test/utests/tests/`, named `<Class>Tests`, with its `.cpp` compiled into the executable (`set(AAMP_SOURCES ...)` in the suite's `CMakeLists.txt`) and everything else coming from the shared `fakes` library. The player suite is the exception: it compiles the player and eleven real direct-rialto collaborators, so it tests orchestration with real policy objects around a mocked Rialto.

| Suite | Tests | Real code compiled in | Notes |
| --- | --- | --- | --- |
| `AampRialtoPlayerTests` | 334 | Player, session, media source, handshake, controller, segment position, pre-roll monitor, subtitle source, TTML offset, state machine, notifiable, AV monitor | Files by topic: configure, attach, injection, flush, playback, position, DRM, AV settings |
| `AampPlayerStateMachineTests` | 60 | `AampPlayerStateMachine.cpp` | Transition table |
| `AampRialtoVideoSourceTests` | 77 | `AampRialtoMediaSource`, handshake, video source | Effectively the base-class suite (attach with DRM, injection, EOS, codec data) |
| `AampRialtoAudioSourceTests` | 20 | Base, handshake, audio source | Codec mapping, segment creation |
| `AampRialtoSubtitleSourceTests` | 21 | Base, handshake, subtitle source, TTML offset | Raw and MP4 paths |
| `AampRialtoNeedDataHandshakeTests` | 25 | Handshake | Pure state logic |
| `AampRialtoPlaybackControllerTests` | 32 | Controller | Holds and play dispatch |
| `AampRialtoSegmentPositionTests` | 13 | Segment position | |
| `AampRialtoPreRollMonitorTests` | 21 | Pre-roll monitor | Injected clock and probe |
| `AampRialtoMonitorAVTests` | 22 | AV monitor | |
| `AampRialtoPipelineSessionTests` | 10 | Session | |
| `AampRialtoMediaPipelineClientTests` | 15 | Client | |
| `AampRialtoControlBackendTests` | 10 | Control backend | |
| `AampRialtoGlibTimerTests`, `AampRialtoProgressTimerTests`, `AampRialtoTimeTests`, `AampRialtoTtmlOffsetTests` | 15, 8, 4, 14 | Header-only or small classes | |
| `AampDrmBridgeTests` | 5 | `AampDrmBridge.cpp` | |
| `PrivateInstanceAAMPNotifiableTests` | 23 | Notifiable | |
| `RialtoMediaKeySystemTests`, `RialtoMediaKeySessionTests`, `RialtoMediaKeySessionAdapterTests` | 15, 14, 13 | The DRM classes | `MockRialtoMediaKeys.h` |

Counts are the number of `TEST`/`TEST_F` macros found in each directory. Related tests outside these directories also touch the path: `ConfigTests` (reverting dynamic `useDirectRialto`, keeping a device-config value), `StreamAbstractionAAMP_MPD/subtitleTests.cpp` (`SelectSubtitleTrack_DirectRialto_DoesNotStopTrackDownloads` and the non-direct counterpart), `PrivAampTests` (`IsUsingRialto()` combinations), `AampMp4DemuxTests`.

### Seams and why they exist

| Seam | Mechanism | Why |
| --- | --- | --- |
| Notifications | `IStreamSinkNotifiable` injected through the second player constructor; `MockIStreamSinkNotifiable` | Assert what the sink tells core without a full `PrivateInstanceAAMP` |
| Control backend | `std::unique_ptr<IRialtoControlBackend>` constructor argument; `MockIRialtoControlBackend::waitForRunning` | Avoid needing an `IControl` and a server |
| Per-track sources | `SourceCreator` lambda returning `NiceMock<MockAampRialtoMediaSource>` | The mock overrides only the virtual hooks (`mapCodecToMime`, `createRialtoSource`, `createSegment`, `injectSingleSample`, `processDataFragment`, counters). Non-virtual base logic (handshake, `attachOrUpdate`) still runs |
| Rialto pipeline | `FakeRialtoFactories.cpp` defines `IMediaPipelineFactory::createFactory()` (and the capabilities factory) to return global mocks (`g_mockPipelineFactory`); `MockIMediaPipeline` | The production constructor has no parameter for the factory, so the seam is link-time |
| DRM bridge | `FakeAampDrmBridge.cpp` forwards to `g_mockDrmBridge` | Same reason |
| Rialto events | `FakeAampRialtoMediaPipelineClient.cpp` keeps the real forwarding behaviour; the fixture posts events (`PostPlaybackState`, `PostNeedData`, ...) through the captured client | Tests drive the player the way Rialto does |
| GLib timers | `MockGLib`: the fixture captures the `g_timeout_add` callback and invokes it on demand | Deterministic progress, pre-roll and AV-monitor ticks |
| Time | `AampRialtoPreRollMonitor` accepts a `Clock`; the player exposes `SetFlushCompleteTimeout()` | Avoid real waits |
| Core | `MockPrivateInstanceAAMP`, `MockAampConfig`, `MockPlayerCCManager` | The player still reads config and calls a few `PrivateInstanceAAMP` methods directly |
| Link stubs for other suites | `FakeAampRialtoPlayer.cpp`, `FakeAampRialtoPipelineSession.cpp`, `FakeAampRialtoMonitorAV.cpp`, `FakeAampRialtoSourceCreators.cpp`, `FakeAampRialtoControlBackend.cpp`, `FakePrivateInstanceAAMPNotifiable.cpp`, `FakeRialtoSessionCreator.cpp`, `FakeAampPlayerStateMachine.cpp` | Let suites that compile `AampStreamSinkManager.cpp` or `priv_aamp.cpp` link without the real Rialto code |

Important consequences of the fakes: fake behaviour intentionally differs from production (for example `makeRialtoSessionCreator()` is a null creator, `makeDefaultSourceCreator()` yields null sources). Assert on how the class under test reacts, not on what a fake returns.

### Building and running

The only approved workflow is in `.github/instructions/l1-build-run.instructions.md`.

| Goal | Command |
| --- | --- |
| First build, or after adding a new suite directory or after a rebase | `cd test/utests && ./run.sh` (builds the tree and runs everything; do not substitute a manual cmake) |
| Inner loop for one suite | `cd test/utests/build/tests/AampRialtoPlayerTests && make && ./AampRialtoPlayerTests` |
| One test by name | add `--gtest_filter=...` to the executable, or `ctest -R <regex>` from the build directory |
| Coverage | `./run.sh -c` (needs `install-aamp.sh -c` first); report in `CombinedCoverage/index.html` |
| Combined JSON report | `./run.sh -e` |
| Longer per-test timeout | `./run.sh -t <seconds>` (default 60) |

Prerequisites: `install-aamp.sh` has installed the dependent headers, GoogleTest/GoogleMock and `jq`; for direct-rialto that includes the Rialto public headers copied to `.libs/include/rialto` (the CI image does this without building Rialto). Do not delete `test/utests/build/`. Typical failure causes: `No rule to make target '<Name>Tests'` (run `run.sh`), link errors (a real and a fake source compiled together, or `fakes` not first in `target_link_libraries`).

Adding a suite: create `tests/<Component>Tests/` with `<Component>Tests.cpp`, test-case files and a `CMakeLists.txt` that follows an existing one (`EXEC_NAME` equal to the directory name, `include(CommonTestIncludes.cmake)`, `AAMP_SOURCES`, `fakes` first when linking, `aamp_utest_run_add`), add `add_subdirectory(...)` to `tests/CMakeLists.txt`, then run `./run.sh`. Use the L1 instructions files for fakes, oracles and review.

## RialtoSimulator

`test/rialto/RialtoSimulator.cpp` builds a shared library named `libRialtoClient` (`OUTPUT_NAME RialtoClient`, version 0.1.0, SOVERSION 0) that implements the Rialto client API in process, so AAMP can run the whole direct-rialto path with no server.

What it provides:

| Rialto API | Simulator behaviour |
| --- | --- |
| `IControlFactory` / `IControl` | `registerClient()` immediately reports `ApplicationState::RUNNING` |
| `IMediaPipelineFactory` / `IMediaPipeline` | `load`, `attachSource`, `allSourcesAttached`, `play`, `pause`, `stop`, `setPosition`, `flush`, `setSourcePosition`, `addSegment`, `haveData`, `getPosition`, `getStats` |
| Events | `notifyNeedMediaData` (24 frames per request), `notifyPlaybackState` (PLAYING, PAUSED, SEEKING then SEEK_DONE after `setPosition`, END_OF_STREAM after a drain, STOPPED), `notifyPosition` every 250 ms |
| Pacing | PLAYING only after at least 1 s of media (or EOS) from each non-subtitle track; stops asking for data at 40 s buffered ahead; re-requests 15 ms after an empty `haveData` (real Rialto backs off up to 500 ms; not modelled) |
| Clock | Position advances at wall-clock speed from the first segment PTS; master-clock clamp tolerances model underflow |
| `IClientLogControlFactory` | No-op |
| `IMediaKeysFactory` | Returns null: no DRM |
| `IMediaPipelineCapabilitiesFactory` | Returns null capabilities (the player then assumes `appliedRate` 1.0) |
| Logging | Every action logs to stderr with a `[RialtoSim]` prefix, which the L2 tests match |
| Playback rate | Only with environment `RIALTO_SIM_ENABLE_PLAYBACK_RATE=1` |

Build and link: `cmake/FindRialto.cmake` selects the simulator when `RIALTO_FORCE_SIMULATOR` is set, on macOS, or when no real library is found; `CMakeLists.txt` then does `add_subdirectory(test/rialto)` and links `RialtoSimulator` into libaamp. `install_aampcli.sh` passes `-DRIALTO_FORCE_SIMULATOR=ON` for the default Linux build, and `make install` puts `libRialtoClient.so.0` (with `.so` and `.so.0.1.0` links) in `.libs/lib`. The resulting `build/aamp-cli` has a run path of `build`, `build/test/rialto`, `build/support/aampmetrics` and `.libs/lib`, and `ldd` shows it loading `build/test/rialto/libRialtoClient.so.0`, so no `LD_LIBRARY_PATH` is needed. The real Rialto build uses SONAME `libRialtoClient.so.1`, which is how the L2 framework tells the two builds apart. See the integration page for the build matrix.

## L2 tests

L2 tests live in the separate internal repository `aamp_test_internal` under `test/l2test`. A pytest framework starts `aamp-cli` with a generated `aamp.cfg`, optionally serves content with a local simulated-linear server, sends commands and matches regular expressions against the log (`expect_list`).

| Suite | Cases | What it checks |
| --- | --- | --- |
| `AAMP-RIALTO-10000_Tune` | VoD DASH, linear DASH, linear low-latency DASH, channel change (tune, stop, retune, stop) | State markers `IDLE -> PIPELINE_CREATED -> SOURCES_ATTACHING -> SOURCES_ATTACHED -> PAUSED`, `pre-roll complete`, `PAUSED -> PLAYING`, then one progress report per second at the expected positions |
| `AAMP-RIALTO-10001_Tune_HLS` | HLS fMP4 tune, URL-variant retune, fMP4 channel change, HLS TS tune, HLS TS muxed tune | The same markers for HLS (including the `SendCopy` muxed path) |
| `AAMP-RIALTO-10002_TrickModes` | Pause and resume, seek, basic trickplay | Flush/seek and rate changes |

Each test sets `only_run_on: direct_rialto` and `RIALTO_SOCKET_PATH=/tmp/rialto-0`. The framework also supports `only_run_on` / `do_not_run_on` and per-sink alternative expectations (`expect_direct_rialto`, `expect_gstreamer`), and a `generate_cfg_params` helper that runs one test under several configs. Sixteen other suites (`AAMP-TSB-5001` to `5008` and `5011` to `5016`, `AAMP-MV-5000`, `AAMP-VOD-5010_preferredTextLanguages`) use `alternate_cfgs = ["useDirectRialto=true", "useDirectRialto=false"]`, so they run once per sink. Any other suite can be forced onto direct-rialto with `./run_l2_aamp.py ... --aamp_cfg "useDirectRialto=true,useRialtoSink=true,useMp4Demux=true"`.

How to run (from `aamp_test_internal/test/l2test`, with the virtual environment active):

| Goal | Command |
| --- | --- |
| Simulator, existing build | `./run_l2_aamp.py -t 10000` (also `10001`, `10002`; `-k 10001_0` picks one case) |
| Build and run | `./run_l2_aamp.py -b -t 10000` (runs `install-aamp.sh`) |
| Real Rialto | build with `bash install-aamp.sh rialto`; in one terminal `sudo journalctl -f` (filter the output for `SRV`); in another `cd aamp_test_internal/test/Rialto && ./startmanager.sh`; then `curl -X POST -d "" localhost:9008/SetState/YouTube/Active` and `./run_l2_aamp.py -t 10000 --aamp_video` |

With `--aamp_video` the framework uses `build-rialto/aamp-cli` and puts `.libs-rialto/lib` ahead of `.libs/lib` in `LD_LIBRARY_PATH`; otherwise it uses `build/aamp-cli` (simulator). Set `GST_DEBUG="*rialto*:6"` for Rialto logging as the suite README files suggest. Log files are written to an `output` directory per test.

## Coverage-gap table

| Component | L1 test file | Gap or comment |
| --- | --- | --- |
| `AampRialtoPlayer` | `AampRialtoPlayerTests` (334) | Good breadth; concurrency between injector, Rialto and control threads is exercised with real threads in some tests but not exhaustively |
| `PlayerStateMachine` | `AampPlayerStateMachineTests` | Covered; a second copy of the table exists in `FakeAampPlayerStateMachine.cpp` (ARCH-36) |
| `AampRialtoMediaSource` (base) | None dedicated | Tested through `AampRialtoVideoSourceTests` (ARCH-43) |
| Video / Audio / Subtitle sources, handshake, controller, segment position, pre-roll, AV monitor, timers, TTML offset | Dedicated suites | Covered |
| `AampRialtoPipelineSession`, `AampRialtoMediaPipelineClient`, `AampRialtoControlBackend` | Dedicated suites | Covered |
| `PrivateInstanceAAMPNotifiable` | Dedicated suite | `ScheduleOwned` leak on scheduler drop (ARCH-17) cannot be seen with a mock scheduler |
| `AampDrmBridge` | `AampDrmBridgeTests` (5) | Small; license-flow error paths mostly covered at the player level via `MockDrmBridge` |
| `RialtoMediaKeySystem`, `RialtoMediaKeySession`, `RialtoMediaKeySessionAdapter` | Dedicated suites | Covered with `MockRialtoMediaKeys` |
| `makeRialtoSessionCreator()` | None (faked) | The real lambda is never run in L1 (ARCH-39) |
| `makeDefaultSourceCreator()` | None (faked) | Trivial but unchecked (ARCH-39) |
| `AampStreamSinkManager::CreateSinkInstance` selection | None | No test that `useDirectRialto` creates `AampRialtoPlayer` (ARCH-39) |
| `IStreamSinkNotifiable`, `IRialtoControlBackend`, `IDrmBridge`, `IDirectRialtoCC` | Interfaces | Exercised through mocks |
| `PlayerDirectRialtoCCManager` | In the middleware-player-interface repository | Not tested here |
| Core special cases (`streamabstraction.cpp` demuxer split, restamp guard, `GetStreamFormat`, HLS text-track default) | Partial | Only the MPD subtitle `StopTrackDownloads` branch has a direct-rialto-named test |
| Real Rialto server timing and DRM | None in L1; simulator L2 has no DRM | See limitations |

## Known testability limitations

- **Link-time global seams.** The pipeline factory and DRM bridge are replaced through global variables (`g_mockPipelineFactory`, `g_mockDrmBridge`) because the production constructor has no parameters for them (ARCH-38). Tests share process state and must reset it.
- **Partial interface seam.** The player reads config and calls `PrivateInstanceAAMP` directly, so every player test needs the core mocks as well as the notifiable mock (ARCH-03).
- **Fake drift.** `FakeAampPlayerStateMachine` and `FakeAampRialtoPlayer` duplicate production structure (ARCH-36, ARCH-37).
- **Simulator is not a server.** No decode, no DRM, a fixed 15 ms re-request, optional playback rate; encrypted content and the real SEEK_DONE/PLAYING ordering are not independently verified (ARCH-41). Real-server L2 runs are manual and need the server manager (ARCH-42).
- **Timers.** Production timers sit on the default GLib context; L1 replaces them with captured callbacks, so the cross-thread behaviour of `AampRialtoGlibTimer` is only tested in its own suite.
- **Public test hooks** on the production class (`StartProgressTimer`, `SetFlushCompleteTimeout`, ARCH-12).
- **CI scope.** Only L1 appears in `.github/workflows/L1-tests.yml`. The three direct-rialto L2 suites carry no `ci_test_set` marker in their test files.

## Related pages

- Direct Rialto - Overview (top page)
- Direct Rialto - Architecture and class model
- Direct Rialto - Integration, configuration and build
- Direct Rialto - Lifecycle and state machine
- Direct Rialto - Media data path
- Direct Rialto - Flush and seek

## Workarounds and improvement candidates

See `_register/ARCH.md`: ARCH-36 to ARCH-43 (test-side duplication, gaps and simulator/L2 limits), ARCH-03 and ARCH-12 (seams that tests have to work around), and ARCH-45 (stale simulator README).

## Open questions

- Are the three `AAMP-RIALTO-*` L2 suites part of any automated pipeline? They are not marked `ci_test_set` and the repository workflow runs only L1.
- Why do the L2 configs set `useRialtoSink=true` as well as `useDirectRialto=true`? It looks redundant (ARCH-40) but was not tested without it.
- The README of `AAMP-RIALTO-10001_Tune_HLS` lists only fMP4 flows while the test also has HLS TS cases.
