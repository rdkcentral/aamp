# Direct Rialto - Threading model and locking

## Purpose
This is the authoritative description of which threads run inside
direct-rialto, which locks and atomics protect which state, and the order
in which locks may be nested. `AampRialtoPlayer` is called concurrently by
the AAMP control thread, one injector thread per track, the Rialto client's
event thread and the GLib main loop, so most past bugs in this component
were interleavings rather than logic errors. Read this page before adding a
lock, a callback, a blocking wait or a timer. DRM thread internals are
summarised only; page "DRM" owns the detail.

## Source files
| File | Role |
|---|---|
| `AampRialtoPlayer.h/.cpp` | The `StreamSink`. Owns every player-level lock (`m_flushCallMutex`, `m_attachMutex` and nine leaf mutexes) and the atomics shared between threads. The class header comment and the comment beside `m_attachMutex` state the intended threading model. |
| `AampPlayerStateMachine.h/.cpp` | `PlayerStateMachine`: lifecycle state under `m_mutex`, plus the `m_stateChanged` condition variable used to wait for a flush to complete. |
| `AampRialtoPlaybackController.h/.cpp` | Decides when `play()` may be issued. Two locks: the recursive play-dispatch lock `m_playMutex` (held while `play()` is issued) and the state lock `m_mutex`; `m_holds` is atomic. |
| `AampRialtoNeedDataHandshake.h/.cpp` | Per-source need-data state; `State::mu` + `State::cv` are what injector threads block on. |
| `AampRialtoMediaSource.h/.cpp` | Per-track source; codec data under `m_codecDataMutex`, counters and IDs atomic. |
| `AampRialtoPipelineSession.h/.cpp` | Holds the `IMediaPipeline` `shared_ptr` under the leaf `m_pipelineMutex`. |
| `AampRialtoMediaPipelineClient.h/.cpp` | The `IMediaPipelineClient` Rialto calls on its event thread; forwards to the player through a sink pointer guarded by `m_sinkMutex`. |
| `AampRialtoControlBackend.h/.cpp` | Waits (with timeout) for the Rialto application state to become RUNNING. |
| `AampRialtoGlibTimer.h` | Periodic timer on the default GLib main context with a recursive lock that makes `Stop()` wait for a running tick. |
| `AampRialtoProgressTimer.h`, `AampRialtoPreRollMonitor.h/.cpp`, `AampRialtoMonitorAV.h/.cpp` | The three users of `AampRialtoGlibTimer`. |
| `AampRialtoSegmentPosition.h/.cpp` | Staged flush position, rate and segment baseline; all atomics, no lock. |
| `PrivateInstanceAAMPNotifiable.h/.cpp` | Adapter from the player to `PrivateInstanceAAMP` (the "notifiable"); the calls into AAMP core. |
| `RialtoMediaKeySystem.*`, `RialtoMediaKeySessionAdapter.*`, `RialtoMediaKeySession.*`, `AampDrmBridge.*` | DRM objects with their own locks and condition variables (summary only, see page "DRM"). |

## 1. Threads at a glance
direct-rialto creates no threads of its own: there is no `std::thread`, `std::async`, `g_idle_add` or private `GMainContext` anywhere in `direct-rialto/`. Every piece of code runs on a thread that belongs to someone else:

- AAMP owns the **control thread(s)** and the **per-track injector threads**, and the **AAMP scheduler thread** that runs the notifications direct-rialto posts back.
- The Rialto client library owns the **event thread** that delivers `IMediaPipelineClient`, `IControlClient` and `IMediaKeysClient` callbacks.
- The host application owns the **GLib main loop** that dispatches `AampRialtoGlibTimer` ticks.

The design therefore relies on three things: locks inside the player, condition variables that injector threads block on, and "post it to the AAMP scheduler" for every notification into AAMP core.

[[diagram:direct-rialto-threading-component]]
*Threads that enter direct-rialto, the entry points each one calls, and where each one can block.*

The class comment on `AampRialtoPlayer` (in `AampRialtoPlayer.h`) lists the same threads. Checked against the code, it is accurate with three omissions:

- `Configure()` is not only called from the control thread. `PrivateInstanceAAMP::SetStreamFormat()` is called by `TSProcessor::processPMTSection()` on the TS processing (injector) path, and it calls `Configure()`. `AampRialtoPlayer.cpp:BeginNewSession` mentions this ("Configure() may be called mid-session from the injection thread"). The comment beside `m_flushCallMutex`, however, assumes that "AAMP stops injection ... before calling Configure()".
- The GLib timer callbacks do not only run on the GLib main loop. `AampRialtoProgressTimer::start()` and `kick()` call `AampRialtoGlibTimer::FireNowIfRunning()`, which runs the progress tick on the **caller's** thread. `HandlePlaybackStarted` is the usual caller, so in practice that is the Rialto event thread.
- The Rialto event thread does more than receive events. Inside the `SEEK_DONE` handler it makes synchronous Rialto calls (`setSourcePosition()`, and `play()` through the playback controller). Inside `OnNeedMediaData` it calls `haveData()`.

## 2. Thread inventory
| Thread (owner) | Entry points it calls | What it can block on | Bound / timeout |
|---|---|---|---|
| AAMP control thread (AAMP: application/JS API thread, tune thread, idle tasks such as `ProcessPendingDiscontinuity`) | `Configure`, `Stream`, `Stop`, `Flush`, `Pause`, `SetPlayBackRate`, `SeekStreamSink`, volume/mute/rectangle/CC setters, `ChangeAamp`, `SetEncryptedAamp`, `QueueProtectionEvent`, `ClearProtectionEvent`, `NotifyFragmentCachingOngoing/Complete`, `NotifyInjectorToPause/Resume`, `UnblockTrackInjection`, `StopBuffering`, destructor | `m_flushCallMutex` (an injector's deferred flush may hold it for a whole `Flush()`); `WaitForFlushToComplete` (condition variable in `PlayerStateMachine`); `AampRialtoControlBackend::waitForRunning` inside `CreateAndLoad`; synchronous Rialto IPC (`load`, `stop`, `setPosition`, `play`, `pause`, `attachSource`); `AampRialtoGlibTimer::Stop()` waiting for a running tick; the controller play lock (waits for an in-flight `play()` or pre-roll step); `m_attachMutex` in `SetVideoMute`/`SetSubtitleMute`/`setCCMute` (held for the length of a DRM licence fetch, see ARCH-04) | Flush wait: `m_flushCompleteTimeout` = 5000 ms, after which the flush is abandoned. Rialto RUNNING wait: `kRialtoRunningTimeoutMs` = 2000 ms. Rialto IPC: no direct-rialto timeout; the Rialto client's own IPC timeout applies. Timer and play-lock waits: as long as the tick or `play()` takes. |
| Per-track injector threads (AAMP: one per track; a single thread for muxed HLS-TS) | `SendCopy`, `SendTransfer`, `SendSample`, `SetStreamCaps`, `Discontinuity`, `EndOfStreamReached`, `PipelineConfiguredForMedia`; the deferred `Flush()` + `Stream()` in `MaybeFlushForPendingPosition`; `Configure` (muxed TS, through `SetStreamFormat`) | Need-data handshake waits in `AampRialtoNeedDataHandshake` (`WaitForAttach`, `WaitForGate`, `WaitForRequest`); `m_attachMutex`; the DRM licence fetch inside `AampDrmBridge::createSession` (called by `attachOrUpdate` under `m_attachMutex`); `m_flushCallMutex` and `WaitForFlushToComplete` in the deferred flush; the play lock | Handshake waits have **no timeout by design**. Only a `needData`, a gate change or a generation bump (`Flush`/`Stop`/`Configure`/`UnblockTrackInjection`/`NotifyInjectorToPause`) releases them. The DRM fetch is bounded by the AAMP DRM layer's licence timeouts (page "DRM"). The flush wait is bounded at 5000 ms. |
| Rialto client event thread (Rialto client library) | `AampRialtoMediaPipelineClient::notify*` forwards to `OnNeedMediaData`, `OnCancelNeedMediaData`, `OnPlaybackState` (`HandlePlaybackStarted`, `HandleSeekDone`, `HandlePlaybackFailure`, PAUSED, END_OF_STREAM), `OnDuration`, `OnBufferUnderflow`, `OnPlaybackError`; `AampRialtoControlBackend::ControlClient::notifyApplicationState` | `m_attachMutex` in `ApplySubtitleSegmentPosition` (SEEK_DONE), which can mean a whole DRM licence fetch; the play lock, and then a synchronous `play()` IPC, when SEEK_DONE releases the last hold; `isVideoMaster`/`setSourcePosition` IPC; the progress-timer lock (`StartProgressTimer` runs the immediate progress tick here); `m_monitorMutex` and the MonitorAV timer locks; the handshake mutex plus `haveData()` IPC in `handleNeedData` | No direct-rialto timeout. While the thread is blocked, every later Rialto event (including `needData`) is delayed. |
| GLib main loop thread (host application; **default** `GMainContext`) | `AampRialtoGlibTimer::Dispatch` runs `OnProgressTimerTick`, `AampRialtoPreRollMonitor::Poll` (on completion: play lock, `ReleaseHold`, possibly `IssuePlay` and a `play()` IPC), and `AampRialtoMonitorAV::onSampleTick`/`onReportTick` (`getPosition`/`getStats` IPC) | The timer's own recursive mutex; the play lock; Rialto IPC | Interval timers: progress = `eAAMPConfig_ReportProgressInterval`, pre-roll poll = 10 ms with a 1000 ms timeout (`kPreRollPollIntervalMs`, `kPreRollTimeoutMs`), MonitorAV = configured intervals. |
| AAMP scheduler thread (AAMP `AampScheduler`) | Runs the tasks posted by `PrivateInstanceAAMPNotifiable` (first frame, tune complete, `MonitorProgress`, `SendErrorEvent`, `ScheduleRetune`, MonitorAV events). One task re-enters the sink: `NotifyOutputProtectionRecovered` calls `StreamSink::SetVideoMute(true)`. | That re-entry takes `m_attachMutex` | Not bounded |
| DRM / OCDM threads (Rialto client and AAMP DRM layer) | `RialtoMediaKeySystem::MediaKeysClient::onLicenseRequest/onLicenseRenewal/onKeyStatusesChanged` wake waiters in `RialtoMediaKeySessionAdapter` | The adapter's `wait_for` on challenge, key status and state (timeouts passed in by the AAMP DRM layer); `deregisterSession` waits, untimed, for callbacks still running on other threads | See page "DRM" |
| Any thread (getters) | `GetPositionMilliseconds`, `GetDurationMilliseconds`, `GetVideoPTS`, `CheckForPTSChangeWithTimeout`, `GetVideoRectangle`, `GetVideoSize`, `IsAssociatedAamp`, `GetVideoPlaybackQuality` | `getPosition`/`getStats` IPC; leaf locks only | IPC timeout only. `GetVideoPlaybackQuality` writes the unguarded `m_playbackQuality` member and returns a pointer to it; it is safe only while one thread calls it (the header says the AAMP monitor thread). |

### What happens without a running GLib main loop
`AampRialtoGlibTimer` registers with `g_timeout_add_full()`. That always attaches to the **global default** `GMainContext`, even when the calling thread has pushed a thread-default context. AAMP core does not run that loop; the host application must (aamp-cli does, see ARCH-22). Without a loop:

- Scheduled ticks never run. Progress reports reduce to the one immediate report that `start()`/`kick()` makes on the calling thread.
- The MonitorAV ticks never run.
- **The pre-roll monitor never completes.** Its 1000 ms timeout is evaluated inside `Poll`, which is itself a tick. With `eAAMPConfig_GStreamerBufferingBeforePlay` set, `PlayHold::PreRollIncomplete` is never released and `play()` is never issued (ARCH-22).
- `Stop()` and destruction remain safe: `Stop()` only removes the GLib source, and nothing waits for a dispatch that never comes.

## 3. Lock catalogue
"Leaf" means that no other lock is taken and no blocking call is made while the lock is held.

### 3.1 Mutexes and condition variables
| Owner class | Member | Protects | Leaf? |
|---|---|---|---|
| `AampRialtoPlayer` | `m_flushCallMutex` (`std::recursive_mutex`) | Serialises `Flush()`, `StopInternal()` and the deferred flush (`MaybeFlushForPendingPosition`, held across its `Flush()` and `Stream()`). Makes the "is this state flushable?" check and the FLUSHING claim atomic with respect to other flushes. Recursive because `Flush()` can recover through `StopInternal()`. | **No.** Outermost lock. Held across `WaitForFlushToComplete` (up to 5000 ms), Rialto IPC (`stop`, `setPosition`), timer stops that wait for ticks, and the play lock. |
| `AampRialtoPlayer` | `m_attachMutex` | `AttachSource` and the drain of deferred attaches; `CheckAllSourcesAttached` (only ever reached from an attach); the cached `m_videoMuted` and `m_subtitleMuted`; the subtitle `setSourcePosition` after SEEK_DONE; `setCCMute`. | **No.** Held across the DRM licence fetch, Rialto IPC (`attachSource`, `setMute`, `allSourcesAttached`, `setSourcePosition`), the state-machine mutex, the play lock and `play()` IPC, the pre-roll monitor start, and `PrivateInstanceAAMP::ResumeTrackDownloads` (which takes AAMP `mLock`). |
| `AampRialtoPlayer` | `m_sourcesMutex` | The `m_sources` array. Entries are only copied or swapped under the lock; a replaced source is destroyed after the lock is released. | Yes |
| `AampRialtoPlayer` | `m_pendingAttachMutex` | `m_pendingAttach` (codec info parked until video attaches) | Yes |
| `AampRialtoPlayer` | `m_pendingProtectionMutex` | `m_pendingProtection` (protection data queued by AAMP before sources exist) | Yes |
| `AampRialtoPlayer` | `m_drmBridgeMutex` | `m_drmBridge`, `m_drmBridgeIsExplicit`. A replaced bridge is released outside the lock. | Yes. It only constructs an `AampDrmBridge`. |
| `AampRialtoPlayer` | `m_ptsCheckMutex` | `m_lastKnownPts` and `m_ptsUpdatedTimeMs`, which must change together | Yes |
| `AampRialtoPlayer` | `m_videoRectangleMutex` | `m_videoRectangle` string | Yes |
| `AampRialtoPlayer` | `m_id3HandlerMutex` | `m_ID3MetadataHandler`. It is copied out under the lock and called without it. | Yes |
| `AampRialtoPlayer` | `m_monitorMutex` | The `m_monitorAV` pointer. The old monitor is destroyed outside the lock because its destructor waits for a tick. | Almost. The only nested call is `AampRialtoMonitorAV::start()`, which takes that monitor's two timer locks. |
| `PlayerStateMachine` | `m_mutex` + `m_stateChanged` (`std::condition_variable`) | The current state. Every `dispatch()` notifies `m_stateChanged`; `waitForFlushComplete` waits on it while the state is FLUSHING and, on timeout, forces FlushComplete under the same lock. | Yes (the wait releases it) |
| `AampRialtoPlaybackController` | `m_playMutex` (`std::recursive_mutex`, the "play lock") | Taken first by every controller operation and held while the play action (`IssuePlay`: ungate + `play()` IPC) or a `RunIfPreRollCurrent` step (`pause()` IPC + ungate) runs. This guarantees that a hold added after `play()` was decided cannot return until that `play()` has finished. | **No.** Covers Rialto IPC, the handshake mutexes, `m_sourcesMutex` and the pipeline mutex. |
| `AampRialtoPlaybackController` | `m_mutex` | `m_playPending`, `m_preRollReady`, `m_preRollEpisode`; writes to `m_holds` | Yes |
| `AampRialtoNeedDataHandshake` (one per source) | `State::mu` + `State::cv` | The pending request, gate mode, generation, EOS flag, `injectorActive`, `attachPending`, batch counters. Injector threads wait on `cv`. The handshake never calls Rialto; it returns a `Claim` and the caller sends `haveData()` after the lock is released. | Yes |
| `AampRialtoMediaSource` | `m_codecDataMutex` | `m_pendingCodecData`, staged by `attachOrUpdate` and taken by the injector | Yes |
| `AampRialtoPipelineSession` | `m_pipelineMutex` | The `m_pipeline` `shared_ptr`. It is only copied or swapped under the lock; the old pipeline is released outside it because its destructor performs IPC. | Yes |
| `AampRialtoMediaPipelineClient` | `m_sinkMutex` | `m_sink` (the player). It is read under the lock and called without it. | Yes |
| `AampRialtoControlBackend` | `SharedState::mutex` + `cv` | Rialto application state. `waitForRunning` waits for up to 2000 ms. | Yes |
| `AampRialtoGlibTimer` (4 instances: progress, pre-roll, MonitorAV sample, MonitorAV report) | `State::mutex` (`std::recursive_mutex`) | `running`, `sourceId`, `generation`, `tick`. **Held while the tick runs**, so `Stop()` waits for a running tick and no tick starts after `Stop()` returns. | **No.** Whatever the tick takes nests under it (see section 4). |
| `AampRialtoProgressTimer` | `m_mutex` | `m_interval` | Yes |
| `AampRialtoPreRollMonitor` | `m_mutex` | `m_running`, `m_epoch`, `m_probe`, `m_startMs`, progress logging. Probes and the completion callback run without it. | Yes |
| DRM classes (`RialtoMediaKeySystem::MediaKeysClient::mutex` + `callbackDone`; `RialtoMediaKeySessionAdapter` `m_mutex`, `m_sessionMutex`, `m_usableKeysMutex`, `m_eventMutex` + three condition variables; `RialtoMediaKeySession::m_keyStatusMutex`) | See page "DRM" | Session registry, callbacks in progress, challenge/key-status hand-off | Internal to DRM. Callbacks are invoked without the registry mutex held. |

### 3.2 Atomics (no lock)
| Owner class | Members | Notes |
|---|---|---|
| `AampRialtoPlayer` | `m_rate`, `m_firstFrameNotified`, `m_durationMs`, `m_aamp`, `m_audioInVideoSlot`, `m_preRollEpisode`, `m_audioVolume` | Independent values. `m_aamp` must be read once per function through `Aamp()` because `ChangeAamp()` can switch it. `m_firstFrameNotified.exchange()` elects exactly one first-frame report. |
| `AampRialtoPlaybackController` | `m_holds` | Written under `m_mutex`, read lock-free by `IsHeld()`. A lock-free read is a hint only. For example, `MaybeFlushForPendingPosition` re-checks the hold after taking `m_flushCallMutex`. |
| `AampRialtoSegmentPosition` | `m_stagedPositionNs`, `m_stagedRate`, `m_authoritative`, `m_flushClaimed`, `m_baselineNs` | No lock. `ClaimDeferredFlush()` uses an atomic exchange to elect one injector. Position and rate are separate atomics, so a reader outside FLUSHING can see a new position with the old rate (THR-07). |
| `AampRialtoMediaSource` | `m_sourceId`, `m_mksId`, `m_streamFormat`, `m_firstPtsMs`, `m_lastAcceptedEndMs`, `m_acceptedFrames`, `m_needDataCount`, `m_droppedSampleCount` | Statistics and IDs read from the event thread and timer ticks. |
| `AampRialtoNeedDataHandshake` | `m_bufferFull` | Read lock-free by the pre-roll probe and the logs |
| `PrivateInstanceAAMPNotifiable` | `m_aamp` | Switched by `ChangeAamp()` |

### 3.3 Shared state that has no lock of its own
- `AampRialtoMediaSource::m_activeProtection`: by convention guarded by the player's `m_attachMutex`, because only `attachOrUpdate` touches it.
- `AampRialtoMediaSource::m_demuxer`: touched only by that track's injector thread.
- `AampRialtoPlayer::m_playbackQuality`: single caller assumed (THR-09).
- `AampRialtoPlayer::m_notifiable`: set only in the constructor.
- `AampRialtoPlayer::m_flushCompleteTimeout`: changed only by a test hook.
- `AampRialtoPipelineSession` members other than `m_pipeline`: control thread only, by contract. The exception is `Capabilities()`, which is read on the event thread. It is written once in `CreateAndLoad` and never reset.

## 4. Global lock order
Locks may only be taken in this order (outermost first). A thread may skip levels but must never take a lock from an earlier level while it holds a later one. Every nesting below was checked against the code.

1. **AAMP core locks** (`PrivateInstanceAAMP::mLock`, `mStreamLock`). AAMP calls some sink methods while holding them; for example, `PrivateInstanceAAMP::Discontinuity` holds `mLock` across `StreamSink::Discontinuity`.
2. **`m_flushCallMutex`** and **`m_attachMutex`**. These are siblings and are never held together: nothing under `m_attachMutex` calls `Flush`/`Stop`/`Configure`, and neither `Flush` nor `StopInternal` takes `m_attachMutex`.
3. **`m_monitorMutex`** and the **`AampRialtoGlibTimer` locks** (progress, pre-roll, MonitorAV sample and report).
4. **The controller play lock** (`AampRialtoPlaybackController::m_playMutex`).
5. **Leaf locks**: controller `m_mutex`, `PlayerStateMachine::m_mutex`, each handshake's `State::mu`, `m_sourcesMutex`, `m_pipelineMutex`, `m_drmBridgeMutex`, `m_pendingAttachMutex`, `m_pendingProtectionMutex`, `m_ptsCheckMutex`, `m_videoRectangleMutex`, `m_id3HandlerMutex`, `m_codecDataMutex`, `AampRialtoPreRollMonitor::m_mutex`, `AampRialtoProgressTimer::m_mutex`, `AampRialtoMediaPipelineClient::m_sinkMutex`.

Rialto IPC can happen at levels 2 to 4, but never under a leaf lock.

[[diagram:direct-rialto-threading-lock-order-component]]
*Verified lock nesting. Each arrow means "is held while taking". Dashed arrows mark Rialto IPC or a blocking DRM call made while the lock is held.*

### 4.1 Nested-lock sites (verified)
| Outer lock | Site (`File:Symbol`) | Inner locks and blocking calls made while the outer lock is held |
|---|---|---|
| `m_flushCallMutex` | `AampRialtoPlayer.cpp:Flush` (and the helpers `FlushByTeardown`, `FlushWithoutCycle`, `ResetSourcesForFlush`, `IssueFlushSeek`) | State-machine mutex and condition-variable wait (`WaitForFlushToComplete`, `onFlush`); play lock (`AddHold`/`ReleaseHold`); pre-roll timer lock (`ArmPreRoll` calls `AampRialtoPreRollMonitor::Stop`, which waits for a running `Poll`); handshake mutexes then `haveData()` IPC; `setPosition()` IPC |
| `m_flushCallMutex` | `AampRialtoPlayer.cpp:StopInternal` | State-machine mutex and flush wait; progress-timer lock (waits for a running tick); pre-roll timer lock; `m_monitorMutex`, then the MonitorAV timer locks (the old monitor is destroyed after `m_monitorMutex` is released but still under `m_flushCallMutex`); handshake mutexes and `haveData()`; `stop()` IPC; `m_pipelineMutex`; play lock (`Reset`); `m_ptsCheckMutex` |
| `m_flushCallMutex` | `AampRialtoPlayer.cpp:MaybeFlushForPendingPosition` (injector thread) | Everything `Flush` takes, then `Stream`, which takes the play lock |
| `m_attachMutex` | `AampRialtoPlayer.cpp:AttachSource`, `DeferAttachUntilVideo`, `ProtectionFor`, `OnSourceNewlyAttached`, `CheckAllSourcesAttached`, `ReportAttachFailure` | State-machine mutex (`onSourceAttaching`, `onAllSourcesAttached`) (**not listed in the header comment**); `m_drmBridgeMutex`; `m_pendingProtectionMutex`; `m_pendingAttachMutex`; handshake mutexes; `m_codecDataMutex`; the **DRM licence fetch** (`AampDrmBridge::createSession`, which takes the DRM-internal locks); `attachSource`, `setMute`, `setVolume` and `allSourcesAttached` IPC; play lock (`ReleaseHold(PositionPending)`, `ReleaseHold(SourcesNotAttached)`), which can run `IssuePlay` (`play()` IPC) or `StartPreRoll` (`pause()` IPC, then `AampRialtoPreRollMonitor::Start`, which takes the pre-roll timer lock) (**not listed**) |
| `m_attachMutex` | `AampRialtoPlayer.cpp:SetStreamCaps` | Everything `AttachSource` takes, plus `PrivateInstanceAAMP::ResumeTrackDownloads` (AAMP `mLock`) for subtitles (**not listed**). AAMP releases `mLock` before it calls `SetStreamCaps` (`priv_aamp.cpp:SetStreamCaps`), so no inversion exists today. |
| `m_attachMutex` | `AampRialtoPlayer.cpp:SetVideoMute`, `SetSubtitleMute`, `setCCMute`, `CreateSubtitleSource` | `m_sourcesMutex`, `m_pipelineMutex`, `setMute()` IPC; `CreateSubtitleSource` also calls `AttachSource` |
| `m_attachMutex` | `AampRialtoPlayer.cpp:ApplySubtitleSegmentPosition` (event thread, SEEK_DONE) | `m_sourcesMutex`, `m_pipelineMutex`, `setSourcePosition()` IPC |
| `m_monitorMutex` | `AampRialtoPlayer.cpp:HandlePlaybackStarted` | `AampRialtoMonitorAV::start()`, which takes the two MonitorAV timer locks (`StartLocked` only; it does not wait for ticks) |
| play lock | `AampRialtoPlaybackController.cpp:Dispatch` invokes `AampRialtoPlayer::IssuePlay` | `m_pipelineMutex`, `m_sourcesMutex`, handshake mutexes (`gateInjection`, `isEos` in `LogSourceSnapshot`), `haveData(EOS)` and `play()` IPC |
| play lock | `AampRialtoPlaybackController.cpp:RunIfPreRollCurrent` invokes the `StartPreRoll` step or the monitor-completion step | `pause()` IPC, ungate (handshake mutexes), recursive `ReleaseHold`, which takes controller `m_mutex` |
| play lock | `AampRialtoPlaybackController.cpp:Dispatch` before the pre-roll action | Released with `playLock.unlock()` before `StartPreRoll` runs, because `AampRialtoPreRollMonitor::Start` waits for a `Poll` that may need the play lock. **The unlock releases one recursion level only** (THR-06). |
| progress timer lock | `AampRialtoGlibTimer.h:Dispatch` / `FireNowIfRunning` invoking `AampRialtoPlayer::OnProgressTimerTick` | `m_sourcesMutex`, handshake mutexes (`LogSourceSnapshot` at TRACE level), `PrivateInstanceAAMPNotifiable::MonitorProgress` (posts a task to the AAMP scheduler; does not block on AAMP locks) |
| pre-roll timer lock | `AampRialtoPreRollMonitor.cpp:Poll` | Pre-roll `m_mutex` (released before probes run); probes take `m_sourcesMutex` and the handshake mutex; the completion callback takes the play lock and can reach `IssuePlay`, so a `play()` IPC can run on the GLib thread |
| pre-roll timer lock | `AampRialtoPreRollMonitor.cpp:Start`, `Stop` (through `AampRialtoGlibTimer::RunLocked`) | Pre-roll `m_mutex`, the same order as `Poll`. Earlier code took `m_mutex` first in `Start`, which could deadlock against a dispatch (fixed in `5b9ddef5`) |
| MonitorAV timer locks | `AampRialtoMonitorAV.cpp:onSampleTick`, `onReportTick` | State-machine mutex and `m_sourcesMutex` (through the player's lambdas); `getPosition()`/`getStats()` IPC on the monitor's own pipeline snapshot; notifiable (async) |
| handshake `State::mu` | all `AampRialtoNeedDataHandshake` methods | None. They return a `Claim`; `AampRialtoMediaSource` sends `haveData()` after the lock is released. |
| AAMP `mLock` (held by AAMP) | `AampRialtoPlayer.cpp:Discontinuity` (injector thread) | Handshake mutexes and `haveData(EOS)`; `StopBuffering` (ungate and `play()` IPC **without** the play lock); `ArmPositionPending` (play lock); `PrivateInstanceAAMPNotifiable::CompleteDiscontinuityDataDeliverForPTSRestamp` (synchronous, takes AAMP `mDiscoCompleteLock`) |

### 4.2 Calls into AAMP core while holding direct-rialto locks
Almost every `IStreamSinkNotifiable` method is asynchronous. `PrivateInstanceAAMPNotifiable` posts the work with `PrivateInstanceAAMP::ScheduleAsyncTask`, so direct-rialto never waits for an AAMP lock from inside a callback (see the comment at the top of `PrivateInstanceAAMPNotifiable.cpp`). The exceptions, all of which are called synchronously, are:

| Call | Called from, with which lock held | Why it is safe today |
|---|---|---|
| `GetState()` | Event thread, `HandlePlaybackStarted`, no lock | `PrivateInstanceAAMP::mState` is atomic |
| `GetProgressReportIntervalSeconds()` | Event thread or control thread (`StartProgressTimer`), no lock | Configuration read |
| `CompleteDiscontinuityDataDeliverForPTSRestamp`, `NotifyPipelinePausedToUnderflowMonitor` | Injector thread inside `Discontinuity`, with AAMP `mLock` held by the caller and no direct-rialto lock | Leaf AAMP locks (documented in `PrivateInstanceAAMPNotifiable.cpp`) |
| `PrivateInstanceAAMP::ResumeTrackDownloads` | `SetStreamCaps` under `m_attachMutex`; `CreateMediaSource` and `TryReuseExistingPipeline` on the `Configure` thread, no lock | Takes `mLock` briefly. Safe only while AAMP never calls an `m_attachMutex` method with `mLock` held (rule 6 in section 7). |
| `ReconfigureForElementaryStreamUpdate`, `WillFlushOnDiscontinuity`, `mConfig` reads | Various, no direct-rialto lock | Reads |
| `NotifyPlaybackError` (async) | `ReportAttachFailure`, under `m_attachMutex` | Only posts a task |
| `m_ID3MetadataHandler` (AAMP ID3 callback) | Injector thread, no lock (copied out under `m_id3HandlerMutex`) | No lock is held |

## 5. Per-class thread-safety
| Class | Thread-safety | Called from | Notes |
|---|---|---|---|
| `AampRialtoPlayer` | Thread-safe per entry point, with the locks and atomics described above. **No single lock covers the whole object.** | All the threads in section 2 | `Configure` does not take `m_flushCallMutex`. It relies on AAMP not injecting (and so not running a deferred flush) while it runs, but `Configure` can itself be called from the TS injector path (THR-02). |
| `PlayerStateMachine` | Fully thread-safe; one mutex | Control, injector, event, GLib (MonitorAV `isPlaying`) | `waitForFlushComplete` is the only blocking method |
| `AampRialtoPlaybackController` | Fully thread-safe | Control (`Stream`, `Flush`, `Stop`, `Pause`, caching), injector (`MaybeFlushForPendingPosition`, `Discontinuity`, attach), event (SEEK_DONE), GLib (pre-roll completion) | The play action and pre-roll steps run under the play lock and must not wait for another thread that uses the controller (header contract of `RunIfPreRollCurrent`) |
| `AampRialtoNeedDataHandshake` | Fully thread-safe; one mutex per source | Injector (waits), event (`OnNeedData`), control (gate, unblock, reset) | Never calls Rialto with its lock held |
| `AampRialtoMediaSource` (and the Video, Audio and Subtitle sources) | Thread-safe through the handshake and atomics, **except** `attachOrUpdate`, which needs the player's `m_attachMutex`; `processInitFragment`/`processDataFragment`/`injectSingleSample`, which assume the track's own injector thread; and `reset`, which assumes `Configure` | See the class comment in `AampRialtoMediaSource.h` | Shared through `shared_ptr` so an in-flight call keeps the source alive if `Configure` replaces it |
| `AampRialtoPipelineSession` | Only `GetPipeline`/`SetPipeline` are thread-safe. Everything else is control-thread only. | Control thread; snapshots from everywhere | Snapshots must not be stored: `Stop` relies on the last release destroying the Rialto session |
| `AampRialtoMediaPipelineClient` | Thread-safe (`SetEventSink` from any thread, including from inside an event) | Rialto event thread | The sink must outlive any event already being delivered. Only `~AampRialtoPlayer` clears it, after releasing the pipeline. |
| `AampRialtoControlBackend` | Thread-safe | Control thread (wait), Rialto thread (notify) | 2000 ms wait for RUNNING |
| `AampRialtoGlibTimer` | Thread-safe; `Stop` waits for a running tick; ticks may stop or restart their own timer | Any thread; ticks on the GLib default context or on the `FireNowIfRunning` caller | A tick must not wait on another thread that is itself calling into the same timer |
| `AampRialtoProgressTimer` | Thread-safe; `kick` never restarts a stopped timer (commit `7efb0a89`) | Event thread (`start`/`kick` from `HandlePlaybackStarted`), control thread (`stop`) | `start` after `stop` **does** restart it (THR-03) |
| `AampRialtoPreRollMonitor` | Thread-safe; `Start` and `Stop` update the state and the timer as one step under the timer lock (`RunLocked`); `Stop` waits for a running `Poll`; `m_epoch` discards stale polls | Control/injector/event (`Start`/`Stop`), GLib (`Poll`) | The completion callback runs on the GLib thread without the monitor mutex and may call `Start`/`Stop` |
| `AampRialtoMonitorAV` | `start`/`stop` thread-safe; the classification state is touched only by ticks | Event thread (`start`), control thread (`stop`, destructor), GLib (ticks) | Holds its own pipeline snapshot. That is why `Stop` drops the monitor before the pipeline. |
| `AampRialtoSegmentPosition` | Lock-free; individual values are consistent, pairs are not | Control, injector, event | Position and rate are only a consistent pair while FLUSHING (header contract) |
| `PrivateInstanceAAMPNotifiable` | Thread-safe (`m_aamp` is atomic; work is posted to the AAMP scheduler) | All | See ARCH-17 for tasks the scheduler drops |
| DRM classes | Thread-safe internally | Injector/control (`createSession`), Rialto DRM callbacks | See page "DRM" |

## 6. Cross-thread protocols
Five protocols carry state between threads. Each one is a handshake: one thread waits on a condition variable or a lock, and another thread changes the state and wakes it.

### 6.1 Flush wait and notify
[[diagram:direct-rialto-threading-flush-sequence]]
*A flush starts on the control thread (or an elected injector thread) and is completed by `SEEK_DONE` on the Rialto event thread.*

- **Starting the cycle.** `AampRialtoPlayer.cpp:Flush` holds `m_flushCallMutex` (recursive) for the whole call. It first calls `WaitForFlushToComplete()`, so a second `Flush()` never overlaps a cycle that is still FLUSHING. It then adds `PlayHold::Flushing`, stages the position and rate in `AampRialtoSegmentPosition`, moves the state machine to FLUSHING, gates every source BLOCKED (bumping the handshake generation, which aborts blocked injectors) and calls `setPosition()`. `Flush()` returns **without** waiting for `SEEK_DONE`.
- **Waiting.** `Configure()`, `Stop()` and the next `Flush()` call `WaitForFlushToComplete()`, which waits on `PlayerStateMachine::m_stateChanged` until the state leaves FLUSHING. Every state transition calls `notify_all`.
- **Completing.** On the event thread `HandleSeekDone` checks the state is still FLUSHING, applies the trick-play and subtitle source positions, commits the staged rate and baseline, releases `PlayHold::Flushing` (which can issue `play()` on the event thread), and only then calls `onFlushComplete()` to enter FLUSHED and wake the waiters. The hold is released *before* leaving FLUSHING, so a waiting `Flush()` cannot add its own `Flushing` hold only to have this release drop it.
- **Timeout.** If `SEEK_DONE` does not arrive within `m_flushCompleteTimeout` (5000 ms), `waitForFlushComplete` forces the transition to FLUSHED under the same mutex, runs the "abandoned" callback (commit the staged rate) and the waiter releases `PlayHold::Flushing` itself. This mirrors the `setPosition()` failure path, so playback can continue even if Rialto never confirms the seek.

### 6.2 Need-data wait and notify (summary)
Rialto pulls data; AAMP pushes it. `AampRialtoNeedDataHandshake` reconciles the two with one mutex and one condition variable per source (`State::mu`, `State::cv`). The injector thread waits in up to three stages (`WaitForAttach`, `WaitForGate`, `WaitForRequest`). The Rialto event thread wakes it by recording a needData (`OnNeedData`). The control thread wakes it by changing the gate or bumping the generation (`unblockInjection`, `gateInjection`, `reset`). No handshake method calls Rialto while holding `State::mu`: they return a `Claim`, and the caller sends `haveData()` after the lock is released. The waits have no timeout (DATA-01). The full protocol, with one sequence diagram per scenario, is on **Media data path**.

### 6.3 Stop versus event thread versus injectors
[[diagram:direct-rialto-threading-stop-sequence]]
*How `Stop()` makes teardown safe against a blocked injector and late Rialto events.*

`AampRialtoPlayer.cpp:StopInternal` performs the teardown in an order chosen so that no other thread is left waiting on something that will never happen:

1. Take `m_flushCallMutex`. An elected injector cannot then run a deferred `Flush()` + `Stream()` in the middle of the teardown and leave a stale play request behind.
2. `WaitForFlushToComplete()`, so `onStop()` is never dispatched from FLUSHING.
3. Stop the progress timer, the pre-roll monitor and the AV monitor. Each `Stop()` waits for a tick that is already running (section 6.4).
4. `unblockInjection("Stop")` on every source. This bumps the generation, sets the gate to DROPPED and answers any pending request, so blocked injectors return and later samples are discarded without blocking. Sources stay DROPPED until the next `Configure()`/`Flush()`.
5. `pipeline->stop()`, then release the pipeline (`SetPipeline(nullptr)`) and the control backend. Event handlers that run after this take a null pipeline snapshot and do nothing.
6. Reset the playback controller (discarding any pending play request), clear the staged position and PTS bookkeeping, and move the state machine to IDLE.

Two further rules complete the protocol:

- **AAMP must unblock before it joins.** An injector blocked inside `SendSample` cannot be joined. `PrivateInstanceAAMP::StopTrackInjection` therefore calls `StreamSink::UnblockTrackInjection(type)` before `StopInjectLoop` joins the thread (DATA-11).
- **The client event sink outlives every event.** `Stop()` does not clear the `IMediaPipelineClient` sink. Only `~AampRialtoPlayer` does, after `m_session.SetPipeline(nullptr)`; the destructor comment states that the last pipeline release joins Rialto's event thread, so no event can still be running on the player when the sink is cleared.

### 6.4 Progress timer tick versus Stop
[[diagram:direct-rialto-threading-timer-stop-sequence]]
*`AampRialtoGlibTimer` makes `Stop()` wait for a running tick and ignores dispatches that arrive afterwards.*

`AampRialtoGlibTimer` (used by the progress timer, pre-roll monitor and AV monitor) guarantees that **no tick starts after `Stop()` returns**:

- Every tick, including the immediate one from `FireNowIfRunning`, runs under the timer's recursive `State::mutex`. `Stop()` takes the same mutex, so it waits for a tick in progress.
- Each `Start()` bumps a generation number, and the GLib source owns a `Handle` holding a `shared_ptr` to the state and that generation. A dispatch that GLib has already picked up when `Stop()` runs, or that arrives after the owner is destroyed, finds `running == false` or a different generation and does nothing.
- The mutex is recursive so that a tick may stop or restart its own timer.

Commit `7efb0a89` closed a race in which `kick()` (called from `HandlePlaybackStarted` on the event thread) could restart a timer that `Stop()` had just stopped. `kick()` now uses `RestartIfRunning`, where the "is it running?" check and the restart happen in one step under the timer lock. The immediate report from `start()`/`kick()` also uses `FireNowIfRunning`, so it is skipped if `Stop()` has already run.

One gap remains (THR-03): when the timer is not running, `StartProgressTimer()` calls `start()`, and `start()` after `stop()` does create a new timer. `HandlePlaybackStarted` does not check the state machine, so a PLAYING event that is processed after `Stop()` restarts progress reporting.

### 6.5 DRM callback reentrancy (summary)
Rialto delivers key-session callbacks (`onLicenseRequest`, `onLicenseRenewal`, `onKeyStatusesChanged`) on its own thread. `RialtoMediaKeySystem::MediaKeysClient` copies the session's callback under its mutex, registers the calling thread in a per-session `running` list with `CallbackScope`, releases the mutex and only then calls the callback. `deregisterSession` removes the session and waits until no *other* thread is still in one of its callbacks, so the owning session object can be freed as soon as it returns. The calling thread itself is excluded from the wait, which lets a callback deregister its own session without deadlocking. Because the owner may then be freed when that callback returns, `onKeyStatusesChanged` stops dispatching a batch once the session is no longer registered. Details, including the timeouts used while waiting for a licence, are on **DRM**.

## 7. Rules for contributors
1. **Respect the lock order** in section 4: AAMP core locks, then `m_flushCallMutex` **or** `m_attachMutex` (siblings, never held together), then `m_monitorMutex` and the timer locks, then the controller play lock, then leaf locks. Never take a lock from an earlier level while holding a later one.
2. **Never call Rialto while holding a leaf lock.** Copy what you need (for example a pipeline snapshot from `GetPipeline()`), release the lock, then make the IPC call. Handshake methods must keep returning a `Claim` instead of calling `haveData()` themselves.
3. **Never block the Rialto event thread on something only the event thread can release.** For example, do not wait for `SEEK_DONE`, needData or a state change from inside an `On...` handler.
4. **Every blocking wait needs a releaser on every teardown path.** If you add a wait on an injector thread, make sure `Stop()`, `Flush()`, `Configure()`, `UnblockTrackInjection()` and `NotifyInjectorToPause()` all wake it (usually through the handshake generation).
5. **Post notifications into AAMP core; do not call them synchronously.** Add new `IStreamSinkNotifiable` methods as tasks posted through `PrivateInstanceAAMP::ScheduleAsyncTask`, as the existing ones are. The synchronous exceptions in section 4.2 are deliberate and documented.
6. **AAMP must never call a method that takes `m_attachMutex` while holding `PrivateInstanceAAMP::mLock`.** `SetStreamCaps` calls `ResumeTrackDownloads` (which takes `mLock`) under `m_attachMutex`, so the reverse order would deadlock.
7. **Timers.** Use `AampRialtoGlibTimer` rather than raw `g_timeout_add`. When an owner must change its own state and the timer together (start/stop), do both inside `RunLocked()` and take the owner's lock inside it, the same order as the tick; never ask the timer anything while holding the owner's lock. A tick must not wait on a thread that is itself calling into the same timer. Remember that a tick can also run on the thread that calls `FireNowIfRunning`.
8. **State is not a lock.** `PlayerStateMachine` makes each event atomic, but "read `currentState()`, then act" is not. If a decision must be atomic with a transition, serialise it with `m_flushCallMutex` (as `Flush()` does) or make it part of the state machine.
9. **Do not store pipeline snapshots.** Keep `GetPipeline()` results on the stack only: the last release must happen in `Stop()`/the destructor, because it is what joins Rialto's event thread.
10. **Annotate shared members.** Every new shared member must name its lock, or say that it is atomic, at its declaration, as the existing members in `AampRialtoPlayer.h` do.

## 8. Known hazards and untested interleavings
These are plausible interleavings found by reading the code. None is covered by an L1 test, and none has a confirmed field report unless stated.

| Hazard | Interleaving | Effect | Register |
|---|---|---|---|
| Late PLAYING after `Stop()` | The event thread runs `HandlePlaybackStarted` after `StopInternal` has stopped the timers | The progress timer (and possibly the AV monitor) restarts on a stopped player | THR-03 |
| `Configure()` from the TS injector path | `TSProcessor::processPMTSection` → `SetStreamFormat` → `Configure()` without `m_flushCallMutex`, while the control thread runs `Flush()` or another injector is elected for a deferred flush | Source reset and session reuse can interleave with a flush cycle | THR-02 |
| Pre-roll start under a nested play lock | `Dispatch` unlocks the recursive play lock once before `StartPreRoll`, but a caller that already held it still holds one level | `AampRialtoPreRollMonitor::Start` waits for a `Poll` that needs the play lock: possible deadlock if the path is ever taken | THR-06 |
| `SEEK_DONE` stuck behind a licence fetch | An injector holds `m_attachMutex` across `AampDrmBridge::createSession` (seconds), and the event thread's `ApplySubtitleSegmentPosition` needs `m_attachMutex` | All Rialto events, including needData, are delayed until the licence arrives; the flush may hit its 5000 ms timeout | THR-04 (see also ARCH-04) |
| `play()` IPC on the GLib main loop | Pre-roll completion runs `ReleaseHold`, then `IssuePlay`, on the GLib thread | The host application's main loop is blocked for the duration of a Rialto IPC call | THR-05 |
| Torn position and rate | `AampRialtoSegmentPosition` reads position and rate as two separate atomics outside FLUSHING | A reader can see the new position with the old rate | THR-07 |
| Concurrent `GetVideoPlaybackQuality` | Two threads call it at once | Both write the unguarded `m_playbackQuality` and return a pointer to it | THR-09 |
| Untimed injector waits | Rialto stops sending needData | Injector threads block until the next flush, stop or unblock | DATA-01 |
| Failed deferred attach | `ReportAttachFailure` clears `attachPending`, but `WaitForAttach` does not test it | The waiting injector stays blocked until the next generation bump | DATA-16 |

## Open questions
- Does the Rialto client guarantee that the **last** `IMediaPipeline` release joins its event thread, even when that release happens on the event thread itself (for example a handler holding the final snapshot while `Stop()` runs)? The destructor comment relies on it, but the guarantee is outside this repository.
- Is `Configure()` from the TS injector path (THR-02) intended to run concurrently with a control-thread `Flush()`, or does AAMP core serialise them? No AAMP-side lock that would do so was found.
- Can any caller reach `AampRialtoPlaybackController::Dispatch` with the play lock already held when `startPreRoll` is true (THR-06)? No such path was found, but nothing prevents one.
- Which thread does AAMP use to call `GetVideoPlaybackQuality`? The header says the AAMP monitor thread; it was not verified.

## Related pages
- Direct Rialto - Architecture and class model
- Direct Rialto - Lifecycle and state machine
- Direct Rialto - Media data path
- Direct Rialto - Flush and seek
- Direct Rialto - DRM
- Direct Rialto - Monitoring and diagnostics
- Direct Rialto - Workarounds and improvement register

## Workarounds and improvement candidates
Rows are in `_register/THR.md`:

- THR-01: the class threading comment in `AampRialtoPlayer.h` omits three callers and two lock nestings.
- THR-02: `Configure()` can run on the TS injector thread without `m_flushCallMutex`.
- THR-03: a late PLAYING event can restart the progress timer after `Stop()`.
- THR-04: the event thread can block behind a DRM licence fetch through `m_attachMutex`.
- THR-05: pre-roll completion makes a `play()` IPC call on the host's GLib main loop.
- THR-06: `Dispatch` unlocks the recursive play lock only one level.
- THR-07: staged position and rate are separate atomics.
- THR-08: there are no L1 tests for multi-threaded interleavings.
- THR-09: `m_playbackQuality` is unguarded.
