# Direct Rialto - Architecture and class model

## Purpose

This page explains how the `direct-rialto/` component is built: the layers, what every class and interface is responsible for, who creates and owns whom, and which seams (interfaces and injected callable objects) exist so that the code can be unit tested without a Rialto server or a running AAMP instance. Read it after the overview page and before the pages that go deep into the lifecycle, the media data path and the flush/seek logic. It deliberately does not repeat the state machine, the needData/haveData protocol or the flush sequence; it points to the pages that own them.

## Source files

| File | Role |
| --- | --- |
| `AampRialtoPlayer.h/.cpp` | The `StreamSink` implementation; orchestrates everything below |
| `AampRialtoPipelineSession.h/.cpp` | Creates and owns the Rialto pipeline, client, factories and control backend |
| `AampRialtoMediaPipelineClient.h/.cpp` | Receives Rialto events and forwards them to `IAampRialtoPipelineEvents` |
| `AampRialtoControlBackend.h/.cpp`, `IRialtoControlBackend.h` | Waits for the Rialto server to report `RUNNING` |
| `AampPlayerStateMachine.h/.cpp` | `PlayerStateMachine`: lifecycle state table |
| `AampRialtoPlaybackController.h/.cpp` | Decides when `play()` may be issued (`PlayHold` bits) |
| `AampRialtoSegmentPosition.h/.cpp` | Staged flush position/rate and segment-start baseline |
| `AampRialtoPreRollMonitor.h/.cpp` | Decides when enough data is queued before `play()` |
| `AampRialtoMonitorAV.h/.cpp` | Periodic AV health classification and reporting |
| `AampRialtoProgressTimer.h`, `AampRialtoGlibTimer.h` | GLib timers (progress reports; shared timer primitive) |
| `AampRialtoMediaSource.h/.cpp` | Abstract per-track source: attach, inject, gate, EOS |
| `AampRialtoVideoSource`, `AampRialtoAudioSource`, `AampRialtoSubtitleSource` (`.h/.cpp`) | Codec-specific source subclasses |
| `AampRialtoNeedDataHandshake.h/.cpp` | State and decisions of one source's needData/haveData handshake |
| `AampRialtoSourceCreators.cpp` | `makeDefaultSourceCreator()`: the production `SourceCreator` |
| `AampRialtoTtmlOffset.h/.cpp` | TTML display-offset detection for the subtitle source |
| `AampRialtoTime.h` | Nanosecond/millisecond constants and rounding helpers |
| `IStreamSinkNotifiable.h`, `PrivateInstanceAAMPNotifiable.h/.cpp` | Notification interface and its adapter onto `PrivateInstanceAAMP` |
| `IDrmBridge.h`, `AampDrmBridge.h/.cpp` | Interface and implementation for creating a DRM session and getting its `mks_id` |
| `RialtoSessionCreator.h/.cpp`, `RialtoMediaKeySystem`, `RialtoMediaKeySession`, `RialtoMediaKeySessionAdapter` | Rialto `IMediaKeys` plugged into AAMP's DRM session manager |
| `IDirectRialtoCC.h` (from middleware-player-interface, installed under `.libs/include`) | Narrow closed-caption control interface that `AampRialtoPlayer` implements |

## Layers

direct-rialto has no GStreamer dependency (`AampRialtoPlayer.h` states that no GStreamer headers are included). It is a single class hierarchy rooted in the `StreamSink` contract that the rest of AAMP already speaks, with the Rialto client API (`IMediaPipeline`, `IMediaKeys`, `IControl`) underneath.

[[diagram:direct-rialto-layers-component]]

*Layers: AAMP core calls the player; the player owns policy helpers, per-track sources and the Rialto connection; adapters plug it into AAMP's notification and DRM machinery.*

| Layer | Contents | Rule of thumb |
| --- | --- | --- |
| L0 AAMP core (outside the folder) | `PrivateInstanceAAMP`, `AampStreamSinkManager`, `StreamAbstractionAAMP`, DRM license manager, `PlayerCCManager` | Calls into the player through `StreamSink`; never includes Rialto headers |
| L1 Facade and orchestration | `AampRialtoPlayer` | The only class AAMP core talks to; decides, but delegates policy |
| L2 Policy helpers | `PlayerStateMachine`, `AampRialtoPlaybackController`, `AampRialtoSegmentPosition`, `AampRialtoPreRollMonitor`, `AampRialtoMonitorAV`, `AampRialtoProgressTimer` | Small objects with one responsibility, each with its own lock or atomics; they never call Rialto directly except `AampRialtoMonitorAV` (queries stats) |
| L3 Per-track sources | `AampRialtoMediaSource` and subclasses, `AampRialtoNeedDataHandshake`, `AampRialtoTtmlOffset` | All per-track state; the only code that calls `addSegment()` and `haveData()` |
| L4 Adapters to the outside | `PrivateInstanceAAMPNotifiable`, `AampDrmBridge`, the Rialto DRM classes | Keep heavy AAMP headers (`priv_aamp.h`) out of the player header |
| L5 Rialto connection | `AampRialtoPipelineSession`, `AampRialtoMediaPipelineClient`, `AampRialtoControlBackend` | Everything that touches Rialto object creation and lifetime |

## Class diagrams

The model is split into four diagrams. Each is deliberately partial; the class-responsibility table below is the complete list.

### Player core and what it owns

[[diagram:direct-rialto-player-core-class]]

*`AampRialtoPlayer` implements three interfaces and owns the policy helpers by value. Only the pre-roll monitor and AV monitor are optional (`unique_ptr`).*

### Source hierarchy

[[diagram:direct-rialto-sources-class]]

*`AampRialtoMediaSource` is a template-method base: it owns pacing, the handshake, DRM annotation and demuxing; subclasses supply codec mapping and segment construction.*

### DRM

[[diagram:direct-rialto-drm-class]]

*There are two separate DRM hooks: `IDrmBridge` (player asks AAMP's license manager for a session) and `makeRialtoSessionCreator()` (AAMP's session manager builds Rialto-backed `DrmSession` objects).*

### Notification and backend interfaces, and ownership of the Rialto objects

[[diagram:direct-rialto-interfaces-ownership-class]]

*`AampRialtoPipelineSession` owns the Rialto objects; the player holds only snapshots of the pipeline for the duration of a call.*

## Class responsibility table

| Class / interface | Responsibility | Created and owned by | Main threads |
| --- | --- | --- | --- |
| `AampRialtoPlayer` (`AampRialtoPlayer.h:AampRialtoPlayer`) | Implements `StreamSink`, `IDirectRialtoCC` and (privately) `IAampRialtoPipelineEvents`. Creates the pipeline on `Configure()`, creates/attaches sources, decides flush/stop/pause behaviour, forwards Rialto events to sources and notifications to AAMP | `AampStreamSinkManager::CreateSinkInstance` (raw `new`); deleted by `AampStreamSinkManager::DeleteStreamSink` | AAMP control thread, injector threads, Rialto event thread, GLib loop, any (getters) |
| `StreamSink` | AAMP core's abstract sink; most methods have no-op defaults | n/a (interface in AAMP core) | n/a |
| `IDirectRialtoCC` | `setTextTrackIdentifier()` and `setCCMute()`; lets `PlayerDirectRialtoCCManager` (middleware) drive CC without Rialto headers | Implemented by `AampRialtoPlayer`; its pointer is handed to core as the "CC decoder handle" (`AampRialtoPlayer::GetCCHandle`) | Caller thread |
| `IAampRialtoPipelineEvents` (`AampRialtoMediaPipelineClient.h`) | Seven callbacks the player needs from Rialto: need-data, cancel-need-data, playback state, position, duration, buffer underflow, playback error | Implemented by `AampRialtoPlayer` (private base) | Rialto event thread |
| `AampRialtoMediaPipelineClient` | Implements Rialto's `IMediaPipelineClient` (all pure virtuals); forwards the events above to the installed sink; the other notifications are only logged | `AampRialtoPipelineSession::EnsureClient` (`shared_ptr`); Rialto holds only a `weak_ptr` | Rialto event thread |
| `AampRialtoPipelineSession` | Creates log bridge, factories, capabilities, control backend, client; waits for `RUNNING`; creates and `load()`s the pipeline; hands out pipeline snapshots; releases things in a safe order | Member of `AampRialtoPlayer` (by value) | Control thread (creation/release); any thread (`GetPipeline`) |
| `AampRialtoControlBackend` / `IRialtoControlBackend` | Registers an `IControlClient` with Rialto and implements `waitForRunning(timeoutMs)`; shared state outlives the backend so a late notification is safe | `AampRialtoPipelineSession::EnsureFactories` (lazily, `unique_ptr`) or injected through the test constructor; released on `Stop()` | Control thread; Rialto thread for the notification |
| `PlayerStateMachine` (`AampPlayerStateMachine.h`) | Table-driven lifecycle FSM (IDLE ... ERROR) with a mutex and condition variable; `waitForFlushComplete()` has a timeout so a lost SEEK_DONE cannot hang `Stop()` | Member of `AampRialtoPlayer` (by value) | Any; every call is atomic |
| `AampRialtoPlaybackController` | Single decision point for "may `play()` be issued": a request flag plus a bit set of `PlayHold`s; fires the injected play action exactly once when a request exists and no hold remains; also fires a pre-roll action | Member of `AampRialtoPlayer`; callbacks bound to `IssuePlay` and `StartPreRoll` | Any; internal recursive play lock plus a state mutex |
| `AampRialtoSegmentPosition` | Atomic staged flush position/rate/authority, the deferred-flush election flag, and the segment-start baseline used by `GetPositionMilliseconds()` | Member of `AampRialtoPlayer` | Control thread, injectors, Rialto thread |
| `AampRialtoPreRollMonitor` | Polls the primary track's accepted frame count on a GLib timer and signals completion exactly once (frame floor, no space, EOS or timeout); counts are client-side so the gate does not depend on the Rialto server implementation | `unique_ptr` member of `AampRialtoPlayer`, created in its constructor | GLib loop (poll); any (`Start`/`Stop`) |
| `AampRialtoMonitorAV` | Two GLib timers: sample position and classify AV health; report via `SendMonitorAvEvent` (reads `getStats()`); runs only while PLAYING | `unique_ptr` created by `AampRialtoPlayer::CreateMonitorAV` when `eAAMPConfig_MonitorAV` is set; recreated per pipeline | Start on Rialto thread; stop on control thread; ticks on GLib loop |
| `AampRialtoProgressTimer` | Timer that fires immediately on start, then every interval, and can be "kicked" for an immediate extra dispatch | Member of `AampRialtoPlayer` | Any; ticks on GLib loop |
| `AampRialtoGlibTimer` | Thread-safe periodic timer on the default GLib main context; `Stop()` waits for a tick in progress and no tick runs afterwards | Member of the three timer users above | GLib loop |
| `AampRialtoMediaSource` | Abstract per-track state: Rialto source id, mks id, handshake, demuxer, pending codec data, statistics; `attachOrUpdate`, `processInitFragment`, `processDataFragment`, `injectSingleSample`, `gateInjection`, `unblockInjection`, `signalEos`, `handleNeedData`; creates the DRM session through `IDrmBridge` | `SourceCreator` called by `AampRialtoPlayer::CreateMediaSource` (`shared_ptr` in `m_sources`) | Injector thread (inject), Rialto thread (needData), control thread (gate/reset) |
| `AampRialtoVideoSource` | H.264 (`video/h264`) and HEVC (`video/h265`) mapping; `MediaSegmentVideo` creation; stream format AVC/HVC1 or byte-stream from NAL prefixing | Via `SourceCreator` | as base |
| `AampRialtoAudioSource` | AAC (`audio/aac`, ADTS `audio/mp4`), AC-3/EAC-3 (`audio/x-eac3`), AC-4 (`audio/x-ac4`) mapping; `MediaSegmentAudio` creation | Via `SourceCreator` | as base |
| `AampRialtoSubtitleSource` | TTML/WebVTT/TTML-in-MP4 (`text/ttml`, `text/vtt`) and inband closed caption (`text/cc`); applies the TTML display offset in `createSegment()` | Via `SourceCreator` | as base |
| `AampRialtoNeedDataHandshake` | Pending request, generation token, gate mode, EOS flag, active-injector flag and attach-pending flag, all under one mutex. Never calls Rialto: operations return a `Claim` and the caller sends `haveData()` after unlocking | Member of `AampRialtoMediaSource` | Injector, Rialto and control threads |
| `makeDefaultSourceCreator()` (`AampRialtoSourceCreators.cpp`) | The production `SourceCreator`: `AampMediaType` to a new Video/Audio/Subtitle source. Lives in its own file so test binaries can substitute a fake without linking the concrete sources | Free function | Control thread |
| `AampRialtoTtmlOffset` | Detects whether TTML cue times are absolute or container-relative and returns the display offset, mirroring `gstvipertransform` | Member of `AampRialtoSubtitleSource` (by value) | Subtitle injector thread only |
| `AampRialtoTime.h` | `kNsPerSecond`, `kNsPerMs`, `SecondsToNs()`, `SecondsToMs()` (round to nearest) | Header only | n/a |
| `IStreamSinkNotifiable` | The notification contract (14 notifications plus two queries) from sink to AAMP core | Interface | n/a |
| `PrivateInstanceAAMPNotifiable` | Production implementation: forwards each void notification onto AAMP's scheduler thread (`ScheduleAsyncTask`) to avoid lock-order deadlocks; two calls are synchronous | `unique_ptr` (`m_notifiableAdapter`) in `AampRialtoPlayer`, only when no test notifiable was injected; `ChangeAamp()` re-targets it | Called from any thread |
| `IDrmBridge` | `createSession(systemId, initData, len, type)` returns an `mks_id` or -1 | Interface | n/a |
| `AampDrmBridge` | Calls `AampDRMLicenseManager::createDrmSession()` and returns the session's media-key-session id; blocks for the whole license flow; rejects init data over 65535 bytes | `shared_ptr` created lazily by `AampRialtoPlayer::GetDrmBridge` or `SetEncryptedAamp` | Fragment collector / injector thread |
| `makeRialtoSessionCreator()` (`RialtoSessionCreator.h/.cpp`) | Returns the `DrmSessionCreator` that builds a `RialtoMediaKeySystem` and wraps it in a `RialtoMediaKeySessionAdapter` for any DRM system id | Called by the `PrivateInstanceAAMP` constructor only when `useDirectRialto` is set | Constructor thread |
| `RialtoMediaKeySystem` | Owns one Rialto `IMediaKeys` and an inner `IMediaKeysClient` that routes challenge/key-status/renewal events to the right session callbacks; `deregisterSession()` waits for callbacks in flight | `unique_ptr` inside `RialtoMediaKeySessionAdapter` (one per DRM session) | Rialto key event thread |
| `RialtoMediaKeySession` | One Rialto key session: `update`, `close`, `destruct`, cached per-key status (`isKeyUsable`, `isKeyOutputRestricted`) | Created by `RialtoMediaKeySystem::createSession`; `shared_ptr` in the adapter | Key event thread writes, license thread reads (mutex) |
| `RialtoMediaKeySessionAdapter` | Implements AAMP's `DrmSession` for the Rialto path (`generateDRMSession`, `generateKeyRequest`, `processDRMKey`, state waits with timeouts, `getMediaKeySessionId`); `decrypt()` is not overridden because decryption happens in the Rialto server | Built by the session creator; owned by `DrmSessionManager` | License thread; key event thread callbacks |

## Ownership and lifetime

[[diagram:direct-rialto-object-creation-sequence]]

*Creation order: player instance, sink manager, player constructor (no Rialto objects yet), then `Configure()` creates the Rialto objects.*

Key rules, all verified in the code:

- **The sink manager owns the player.** `AampStreamSinkManager` stores it in `mActivePlayersMap` (or `mStreamPlayer` in single-pipeline mode) and calls `delete` in `DeleteStreamSink`. If a client supplies its own `StreamSink` (`SetStreamSink`), the manager does not delete it and `useDirectRialto` plays no part.
- **No Rialto object exists at construction.** The constructor only builds the policy helpers. The control backend, log handler, factories, capabilities and client are created by the first `Configure()` (`AampRialtoPipelineSession::CreateAndLoad`). A player that is created but never tuned never registers with the Rialto server (see the comment in `AampRialtoPipelineSession::EnsureFactories`).
- **The session owns the pipeline; everyone else holds a snapshot.** `AampRialtoPlayer::GetPipeline()` returns a `shared_ptr` copy. Callers keep it only for one call, because `Stop()` must release the pipeline synchronously; a snapshot kept in a member or a long-lived lambda would keep the Rialto session alive. The monitor (`AampRialtoMonitorAV`) is the one deliberate exception: it holds its own `shared_ptr`, so `Stop()` and `Configure()` call `SetMonitorAV(nullptr)` before releasing the pipeline.
- **Sources are `shared_ptr`.** Injector threads call `getSource()` and keep the returned pointer for the duration of an injection, so `Configure()` can replace a source (`SetSource`) without a use-after-free. `m_sourcesMutex` is held only to copy or swap entries; a replaced source is destroyed after the lock is dropped.
- **Rialto holds the client weakly.** `createMediaPipeline` receives a `weak_ptr<IMediaPipelineClient>`. The client's event sink is a raw pointer to the player, installed before `load()` (so events delivered during load are not dropped) and cleared after the pipeline is released.
- **Destruction order.** `~AampRialtoPlayer` stops the monitors and timers, unblocks every source, releases the AV monitor, then releases the pipeline and clears the client's event sink. The last pipeline release joins Rialto's event thread, so no event can run on a destroyed player. Members declared after `m_session` are destroyed before it. The destructor also calls `PlayerCCManager::InvalidateHandle()` so the CC manager singleton does not keep a dangling `IDirectRialtoCC*`.
- **Raw pointers.** `m_notifiable` is not owned (the adapter, or the injected test object, owns it). `m_aamp` is an `atomic<PrivateInstanceAAMP*>`, switched by `ChangeAamp()`; every use takes one snapshot through `Aamp()`. `AampDrmBridge` and `PrivateInstanceAAMPNotifiable` keep non-owning `PrivateInstanceAAMP*`; the caller guarantees the AAMP instance outlives them.
- **Single-pipeline mode re-targets rather than recreates.** `AampStreamSinkManager::SetActive` calls `ChangeAamp()` (which also resets the DRM bridge unless one was installed through `SetEncryptedAamp()`), then `Flush(position, rate, true, true)` with an authoritative position.

## Interface and seam design (dependency injection for tests)

direct-rialto is built so the heavy collaborators can be replaced. Production wiring is in the first constructor; the second constructor is the seam used by tests.

| Seam | Production binding | Test binding | Notes |
| --- | --- | --- | --- |
| `IStreamSinkNotifiable *` | `PrivateInstanceAAMPNotifiable` (adapter on `PrivateInstanceAAMP`) | `MockIStreamSinkNotifiable` passed to the test constructor | The player header never includes `priv_aamp.h` |
| `std::unique_ptr<IRialtoControlBackend>` | `nullptr`, so the session lazily creates `AampRialtoControlBackend` | `MockIRialtoControlBackend` | Controls the "wait for RUNNING" step |
| `SourceCreator` (`std::function`) | `makeDefaultSourceCreator()` | Lambda returning `MockAampRialtoMediaSource` | The mock overrides only the virtuals; base-class logic still runs |
| `IDrmBridge` | `AampDrmBridge` | `FakeAampDrmBridge` delegates to a global `MockDrmBridge` | Link-time seam: the production constructor has no parameter for it |
| Rialto factories | Real `libRialtoClient.so` (`IMediaPipelineFactory::createFactory()`) | `FakeRialtoFactories` returns a global mock factory | Link-time seam |
| `IAampRialtoPipelineEvents` | `AampRialtoMediaPipelineClient` forwards real events | `FakeAampRialtoMediaPipelineClient` forwards to the installed sink so tests can post events | |
| `std::function` getters/actions | Lambdas bound to the player | Lambdas bound to test objects | `AampRialtoPlaybackController`, `AampRialtoMonitorAV`, `AampRialtoPreRollMonitor` (probe, clock) |
| `DrmSessionCreator` | `makeRialtoSessionCreator()` | Null creator (`FakeRialtoSessionCreator`) | |

The seams are not complete. `AampRialtoPlayer` still reaches into `PrivateInstanceAAMP` directly for `mConfig` reads (nine call sites), `ReconfigureForElementaryStreamUpdate()`, `WillFlushOnDiscontinuity()` and `ResumeTrackDownloads()`, so tests of the player need `MockPrivateInstanceAAMP` and `MockAampConfig` in addition to the notifiable mock. See the testing page.

## Design patterns used

| Pattern | Where | Why |
| --- | --- | --- |
| Facade | `AampRialtoPlayer` | One class presents the `StreamSink` contract AAMP already uses |
| Adapter | `PrivateInstanceAAMPNotifiable`, `AampRialtoMediaPipelineClient`, `RialtoMediaKeySessionAdapter`, `AampDrmBridge` | Translate between AAMP's and Rialto's interfaces without letting either include the other's headers |
| Template Method | `AampRialtoMediaSource` and subclasses | Common pacing/DRM/injection; subclasses supply codec mapping and segment creation |
| Factory (injectable) | `SourceCreator`, `makeRialtoSessionCreator()` | Let tests and platforms substitute creation |
| Dependency Inversion / constructor injection | Second `AampRialtoPlayer` constructor, `IDrmBridge`, `IRialtoControlBackend` | Unit testing without a server |
| Table-driven state machine | `PlayerStateMachine` (one transition table, not the GoF State pattern) | Single place to audit legal transitions; unlisted events are ignored with a log |
| Arbiter / hold set | `AampRialtoPlaybackController` | Replaces scattered "is it safe to play yet" flags with one bit set and one decision point |
| Claim (return value instead of callback under lock) | `AampRialtoNeedDataHandshake` | `haveData()` is a Rialto IPC call; it is made only after the handshake mutex is released |
| Snapshot via `shared_ptr` | `GetPipeline()`, `getSource()`, `GetDrmBridge()` | Lock held only to copy; objects destroyed outside the lock because destructors do Rialto IPC or wait for threads |
| Epoch / generation token | Handshake `generation`, pre-roll `m_epoch` and `m_preRollEpisode` | Lets late completions from an ended episode be recognised and dropped |
| Deferred async dispatch | `ScheduleOwned()` in `PrivateInstanceAAMPNotifiable.cpp` | Breaks the Rialto-lock / AAMP-lock cycle |
| Observer | `IAampRialtoPipelineEvents`, `IStreamSinkNotifiable` | Event delivery from Rialto to the player and from the player to core |

## Threading and locking summary

The class comment in `AampRialtoPlayer.h` lists the calling threads. The lock hierarchy documented there is:

- `m_flushCallMutex` (recursive) is outermost, then the state machine mutex or the playback controller's play lock.
- `m_attachMutex` is taken before the playback controller's play lock. `m_attachMutex` and `m_flushCallMutex` are never held together.
- Every other mutex (`m_sourcesMutex`, `m_drmBridgeMutex`, `m_pendingAttachMutex`, `m_pendingProtectionMutex`, `m_ptsCheckMutex`, `m_videoRectangleMutex`, `m_monitorMutex`, `m_id3HandlerMutex`, the session's `m_pipelineMutex`) is a leaf lock.
- `m_attachMutex` can be held for the length of a DRM license fetch, so the SEEK_DONE handler (`ApplySubtitleSegmentPosition`) can wait that long on the Rialto event thread, and `SetVideoMute` / `SetSubtitleMute` / `setCCMute` (which also take it) can block the caller for the same time.

## Related pages

- Direct Rialto - Overview (top page)
- Direct Rialto - Lifecycle and state machine
- Direct Rialto - Media data path
- Direct Rialto - Flush and seek
- Direct Rialto - Threading model and locking
- Direct Rialto - DRM
- Direct Rialto - Subtitles and TTML
- Direct Rialto - Monitoring and diagnostics
- Direct Rialto - Integration, configuration and build
- Direct Rialto - Testing

## Workarounds and improvement candidates

See the ARCH register (`_register/ARCH.md`). Rows relevant to this page: the size of `AampRialtoPlayer` (ARCH-02), the incomplete notification seam (ARCH-03), the blocking `m_attachMutex` (ARCH-04), stale comments (ARCH-01, ARCH-05, ARCH-06), public test hooks on the production class (ARCH-12), log-only Rialto notifications and the unused `OnPosition` (ARCH-14), the never-called `NotifySpeedChanged` (ARCH-15), the notification task leak (ARCH-17), the GLib main-context dependency of the timers (ARCH-22) and the test-side duplication of fakes (ARCH-36 to ARCH-38).

## Open questions

- `AampRialtoMediaPipelineClient` only logs `notifyNetworkState`, `notifyQos`, `notifyNativeSize`, `notifyVideoData`, `notifyAudioData`, `notifySourceFlushed`, `notifyPlaybackInfo` and `notifyFirstFrameReceived`; the player treats the first PLAYING state as "first frame". Whether Rialto's `notifyFirstFrameReceived` is meant to replace that is not documented in the code.
- `AampRialtoPlayer::OnPosition` is empty; position is polled with `getPosition()`. It is not stated whether `notifyPosition` is deliberately unused for latency or simply unneeded.
