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

# Compatibility accommodations, limitations and technical debt

Back to [overview](README.md).

Classification:

- **Confirmed limitation (CL)**: established from the code.
- **Potential risk (PR)**: a credible concern that needs verification. It is
  not a confirmed bug.
- **Improvement opportunity (IO)**: an enhancement, not a defect.

"Apparent reason" is taken from code comments only. Where there is none, the
cell says so.

## Compatibility accommodations

| Finding and location | Why it matters | Apparent reason | Possible improvement |
|---|---|---|---|
| **W1 (CL).** Non-video sources wait for video to attach first. `AampRialtoPlayer::AttachSource()` defers them in `m_pendingAttach` and blocks their injector. | Adds a cross-track dependency and a blocking wait. If video never attaches, audio injection waits until a flush or stop. | Comment: a server "audsrc: not-linked" error was seen when audio attached first. Marked **"THEORY (unproven)"**. | Reproduce with a controlled audio-first attach on real Rialto, then either remove it or document the Rialto requirement. |
| **W2 (CL).** `hasDrm` is hard-coded to `true` for video and audio sources (`AampRialtoVideoSource::createRialtoSource`, `AampRialtoAudioSource::createRialtoSource`). | Differs from the GStreamer path, which uses `has-drm=false` for clear HLS-TS video. Clear content is treated as possibly encrypted. | Comment: avoided a Rialto server crash seen in testing. | Find the crash cause and restore the HLS-TS exception. |
| **W3 (CL).** A subtitle or in-band CC source is created with video (unless `RIALTO_PLAYER_DISABLE_SUBTITLES` is defined), and `fragmentcollector_mpd.cpp` saves and reuses a subtitle init header. | Content with no subtitles still carries a dummy source and an EOS signal. Extra moving parts in ad transitions. | Comments: Rialto reports end of stream only after every source delivers EOS, and will not start video without a complete pipeline. | Verify whether current Rialto still needs it. |
| **W4 (CL).** On the first `SendCopy()` to an unattached source, **all** unattached sources are attached from codec format alone (`SendCopy`). | Attaches before real caps are known. | Comment: avoid a deadlock with muxed HLS-TS, which has one injection thread. | Attach lazily once the TS processor identifies each track. |
| **W5 (CL).** AC-3 is mapped to `audio/x-eac3` (`AampRialtoAudioSource::mapCodecToMime`). | Ties behaviour to one rialto-gstreamer version. | Comment cites the pinned rialto-gstreamer v0.20.1 mapping. | Re-check when the pinned version changes. |
| **W6 (CL).** `needData` received while the source is gated is answered `NO_AVAILABLE_SAMPLES` at once (`handleNeedData`). | Rialto must re-issue the request later. A missed re-issue would stall that track. | Comment: Rialto treats data on a request issued before `SEEK_DONE` as stale and discards it. | Confirm against Rialto server behaviour (unverified here). |
| **W7 (CL).** After `SEEK_DONE` the subtitle position is set again with `setSourcePosition()`. | Extra blocking IPC call on the event thread under `m_attachMutex`. | Comment: "suspected Rialto issue", subtitles otherwise appear late. | Track the Rialto fix and remove. |
| **W8 (CL).** `Configure()` waits at most 2 s for `RUNNING`, then continues. | Pipeline creation can proceed against a server that is not ready. | Comment: avoid a permanent hang. | Surface the timeout as an error. |
| **W9 (CL).** Fixed pipeline requirements 3840 x 2160, `isLive=false` with a TODO. | Capability limits may not match the content. Live is not reported to Rialto. | TODO in code. | Derive from the stream. |
| **W10 (CL).** Compile-time `RIALTO_PLAYER_DISABLE_SUBTITLES` switch. | Dead build variant. | Comment: real Rialto on Ubuntu locks up intermittently with no text-track sink. | Keep documented or remove. |
| **W11 (CL).** `useMp4Demux` is forced on (`AampConfig::DoCustomSetting`). | Config silently overridden (with a warning). | Direct Rialto needs demuxed samples. | None needed. |

## Not implemented or partial StreamSink methods

| Method | Behaviour |
|---|---|
| `FlushTrack`, `SignalTrickModeDiscontinuity`, `SetPauseOnStartPlayback`, `SetSubtitlePtsOffset`, `ResetEOSSignalledFlag` | Log only |
| `SetVideoZoom` | Warns, ignored |
| `SetTextStyle` | Warns, ignored, **returns true** |
| `IsCacheEmpty` | Always returns false |
| `GetVideoRectangle`, `GetVideoSize` | Derived from the last `SetVideoRectangle()` |
| `StopBuffering(forceStop)` | `forceStop` is ignored. Both values call `play()` because Rialto exposes no decoder queue depth. |
| `GetVideoPTS` | Derived from `getPosition()`, no PTS API |
| `GetVideoPlaybackQuality` | Only in `PLAYING` or `PAUSED`, via `getStats()` |

## Other confirmed limitations

| Finding and location | Why it matters | Apparent reason | Possible improvement |
|---|---|---|---|
| **L1 (CL).** `PlaybackState::FAILURE` only moves the state machine to `ERROR` (`OnPlaybackState`). No AAMP error is raised. | A fatal pipeline failure can leave playback stalled with no event. Rialto may also send `notifyPlaybackError`, which is handled, but that was not verified for every failure. | No comment. | Call `NotifyPlaybackError` from the `FAILURE` case. |
| **L2 (CL).** Setup failures are only logged: no factory, `createMediaPipeline` null, `load()` failure, `attachSource` failure, unsupported codec, `allSourcesAttached` failure (`Configure`, `AttachSource`, `CheckAllSourcesAttached`). | The tune appears to proceed, then stalls. | No comment. | Report a tune failure from these points. |
| **L3 (CL).** Audio MP3 and Atmos cannot attach: `AampRialtoAudioSource::mapCodecToMime` has no mapping, although `isAudioOutputFormat()` in the player treats them as audio. | Such tracks fail to attach (see L2). | No comment. | Map or reject early with a clear error. |
| **L4 (CL).** First frame is reported on the first `PLAYING` notification. `notifyFirstFrameReceived` in the pipeline client is a stub. | Time to first frame is likely reported earlier than the first rendered frame (inferred). | Rialto exposes `notifyFirstFrameReceived`, unused here. | Use it, if the server sends it. |
| **L5 (CL).** `WaitForFlushToComplete()` has no timeout. | If `SEEK_DONE` never arrives, `Configure`, `Stop` and `Flush` block forever. Only `FAILURE` or a `setPosition` failure releases it. | Relies on Rialto always sending `SEEK_DONE`. | Add a bounded wait with an error path. |
| **L6 (CL).** The `async` result of `play()` and `flush` is ignored. | Completion is inferred from notifications only. | No comment. | Log or act on it. |
| **L7 (CL).** `ShouldRecreatePipeline()` rebuilds the Direct Rialto pipeline for source-set or codec changes, with exceptions such as audio EOS during trick play. | This is not a demonstrated disadvantage relative to Rialto GStreamer sinks: those sinks share the server restriction on adding sources after `allSourcesAttached()`. Retaining a client-side GStreamer container does not imply retaining a usable Rialto session. The plugin also uses the narrower `switchSource()` API for audio caps changes. | Plugin `GStreamerMSEMediaPlayerClient::sendAllSourcesAttachedIfPossibleInternal()` documents the server restriction; `PullModeAudioPlaybackDelegate` uses `switchSource()`. | Evaluate supported source-switching cases separately from changes to the complete source set. See [comparison.md](comparison.md#advantages-and-trade-offs). |
| **L8 (CL).** `AampRialtoMonitorAV` cannot detect AV sync loss: both tracks use the pipeline position. | The `avsync` class never triggers. | Header comment states it. | Document in the monitor's user docs. |
| **L9 (CL).** `notifyPosition`, `notifyQos`, `notifyNetworkState`, `notifyNativeSize`, `notifyVideoData`, `notifyAudioData` are ignored. | Information Rialto already provides is unused. | Not implemented. | Use where useful. |
| **L10 (CL).** `useDirectRialto` is absent from `README.md` and `CONFIGURATION.md`. | Operators cannot discover it. | n/a | Document it. |

## Dead code and stale comments

| Finding and location | Notes |
|---|---|
| **D1 (CL).** `PlayerStateId::STOPPED` and `StoppedState` are never entered (`AampPlayerStateMachine.*`). `onStop()` goes to `IDLE`. Comments in `AampPlayerStateMachine.h` and `StopInternal()` still describe a `STOPPED` state and a "FLUSHING to STOPPED arc". | Remove or update. |
| **D2 (CL).** `AampRialtoMediaSource::flushSource()` has no callers. Its comment refers to `OnSourceFlushed()`, which now only logs a warning. `AampRialtoMediaPipelineClient::notifySourceFlushed` is a no-op and `SetSourceFlushedCallback` is never installed. | Remove or document. |
| **D3 (CL).** `AampRialtoMediaPipelineClient::init()` returns false and has no callers in `direct-rialto/`. Its file header calls it a "stub implementation". | Remove. |
| **D4 (CL).** `AampRialtoPlayer.cpp` file header says calls are forwarded to an internally owned `AAMPGstPlayer`. They are not. | Fix comment. |
| **D5 (CL).** `IDrmBridge::clearSessions()` is documented as "Called from `ClearProtectionEvent()`", but `ClearProtectionEvent()` only clears `m_pendingProtection`, and nothing in `direct-rialto/` calls `clearSessions()` (`IDrmBridge.h`, `AampRialtoPlayer.cpp`). `AampDrmBridge.h` also says the bridge's `PrivateInstanceAAMP *` lifetime is "guaranteed by AampRialtoPlayer", which `ChangeAamp()` does not maintain (see C12). | Call it from the intended place or remove it. |

## Concurrency and lifetime

Background and the unsynchronised member list are in
[threading.md](threading.md). All items here are **Potential risk** unless
stated.

### Recreation without Stop

| Finding and location | Why it matters | Apparent reason | Possible improvement |
|---|---|---|---|
| **C1 (PR).** `Configure()` can recreate the pipeline without `Stop()`. `PrivateInstanceAAMP::ProcessPendingDiscontinuity()` calls `Configure(..., GetESChangeStatus(), ...)` after only stopping injection, and an ES change makes `ShouldRecreatePipeline()` true. The new pipeline is created **before** the old one is released by the assignment `m_pipeline = ...`. The old pipeline never gets `stop()`. `onReconfigure` resets the state machine to `IDLE` while the old pipeline is alive. The old `AampRialtoMonitorAV` (holding the old pipeline) keeps running until replaced, and only if `load()` succeeds. The control backend is not recreated. | Two `IMediaPipeline` objects exist briefly. Whether the Rialto server accepts that is **unverified**. Callbacks installed on the shared `m_client` can be reassigned while the old pipeline still delivers. | No comment. | Release the old pipeline (monitor, `stop()`, reset) before `createMediaPipeline()`, using the same path as `StopInternal()`. |

### Other concurrency and lifetime items

| Finding and location | Why it matters | Apparent reason | Possible improvement |
|---|---|---|---|
| **C2 (PR).** `m_pipeline` (`shared_ptr`) and `m_sources[]` are read on injector, event and timer threads and written by `Configure`/`Stop` with no lock. Injectors hold `IMediaPipeline &` taken from `*m_pipeline`. The `SEEK_DONE` handler dereferences `m_pipeline` without a null check. | Concurrent `shared_ptr` read and reset is a data race. Safe only if injection is stopped (joined) before `Stop()` and `Configure()`. That holds in `TeardownStream` but is not enforced by the sink. | Not stated. | Copy `m_pipeline` to a local `shared_ptr` at each use, or guard with a mutex. Add a debug assertion for quiesced injection. |
| **C3 (PR).** `AttachSource()` can block on a DRM licence round trip while the caller holds `m_attachMutex` (`SetStreamCaps`, `SendTransfer`). The `SEEK_DONE` handler on the Rialto event thread, and the mute setters on the control thread, take the same mutex. | A seek or mute during licence acquisition stalls the event thread (and so flush completion) or the control thread. | Comment: serialises attach across download threads. | Do DRM session creation outside `m_attachMutex`. See IO7. |
| **C4 (PR).** `Flush()` decides "flushable" and then claims `FLUSHING` in two steps. The comment says only the control thread calls it, but `MaybeFlushForPendingPosition()` calls it from an injector thread. | A control-thread `Flush()` between the two steps can double-claim. | Comment relies on thread confinement. | Claim `FLUSHING` atomically with the state check. |
| **C5 (PR).** `~AampRialtoPlayer` does not release `m_pipeline` first, and `m_pipeline` is declared before `m_sources`, `m_stateMachine`, `m_monitorAV`. | If the player is destroyed without `Stop()`, those members die while the pipeline event thread can still call back. | Normal flows call `Stop()` first. | `m_pipeline.reset()` at the start of the destructor. |
| **C6 (PR).** `AampRialtoControlBackend` declares its mutex and condition variable after `m_control`, so they are destroyed first. | A `notifyApplicationState` in flight at destruction would lock a destroyed mutex. Narrow window. | Not stated. | Reorder members, or unregister first. |
| **C7 (PR).** `ProgressTimer` and `AampRialtoMonitorAV` use `g_timeout_add` on the default context. `started` and `source_id` are plain members written from the event thread and the control thread. Destructors call `g_source_remove`, which does not wait for a running callback. `AampRialtoMonitorAV` also makes blocking `getPosition()` / `getStats()` IPC calls on the main context. | Race on timer state. A callback can run on a destroyed object. A slow Rialto call stalls the application main loop. | Not stated. | Guard timer state, and move the IPC off the main loop. |
| **C8 (PR).** `RialtoMediaKeySystem::MediaKeysClient::onKeyStatusesChanged` invokes adapter callbacks while holding its mutex, unlike `onLicenseRequest`. | Calling `deregisterSession` from one of those callbacks would self-deadlock. Currently the callbacks do not do that. | Not stated. | Copy callbacks under the lock and call outside it. |
| **C9 (PR).** `m_videoRectangle`, `m_audioVolume`, `m_pendingPlayOnFragCaching`, `m_drmBridge` and `PrivateInstanceAAMPNotifiable::m_aamp` are plain members written by one thread and read by others. | Data races on a `std::string` and shared pointer in particular. | Not stated. | Use atomics or a mutex. |
| **C10 (PR).** Scheduled tasks capture a raw `PrivateInstanceAAMP *`. | Valid only if the scheduler is drained before `PrivateInstanceAAMP` is destroyed. Not checked here. | Not stated. | Confirm scheduler shutdown order. |
| **C12 (PR).** `m_drmBridge` is created once, lazily, in `AttachSource()` with the then-current `m_aamp` and is replaced only by `SetEncryptedAamp()`. `Stop()` and `ChangeAamp()` do not reset it. `AampStreamSinkManager` calls `ChangeAamp()` in single-pipeline mode when the player associated with the sink is deleted (`AampStreamSinkManager.cpp`, player deletion) and from `SetActive()`. | Confirmed code path: after `ChangeAamp()`, DRM requests still go through the earlier player's `mDRMLicenseManager`, and a deleted earlier player would leave a dangling pointer. A crash was not reproduced. Routing through the encrypted player is intended for pre-roll ads, but is not distinguished from the accidental case. | `AampDrmBridge.h` says the lifetime is guaranteed by the player. | Rebind or reset the bridge in `ChangeAamp()` and `Stop()`. |
| **C13 (PR).** `AampDrmBridge::createSession()` hard-codes `eMEDIAFORMAT_DASH` (`AampDrmBridge.cpp`), so HLS content is presented to the DRM manager as DASH. | Confirmed effect on helpers (`.libs/middleware-player-interface/drm/helper`): `ClearKeyHelper` takes its DASH branches for init data and does not transform the key response into a JWK; `VerimatrixHelper` takes its DASH branches. Widevine and PlayReady accept DASH and HLS fMP4 alike, so they are unaffected. Whether ClearKey or Verimatrix HLS is used with Direct Rialto is not known. | Not stated. | Pass the real media format through `IDrmBridge`. |
| **C14 (CL).** `AampRialtoPlayer` does not override `StreamSink::GetEncryptedAampId()`, so it returns the default `-1`, and `SetEncryptedAamp()` stores no player id. In single-pipeline mode `AampStreamSinkManager::SetEncryptedHeaders()` therefore always finds `encryptedPlayerId != aamp->mPlayerId`, logs an error, and clears `mEncryptedHeaders` and `mEncryptedHeadersInjected`. | The "encrypted headers already set" shortcut never applies to Direct Rialto, so headers are re-set each time. `AAMPGstPlayer` stores `mEncryptedAampId` for this check (`aampgstplayer.cpp`). Whether the extra injection is harmful was not traced. | Not stated. | Store the player id in `SetEncryptedAamp()` and override `GetEncryptedAampId()`. |
| **C11 (IO).** The play rendezvous can issue `play()` twice when both sides see each other's flag. | Harmless to Rialto but redundant. | Comment: either side always plays. | Compare-exchange `m_playRequested`. |

## Improvement opportunities

- **IO1.** Remove dead code and fix stale comments (D1 to D4).
- **IO2.** Add tests for `AampDrmBridge`, `makeRialtoSessionCreator` and for
  concurrent flush, stop and callback orderings, ideally with a
  ThreadSanitizer build.
- **IO3.** Reduce `ENTRY` / `EXIT` INFO logging on per-sample paths.
- **IO4.** Add a simulator mode that overlaps two pipelines or delays
  `SEEK_DONE`, to exercise C1 and L5.
- **IO5.** Document `useDirectRialto` (L10).
- **IO6.** Report setup failures and `FAILURE` to AAMP (L1, L2).
- **IO7.** Decouple `attachSource` from DRM session creation, to shorten tune
  time for encrypted content.
  - *Current behaviour.* `AampRialtoMediaSource::attachOrUpdate()` calls
    `AampDrmBridge::createSession()` before `IMediaPipeline::attachSource()`,
    on the injector thread and under `m_attachMutex`. Audio and subtitle wait
    for video to attach, so one slow licence delays attach, `allSourcesAttached`
    and `play()` for every track.
  - *Why it need not be so.* The `mksId` passed to `createRialtoSource()` as
    `hasDrm` is ignored: the video and audio sources hard-code `hasDrm = true`
    and the subtitle source discards it. On the Rialto server `hasDrm` only
    decides whether a decryptor element is inserted
    (`media/server/gstplayer/source/GstSrc.cpp`, `.libs/rialto`). The key
    session is used per encrypted segment, by `annotateEncryption()` on the
    client and by the decryptor on the server. So the licence only has to be
    ready before the first encrypted `addSegment`.
  - *Possible gain.* Server pipeline setup, `pause` and the first
    `needMediaData` could overlap the licence round trip. The gain is limited
    to the smaller of the licence time still outstanding and that setup time.
    The licence already starts early on the prefetch thread, so the gain may be
    small. It is **not measured**, because the available device logs contain no
    DRM tune.
  - *Required changes.* Move session creation out of `attachOrUpdate()` and
    out of `m_attachMutex` (this also addresses C3). Make `injectOneSample()`
    wait for the session before the first encrypted sample, with a new
    "DRM pending" wait like `attachPending`. The source gate cannot serve,
    because `UngateAllSources()` clears it when `play()` is issued. Keep the
    current failure behaviour, where a failed session is logged and the source
    still attaches, and report an asynchronous failure. Keep the existing
    replacement of a stale `mksId` on a period change.
  - *Risk to check.* That a real Rialto server accepts `play()` with an
    encrypted source attached and no key session yet. This follows from the
    server code but has not been run.
  - *Measure first.* In a DRM tune log, the gap between
    `createSession returned mksId=` and the following `Attached source id=`
    (both INFO lines) is the DRM wait on the attach path. If it is small, the
    change is not worth the added concurrency. The same logs would show whether
    the Rialto GStreamer path waits on DRM before its `attachSource`, which is
    still open in [comparison.md](comparison.md#drm-licence-acquisition-and-the-attach-path).
