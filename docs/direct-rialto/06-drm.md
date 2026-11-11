# Direct Rialto - DRM

## Purpose
Explains how encrypted content plays on the direct-rialto path: how AAMP's existing DRM framework (license manager, `DrmSessionManager`, DRM helpers) is reused unchanged, and how direct-rialto plugs in a Rialto-backed `DrmSession` so that keys live in the Rialto server instead of in a local OCDM/GStreamer decryptor. It covers the license flow, the threads and waits involved, how the resulting media key session ID (mksId) reaches every encrypted sample, teardown, and the callback safety machinery (`CallbackScope` and the per-thread `running` map).

## Source files
| File | Role |
|---|---|
| `direct-rialto/IDrmBridge.h` | Player-side interface: "create a DRM session for this PSSH and give me its mksId". Lets the player and sources be unit-tested without the DRM stack. |
| `direct-rialto/AampDrmBridge.{h,cpp}` | Production `IDrmBridge`. Calls `AampDRMLicenseManager::createDrmSession()` and returns `DrmSession::getMediaKeySessionId()`. |
| `direct-rialto/RialtoSessionCreator.{h,cpp}` | `makeRialtoSessionCreator()`: the `DrmSessionCreator` factory injected into `DrmSessionManager` so it builds Rialto sessions instead of `OCDMSessionAdapter`. |
| `direct-rialto/RialtoMediaKeySessionAdapter.{h,cpp}` | `DrmSession` implementation (the adaptor AAMP's license manager drives): `generateDRMSession`, `generateKeyRequest`, `processDRMKey`, `clearDecryptContext`. Turns Rialto's asynchronous callbacks into the blocking calls AAMP expects. |
| `direct-rialto/RialtoMediaKeySystem.{h,cpp}` | Owns one Rialto `IMediaKeys` per key system instance; creates key sessions; its inner `MediaKeysClient` receives Rialto callbacks and routes them per session (`RialtoSessionCallbacks`, `CallbackScope`, `running`). |
| `direct-rialto/RialtoMediaKeySession.{h,cpp}` | One Rialto key session: `update`, `close`, `destruct` (release), per-key status cache. |
| `direct-rialto/AampRialtoMediaSource.{h,cpp}` | Calls the bridge when a protection event is attached and stamps the mksId onto encrypted `MediaSegment`s. |
| `direct-rialto/AampRialtoPlayer.{h,cpp}` | Owns the `IDrmBridge` (`shared_ptr`), queues protection events, replaces the bridge on `SetEncryptedAamp()` / `ChangeAamp()`. |
| `priv_aamp.cpp` (AAMP core) | Installs the Rialto session creator in the `PrivateInstanceAAMP` constructor when `useDirectRialto` is set. |
| `drm/AampDRMLicManager.cpp`, `middleware-player-interface/drm/DrmSessionManager.cpp` (outside) | Unchanged AAMP DRM framework: slot selection, session reuse, license request, timeouts, error mapping. `DrmSessionManager` gained the `DrmSessionCreator` injection point for this component. |

## 1. How AAMP DRM reaches Rialto
[[diagram:direct-rialto-drm-class]]
*Two separate hooks into AAMP's DRM stack: `IDrmBridge` (the player asks the license manager for a session) and `makeRialtoSessionCreator()` (the session manager builds Rialto-backed `DrmSession` objects). Owned by page "Direct Rialto - Architecture and class model".*

direct-rialto does not have its own DRM framework. It reuses AAMP's (license manager, DRM helpers, session slots, license pre-fetcher, retries, error mapping) and changes only the *kind* of `DrmSession` that framework creates. Two independent hooks make this work:

- **Session side: the creator injection.** In the `PrivateInstanceAAMP` constructor (`priv_aamp.cpp`), if `useDirectRialto` is set, `makeRialtoSessionCreator()` is passed to `AampDRMLicenseManager`, which passes it to `DrmSessionManager` (`m_sessionCreator`). When `DrmSessionManager::getDrmSession()` needs a new session it calls this creator instead of `DrmSessionFactory::GetDrmSession()`. The creator builds a `RialtoMediaKeySystem` for `drmHelper->ocdmSystemId()` and wraps it in a `RialtoMediaKeySessionAdapter`. From then on the license manager drives the adapter exactly as it would drive `OCDMSessionAdapter`.
  - Why a creator function and not a new `DrmSessionFactory` branch: the factory is a static function in the middleware library, chosen at build time (`USE_OPENCDM_ADAPTER`). A per-player creator lets the GStreamer and direct-rialto paths coexist in one binary and lets tests inject a null creator. `RialtoSessionCreator.h` exposes only the `DrmSessionCreator` type so `priv_aamp.cpp` does not include Rialto headers.
  - Why only in the constructor: `useDirectRialto` is read once there. `AampConfig::DoCustomSetting` reverts attempts to change it later (stream, application or tune settings) because the creator, the CC manager and the stream sink have already been chosen.
- **Player side: `IDrmBridge`.** The Rialto `MediaSegment` needs the numeric key session ID (mksId) of the session that holds its keys. `AampRialtoMediaSource::attachOrUpdate()` asks `IDrmBridge::createSession(systemId, initData, len, type)` for it. `AampDrmBridge` calls `AampDRMLicenseManager::createDrmSession()` and returns `DrmSession::getMediaKeySessionId()`, which only the Rialto adapter implements (the base returns -1).
  - Why an interface: `AampRialtoPlayer` and the sources stay testable without the DRM stack (tests link `FakeAampDrmBridge`, see ARCH-38), and `priv_aamp.h` stays out of the player header.

How a protection event flows:

1. AAMP core learns about protection: DASH `StreamAbstractionAAMP_MPD::QueueContentProtection`, HLS `InitiateDrmProcess` (both through `AampDRMLicenseManager::QueueProtectionEvent`), or the MP4 demuxer finding a PSSH in an init segment (`AampMp4Demuxer` -> `PrivateInstanceAAMP::QueueProtectionEvent`). Each calls `StreamSink::QueueProtectionEvent(systemId, pssh, len, type)`.
2. `AampRialtoPlayer::QueueProtectionEvent` only stores a `ProtectionParams{systemId, initData, type}` in `m_pendingProtection[type]` (guarded by `m_pendingProtectionMutex`). Nothing blocks here.
3. In parallel the same DASH/HLS code queues the content protection to `AampLicensePreFetcher`, whose `PreFetchThread` calls `AampDRMLicenseManager::createDrmSession()` and usually performs the real license acquisition (section 3).
4. When the track's init segment is processed, `AampRialtoPlayer::AttachSource` takes a value copy of the matching params (`ProtectionFor`) and a snapshot of the bridge (`GetDrmBridge`), and calls `attachOrUpdate()`, which calls `IDrmBridge::createSession()`. Because the session for that key is usually already in a `DrmSessionManager` slot, this mostly returns the existing session's mksId.
5. The source stores the mksId in `m_mksId` (atomic) and stamps it on every encrypted segment (section 5).

`ProtectionFor` details: AAMP queues protection under its own track type, which differs from the source slot when audio is carried in the video slot, so the AAMP track's entry is preferred, then the source's own slot. HLS always queues under `eMEDIATYPE_VIDEO` even for audio-only keys, so an encrypted non-video source with no params falls back to the video entry (logged at WARN).

Bridge ownership (`AampRialtoPlayer`, `m_drmBridgeMutex`):

| Entry point | Calling thread | Effect |
|---|---|---|
| `GetDrmBridge()` | Injector or control thread inside `AttachSource` | Lazily creates `AampDrmBridge(Aamp())` and returns a `shared_ptr` snapshot, so the bridge can be replaced while an attach still uses the old one |
| `SetEncryptedAamp(aamp)` | Called by `AampStreamSinkManager::SetEncryptedHeaders` | Pre-roll ads: the clear ad player gets a bridge bound to the *encrypted* main player's `PrivateInstanceAAMP`, so sessions are created in that player's license manager. Sets `m_drmBridgeIsExplicit`. Null reverts to the lazy default |
| `ChangeAamp(newAamp)` | Single-pipeline player switch | Drops the bridge unless it was installed by `SetEncryptedAamp()` (that one targets another instance on purpose) |
| `QueueProtectionEvent` / `ClearProtectionEvent` | Fragment collector / demuxer thread; stream abstraction `Init` | Store or clear `m_pendingProtection` under `m_pendingProtectionMutex`. `Configure()` deliberately does not clear it because AAMP queues protection before `Configure()` |

## 2. Key systems and session types
**Key systems.** The adapter does not choose a key system. AAMP's `DrmHelperEngine` picks a `DrmHelper` from the system UUID (and the stream's media format), and the creator passes `drmHelper->ocdmSystemId()` to `IMediaKeysFactory::createMediaKeys()`. The strings that can reach Rialto are:

| DRM | System UUID | Key system string | Notes |
|---|---|---|---|
| Widevine | `edef8ba9-...` | `com.widevine.alpha` | Normal challenge/response |
| PlayReady | `9a04f079-...` | `com.microsoft.playready` | Normal challenge/response; `DrmSessionManager` calls `setOutputProtection(true)` when `enablePROutputProtection` is set (no effect on this adapter) |
| ClearKey | `1077efec-...` | `org.w3.clearkey` | On the GStreamer path this is an in-process `ClearKeySession`; here it becomes a Rialto `IMediaKeys` session |
| Verimatrix | `9a27dd82-...` | `com.verimatrix.ott` | External license: no challenge is requested; the license manager passes a helper-built response (`transformLicenseResponse`) to `processDRMKey`. For other external-license helpers the response is null and `processDRMKey` only waits for key statuses |

Whether a key system actually works depends on the Rialto server build: if `createMediaKeys()` returns null, `RialtoMediaKeySystem::isValid()` is false, the creator returns null and `DrmSessionManager` reports `MW_DRM_INIT_FAILED`. When a stream offers several DRMs, the choice is made by AAMP's normal helper selection in the stream abstraction, unchanged by this component.

`AampDrmBridge` maps the manifest type before calling the license manager: helpers choose behaviour by `MediaFormat`, and with the MP4 demuxer HLS is never promoted to `HLS_MP4`, while no helper accepts plain `HLS`. Sessions here are always created from ISO BMFF PSSH, so `eMEDIAFORMAT_HLS` is passed as `eMEDIAFORMAT_HLS_MP4`.

**Session types.**

- Every Rialto key session is `KeySessionType::TEMPORARY` (`RialtoMediaKeySystem::createSession`). Persistent licenses and offline playback are not supported.
- The init data type is always `"cenc"` because the adapter hard-codes it in `generateDRMSession()`. `RialtoMediaKeySystem::createSession` can map `"webm"` and `"keyids"`, but nothing passes them.
- There is **one `IMediaKeys` per `DrmSession`**. Each adapter owns its own `RialtoMediaKeySystem`, so each session has its own Rialto `IMediaKeys` object and its own Rialto client event thread (`rialto-media-keys-events`). This mirrors one OCDM system per `OCDMSessionAdapter` and keeps sessions independent, but costs one IPC object and thread per session.
- The number of concurrent sessions is AAMP's `maxDASHDRMSessions` slots in `DrmSessionManager`. A session is reused for the same key ID, and the oldest slot is evicted (deleted) for a new key ID. This is unchanged AAMP behaviour.

## 3. License acquisition flow
[[diagram:direct-rialto-drm-license-sequence]]
*Typical flow: the license pre-fetcher thread performs the acquisition; the injector thread's `IDrmBridge::createSession()` then finds the ready session in its slot and only reads its mksId.*

**Steps** (all inside `DrmSessionManager::createDrmSession`, which holds `mDrmSessionLock` for the whole call):

1. **Slot lookup** (`getDrmSession`). If a slot already holds a session for the same key ID in `KEY_READY`, it is returned at once. If that session is still `KEY_INIT`/`KEY_PENDING`, the caller waits on `RialtoMediaKeySessionAdapter::waitForState(KEY_READY, keyProcessTimeout)`. Otherwise the oldest slot's session is deleted and the injected creator builds a new adapter.
2. **Create the Rialto session** (`initializeDrmSession` -> `generateDRMSession`, under the slot's `sessionMutex` and the adapter's `m_mutex`). The adapter wires four lambdas into a `RialtoSessionCallbacks` and calls `RialtoMediaKeySystem::createSession`, which does `IMediaKeys::createKeySession(TEMPORARY)`, registers the callbacks and the session pointer in `MediaKeysClient` **before** calling `generateRequest(CENC, initData)`. The order matters: Rialto delivers `onLicenseRequest` on its own event thread and it can arrive before `generateRequest` returns; registering first means it is never dropped as "no handler". Failure leaves the adapter in `KEY_ERROR` (no valid key system) or `KEY_ERROR_SESSION_CREATE_FAILED`; the manager maps that to `MW_DRM_SESSION_CREATE_FAILED` or `MW_DRM_DATA_BIND_FAILED`.
3. **Challenge** (`AampDRMLicenseManager::acquireLicense` -> `generateKeyRequest(url, licenseGenerateTimeout)`, slot `sessionMutex` held). The adapter first sets `KEY_ERROR`, then waits on `m_challengeReady` until `onChallenge` has stored the challenge. On success it strips an optional `<type>:Type:` prefix (same format as OCDM), returns the challenge and the server-suggested URL, and sets `KEY_PENDING`. Challenges whose type is `individualization-request` or `3` are not license requests: they are forwarded to `DrmCallbacks::Individualization` (AAMP raises a DRM message event for the application) and the wait continues for the real challenge.
4. **License request.** Unchanged AAMP code: access token, license URL and method chosen by `DrmHelper::generateLicenseRequest` (configured server URL or the one returned with the challenge), HTTP request via `AampCurlDownloader`, retry on curl timeout/connect errors, abort on `setLicenseRequestAbort`.
5. **Apply the license** (`processDRMKey(response, keyProcessTimeout)`). The adapter resets `m_keyStatusReceived`, calls `RialtoMediaKeySession::update()` (`IMediaKeys::updateSession`), then waits on `m_keyStatusReady` for the key statuses that this update produces (section 4). It returns 0 and sets `KEY_READY` if any reported key is `USABLE`; returns `HDCP_OUTPUT_PROTECTION_FAILURE` with `KEY_ERROR` if a key is `OUTPUT_RESTRICTED` (checked in the order keys were reported); otherwise returns -1 with `KEY_ERROR`. A null response (external license helpers) skips `update()` and only waits.

**Which thread, what blocks, for how long.**

| Thread | Blocks on | Bound (default config) |
|---|---|---|
| `AampLicensePreFetcher::PreFetchThread` (normal acquirer for DASH and HLS) | `mLicenseAcquisitionMutex`, `mDrmSessionLock`, slot `sessionMutex`, challenge wait, HTTP, key-status wait | Challenge 5 s (`DrmHelper` `TIMEOUT_SECONDS` = 5000 ms; no helper overrides it) + HTTP 5 s per attempt (`drmNetworkTimeout`) x up to 2 attempts (`MAX_LICENSE_REQUEST_ATTEMPTS`) + key status 5 s: roughly 20 s worst case |
| Track injector thread in `AampRialtoPlayer::AttachSource` -> `IDrmBridge::createSession` (from `SendTransfer` of an init fragment or `SetStreamCaps`) | `mDrmSessionLock` held by the pre-fetcher, then `waitForState(KEY_READY)` up to 5 s; or the full flow above if no matching session exists yet | Up to the pre-fetcher's remaining time plus 5 s, or the full ~20 s. `m_attachMutex` is held throughout (see ARCH-04 for the effect on mute calls) |
| `AampDRMLicenseManager::licenseRenewalThread` (one per slot) | Same as the pre-fetcher for a renewal | Same |
| `rialto-media-keys-events` (one per `IMediaKeys`) | `MediaKeysClient::mutex` briefly, then the adapter's `m_eventMutex`/`m_usableKeysMutex` | Short; never waits on the network |

Why `createSession` blocks: `IDrmBridge` returns only a number, and the source must know whether it has a session before it creates the Rialto source and before it stamps segments. Blocking the injector thread is acceptable because that track cannot inject data the server can decrypt without keys anyway; other tracks are only blocked if they need `m_attachMutex` or `mDrmSessionLock` at the same time.

Why the adapter converts callbacks into waits: AAMP's `DrmSession` contract is synchronous (`generateKeyRequest` returns the challenge, `processDRMKey` returns a result), while Rialto, like OCDM, is event-driven. The condition variables `m_challengeReady`, `m_keyStatusReady` and `m_stateChanged` all use `m_eventMutex`; `setState()` stores the state under that mutex so `waitForState()` cannot miss a change between testing and sleeping.

**License renewal.** Rialto's `onLicenseRenewal` is routed to `DrmCallbacks::LicenseRenewal`, which starts `AampDRMLicenseManager::licenseRenewalThread` for the session's slot; that thread runs `acquireLicense(isLicenseRenewal=true)`. The renewal message from Rialto is not stored by the adapter (see section 9 and DRM-01).

## 4. Key status updates
Rialto reports key statuses with `IMediaKeysClient::onKeyStatusesChanged(keySessionId, [(keyId, status)...])` on the `rialto-media-keys-events` thread. `MediaKeysClient::onKeyStatusesChanged` handles one batch in two phases:

1. **Under `MediaKeysClient::mutex`**: copy the session's `onKeyUpdate`/`onKeysUpdated` callbacks, open a `CallbackScope` (section 8), and write every `(keyId, status)` into the session's cache via `RialtoMediaKeySession::updateKeyStatus` (`m_keyStatusMutex`). The cache is updated first so that when the waiter wakes it sees the new statuses.
2. **Without the client mutex**: call `onKeyUpdate(keyId)` for every key in the batch, then `onKeysUpdated()` once. After each `onKeyUpdate`, and before `onKeysUpdated`, the client checks that the session is still registered and stops dispatching the batch if it is not. A callback may deregister its own session, and its owner may then be freed as soon as the callback returns (section 8; commit `740f04e4`).

In the adapter:

- `onKeyUpdate` appends the key ID to `m_usableKeys` (`m_usableKeysMutex`) if not already present. Like `OCDMSessionAdapter`, it does this for every reported key regardless of status; the status itself is held in the session cache.
- `onKeysUpdated` sets `m_keyStatusReceived` and wakes `processDRMKey`.
- `processDRMKey` then walks `m_usableKeys` and asks the session cache `isKeyUsable` / `isKeyOutputRestricted`.
- `getUsableKeys()` returns `m_usableKeys`; `DrmSessionManager::ValidateMultiKeySlot` uses it to check multi-key slots.

Status values: only `USABLE` makes a session ready and only `OUTPUT_RESTRICTED` gets a specific error (HDCP). `EXPIRED`, `RELEASED`, `PENDING` and `INTERNAL_ERROR` are cached but have no specific handling. Status changes **after** `KEY_READY` (for example a key expiring during playback) are cached but nothing reads them until the next `processDRMKey`; decryption failures then surface from the Rialto pipeline as `PlaybackError::DECRYPTION` (section 7).

Lock order on this path: `MediaKeysClient::mutex` -> `RialtoMediaKeySession::m_keyStatusMutex`; on the license thread `m_usableKeysMutex` -> `m_keyStatusMutex`. The adapter's callbacks run without the client mutex, so there is no cycle.

## 5. Linking sessions to segments (mksId)
The mksId is the Rialto key session ID returned by `IMediaKeys::createKeySession()`. The Rialto server generates it from a process-wide counter starting at 0, so it is unique across all `IMediaKeys` objects in that server; `-1` means "no session" throughout direct-rialto. (Logs therefore show `mksId=0` for the first session after server start.)

- `RialtoMediaKeySessionAdapter::getMediaKeySessionId()` returns it while a session exists, and -1 after `clearDecryptContext()`.
- `AampDrmBridge::createSession` reads it from the `DrmSession` returned by the license manager.
- `AampRialtoMediaSource::attachOrUpdate` stores it in `m_mksId` (atomic, release/acquire):
  - **First attach**: create the session if `ProtectionParams` exist, then attach the Rialto source. Video and audio sources are always attached with `hasDrm = true` (see DATA-04), so a session that arrives later can still be used.
  - **Already attached**: a new session is requested only when the params differ from `m_activeProtection` (new period with a different PSSH or DRM system). Params are re-queued every period, so comparing avoids a redundant license round trip. On failure the previous mksId is kept.
  - `reset()` sets the mksId back to -1 and clears `m_activeProtection`.
- Every encrypted sample is stamped with `MediaSegment::setMediaKeySessionId(mksId)` together with key ID, IV, cipher mode and sub-samples. An encrypted sample with mksId -1 is dropped. The per-sample rules are in "Direct Rialto - Media data path", section 12 "Encrypted samples".
- In the Rialto server the mksId on each segment selects the key session used to decrypt that sample. The client never sends key material; it only names the session.

Several sources can share an mksId: if audio and video use the same key ID, `DrmSessionManager` returns the same slot and therefore the same Rialto session.

## 6. Session close, cleanup and teardown ordering
[[diagram:direct-rialto-drm-session-state]]
*`KeyState` of the adapter as AAMP's DRM framework drives it, including renewal and clean-up.*

**Who ends a session.** direct-rialto never deletes a DRM session itself. Sessions belong to AAMP's `DrmSessionManager` slots, and the manager deletes the `DrmSession` (the adapter) when:

- a slot is reused for a new key ID (the oldest slot is evicted when all slots are in use),
- `DrmSessionManager::clearDrmSession()` runs on tune or stop (subject to AAMP's session-reuse configuration),
- `DrmSessionManager::clearSessionData()` or the manager's destructor runs.

**What deleting an adapter does.** `~RialtoMediaKeySessionAdapter` calls `clearDecryptContext()`, which under the adapter's `m_mutex`:

1. moves the `RialtoMediaKeySession` out of `m_session` (so `getMediaKeySessionId()` returns -1 from now on);
2. `RialtoMediaKeySession::close()` calls `IMediaKeys::closeKeySession(id)`;
3. `RialtoMediaKeySession::destruct()` calls `IMediaKeys::releaseKeySession(id)` and then `deregisterSession(id)` (which waits for in-flight callbacks, section 8);
4. clears the usable-key list and the "challenge received" / "key status received" flags, and sets `KEY_INIT`.

Failures of `closeKeySession`/`releaseKeySession` are logged at ERROR and otherwise ignored. If `destruct()` was never called, `~RialtoMediaKeySession` still deregisters, so the client maps never keep a dangling session pointer. The `RialtoMediaKeySystem` (and its `IMediaKeys`, which owns the `rialto-media-keys-events` thread) is destroyed with the adapter, after the session.

**The player side.** Player teardown does not touch DRM sessions:

- `Stop()` releases the pipeline but leaves `m_pendingProtection` and the bridge alone, because AAMP queues protection before the next `Configure()`.
- A source's `reset()` (new session in `Configure()`) sets its mksId to -1 and clears `m_activeProtection`, so the next attach asks the bridge again and gets the mksId of whatever session the manager now has.
- `ClearProtectionEvent()` (called from the stream abstraction's `Init`) drops queued protection for a new tune.

**Ordering consequence.** The DRM session lifetime (owned by AAMP) and the pipeline lifetime (owned by the player) are independent. In normal tune/stop sequences AAMP stops the sink before clearing DRM sessions. But slot eviction can release a Rialto key session whose mksId a running source still stamps on its samples. Those samples then fail to decrypt in the server (see Open questions).

## 7. Error handling
| Failure | Where detected | Result in AAMP |
|---|---|---|
| `IMediaKeysFactory` missing or `createMediaKeys(keySystem)` fails | `RialtoMediaKeySystem` constructor (logs ERROR) | `generateDRMSession` finds no valid system and sets `KEY_ERROR`; the manager reports `MW_DRM_SESSION_CREATE_FAILED`, which AAMP turns into a DRM tune failure |
| `createKeySession` fails, or `generateRequest` fails | `RialtoMediaKeySystem::createSession` (the session is deregistered and released on `generateRequest` failure) | `KEY_ERROR_SESSION_CREATE_FAILED`; the manager reports a data-bind/session-create failure |
| No challenge within `licenseGenerateTimeout` (5 s), or empty challenge | `generateKeyRequest` | Returns null with the state left at `KEY_ERROR`; AAMP reports a challenge/licence-generation failure (for example `AAMP_TUNE_DRM_CHALLENGE_FAILED`) |
| Licence HTTP failure | Unchanged AAMP code | AAMP's usual licence error mapping and retries |
| `updateSession` fails | `processDRMKey` | -1 and `KEY_ERROR` |
| No key statuses within `keyProcessTimeout` | `processDRMKey` | -1 and `KEY_ERROR` (logged "timed out waiting for key status") |
| A key is `OUTPUT_RESTRICTED` | `processDRMKey` | `HDCP_OUTPUT_PROTECTION_FAILURE` and `KEY_ERROR`; AAMP's HDCP handling |
| No `USABLE` key | `processDRMKey` | -1 and `KEY_ERROR` ("key not usable") |
| Encrypted sample with mksId -1 or a bad sub-sample table | `AampRialtoMediaSource` (Media data path, section 12) | Sample dropped and logged (ERROR on the 1st and every 100th) |
| Decryption fails in the server | Rialto `notifyPlaybackError(DECRYPTION)` -> `AampRialtoPlayer::OnPlaybackError` | `NotifyPlaybackError(AAMP_TUNE_DRM_DECRYPT_FAILED, retry=true)` |
| HDCP recovered | Rialto `notifyPlaybackError(OUTPUT_PROTECTION)` | Not an error: `NotifyOutputProtectionRecovered()` schedules a retune to resynchronise A/V, mirroring the GStreamer HDCP path |

Rialto's `PlaybackError` carries no message text, so only these coarse categories can be mapped, unlike the GStreamer path, which matches error strings.

## 8. Callback reentrancy and destruction during callbacks
[[diagram:direct-rialto-drm-callback-reentrancy-sequence]]
*How `CallbackScope` and `deregisterSession` keep an adapter alive until every callback running on another thread has returned.*

**The problem.** Rialto calls `IMediaKeysClient` methods on the `rialto-media-keys-events` thread. The callbacks capture a raw `this` pointer to the adapter. Meanwhile AAMP can delete the adapter on another thread (slot eviction, stop). Without coordination, a callback could run on a freed adapter. Holding the client mutex while calling the callbacks is not an option either: callbacks take the adapter's own mutexes, and teardown holds those while it deregisters, so lock-order inversion and deadlock would follow.

**The mechanism.** `RialtoMediaKeySystem::MediaKeysClient` keeps, per key session ID:

- `callbacks` (the `RialtoSessionCallbacks` to call) and `sessions` (the `RialtoMediaKeySession` whose key-status cache to update);
- `running`: the thread IDs currently executing one of that session's callbacks, plus a `callbackDone` condition variable.

Each `on...` handler, under `mutex`, copies the callbacks it needs and constructs a `CallbackScope`, which adds the current thread ID to `running[id]`. It releases the mutex, runs the callbacks, and the scope's destructor (taking the mutex again) removes the thread ID and calls `notify_all`.

`deregisterSession(id)` erases the callbacks and session pointer under the mutex, then waits on `callbackDone` until `running[id]` contains no thread **other than the caller**. When it returns, no other thread can be inside a callback for that session and no new callback can start (the entry has been erased), so the adapter may be freed.

**Why the caller is excluded.** A callback may itself lead to the session being torn down on the same thread (for example a licence error path). Waiting for "no thread at all" would then wait for itself forever. Using thread IDs, rather than a counter, is what makes this distinction possible. The flip side is that `deregisterSession` returns immediately in that case, so the session's owner may be freed as soon as the callback returns. `onKeyStatusesChanged` therefore re-checks that the session is still registered after each `onKeyUpdate` and before `onKeysUpdated`, and stops dispatching the rest of the batch if it is not (commit `740f04e4`).

**Why the session is registered before `generateRequest`.** `onLicenseRequest` can arrive on the event thread before `generateRequest` returns. `createSession` therefore registers the callbacks and session pointer first, so the first challenge is never dropped as "no handler".

**Other safety details.**

- `deregister` in `RialtoMediaKeySession` holds a `weak_ptr` to the client, so a session that outlives its `RialtoMediaKeySystem` deregisters safely (it becomes a no-op).
- `m_eKeyState` is atomic and `setState` writes it under `m_eventMutex`, so `waitForState` cannot miss a transition between testing and sleeping.
- The `deregisterSession` wait has no timeout. A callback that blocks (for example on an adapter mutex held by the tearing-down thread) would hang teardown. Today no callback takes `m_mutex`, which `clearDecryptContext` holds; they take only `m_eventMutex` and `m_usableKeysMutex` (DRM-03).

## 9. Unsupported features
| Feature | Status | Notes |
|---|---|---|
| Licence renewal with the renewal message | **Not supported correctly** (DRM-01) | `onLicenseRenewal` discards the message, and `m_challengeReceived` stays true from the first challenge, so `generateKeyRequest` during renewal immediately returns the **original** challenge |
| Persistent licences / offline playback | Not supported | Sessions are always `KeySessionType::TEMPORARY` |
| Init data types other than CENC | Not used | `RialtoMediaKeySystem::createSession` can map `webm` and `keyids`, but the adapter always passes `"cenc"` (DRM-02) |
| Client-side decryption (`DrmSession::decrypt`) | Not supported, by design | The adapter does not override `decrypt`; the Rialto server decrypts using the mksId on each segment |
| Reacting to key status changes during playback | Not supported | Statuses after `KEY_READY` are cached only; failures surface as `PlaybackError::DECRYPTION` (DRM-04) |
| Individualisation | Forwarded | Challenges of type `individualization-request`/`3` go to `DrmCallbacks::Individualization` (AAMP raises a DRM message event) |
| Key systems | Whatever `DrmHelperEngine` selects | The key-system string is `drmHelper->ocdmSystemId()`; support depends on the Rialto server's CDMs, not on direct-rialto |
| Secure stop, metrics (`getDRMStatus`-style queries) | Not implemented | No equivalent calls on the adapter |

## Open questions
- **Slot eviction while a source still uses the mksId.** If `DrmSessionManager` evicts a slot whose session a running source still stamps on its segments, the server receives samples for a released key session. Does AAMP avoid evicting in-use slots, and how does the OCDM path handle the same case?
- **Renewal on real devices.** Has licence renewal (DRM-01) been tested with a server that requires the renewal message? Does Rialto also send a fresh `onLicenseRequest` for renewals, which would mask the issue?
- **Release order.** Does the Rialto server require `releaseKeySession` to happen after the media pipeline has stopped using that session, or does it handle it either way?
- **Key system support.** Which key systems are validated with direct-rialto (PlayReady, Widevine, ClearKey)? The code is agnostic.

## Related pages
- Direct Rialto - Architecture and class model
- Direct Rialto - Media data path
- Direct Rialto - Threading model and locking
- Direct Rialto - Integration, configuration and build
- Direct Rialto - Testing
- Direct Rialto - Workarounds and improvement register

## Workarounds and improvement candidates
Rows are in `_register/DRM.md`. Related rows from other pages: ARCH-04 and THR-04 (`m_attachMutex` held across a blocking licence fetch), ARCH-44 (`GetEncryptedAampId` not overridden), DATA-04 (`hasDrm` forced true), ARCH-41 (the simulator has no DRM).

- DRM-01: licence renewal reuses the original challenge, because the renewal message is discarded.
- DRM-02: only CENC init data is ever passed; the `webm`/`keyids` mapping is dead code.
- DRM-03: `deregisterSession` waits without a timeout for callbacks on other threads.
- DRM-04: key status changes after `KEY_READY` are not acted on.
- DRM-05: the injector thread can block for about 20 s in `createSession` while holding `m_attachMutex`.
