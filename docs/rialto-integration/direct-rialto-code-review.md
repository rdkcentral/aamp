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
| H6 | Open | `Stop()`/`Configure()` can wait forever for a flush |
| H7 | Open | Play holds leak across sessions |
| H8 | Open | `ChangeAamp` leaves stale pointers (DRM bridge) |

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
| M1 | Open | `ProgressTimer` has no locking |
| M2 | Open | `m_monitorAV` unique_ptr and `AampRialtoMonitorAV` timer IDs unguarded |
| M3 | Open | `AampRialtoPreRollMonitor::Stop()` does not wait for in-flight `Poll()` |
| M4 | Open | `AampRialtoMediaSource` fields read lock-free |
| M5 | Open | `Flush()` documented as control-thread only but called from injector threads |
| M6 | Open | Hold release and `play()` not atomic; other direct `play()` call sites bypass holds |
| M7 | Open | Unsynchronised plain members (`m_videoRectangle`, `m_audioVolume`, etc.) |
| M8 | Open | Rialto callback setters race with delivery |
| M9 | Needs verification | Possible deadlock: `m_attachMutex` held across DRM/IPC |
| M10 | Needs verification | Blocking Rialto IPC from the Rialto callback thread |
| M11 | Open | `MediaKeysClient::onKeyStatusesChanged` invokes callbacks under mutex |
| M12 | Open | `RialtoMediaKeySessionAdapter` lost wakeups and stale state |
| M13 | Open | `AampRialtoControlBackend` member order |
| M14 | Open | `PrivateInstanceAAMPNotifiable::m_aamp` racy and not null-checked |

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

---

## 3. Potential Crashes and Logic Defects (Medium/Low)

| ID | Status | Title |
|---|---|---|
| L1 | Open | Pipeline replaced without stopping on ES-change reconfigure |
| L2 | Open | `ShouldRecreatePipeline` forces rebuild for audio-only content |
| L3 | Open | Subtitle negative display offset never applied |
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

## Verification Notes

Record the outcome of each **verify** item here (Rialto threading model, `NO_AVAILABLE_SAMPLES` retry behaviour, default `GMainContext` usage).

| ID | Question | Result | Date |
|---|---|---|---|
| M9 | Do pipeline and key-session events share one Rialto IPC thread? | | |
| M10 | Is blocking IPC from the notification thread safe? | | |
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
