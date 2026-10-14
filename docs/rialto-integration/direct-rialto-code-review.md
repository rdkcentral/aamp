# direct-rialto Code Review and Fix Tracker

Scope: all files under `direct-rialto/` (40 files, ~11.9k lines).
Review date: 2026-10-13. Static review only (read, not compiled or run).

This document is the working tracker for the review findings. Update the
**Status** column as fixes land, add anything learned during implementation to
[Discoveries](#discoveries-during-implementation), and record finished work in
the [Completed Work Log](#completed-work-log).

Status values: `Open`, `In progress`, `Done`, `Won't fix`, `Needs verification`.

Items marked **verify** depend on Rialto's threading model or runtime behaviour
and should be confirmed before or while fixing.

## Summary

| Area | Verdict |
|---|---|
| SAFE practices | Partial. Mutexes, atomics and RAII are used well in places, but several shared members have no synchronisation. |
| Architecture clarity | Mixed. Extraction of the controller, state machine, segment position and pre-roll monitor is good. The gating/ungating design is hard to follow and docs are partly stale. |
| Good practices | Mostly good. Dead code, ignored parameters, unfinished work, hot-path INFO logging, inconsistent style. |
| Threading | Several real races and some deadlock/hang risks. |
| Crashes | A handful of plausible crash paths. |

## Priority Order

1. Snapshot `m_pipeline` under a lock; fix destruction order (H1, H2).
2. Guard the null pipeline in `CreateSources`/`CreateSubtitleSource`; take `m_attachMutex` there (H3).
3. Surface `FAILED` attach and `PlaybackState::FAILURE` to AAMP; clear `attachPending` (H4, H5).
4. Bound `WaitForFlushToComplete`; add a controller reset on `Stop()` (H6, H7).
5. Make `m_sourceId`/`m_mksId` atomic; synchronise `ProgressTimer`, `MonitorAV`, `m_videoRectangle`; fix `ChangeAamp` and the `AampDrmBridge` pointer (M1, M2, M4, M7, H8).
6. Decide whether `Flush()` may be called from injector threads; serialise it or move the claim under `m_flushMutex` (M5).
7. Re-examine `m_attachMutex` being held across DRM/IPC; document a lock order (M9).
8. Fix the `ShouldRecreatePipeline` null-subtitle condition and the subtitle negative display offset.
9. Clean up dead parameters and stale comments, downgrade hot-path logs, consider structural refactors.

---

## 1. Critical and High Severity

| ID | Status | Title |
|---|---|---|
| H1 | Done | `m_pipeline` shared across threads without synchronisation |
| H2 | Done | Destruction order lets Rialto callbacks hit destroyed members |
| H3 | Done | Null-pipeline dereference in `CreateSubtitleSource`; `AttachSource` contract violated |
| H4 | Done | Failed attach is silent and can hang injection |
| H5 | Done | `PlaybackState::FAILURE` never reported to AAMP |
| H6 | Done | `Stop()`/`Configure()` can wait forever for a flush |
| H7 | Done | Play holds leak across sessions |
| H8 | Done | `ChangeAamp` leaves stale pointers (DRM bridge) |

### H1. `m_pipeline` shared across threads without synchronisation (use-after-free)
- `m_pipeline` is a `std::shared_ptr` reset in `StopInternal` and reassigned in `CreateAndLoadPipeline` (`AampRialtoPlayer.cpp`, `m_pipeline = ...createMediaPipeline`).
- It is dereferenced as `*m_pipeline` or `m_pipeline.get()` from injector threads (`SendCopy`, `SendTransfer`, `SendSample`), the Rialto callback thread (`OnNeedMediaData`, `HandleSeekDone`, `ApplyTrickplaySegmentPosition`), the GLib main loop (`MonitorAV`, pre-roll probes) and the app thread (`GetPositionMilliseconds`, `SetVideoRectangle`).
- A concurrent `shared_ptr` read and write is a data race.
- Raw pointers are passed down into `handleNeedData`, `sendHaveData` and `processDataFragment`, and can outlive the pipeline if `Stop()` runs mid-call.
- **Fix:** take a local `shared_ptr` snapshot under a small mutex at each use site, or pass the snapshot down instead of raw pointers.

### H2. Destruction order lets Rialto callbacks hit destroyed members
- The destructor does not stop the pipeline or clear `AampRialtoMediaPipelineClient` callbacks.
- In `AampRialtoPlayer.h`, `m_pipeline` and `m_client` are declared before `m_sources`, `m_stateMachine`, `m_monitorAV`, `m_progressTimer`. Members are destroyed in reverse order, so those die while the pipeline can still deliver callbacks that capture `this`.
- **Fix:** reset the pipeline and null the client callbacks first in the destructor, or reorder members.

### H3. Null-pipeline dereference in `CreateSubtitleSource`; `AttachSource` contract violated
- `Configure()` calls `CreateSources()` even when `CreateAndLoadPipeline()` failed (e.g. Rialto server not running).
- `CreateSubtitleSource` calls `AttachSource()`, which dereferences `*m_pipeline` at the `source.attachOrUpdate(*m_pipeline, ...)` call. Crash for audio-only content with a TTML/WebVTT track (the video-first deferral does not trigger).
- The call also breaks the documented contract that `AttachSource` runs with `m_attachMutex` held (touches `m_pendingAttach`, `m_drmBridge`, `CheckAllSourcesAttached` unlocked).
- **Fix:** skip `CreateSources` when the pipeline is null; take the lock in `CreateSubtitleSource`.

### H4. Failed attach is silent and can hang injection
- In `AttachSource`, `AttachResult::FAILED` is ignored: no error to AAMP, `SendTransfer` still returns `true`, `allSourcesAttached()` is never called.
- For a deferred source, `attachPending` is only cleared on success, so `waitForAttach()` blocks the injector until the next flush/stop.
- Unsupported codecs hit this: AC3, MP3, ATMOS, MPEG2 are absent from the `mapCodecToMime` switches.
- **Fix:** surface via `NotifyPlaybackError`, clear `attachPending`, return `false` from `SendTransfer`.

### H5. `PlaybackState::FAILURE` never reported to AAMP
- `OnPlaybackState` only moves the internal state machine to `ERROR` and wakes flush waiters. No `NotifyPlaybackError`, so the app never sees the failure and playback stalls. The GStreamer path raises an error here.
- `PlaybackError` via `OnPlaybackError` is handled; `FAILURE` is not.

### H6. `Stop()`/`Configure()` can wait forever for a flush
- `WaitForFlushToComplete` uses an untimed `cv.wait`.
- If `setPosition` succeeds but Rialto never sends `SEEK_DONE` (server crash, lost IPC), the control thread blocks permanently. `Stop()` is the recovery path, so this is the worst place to hang.
- **Fix:** bounded wait, then force-complete the flush.

### H7. Play holds leak across sessions
- On the `FAILURE` path, `PlayHold::Flushing` is never released (`SEEK_DONE` is the only releaser on the flushable path).
- `PlayHold::FragmentCaching` survives `Stop()` if `NotifyFragmentCachingComplete` never arrives.
- `StopInternal` and `BeginNewSession` do not reset the controller's holds, so after a retune `RequestPlay` is deferred indefinitely.
- **Fix:** add a controller `Reset()` called from `Stop()`.

### H8. `ChangeAamp` leaves stale pointers
- `ChangeAamp` updates `m_aamp` and the notifiable but not the already-created `AampDrmBridge`, whose `m_aamp` dangles after the instance switch.
- `SetEncryptedAamp` hands the bridge a raw pointer to a different player with no lifetime guarantee that it outlives the ad player.
- `m_aamp` itself is written on one thread and read on many.

---

## 2. Threading Issues (Medium)

| ID | Status | Title |
|---|---|---|
| M1 | Done | `ProgressTimer` has no locking |
| M2 | Done | `m_monitorAV` unique_ptr and `AampRialtoMonitorAV` timer IDs unguarded |
| M3 | Done | `AampRialtoPreRollMonitor::Stop()` does not wait for in-flight `Poll()` |
| M4 | Done | `AampRialtoMediaSource` fields read lock-free |
| M5 | Done | `Flush()` documented as control-thread only but called from injector threads |
| M6 | Done | Hold release and `play()` not atomic; other direct `play()` call sites bypass holds |
| M7 | Done | Unsynchronised plain members (`m_videoRectangle`, `m_audioVolume`, etc.) |
| M8 | Done | Rialto callback setters race with delivery |
| M9 | Done | Possible deadlock: `m_attachMutex` held across DRM/IPC |
| M10 | Done | Blocking Rialto IPC from the Rialto callback thread |
| M11 | Done | `MediaKeysClient::onKeyStatusesChanged` invokes callbacks under mutex |
| M12 | Done | `RialtoMediaKeySessionAdapter` lost wakeups and stale state |
| M13 | Done | `AampRialtoControlBackend` member order |
| M14 | Done | `PrivateInstanceAAMPNotifiable::m_aamp` racy and not null-checked |
| M15 | Open | Run the L1 stress suite and fix the flaky tests it exposes (T1 to T3) |

- **M1. `ProgressTimer`** (`AampRialtoPlayer.cpp`, nested class): `started`, `source_id`, `callback` unlocked.
  - `start`/`stop`/`kick` run on the control thread and the Rialto callback thread; the callback runs on the GLib thread.
  - `stop()` clears the `std::function` while `runOnce()` may be calling it.
  - A tick can be in flight while the destructor runs. `m_progressTimer` is lazily created with no lock.
- **M2. `m_monitorAV`:** `HandlePlaybackStarted` calls `m_monitorAV->start()` on the callback thread while `StopInternal` resets it on the control thread.
  - `AampRialtoMonitorAV::start/stop` have unguarded timer IDs; `stop` does not wait for an in-flight tick.
- **M3. `AampRialtoPreRollMonitor`:** `Stop()` removes the timer but does not wait for an in-flight `Poll()`.
  - `m_onComplete()` can fire after `Stop`/`Flush` and issue play. Destruction can race with a tick.
  - Probes call `getSource()` on the main loop while `CreateMediaSource` replaces `m_sources[...]`.
- **M4. `AampRialtoMediaSource`:** `m_sourceId`, `m_mksId`, `m_streamFormat`, `m_pendingCodecData` are plain members written by attach/control threads and read from `findSourceByRialtoId`, `waitForAttach`, `createSegment`, `isInbandCC`, the MonitorAV getter.
  - `m_sourceId` is assigned only after `attachSource()` returns, so an early `needData` is dropped as "unknown sourceId".
  - **Fix:** make `m_sourceId` and `m_mksId` `std::atomic`. `LogSourceSnapshot` also reads `state().eos` without the mutex.
- **M5. `Flush()`:** `MaybeFlushForPendingPosition` calls it from injector threads.
  - Pre-flush state check and `FLUSHING` claim are not atomic, so two concurrent flushes can both claim.
  - `PlayHold::Flushing` is a single bit, so one `SEEK_DONE` releases play while the second flush is outstanding.
  - The injector flush also calls `Stream()`, which can race `Stop()`.
- **M6. Controller:** `Dispatch` runs `m_onPlay` after unlocking, so a concurrent `AddHold(Flushing)` can land between "all holds clear" and `play()`.
  - The controller claims to be the single decision point, but `Pause(false)`, `StopBuffering` and `NotifyFragmentCachingOngoing` call `m_pipeline->play()`/`pause()` directly and bypass holds.
- **M7. Plain members:** `m_videoRectangle` (`std::string`) is written in `SetVideoRectangle` and read in `GetVideoRectangle`/`GetVideoSize` on other threads (crash-class race).
  - `m_audioVolume`, `m_playbackQuality`, `m_rate` mix locked and unlocked access.
  - `m_pendingAttach` is cleared in `BeginNewSession` without `m_attachMutex`; `m_drmBridge` written in `SetEncryptedAamp` without a lock.
- **M8. Callback setters:** `Set*Callback` in the pipeline client assigns `std::function` members with no lock, after `createMediaPipeline`/`load` has begun delivering events. Events between `load()` and `RegisterPipelineCallbacks` are lost.
  - **Fix:** install callbacks before `load()`, or guard with a mutex.
- **M9. Possible deadlock (verify):** the injector thread holds `m_attachMutex` across `AttachSource`, which can run DRM `createSession` (blocks on license/key-status events). `ApplySubtitleSegmentPosition` takes `m_attachMutex` on the Rialto callback thread (`HandleSeekDone`).
  - If Rialto delivers pipeline and key events on one IPC thread, the key-status callback queues behind a thread waiting for the lock held by the thread waiting for that key status.
  - `SetVideoMute` and `setCCMute` also block the app thread for the whole license fetch.
  - No documented lock hierarchy: `m_attachMutex`, `m_flushMutex`, `m_pendingProtectionMutex`, `m_ptsCheckMutex`, per-source `state.mu`, state machine mutex, controller mutex.
- **M10. (verify):** `HandleSeekDone` calls `setSourcePosition`, `play()` and `haveData()` from the notification thread. `rialto-gstreamer` bounces these onto a worker thread.
- **M11. `MediaKeysClient::onKeyStatusesChanged`** invokes callbacks while holding `mutex` (the other two handlers copy the callback out first).
  - A callback touching `deregisterSession` self-deadlocks.
  - `sessions` stores raw `RialtoMediaKeySession*`; the session destructor is `= default` and does not deregister, so a session dropped without `destruct()` leaves a dangling pointer.
- **M12. `RialtoMediaKeySessionAdapter`:** `m_stateChanged.notify_all()` is called without holding `m_eventMutex`, so `waitForState` can miss the notification and sit until timeout.
  - `m_challengeReceived` and `m_keyStatusReceived` are never reset between requests, so renewal can read stale data.
  - `m_session` is used in `processDRMKey` and `getMediaKeySessionId` without `m_mutex`, while `clearDecryptContext` resets it.
- **M13. `AampRialtoControlBackend`:** `m_stateCv` and `m_mutex` are declared after `m_control`, so they are destroyed before it; an in-flight `notifyApplicationState` can use a destroyed mutex. `m_rialtoClientState` is also written by `registerClient` with no lock.
- **M14. `PrivateInstanceAAMPNotifiable`:** `m_aamp` written by `ChangeAamp` and read on callback threads; dereferenced without null check in most methods, unlike `GetProgressReportIntervalSeconds`.
- **M15. Stress-test run and flaky-test fixes (do last, after M14).** Every M-series change has had to be stress-tested, and the stress runs keep failing in tests the change did not touch (see section 6, T1 to T3), so each task spends time proving the failure is not its own. This task removes that noise and sets the pass bar for the series.
  - **Method:** run N parallel workers, each looping the full `AampRialtoPlayerTests` binary with a per-run timeout, keeping the output of each failing run and classifying it as timed out or assertion failure (details in section 6, T3). Compare every result against a baseline run of the code before the M-series (the commit before `5b1fd65d`) on the same machine and load, never against zero.
  - **Fix, in this order:** (1) the T1 tests: wait until the sender is actually blocked instead of a fixed sleep; (2) any test that `join()`s a thread without a bounded wait; (3) tests that use a short fixed `WaitFor` before an action; (4) re-run and re-rank what still fails.
  - **Acceptance:** on the CI runner (or a quiet physical machine), 0 failures in at least 500 full-suite runs at one worker per core, and no run killed by the timeout. Results from the current VM are for comparison only: it is VirtualBox and logs soft lockups under load, so it is not a valid acceptance environment.
  - **Also decide:** whether the L1 suite should run in CI under ThreadSanitizer (`-fsanitize=thread`, run under `setarch -R`). TSAN found the M4 and M7 races deterministically, which stress runs could not.
  - **Evidence so far:** the new `Flush_ConcurrentCallers_NeverOverlapInFlight` test alone passed 360 of 360 runs at 6 parallel workers, so the full-suite failures at that load come from other tests or from the environment.

---

## 3. Potential Crashes and Logic Defects (Medium/Low)

| ID | Status | Title |
|---|---|---|
| L1 | Done | Pipeline replaced without stopping on ES-change reconfigure |
| L2 | Done | `ShouldRecreatePipeline` forces rebuild for audio-only content |
| L3 | Done | Subtitle negative display offset never applied |
| L4 | Open | HEVC stream format hard-coded `HVC1` |
| L5 | Open | DRM size truncation and misc DRM defects |
| L6 | Open | Encrypted sample handling gaps |
| L7 | Open | `Flush()` before `Configure()` runs a pointless recovery `Stop()` |
| L8 | Open | Test-only constructor with null `sourceCreator` throws on `Configure` |
| L9 | Needs verification | `NO_AVAILABLE_SAMPLES` on gated `needData` relies on Rialto re-requesting |
| L10 | Open | Per-frame `haveData` round trip on single-sample paths |
| L11 | Open | Timer intervals unchecked (zero/negative) |
| L12 | Needs verification | All GLib timers use the default `GMainContext` |
| L13 | Open | Hard-coded pipeline requirements, `isLive=false`, `"mse://1"` (TODO) |

- **L1.** On the ES-change path, `BeginNewSession` + `CreateAndLoadPipeline` run with no `Stop()` (`m_pipeline = ...createMediaPipeline`).
  - A second Rialto pipeline is created while the first is alive (may fail on resource-constrained devices).
  - `m_sources[type] = std::move(src)` can destroy a source an injector is still blocked inside.
- **L2.** `ShouldRecreatePipeline`: `(subtitleSrc == nullptr) || ...` returns `true` for audio-only content with no subtitle track, even when `subFormat == FORMAT_INVALID`. This forces a full rebuild on every normal-rate `Configure`. The adjacent comment says it should tolerate null, not treat it as a change.
- **L3.** `AampRialtoSubtitleSource::createSegment` only calls `setDisplayOffset` when `displayOffsetMs > 0`. For WebVTT, `processDataFragment` deliberately passes `-fragmentPTSoffset`, so the intended negative offset is never applied. Likely functional bug.
- **L4.** `AampRialtoVideoSource::mapCodecToMime` hard-codes `HVC1` for HEVC; H.264 honours `mNaluLengthPrefixed`. Annex-B HEVC from HLS-TS would be mislabelled.
- **L5. DRM:**
  - `static_cast<uint16_t>(len)` in `AampDrmBridge::createSession` truncates init data over 64 KB.
  - `static_cast<uint16_t>(key->getDataLength())` in `processDRMKey` truncates license responses over 64 KB.
  - Challenge and key-id sizes in `MediaKeysClient` truncate the same way.
  - `eMEDIAFORMAT_DASH` is hard-coded even for HLS.
  - `IDrmBridge::clearSessions()` is never called (its doc says `ClearProtectionEvent` calls it), so DRM sessions are never released.
- **L6.** A failed `createSession` still lets encrypted samples be injected without encryption metadata (WARN only). A partial sub-sample table (`break` on a short buffer in `annotateEncryption`) is silently accepted. `static_cast<uint32_t>(mDataSize)` truncates.
- **L7.** `Flush()` before `Configure()` with default `shouldTearDown=true` runs a pointless recovery `Stop()` when the state is idle.
- **L8.** Test constructor default `sourceCreator = nullptr`: `CreateMediaSource` would throw `std::bad_function_call`.
- **L9.** `handleNeedData` answers any gated `needData` with `NO_AVAILABLE_SAMPLES` and assumes Rialto issues a fresh `needData`. Only documented in comments; stall risk if it does not hold.
- **L10.** Single-sample paths (`SendCopy`, `injectSingleSample`) pass `morePending=false`, so every frame costs a `haveData` round trip.
- **L11.** `AampRialtoMonitorAV` passes `sampleIntervalMs`/`reportIntervalMs` to `g_timeout_add` unchecked: zero busy-loops, negative wraps.
- **L12.** `ProgressTimer`, `MonitorAV`, `PreRollMonitor` use the default `GMainContext`. Confirm the product iterates it; a missed pre-roll timer leaves `PreRollIncomplete` held and play is never issued.
- **L13.** `3840x2160` video requirements hard-coded; `isLive=false` and `"mse://1"` are placeholders (`TODO: How do we determine value for isLive` in `CreateAndLoadPipeline`).

---

## 4. SAFE and Good-Practice Observations

| ID | Status | Title |
|---|---|---|
| P1 | Open | Raw `new`/`delete` in `PrivateInstanceAAMPNotifiable.cpp` |
| P2 | Open | Dead or ignored parameters and state |
| P3 | Open | Fragile logic in `MonitorAV` and subtitle TTML parsing |
| P4 | Open | State machine log noise |
| P5 | Open | Truncating PTS/duration conversions |
| P6 | Open | Header hygiene |
| P7 | Open | Constructor init-list order |
| P8 | Open | Hot-path INFO logging |
| P9 | Open | Formatting and style inconsistencies |

- **P1.** Seven raw `new`/`delete` pairs for async task args violate the repo prohibition, and the args leak if the scheduler drops a queued task. One shared helper would fix both and remove the repeated boilerplate. (`RialtoMediaKeySessionAdapter::generateKeyRequest` returning `new DrmData` matches the `DrmSession` interface and is acceptable.)
- **P2.** Dead/ignored:
  - `exportFrames` constructor parameter is ignored.
  - `m_ID3MetadataHandler` is stored but never used.
  - `AampRialtoMediaPipelineClient::init()` is a stub returning `false`; the file is still described as a "stub".
  - `hasDrm` argument ignored in video/audio sources (hard-coded `true`).
  - `StoppedState` and the `STOPPED` enum are unreachable.
  - `ProgressTimer` has a pointless `friend`; `LICENSE_RENEWAL_MESSAGE_TYPE` is unused; `SendTransfer`'s `discontinuity` parameter is unused.
- **P3.** Fragile logic:
  - `AampRialtoMonitorAV` compares description strings by pointer and also does `std::string(description) == "ok"`; relies on literal merging. Use an enum.
  - `avsync` and `audio drop` branches can never trigger (both tracks share one position).
  - `AampRialtoSubtitleSource` finds TTML timing with `find("begin=\"")` rather than parsing, and copies the whole payload into a `std::string` per fragment.
- **P4.** `onSourceAttaching` is dispatched for every source but only `PIPELINE_CREATED` handles it, so the 2nd/3rd source log a WARN "no transition". `Stop()` on an idle player also logs a WARN.
- **P5.** PTS/duration use `static_cast<int64_t>(x * 1e9)` in places and `llround` in others. Round consistently.
- **P6.** Header hygiene:
  - `AampRialtoMediaPipelineClient.h` has `using namespace firebolt::rialto;` in a header and an unneeded `<iostream>`.
  - Missing includes: `<limits>` and `<utility>` in `AampRialtoMediaSource.h`, `<utility>` in a few `.cpp` files, `<glib.h>` in `AampRialtoPlayer.h` for `guint`.
  - `AampRialtoMediaSource.h` says "forward decl" but includes `MP4Demux.h`.
  - `AampRialtoSegmentPosition.h` pulls in all of `AampDefine.h` for one constant.
  - `AampDemuxDataTypes.h` included twice in `AampRialtoPlayer.h`; `<deque>`, `<thread>` unused.
- **P7.** Constructor init list is not in declaration order (`m_ID3MetadataHandler` after `m_controlBackend`), giving `-Wreorder`.
- **P8.** INFO ENTRY/EXIT or per-event logs on hot paths: `SendCopy`, `GetPositionMilliseconds`, `GetDurationMilliseconds`, `handleNeedData`, `processDataFragment`, every pipeline-client notify (`notifyPosition`, `notifyNeedMediaData`).
- **P9.** Style:
  - Some lines start with a space (e.g. `ProgressTimer::stop`/`kick`).
  - Brace-less `if (!m_aamp) return;` in `AampDrmBridge::clearSessions`.
  - Lines over 80 characters are widespread.
  - Single-exit and early-return styles mixed; naming mixes PascalCase, camelCase and snake_case (`ProgressTimer`).
  - File/guard conventions vary: `RialtoMediaKey*` lack the `Aamp` prefix; guards are `AAMP_...`, `Foo_h` or `#pragma once`.

Positives worth keeping: `AampRialtoSegmentPosition` and `AampRialtoPlaybackController` abstractions, epoch tokens in `PreRollMonitor`, consistent `AAMPLOG` macros, well-explained async dispatch in `PrivateInstanceAAMPNotifiable`, weak-pointer deregistration in `MediaKeysClient`, licence headers on every file, interface seams (`IDrmBridge`, `IStreamSinkNotifiable`, `IRialtoControlBackend`, `SourceCreator`), existing unit tests for player, `MonitorAV` and `PreRollMonitor`.

---

## 5. Architecture and Maintainability

| ID | Status | Title |
|---|---|---|
| A1 | Open | `AampRialtoPlayer` is a god class |
| A2 | Open | Three separate GLib timer wrappers with differing thread safety |
| A3 | Open | Concurrency model is hard to follow; no diagram |
| A4 | Open | Stale documentation |
| A5 | Open | Leaky GStreamer dependencies in the "GStreamer-free" path |

- **A1.** `AampRialtoPlayer.cpp` is ~3.1k lines with ~70 methods covering pipeline lifecycle, flush/position election, play gating, pre-roll, DRM wiring, CC, progress timer and monitor lifetime. `Flush` (~180 lines), `AttachSource`, `Discontinuity`, `injectOneSample` are long and deeply nested. Suggested extractions: a pipeline-session/lifecycle class, a flush coordinator, `ProgressTimer` into its own file.
- **A2.** `ProgressTimer`, `MonitorAV`, `PreRollMonitor` each wrap `g_timeout_add` differently. One shared RAII timer helper with a mutex and epoch would remove most of M1 to M3.
- **A3.** Understanding "when may data flow" needs five concepts (`GateMode`, generation tokens, `PlayHold` bits, state machine, deferred-flush election) spread over four classes with prose comments. Add one state/sequence diagram (under `docs/diagrams/` as PlantUML). The state machine comment promises thread safety, but check-then-act sequences in the player (`currentState()` then act) are not atomic.
- **A4.** Stale docs:
  - `gateMode`, `UngateAllSources` and `waitForAttach` comments list ungate sites (`Stream()`, `CheckAllSourcesAttached()`, the PLAYING handler) that no longer ungate; `HandlePlaybackStarted` says it deliberately does not.
  - State machine header describes `STOPPED`, but `onStop` goes to `IDLE`.
  - `IStreamSinkNotifiable` says "push-notification only" but includes `GetState` and `GetProgressReportIntervalSeconds`.
  - The `THEORY (unproven - revert this block if disproved)` comment in `AttachSource` describes a behaviour-changing hypothesis in production code.
- **A5.** Direct path still uses `GstStreamOutputFormat`/`GST_FORMAT_*`, and the DRM bridge relies on `eGST_MEDIATYPE_*`. Test-only members (`StartProgressTimer`, `StopProgressTimer`, `OnProgressTimerTick`) are public on the production surface.

---

## 6. Test Reliability (flaky L1 tests)

Found while stress-testing the fixes. None of these were introduced by the M-series changes: the same hangs occur on the code before M5 (see T1). They only show under CPU oversubscription, so they are unlikely to fail a normal CI run, but they will fail on a busy shared builder.

| ID | Status | Title |
|---|---|---|
| T1 | Open (partly mitigated) | Flush-while-blocked tests hang when the sender thread starts late |
| T2 | Open | `Flush_ConcurrentCallers_NeverOverlapInFlight` is slow or hangs under heavy load |
| T3 | Open | Other `AampRialtoPlayerTests` hangs under heavy load, not root-caused |

- **T1. Sender thread starts after the flush.** Affected: `SendTransfer_DeferredAudio_FlushWhileBlockedAbortsSendTransfer`, `SendSample_AfterFlush_DropsRemainingSamples`, `BackPressure_FlushWhileBlocked_UnblocksSendTransfer`.
  - Each test starts a sender thread, waits a fixed 30 to 50 ms, then calls `Flush()` and expects the sender to wake. If the sender has not yet reached `waitForAttach()` (or the `needData` wait), the flush bumps `generation` first, the late sender records the new generation, and nothing ever wakes it; `sender.join()` then blocks forever. Confirmed from a gdb thread dump of a hung run (sender in `AampRialtoMediaSource::waitForAttach` on a condition variable with no timeout, main thread in `std::thread::join`).
  - The product behaviour is correct: a thread that begins waiting after a flush must wait for new content.
  - **Mitigation already in place (uncommitted with M6):** a `RepeatUntil` helper re-issues the `Flush()` until the sender returns, bounded at 5 s. Rate under 8 workers + 6 CPU spinners on 6 cores fell from about 22/240 to 5/320, all five in these same tests, so it is not fully fixed.
  - **Remaining fix:** have the test wait until the sender is actually blocked (for example poll a source-level "waiting" indicator or a mock hook that the sender hits just before blocking) instead of a fixed sleep, and never `join()` without a bounded wait (stop the player to release the thread, as done in the M5 pending-position test).
- **T2. New M5 test timing.** `Flush_ConcurrentCallers_NeverOverlapInFlight` runs 40 rounds of two threads spinning on `yield()` barriers. It takes about 1.6 s alone, but was the last test running in 20 of 59 killed runs under heavy load. Not yet determined whether it is slow (busy-wait contention) or deadlocked. Later evidence: it takes about 90 ms alone and passed 360 of 360 runs at 6 parallel workers with nothing else running, so it is probably not at fault in itself; its appearance in full-suite failures at that load needs the T3 comparison. Next step: run it alone under the same load with a longer timeout and capture a thread dump if it does not finish; replace the spin barriers with a condition variable.
- **T3. Other heavy-load hangs.** In the same stress runs, a spread of other tests were last seen when runs hit the 30 s timeout (for example `SendSample_PositionPending_DuringActiveFlush_DoesNotIssueOverlappingFlush`, `Configure_FlushNeverCompletes_IssuesDeferredPlay`, `Stop_NeedDataAfterStop_DoesNotCrash`, `StopThenReconfigure_SecondPipelineCreatedAfterFirstDestroyed`, `SendTransfer_DeferredAudio_BlocksUntilVideoAttaches`). The pre-M5 code showed 119/240 failing runs and the M5+M6 code 86/240 with the same load, so this is not a regression. Many probably share the T1 pattern (fixed sleep or short `WaitFor` before an action), but this is unverified.
  - **Method:** run the full suite with 8 parallel workers (30 s timeout each) plus 6 CPU-spinning processes on a 6-core VM; keep each failing run's output; classify as killed-by-timeout or assertion failure; capture stacks for a hung run by launching the test under `gdb -batch` (attaching is blocked by Yama `ptrace_scope=1`, but launching under gdb works) and interrupting it after 25 s. Moderate load (6 workers, no spinners) gave 0/240 failures and serial runs on an idle VM 0/600, so the effect needs real oversubscription.
  - **Test helper:** the default `WaitFor` timeout was 500 ms, too short for a starved thread; raised to 5 s (returns as soon as the condition holds). Tests using it as a "nothing should happen" delay pass an explicit 30 ms and are unaffected.
  - **Environment caveat (unresolved):** the build VM is VirtualBox with 6 vCPUs. The kernel log shows 63 `soft lockup ... stuck for 26s` lines during the heavy runs, so whole-VM stalls do occur and would freeze every worker at once. At moderate load (6 workers, no spinners) 7/240 and 10/240 full-suite runs still failed, and the failures clustered in time (5 workers at iteration 6, 3 at iteration 33, each in a different test), which looks like a shared stall rather than independent flaky tests. No soft-lockup message was logged at those two moments, so a VM stall is a hypothesis, not a confirmed cause. Next step: rerun the same moderate-load test on a physical machine or a less contended runner; if the clusters disappear, this is environmental and only T1/T2 need fixing.

---

## Discoveries During Implementation

Record anything found while fixing (new bugs, wrong assumptions in this review,
Rialto behaviours confirmed or disproved, test gaps, design decisions).

| Date | Related ID(s) | Discovery |
|---|---|---|
| 2026-10-13 | H1 | The pipeline must still be released synchronously by `Stop()`: existing tests (`Stop_AfterPreRollCompleted_ReleasesPipeline`, `StopThenReconfigure_SecondPipelineCreatedAfterFirstDestroyed`) guard against a real-device failure ("Max session number reached"). Snapshots returned by `GetPipeline()` are therefore short-lived locals only; never store one in a member or a long-lived lambda. |
| 2026-10-13 | H1 | `IMediaPipelineFactory::createMediaPipeline` returns `unique_ptr`; the player converts it to `shared_ptr` when installing it. |
| 2026-10-13 | H1 | `ApplyTrickplaySegmentPosition` dereferenced `m_pipeline` with no null check; it is now guarded. |
| 2026-10-13 | H1 | `GetPipeline()`/`SetPipeline()` are the only accessors; any new code touching the pipeline must use them. `m_pipelineMutex` is a leaf lock (held only to copy or swap). |
| 2026-10-13 | H2 | `AampRialtoMonitorAV` holds its own `shared_ptr` to the pipeline, so `m_monitorAV` must be reset before `SetPipeline(nullptr)` or the pipeline outlives the call (same constraint as `Stop()`). |
| 2026-10-13 | H2 | Reordering the members was rejected: the pipeline destructor performs Rialto IPC and may fire final callbacks, so it must run explicitly, first, in the destructor body rather than depend on declaration order. |
| 2026-10-13 | H2 | `AampRialtoMediaPipelineClient::ClearCallbacks()` assigns the `std::function` members with no lock, so it is only safe after the pipeline is destroyed. A callback already executing on the Rialto thread is still not covered; that is M8. |
| 2026-10-13 | H3 | The null-pipeline dereference was already closed by the H1 guard in `AttachSource`; H3 added only the missing `m_attachMutex` around the two `AttachSource` calls in `CreateSubtitleSource`. Source creation is deliberately still performed when the pipeline is null (keeps `ResumeTrackDownloads` and existing failure-mode behaviour unchanged). |
| 2026-10-13 | H3 | Behaviour change to watch: `Configure()` can now wait on `m_attachMutex` if an injector thread holds it (e.g. inside DRM `createSession`). AAMP stops injection before `Configure()`, so this should not occur in practice, but it adds to the lock-hierarchy concern in M9. |
| 2026-10-13 | H4 | Before the fix a failed attach never reached `allSourcesAttached()`, so playback stalled in `SOURCES_ATTACHING` with no event; it is now surfaced as `AAMP_TUNE_GST_PIPELINE_ERROR` (retry disabled, since the common cause, an unsupported codec, is deterministic). |
| 2026-10-13 | H4 | `FORMAT_UNKNOWN` on a video/audio source (muxed HLS-TS before the codec is identified, reached via `SendCopy`'s `SetStreamCaps` loop) also yields `FAILED`, but a later `Configure()` recovers it. It is logged and not reported, to avoid turning a recoverable case into a fatal error. Inband CC legitimately uses `GST_FORMAT_UNKNOWN`, so subtitle is excluded from that exemption. |
| 2026-10-13 | H4 | `PrivateInstanceAAMP::SendStreamTransfer` ignores `SendTransfer()`'s return value, so returning `false` on a failed attach has no side effect for callers. |
| 2026-10-13 | H4 | Possible follow-up: a failed subtitle or inband-CC attach could be made non-fatal (drop that source and continue) rather than ending the tune. Not done here as it changes tune semantics. |
| 2026-10-13 | H5 | The `PlaybackState::FAILURE` branch had no unit tests at all before this change. |
| 2026-10-13 | H5 | `SendErrorEvent` only acts the first time (it is a no-op once AAMP is in `eSTATE_ERROR`), so a `FAILURE` that follows an earlier `PlaybackError` or attach failure is harmless. The player also reports only on the transition into `ERROR`, so repeats are not forwarded. |
| 2026-10-13 | H5 | `FAILURE` in `IDLE` (a stale notification after `Stop()`, via a client still held by Rialto) is deliberately not reported, otherwise it could fail the next tune. The state machine still logs a "no transition" WARN for it (see P4). |
| 2026-10-13 | H5 | The error is sent with retry enabled, matching `OnPlaybackError` and the GStreamer generic-pipeline-error path. |
| 2026-10-13 | H6 | Timeout is 5 s, deliberately generous: abandoning a flush that is merely slow would skip `HandleSeekDone`'s position/rate commit, because a late `SEEK_DONE` arriving after the forced completion is ignored as "outside FLUSHING". The value is a guess and should be revisited if real-device `SEEK_DONE` latency data is available. |
| 2026-10-13 | H6 | A forced completion releases `PlayHold::Flushing`, so an outstanding play request can be issued at that point (for `Stop()` this means a `play()` just before `stop()`; harmless but wasteful). It follows the existing `setPosition()`-failure path in `Flush()`. |
| 2026-10-13 | H6 | Narrow race left as is: a `SEEK_DONE` handled at the same instant as the timeout can find the state already `FLUSHED`; `onFlushComplete` then logs a "no transition" WARN and the remaining steps are idempotent. |
| 2026-10-13 | H6 | Added public `SetFlushCompleteTimeout()` so tests need not wait 5 s; same "public for testing" pattern as `OnProgressTimerTick()` (see A5). |
| 2026-10-13 | H7 | Both leaks in the review were reproduced before the fix: a `Flushing` hold left by a flush that ended in `FAILURE`, and a `FragmentCaching` hold from an unfinished caching cycle, each stopped `Stream()` from issuing `play()` in the next session. |
| 2026-10-13 | H7 | `Reset()` restores only `SourcesNotAttached` (the construction state) and also clears the pre-roll-ready latch, otherwise a new session's pre-roll start would be suppressed. Not done: `Configure()` on the reuse path (no `Stop()`) still does not clear stale holds. |
| 2026-10-13 | H7 | `Flush()`'s internal teardown-recovery `Stop()` also resets the holds. This is harmless because `Flush()` has already released `PositionPending` and releases `Flushing` itself immediately afterwards. |
| 2026-10-13 | H7 | `CancelPlayRequest()` remains for `Pause(true)`, which must not drop the other holds. |
| 2026-10-13 | H8 | `SetEncryptedAamp()` deliberately points DRM at a different instance (the encrypted VOD player) for a shared pipeline, so `ChangeAamp()` must not replace that bridge; only the default bridge made for `Aamp()` is dropped and rebuilt lazily. A test guards the explicit case. |
| 2026-10-13 | H8 | Residual risk, outside `direct-rialto/`: a bridge installed by `SetEncryptedAamp()` holds a raw pointer to the encrypted instance and its lifetime depends on `AampStreamSinkManager`, which never calls `SetEncryptedAamp(nullptr)` when that player is deleted. Suggest the manager does so; the player now handles null by reverting to the default bridge. |
| 2026-10-13 | H8 | Before the change `SetEncryptedAamp(nullptr)` built a bridge with no target, making every later `createSession` fail; it now falls back to the default bridge. |
| 2026-10-13 | H8 | `Aamp()` returns a snapshot; functions that use the instance more than once (`CreateMonitorAV`, `Discontinuity`) take a single local so one call never mixes two instances. |
| 2026-10-13 | H8 | Not covered: `PrivateInstanceAAMPNotifiable::m_aamp` is still a plain pointer written by `ChangeAamp` and read on callback threads (M14). `AampDrmBridge::m_aamp` is only written at construction. |
| 2026-10-13 | H8 | Test fake `FakeAampDrmBridge` now records the target instance and creation count (`g_lastDrmBridgeAamp`, `g_drmBridgeCreateCount`), as the old fake discarded the constructor argument. |
| 2026-10-13 | M1 | Root cause was shared by M1-M3: three hand-rolled GLib timers with no locking and no wait for an in-flight tick. Added header-only `AampRialtoGlibTimer` (state in a `shared_ptr` that GLib also owns via a destroy-notify, recursive lock, generation check) and moved `ProgressTimer` onto it; `AampRialtoMonitorAV` and `AampRialtoPreRollMonitor` follow in M2 and M3. |
| 2026-10-13 | M1 | `Stop()` now blocks until a tick running on another thread returns. The tick runs under the timer lock, so a tick must not wait on a thread that is calling into the same timer; `OnProgressTimerTick` only schedules an async task, so this holds. The lock is recursive so a tick may stop or restart its own timer. |
| 2026-10-13 | M1 | `m_progressTimer` is now a plain member rather than a lazily created `unique_ptr`, which removes the unsynchronised creation race. |
| 2026-10-13 | M1 | The helper uses `g_timeout_add_full`; the test GLib fake now forwards it to the existing `g_timeout_add` mock (it previously returned 0 and nothing else in production uses it), so existing timer expectations still apply. The fake does not call the destroy notify, so tests leak one small state object per timer. |
| 2026-10-13 | M1 | `start()` now schedules before its immediate callback (previously after). If scheduling fails the immediate callback still runs, as before. |
| 2026-10-13 | M2 | `AampRialtoMonitorAV` now uses two `AampRialtoGlibTimer`s, so its `stop()` waits for a tick in progress and a dispatch after `stop()` or destruction does nothing. The destruction case segfaulted on the old code. |
| 2026-10-13 | M2 | The player's `m_monitorAV` is now guarded by a leaf `m_monitorMutex` and replaced only through `SetMonitorAV()`, which destroys the old monitor outside the lock because its destructor waits for a tick. `HandlePlaybackStarted` (Rialto callback thread) starts it under the same lock. |
| 2026-10-13 | M2 | The sample and report ticks both run on the GLib thread, so their shared classification state is serialised in production; it is not protected against a main context iterated by several threads. |
| 2026-10-13 | M2 | Not changed: zero or negative `sampleIntervalMs`/`reportIntervalMs` are still passed straight to GLib (tracked as L11). |
| 2026-10-13 | M2 | A single full-suite run hung once in `Configure_ReusedPipelineWithoutDiscontinuity_StillDefersPositionToFirstSample`, right after a segfaulting red check. It did not reproduce in 60 isolated runs or 4 full-suite runs, so it is recorded as a timing-sensitive test (it joins a sender thread with no timeout), not as a regression. |
| 2026-10-13 | M3 | `AampRialtoPreRollMonitor` keeps its epoch and `m_running` logic (its tests drive `Poll()` directly) and only its scheduling moved onto `AampRialtoGlibTimer`. `Stop()` now waits for a `Poll` in progress, so a completion can no longer fire after `Stop()`/`Flush()` returns, and `Start()` stops the old timer first so `Poll` cannot read a probe being replaced. |
| 2026-10-13 | M3 | This also closes the review's note that the probes call `getSource()` on the GLib thread while `CreateMediaSource` replaces `m_sources[...]`: `BeginNewSession` stops the monitor before sources are recreated, and that stop now waits. |
| 2026-10-13 | M3 | `Poll()` and the completion action now run under the timer lock when driven by the timer. The completion releases a hold, which can issue `play()` (Rialto IPC), so a `Stop()` from another thread waits for that IPC. No lock cycle was found, since `Stop()` holds only the timer lock while waiting. |
| 2026-10-13 | M3 | Two of the three new tests (`DispatchAfterStop_DoesNothing`, `DispatchAfterDestruction_IsSafe`) pass on the old code too: in a unit test the freed monitor is still readable. They remain as guards; `Stop_WaitsForPollInProgress` is the one that fails on the old code. |
| 2026-10-13 | M3 | Found while running M2/M3: `Configure_ReusedPipelineWithoutDiscontinuity_StillDefersPositionToFirstSample` hung in about 2.5% of runs under CPU contention (0/200 on the pre-M1 baseline, ~5/200 after M1/M2, so my timing changes made an existing race likelier). Cause was the test, which posted `needData` before the sender thread's `Stream()` had issued `play()`; fixed in its own commit (0/320 afterwards). |
| 2026-10-13 | M4 | `m_sourceId` is still assigned after `attachSource()` returns (Rialto supplies the ID only then), so a `needData` arriving in that window is dropped as unknown sourceId. Not changed; the pre-roll/need-data retry path covers it. Left as a documented limitation. |
| 2026-10-13 | M4 | `m_activeProtection` is also accessed from several threads; out of scope for M4, not changed. |
| 2026-10-13 | M4 | The new stress test passes in a normal build even on the old code. Races are only reported under ThreadSanitizer; verified with a manual `-fsanitize=thread` build run via `setarch -R` (the old code reported races on `m_streamFormat`, `m_pendingCodecData` and `m_sourceId`, the new code none). |
| 2026-10-13 | M5 | `HandleSeekDone` woke `Flush()` waiters (state to FLUSHED plus notify) before releasing `PlayHold::Flushing`, so a waiting flush could apply its hold and then have it dropped by the stale release, letting `play()` run mid-flush. The release now happens while the state is still FLUSHING. |
| 2026-10-13 | M5 | `SendSample_PositionPending_VideoFlushesToSamplePts` hung or failed under CPU load on the unmodified code too (about 1.7% of runs with extra load): its 500 ms wait for FLUSHING expired while the sender thread was starved, so SEEK_DONE was posted early and ignored. Fixed in the test (5 s waits, wait for play() before needData, stop the player instead of joining forever). |
| 2026-10-13 | M5 | Flush() called from an injector thread still blocks that injector for up to the SEEK_DONE timeout (5 s) if a previous flush is outstanding; this is unchanged behaviour, now serialised. |
| 2026-10-14 | M6 | The controller decided play() under its mutex but ran the action after releasing it, so a concurrent `AddHold(Flushing)` could return while play() was still pending. `IssuePlay` then ungated sources the flush had just gated. A new `m_playMutex` (recursive) is taken first by every controller operation and held until the play action returns, so `AddHold` and `Reset` wait for a play already decided. |
| 2026-10-14 | M6 | The pre-roll action is run with `m_playMutex` released: the monitor's `Start()` waits for an in-flight poll, and that poll calls `ReleaseHold`, which would otherwise deadlock (confirmed by removing the unlock; the guard test hangs). |
| 2026-10-14 | M6 | Not changed, by decision: `Pause(false)` and `StopBuffering()` still call `play()` directly. They are explicit AAMP resume commands, the gate-clearing after a paused seek depends on them (existing tests and comments), and routing them through holds could leave a resume deferred indefinitely behind a hold such as FragmentCaching. `NotifyFragmentCachingOngoing()` still issues `pause()` directly for the same reason. Revisit if a concrete mis-ordering is found. |
| 2026-10-14 | M6 | Cost: `AddHold`/`ReleaseHold`/`RequestPlay` callers on other threads now wait for an in-flight `play()` IPC to return (previously they did not). No lock cycle found: the play action takes only source locks and no controller call is made from another lock holder. |
| 2026-10-14 | M7 | `m_videoRectangle` (a `std::string`) is now guarded by `m_videoRectangleMutex`; `GetVideoSize()` reads it through `GetVideoRectangle()`. TSAN on the old code reported races inside `std::string` for this member. |
| 2026-10-14 | M7 | `m_audioVolume` is now `std::atomic<int>`, and `applyAudioVolume()` loads it once so the mute decision and the `setVolume()` value come from the same read. TSAN on the old code reported the race in `SetAudioVolume`. |
| 2026-10-14 | M7 | `m_pendingAttach` is now guarded by a leaf mutex `m_pendingAttachMutex` (taken in `AttachSource` when deferring, in the deferred-drain loop, and in `BeginNewSession`). It deliberately does not use `m_attachMutex`: an injector thread can hold that across a DRM license fetch (M9), and `Configure()` would then block behind it. The drain loop now moves the entry out under the lock and resets it before calling `AttachSource`, instead of after; the result is the same because a drained non-video source is never re-deferred once video is attached. |
| 2026-10-14 | M7 | No automated test for the `m_pendingAttach` change: exercising it needs `Configure()` to run concurrently with an attach, and `Configure()` also replaces `m_sources` under the injecting thread, which is a separate hazard outside M7, so such a test would be unsound. The fix rests on code review (every access is now under the leaf mutex). |
| 2026-10-14 | M7 | Not changed, by decision: `m_playbackQuality`. `GetVideoPlaybackQuality()` returns a pointer into the member, so a mutex cannot protect the caller's read, and it is only called from the AAMP monitor thread. A real fix needs a by-value or caller-supplied-buffer interface in `StreamSink`. `m_drmBridge` was already guarded by H8; `m_rate` was already atomic. |
| 2026-10-14 | M7 | The new `Flush_ConcurrentCallers` test alone passed 360/360 at 6 workers; moderate-load full-suite runs fail at about the same rate before and after M7 (see section 6, T3). |
| 2026-10-14 | M8 | `AampRialtoMediaPipelineClient` assigned its eight `std::function` members from `Set*Callback()`/`ClearCallbacks()` on the control thread while Rialto read and invoked them on its own thread. All access now goes through `m_callbackMutex`; delivery copies the callback under the lock (`Snapshot()`) and invokes the copy without it, so a callback may call a setter. TSAN on the old client reported the race in `SetNeedDataCallback`, and the old code did not finish the stress test (it was spinning after 8 minutes and had to be killed). |
| 2026-10-14 | M8 | The player now registers the pipeline callbacks right after `createMediaPipeline()` returns and before `load()`, instead of after `load()` succeeds. Events Rialto delivers while loading are no longer dropped; `OnDuration` is the one observed in the test. |
| 2026-10-14 | M8 | Not covered: events delivered from inside `createMediaPipeline()` itself still arrive before any callback is installed, because the client must exist (and be passed to the factory) before the callbacks can reference the player. Closing that gap would mean installing the callbacks before creating the pipeline, so handlers could run with no pipeline set; not done. |
| 2026-10-14 | M8 | Behaviour change to be aware of: handlers such as `OnPlaybackState` can now run between `load()` starting and `onPipelineLoaded()`, when the state machine is still IDLE. A FAILURE there is ignored as 'no active session' by `HandlePlaybackFailure` (as before it was dropped silently), other events are logged and ignored by the state machine. |
| 2026-10-14 | M8 | `Callback_MayReplaceItselfWhileRunning_DoesNotDeadlock` is a guard against holding the lock across the invocation; it passes on both old and new code and was not red-checked. |
| 2026-10-14 | M8 | One moderate-load run (6 workers x 40) after M8 had 0/240 failures and no kernel soft lockups, against 7 to 11 per 240 in earlier runs; the environment varies run to run, so this is not evidence about M8 either way (see section 6, T3). |
| 2026-10-14 | M9 | Verified from Rialto's client source that pipeline events and key events are dispatched on separate per-object event threads, so the suspected deadlock (a key-status callback queued behind a pipeline handler waiting on `m_attachMutex`) cannot occur through a shared thread. See Verification Notes. |
| 2026-10-14 | M9 | Lock order checked and written beside `m_attachMutex` in `AampRialtoPlayer.h`: `m_flushCallMutex` -> `m_flushMutex` and -> the controller play lock; `m_attachMutex` -> the controller play lock; the two outer mutexes are never held together (`AttachSource` never calls Flush/Stop/Configure; `ApplySubtitleSegmentPosition` is only called from `HandleSeekDone`). No cycle found. |
| 2026-10-14 | M9 | Left as is, by decision: `m_attachMutex` is still held by an injector thread across a DRM license fetch inside `AttachSource`, and `SetVideoMute`, `SetSubtitleMute`, `setCCMute` and the SEEK_DONE subtitle-position update take the same mutex. A slow licence fetch therefore stalls those calls (and the pipeline event thread for SEEK_DONE) for its duration. Shortening it means releasing the lock around `createSession`, which changes the serialisation of attach against caps updates, and needs a device test; not done here. |
| 2026-10-14 | M9 | The lock-order check was done by reading the code (a static call scan over-approximated and was discarded); it is not enforced by a test or a tool. |
| 2026-10-14 | M10 | Verified from Rialto's client source (`MediaPipelineIpc.cpp`): server notifications are queued to a dedicated event thread (`rialto-media-player-events`); the blocking RPCs `HandleSeekDone` makes (`setSourcePosition`, `play`, `haveData`) wait on the IPC channel thread, which no handler occupies. Blocking IPC from the notification thread therefore cannot deadlock. See Verification Notes. |
| 2026-10-14 | M10 | Why `rialto-gstreamer` bounces these calls to a worker: not established. The reason may be latency (the event thread is held for each round trip, so later events queue behind it) rather than deadlock; I did not find a statement of intent in the sources read. |
| 2026-10-14 | M10 | Not changed, by decision: `HandleSeekDone` still issues `setSourcePosition`, `play()` and `haveData()` from the event thread. Moving them to a worker would add a thread and an ordering problem (SEEK_DONE handling must finish before the next event) for a latency benefit that has not been measured. Revisit if a trace shows events delayed behind these calls. |
| 2026-10-14 | M10 | Source reading, not a runtime test; the Rialto version on the target was not checked. |
| 2026-10-14 | M11 | `MediaKeysClient::onKeyStatusesChanged` ran the per-key and keys-updated callbacks while holding the client mutex. A callback that destructs its own session (which deregisters under that mutex) deadlocked; the new test hangs on the old code and is killed at its timeout. The session caches are now updated under the lock, the callbacks copied out, and run after the lock is released (as `onLicenseRequest` and `onLicenseRenewal` already did). |
| 2026-10-14 | M11 | `RialtoMediaKeySession` had a defaulted destructor, so a session dropped without `destruct()` left a dangling raw pointer in `MediaKeysClient::sessions`; the next key-status event wrote to freed memory. AddressSanitizer on the old code reports a heap-use-after-free in `updateKeyStatus`. The destructor now deregisters, and `destruct()` clears its deregister callback so the destructor does not repeat it. Because deregistering takes the client mutex that the cache update also holds, a session cannot be destroyed in the middle of an update. |
| 2026-10-14 | M11 | Behaviour change: the session's key-status cache is now fully updated for every key in an event before any `onKeyUpdate` callback runs; before, the callback for key N saw only keys up to N updated. A callback that queries other keys of the same update now sees their new status. The callbacks still run in the same order and `onKeysUpdated` still fires last. |
| 2026-10-14 | M11 | A session dropped without `destruct()` now drops its callbacks (no further events reach them) but the Rialto-side key session is still not released: that needs `destruct()`, by design, and is not changed. |
| 2026-10-14 | M11 | The moderate-load run after M11 had 14/240 failures, all in `AampRialtoPlayerWithDemuxTest` tests of the T3 family and clustered in time across workers; the player suite does not link the key-system sources, and no kernel soft lockups were logged in the window. Recorded as environment noise, not evidence about M11 (see section 6). |
| 2026-10-14 | M12 | Lost wakeup: `m_eKeyState` was stored and `m_stateChanged.notify_all()` called without holding `m_eventMutex`, so a `waitForState()` that had tested its predicate but not yet blocked missed the change and slept until its timeout. A new `setState()` stores under `m_eventMutex` and then notifies; every state change in the adapter goes through it. The new test, on the old adapter, missed 2, 4, 7 and 8 wakeups in 150 iterations over four runs; none on the fixed adapter. |
| 2026-10-14 | M12 | `m_session` was read by `processDRMKey()` and `getMediaKeySessionId()` while `clearDecryptContext()` reset it (TSAN reported data races and a heap-use-after-free on the old code). It is now a `shared_ptr` guarded by a leaf `m_sessionMutex`; readers take a snapshot with `getSession()` and use it outside the lock, so the session stays alive for a `processDRMKey()` in progress. `m_mutex` is not held across the key-status wait. |
| 2026-10-14 | M12 | Stale key status: `m_keyStatusReceived` was never reset, so a second `processDRMKey()` did not wait for the status its own response produced and read the previous one (a restricted key was reported as usable). It is now cleared immediately before a response is applied. It is deliberately NOT cleared when `processDRMKey()` is called with a NULL key (external acquisition), because there the status may already have arrived. |
| 2026-10-14 | M12 | Not changed, by decision: `m_challengeReceived` is still not consumed when `generateKeyRequest()` reads the challenge. Renewal is not wired up here (the renewal callback ignores its message and does not issue a new request), so consuming the flag could turn today's re-read into a timeout. When renewal is implemented, clear the flag when a renewal starts. `generateKeyRequest()` now also notifies state waiters when it sets KEY_ERROR/KEY_PENDING, which it did not before. |
| 2026-10-14 | M12 | Test notes: the existing `ProcessDRMKeySuccess` test delivered the key status from a thread started before `processDRMKey()`, which can now land before the flag is cleared; it delivers it from inside the `updateSession` mock instead (a real status can only follow the update). My first versions of two new tests were themselves unsound and were rewritten: a 300 ms elapsed-time threshold that a stalled VM trips (33/360 failures), and a result check that cannot fail because `wait_for` returns the predicate's value after a timeout (the test passed on the old code). The final tests fail on the old code and passed 360/360 parallel runs on the new. |
| 2026-10-14 | M13 | `AampRialtoControlBackend` declared `m_mutex` and `m_stateCv` after `m_control`, so they were destroyed before the `IControl` that can still deliver a notification; and the nested `ControlClient` held a reference to the backend, so a notification still running (Rialto's event thread holds the client for the call) when the backend was destroyed used a freed object. Reordering the members would only have fixed the first. The mutex, condition variable and state now live in a small `SharedState` that the client co-owns, so neither the declaration order nor the backend's lifetime matters. AddressSanitizer on the old code reports a heap-use-after-free in `onApplicationStateChanged`. |
| 2026-10-14 | M13 | `m_rialtoClientState` was passed by reference to `registerClient()` and written there without the lock while a notification could write it under the lock (TSAN data race on the old code). `registerClient()` now receives a local; the result is merged under the lock only if no notification has arrived, so an older initial state cannot overwrite a newer RUNNING (the old code did, and the new test for it fails there). |
| 2026-10-14 | M13 | `waitForRunning()` no longer has the early-return fast path; it waits on the predicate, which is equivalent. `onApplicationStateChanged()` is removed (the client does that work) and `notify_one` became `notify_all`; the class is only used through `IRialtoControlBackend`, so nothing else changes. |
| 2026-10-14 | M13 | Test fake `FakeAampRialtoControlBackend.cpp` implemented the removed member and initialised the old fields, so it was updated with the class. Note for next time: building the suite before updating the fake failed, and the 'PASSED' line printed after that came from a stale binary; the binary timestamp must be checked. |
| 2026-10-14 | M13 | Moderate-load run after M13: 0/240 failures and no kernel soft lockups (earlier runs of the same load had 7 to 14); the environment varies, so this is not evidence about M13. |
| 2026-10-14 | M14 | `PrivateInstanceAAMPNotifiable::m_aamp` was a plain pointer written by `ChangeAamp()` while Rialto and injector threads read it (ThreadSanitizer reports a data race at the `ChangeAamp` write on the old code). Most methods also dereferenced it without a null check, unlike `GetProgressReportIntervalSeconds()`, and some read it twice in one call, so a change between the reads mixed two instances. `m_aamp` is now `std::atomic<PrivateInstanceAAMP *>` (release store, acquire load). Each notification takes one snapshot through the new private `Target()` and uses only that; `Target()` logs a warning and returns null when there is no instance, and the method then returns without doing anything. `GetState()` returns `eSTATE_IDLE` in that case. |
| 2026-10-14 | M14 | Behaviour change: a notification delivered to a notifiable that has no instance used to crash; it is now dropped with a warning. The snapshot is also what is handed to the scheduled task, so a task already queued keeps the instance it was queued for. |
| 2026-10-14 | M14 | Not changed (out of scope): the raw `new`/`delete` of the argument structs in this file (P1). `FakePrivateInstanceAAMPNotifiable.cpp` is unaffected (the header only gained a private member). The `PrivateInstanceAAMPNotifiableTests` suite is not in the 14-suite regression script, so it was run separately. |
| 2026-10-14 | M14 | Moderate-load run after M14: 4 of 240 runs hit the 30 s cap of the stress script (a full run takes 1.7 s unloaded); none is an assertion failure, each log ends mid-test (four different tests, none touching the notifiable), and they fall in two pairs that started together at 12:13:00 and 12:13:07, with no kernel soft lockups logged. Cause not established; recorded as unexplained noise for M15, not as evidence about M14. |
| 2026-10-14 | L1 | `Configure()` rebuilt the pipeline on an elementary-stream change without stopping the old one: `CreateAndLoadPipeline()` called `createMediaPipeline()` while `m_pipeline` and the AV monitor (which holds its own reference) still kept the old Rialto session alive, and the old one was only released when the new pipeline was stored. Rialto's server opens a session per `createMediaPipeline()`, so two sessions coexisted. New `ReleasePipelineForReconfigure()` runs before `BeginNewSession()`: it wakes injectors, drops the AV monitor, calls `stop()` and releases the pipeline. It deliberately is not `Stop()`, which resets the play controller and control backend, because `Configure()` can run mid-session with a play request pending. |
| 2026-10-14 | L1 | `m_sources` held `unique_ptr` sources and injector threads (`SendSample`/`SendCopy`/`SendTransfer`/`EndOfStreamReached`, Rialto callbacks) used raw pointers from `getSource()` with no lock, so replacing a source in `Configure()` could free it under a thread still inside it, and the array itself was read and written from different threads. `m_sources` is now `shared_ptr` behind a leaf `m_sourcesMutex`; `getSource()`, `findSourceByRialtoId()` and the new `SnapshotSources()` return shared references, `SetSource()` swaps under the lock and releases the old source after it is dropped, so a replaced source lives until the last injector leaves. Lock order note in `AampRialtoPlayer.h` updated (leaf lock). |
| 2026-10-14 | L1 | Existing sources that a reconfigure does not recreate (for example video going away) are still kept as before, only reset; that is unchanged and out of scope for L1. |
| 2026-10-14 | L1 | Verification: ThreadSanitizer run of the whole player suite on the new code: 299 tests, 0 warnings. The old code fails all three new tests (pipeline `stop()` never called, old pipeline still alive when the new one is created, source destroyed while an injector is inside it). 14-suite regression passes. |
| 2026-10-14 | L2 | `ShouldRecreatePipeline()` returned true whenever the subtitle source was null at normal rate. Audio-only content never creates one (the sidecar branch needs a subtitle format and the inband-CC fallback needs video), so every `Configure()` of audio-only content rebuilt the pipeline. A missing subtitle source is now a change only when a subtitle format is requested; a codec change on an existing subtitle source still recreates. The same condition covers builds with `RIALTO_PLAYER_DISABLE_SUBTITLES`, where the source is never created. |
| 2026-10-14 | L2 | The existing test `Configure_AudioOnlyNoSubtitleSource_SecondCallDoesNotDereferenceNull` asserted two `createMediaPipeline` calls, i.e. it pinned the defect. It is renamed `Configure_AudioOnlyNoSubtitleSource_SecondCallDoesNotRecreate` and expects one; `Configure_AudioOnly_SubtitleFormatAppears_RecreatesPipeline` covers the case that must still rebuild. |
| 2026-10-14 | L2 | Unexplained, recorded for M15: in the first three runs of the 14-suite script after this change `AampRialtoVideoSourceTests` reported no result line in two of them (the suite passes 62/62 when run directly 3/3 times, 180 of 180 times in a 3-worker loop, and in the next five full-sequence runs; no kernel soft lockups logged). The raw output of the two failing runs was not captured; the helper script now keeps raw output (`/tmp/raw_<suite>.out`) so a recurrence can be diagnosed. Nothing in that suite's inputs changed. |
| 2026-10-14 | L3 | `AampRialtoSubtitleSource::createSegment()` called `setDisplayOffset()` only for offsets above zero, while `processDataFragment()` passes `-fragmentPTSoffset` for WebVTT on purpose (the same idea as `GST_BUFFER_OFFSET = -(ptsOffset*1000)` in the non-direct path). So the negative offset was never applied. It now applies any non-zero offset, passed as its two's-complement value. Checked in the local Rialto source: `MediaSegment::setDisplayOffset` takes `uint64_t`, the metadata proto field is `uint64`, the server stores it in `GST_BUFFER_OFFSET` (`ReadShmDataAndAttachSamples.cpp`) and `GstTextTrackSink` casts it back to `int64_t` before use, so the sign survives. One value cannot be expressed: -1 ms equals `GST_BUFFER_OFFSET_NONE` and is read as no offset (1 ms error, accepted). |
| 2026-10-14 | L3 | Behaviour change: negative TTML offsets (a first cue earlier than the segment PTS, from `refineDisplayOffset()`) are now applied too, where before they were silently dropped. That matches the sign convention above; it is flagged here because it was not exercised on a device. |

## Verification Notes

Record the outcome of each **verify** item here (Rialto threading model, `NO_AVAILABLE_SAMPLES` retry behaviour, default `GMainContext` usage).

| ID | Question | Result | Date |
|---|---|---|---|
| M9 | Do pipeline and key-session events share one Rialto IPC thread? | No. In Rialto's client source (`MediaPipelineIpc.cpp`, `MediaKeysIpc.cpp`, master branch) the IPC channel callbacks only enqueue to a per-object event thread: `rialto-media-player-events` for the pipeline and `rialto-media-keys-events` for keys. A key-status callback therefore cannot queue behind a pipeline handler. Source reading, not a runtime test; the Rialto version on the target was not checked. | 2026-10-14 |
| M10 | Is blocking IPC from the notification thread safe? | Yes for deadlock. Blocking RPCs (`play`, `haveData`, `setSourcePosition`) issued from a handler wait on the IPC channel thread (`blockingClosure->wait()`), which no handler blocks, because handlers run on the separate event thread. Residual: the pipeline event thread is blocked for the length of each call, so events queue behind it. | 2026-10-14 |
| L9 | Does Rialto re-issue `needData` after `NO_AVAILABLE_SAMPLES`? | | |
| L12 | Is the default `GMainContext` iterated in the product? | | |

## Completed Work Log

Record each completed task: what was done, files changed, tests added or run.

| Date | ID(s) | What was done | Files changed | Tests |
|---|---|---|---|---|
| 2026-10-13 | H1 | Added `GetPipeline()` (mutex-guarded snapshot) and `SetPipeline()` (swap, old pipeline released outside the lock). All ~45 pipeline use sites in `AampRialtoPlayer.cpp` now use a local snapshot, so a concurrent `Stop()` cannot destroy the pipeline under an in-flight call. `AttachSource` and the SEEK_DONE helpers gained a null guard. `Stop()` still releases the pipeline before returning. | `direct-rialto/AampRialtoPlayer.h`, `direct-rialto/AampRialtoPlayer.cpp` | New `Stop_DuringPipelineCall_KeepsPipelineAliveUntilCallReturns` (fails without the fix, passes with it). All 13 Rialto L1 suites pass (Player 268). |
| 2026-10-13 | H2 | `~AampRialtoPlayer` now tears down in order: stop pre-roll and progress timer, unblock sources (pipeline snapshot scoped so it is released), reset `m_monitorAV`, `SetPipeline(nullptr)`, then `m_client->ClearCallbacks()`. Added `AampRialtoMediaPipelineClient::ClearCallbacks()`. No `stop()` is issued, as before. | `direct-rialto/AampRialtoPlayer.cpp`, `direct-rialto/AampRialtoMediaPipelineClient.h` | New `Destructor_PipelineReleasedWhilePlayerStillValid` (crashes without the fix), `Destructor_ClearsClientCallbacks`, `ClearCallbacks_RemovesInstalledCallbacks`. All 13 Rialto L1 suites pass (481 tests). |
| 2026-10-13 | H3 | `CreateSubtitleSource` now holds `m_attachMutex` around `AttachSource` for both the sidecar and inband-CC branches; `AttachSource` doc updated. | `direct-rialto/AampRialtoPlayer.cpp`, `direct-rialto/AampRialtoPlayer.h` | New `Configure_SubtitleAttach_HoldsAttachMutex` (fails without the lock), `Configure_PipelineCreationFails_AudioOnlyWithSubtitle_DoesNotCrash`. Player suite 272 pass; all 13 Rialto suites pass. |
| 2026-10-13 | H4 | `AttachSource` now returns `bool`. New `ReportAttachFailure` clears `attachPending` (waking injectors blocked in `waitForAttach`) and calls `NotifyPlaybackError`, except when the codec is not yet identified. `SendTransfer` returns `false` when the attach fails. | `direct-rialto/AampRialtoPlayer.cpp`, `direct-rialto/AampRialtoPlayer.h` | 4 new tests (3 fail without the fix). Player suite 276 pass; all 13 Rialto suites pass (487 tests). |
| 2026-10-13 | H5 | New `HandlePlaybackFailure()` (called from `OnPlaybackState`): records the state, moves the machine to `ERROR`, wakes flush waiters as before, and calls `NotifyPlaybackError(AAMP_TUNE_GST_PIPELINE_ERROR)` unless the prior state was `IDLE` or `ERROR`. | `direct-rialto/AampRialtoPlayer.cpp`, `direct-rialto/AampRialtoPlayer.h` | 3 new `PlaybackStateFailure_*` tests (2 fail without the fix; the third guards the stale-IDLE case). Player suite 279 pass; all 13 Rialto suites pass (490 tests). |
| 2026-10-13 | H6 | `WaitForFlushToComplete` now uses `wait_for` with `m_flushCompleteTimeout` (5 s). On timeout it logs an error, moves the state machine out of `FLUSHING`, commits the staged rate, notifies waiters and releases `PlayHold::Flushing`. | `direct-rialto/AampRialtoPlayer.cpp`, `direct-rialto/AampRialtoPlayer.h` | 3 new tests (all hang without the fix): `Stop_FlushNeverCompletes_ReturnsAfterTimeout`, `Flush_PreviousFlushLost_RecoversAfterTimeout`, `Configure_FlushNeverCompletes_IssuesDeferredPlay`. Player suite 282 pass; all 13 Rialto suites pass (493 tests). |
| 2026-10-13 | H7 | Added `AampRialtoPlaybackController::Reset()` (discard the play request, restore the initial hold set, clear the pre-roll latch; never issues `play()`) and call it from `StopInternal` in place of `CancelPlayRequest`. | `direct-rialto/AampRialtoPlaybackController.h`, `direct-rialto/AampRialtoPlaybackController.cpp`, `direct-rialto/AampRialtoPlayer.cpp` | 4 new tests: controller `Reset_DropsLeakedHoldsAndRequest`, `Reset_ForgetsPreRollReadiness`; player `Stop_AfterFailureDuringFlush_NextSessionPlays` and `Stop_DuringFragmentCaching_NextSessionPlays` (both fail with the old `Stop`). All 13 Rialto suites pass (497 tests). |
| 2026-10-13 | H8 | `m_aamp` is now `std::atomic` and read through `Aamp()`. The DRM bridge is created and swapped under `m_drmBridgeMutex` via `GetDrmBridge()`, which returns a snapshot so an in-flight `AttachSource` is safe. `ChangeAamp()` drops the default bridge (kept if installed by `SetEncryptedAamp()`); `SetEncryptedAamp(nullptr)` reverts to the default. | `direct-rialto/AampRialtoPlayer.cpp`, `direct-rialto/AampRialtoPlayer.h`, `test/utests/fakes/FakeAampDrmBridge.cpp`, `test/utests/mocks/MockDrmBridge.h` | 3 new tests (2 fail without the fix; the third guards the explicit-bridge case). Player suite 287 pass; all 13 Rialto suites pass (502 tests). |
| 2026-10-13 | M1 | Added `AampRialtoGlibTimer.h`. `ProgressTimer` is reimplemented on it: callbacks run under the timer lock, `stop()` waits for an in-flight tick and later dispatches are ignored, including after the player is destroyed. Removed the lazily created timer pointer. | `direct-rialto/AampRialtoGlibTimer.h` (new), `direct-rialto/AampRialtoPlayer.h`, `direct-rialto/AampRialtoPlayer.cpp`, `test/utests/fakes/FakeGLib.cpp`, `test/utests/fakes/FakeAampRialtoPlayer.cpp` | 3 new tests, all fail on the old code: `StopProgressTimer_WaitsForTickInProgress`, `ProgressTimer_DispatchAfterStop_DoesNothing`, `ProgressTimer_DispatchAfterPlayerDestroyed_IsSafe`. Player suite 290; all 13 Rialto suites plus AampStreamSinkManagerTests pass. |
| 2026-10-13 | M2 | Moved `AampRialtoMonitorAV` onto `AampRialtoGlibTimer` and removed its static GLib callbacks and raw timer IDs. Guarded the player's `m_monitorAV` with `m_monitorMutex` and `SetMonitorAV()`. | `direct-rialto/AampRialtoMonitorAV.h`, `direct-rialto/AampRialtoMonitorAV.cpp`, `direct-rialto/AampRialtoPlayer.h`, `direct-rialto/AampRialtoPlayer.cpp` | 4 new tests: `stop_WaitsForTickInProgress`, `dispatchAfterStop_DoesNothing`, `dispatchAfterDestruction_IsSafe` (MonitorAV suite) and `Stop_WaitsForMonitorAvTickInProgress` (player suite). All fail or crash on the old code. Full set passes (MonitorAV 19, Player 291). |
| 2026-10-13 | M3 | Moved the pre-roll poll scheduling onto `AampRialtoGlibTimer`; removed the raw timer ID and `TimerCallback`. `Stop()` waits for a poll in progress and `Start()` stops the old timer before replacing the probe. | `direct-rialto/AampRialtoPreRollMonitor.h`, `direct-rialto/AampRialtoPreRollMonitor.cpp` | 3 new tests (1 fails on the old code, 2 are guards): `Stop_WaitsForPollInProgress`, `DispatchAfterStop_DoesNothing`, `DispatchAfterDestruction_IsSafe`. PreRoll suite 21; full set passes. |
| 2026-10-13 | M4 | `m_sourceId`, `m_mksId` and `m_streamFormat` are now `std::atomic`; `m_pendingCodecData` is guarded by a new `m_codecDataMutex` (taken in `attachOrUpdate`, `reset`, `takePendingCodecData`); new locked `isEos()` replaces unlocked `state().eos` reads in `LogSourceSnapshot` and the subtitle checks in the player. | AampRialtoMediaSource.{h,cpp}, AampRialtoPlayer.cpp, AampRialtoSubtitleSource.cpp, AampRialtoAudioSource.cpp, AampRialtoVideoSource.cpp | VideoSource: IsEos_ReflectsSignalEosAndReset, ConcurrentAttachResetAndReaders_SeeOnlyValidValues (TSAN-verified red/green); all 14 Rialto suites pass |
| 2026-10-13 | M5 | New recursive `m_flushCallMutex` serialises `Flush()`, `StopInternal()` and the injector-thread deferred flush (flush plus its `Stream()` are atomic against `Stop()`; the PositionPending hold is re-checked after taking the lock). `HandleSeekDone` releases the Flushing hold before leaving FLUSHING. | AampRialtoPlayer.{h,cpp} | Player: SeekDone_ReleasesFlushingHoldBeforeLeavingFlushing, Flush_ConcurrentCallers_NeverOverlapInFlight (both red on the old sources, green now); SendSample_PositionPending_VideoFlushesToSamplePts made load-robust; 14 suites pass, 0/200 stress failures |
| 2026-10-14 | M6 | `AampRialtoPlaybackController` gains `m_playMutex`, held across the play decision and action; `Dispatch` releases it around the pre-roll action. | AampRialtoPlaybackController.{h,cpp} | Controller: AddHold_WaitsForPlayAlreadyDecided (red on the old controller, green now), PreRollAction_MayWaitForAnotherThreadUsingTheController (guard; deadlocks if the unlock is removed); 14 suites pass |
| 2026-10-14 | M7 | Guarded `m_videoRectangle` (mutex), `m_audioVolume` (atomic, single read per apply) and `m_pendingAttach` (leaf mutex). | AampRialtoPlayer.{h,cpp} | Player: VideoRectangle_ConcurrentSetAndGet_NeverReadsATornValue and SetAudioVolume_ConcurrentCallers_ForwardOnlyWrittenValues (pass normally on old and new code; TSAN reports races on the old code, none on the new); 14 suites pass |
| 2026-10-14 | M8 | Client callbacks guarded by `m_callbackMutex` with copy-then-invoke delivery; player callbacks installed before `load()`. | AampRialtoMediaPipelineClient.{h,cpp}, AampRialtoPlayer.cpp | Client: Callbacks_ConcurrentInstallAndDelivery_OnlyRunsWholeCallbacks (TSAN red on old, clean on new), Callback_MayReplaceItselfWhileRunning_DoesNotDeadlock (guard). Player: Configure_EventDeliveredDuringLoad_IsNotDropped (red on old, green now). 14 suites pass. |
| 2026-10-14 | M9 | Documented the lock order and the long-hold caveat on `m_attachMutex`; verified the thread model from Rialto's source; no behaviour change. | AampRialtoPlayer.h (comment only) | None added (comment-only change). Player suite builds and passes. |
| 2026-10-14 | M10 | Verified the thread model and recorded the outcome; no code change. | docs only | None (verification task). |
| 2026-10-14 | M11 | Callbacks in `onKeyStatusesChanged` run outside the client mutex; `RialtoMediaKeySession` destructor deregisters (with `destruct()` clearing its callback). | RialtoMediaKeySystem.cpp, RialtoMediaKeySession.{h,cpp} | KeySystem: OnKeyStatusesChanged_UpdatesSessionThenRunsCallbacks (behaviour), _CallbackMayDestructItsSession_DoesNotDeadlock (hangs on old), _AfterSessionDroppedWithoutDestruct_IsIgnored (ASAN use-after-free on old). 14 suites pass. |
| 2026-10-14 | M12 | Adapter state changes go through `setState()` (store and notify under `m_eventMutex`); `m_session` is a `shared_ptr` behind `m_sessionMutex`; the key-status flag is cleared before a new response is applied. | RialtoMediaKeySessionAdapter.{h,cpp} | Adapter: WaitForState_StateSetJustAfterPredicateCheck_IsNotMissed, ClearDecryptContext_DuringProcessDRMKey_IsSafe (TSAN red on old, clean on new), ProcessDRMKey_SecondResponse_WaitsForItsOwnKeyStatus (red on old). ProcessDRMKeySuccess made order-correct. 14 suites pass; adapter suite 0 TSAN warnings and 0/360 parallel failures. |
| 2026-10-14 | M13 | State, mutex and condition variable moved into a `SharedState` co-owned by the control client; `registerClient()` writes a local merged under the lock. | AampRialtoControlBackend.{h,cpp}, test fake FakeAampRialtoControlBackend.cpp | ControlBackend: NotificationAfterBackendDestroyed_DoesNotTouchFreedBackend (ASAN use-after-free on old), RegisterClient_ConcurrentNotification_IsRaceFree (TSAN race and failure on old), RegisterClient_NotificationDuringRegistration_IsNotOverwritten (fails on old). 14 suites pass; 0 ASAN errors and 0 TSAN warnings on the new code. |
| 2026-10-14 | M14 | `m_aamp` is atomic and every method uses one snapshot via `Target()`; a null instance drops the notification (warning) and `GetState()` returns IDLE. | `direct-rialto/PrivateInstanceAAMPNotifiable.h`, `direct-rialto/PrivateInstanceAAMPNotifiable.cpp`, `test/utests/tests/PrivateInstanceAAMPNotifiableTests/PrivateInstanceAAMPNotifiableTestCases.cpp` | New: NullInstance_NotificationsAreDroppedAndGetStateIsIdle, ChangeAamp_ToNull_StopsForwarding, ChangeAamp_ConcurrentWithNotifications_IsRaceFree. Red on old code (null tests fail; TSAN data race); green on new code (22/22, TSAN clean); 14-suite regression passes. |
| 2026-10-14 | L1 | Old pipeline (and the AV monitor's reference) is stopped and released before a new one is created; `m_sources` is `shared_ptr` behind a leaf lock and injectors keep their source alive for the call. | `direct-rialto/AampRialtoPlayer.h`, `direct-rialto/AampRialtoPlayer.cpp`, `test/utests/tests/AampRialtoPlayerTests/AampRialtoPlayerTestCases.cpp` | New: Configure_Recreate_StopsAndReleasesOldPipelineBeforeCreatingNewOne, Configure_Recreate_ReleasesOldPipelineHeldByMonitorAV, Configure_Recreate_KeepsReplacedSourceAliveWhileInjectorIsInsideIt (fixture gains TrackedMockSource/TrackedMockPipeline). Red on old code, green on new; TSAN clean; 14 suites pass (Player 299). |
| 2026-10-14 | L2 | A missing subtitle source no longer forces a rebuild unless a subtitle format is requested. | `direct-rialto/AampRialtoPlayer.cpp`, `test/utests/tests/AampRialtoPlayerTests/AampRialtoPlayerTestCases.cpp` | Changed: Configure_AudioOnlyNoSubtitleSource_SecondCallDoesNotRecreate (was ...DoesNotDereferenceNull, expected 2 creations). New: Configure_AudioOnly_SubtitleFormatAppears_RecreatesPipeline. Red on old code for the changed test; green on new (player suite 300); 14 suites pass. |
| 2026-10-14 | L3 | Non-zero (including negative) subtitle display offsets are applied to the segment. | `direct-rialto/AampRialtoSubtitleSource.cpp`, `test/utests/tests/AampRialtoSubtitleSourceTests/AampRialtoSubtitleSourceTestCases.cpp` | New: CreateSegment_NegativeDisplayOffset_IsApplied (red on old code), CreateSegment_PositiveDisplayOffset_IsApplied, CreateSegment_ZeroDisplayOffset_IsNotSet. Subtitle suite 16 tests; 14 suites pass. |
