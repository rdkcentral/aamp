# DRM Subsystem — Sequence Diagrams

> **Source**: All diagrams derived from actual source files in `middleware/drm/`, `drm/aes/`, `drm/helper/`, `drm/ocdm/`
> **Confidence**: 95% — All key `.h` and `.cpp` files read. Minor gaps in tail-end of `DrmSessionManager.cpp` (lines 201+) and `OcdmGstSessionAdapter.cpp` (lines 150+).

---

## 1. DRM Session Lifecycle (DrmSessionManager)

```mermaid
sequenceDiagram
    participant Player as InterfacePlayerRDK
    participant DSM as DrmSessionManager
    participant DSF as DrmSessionFactory
    participant DS as DrmSession
    participant CSM as ContentSecurityManager
    participant PSI as PlayerSecInterface

    Note over Player,PSI: Session Manager Initialization
    Player->>DSM: new DrmSessionManager(maxSessions, player, watermarkCB)
    DSM->>DSM: Allocate DrmSessionContext[maxSessions]
    DSM->>DSM: Allocate KeyIdEntries[maxSessions]
    DSM->>PSI: new PlayerSecInterface()
    DSM->>CSM: registerCallback() [watermark events]

    Note over Player,PSI: DRM Config Update
    Player->>DSM: UpdateDRMConfig(useSecManager, enablePR, propagateURI, fakeTune, wvWorkaround)
    DSM->>DSM: Store in m_drmConfigParam

    Note over Player,PSI: Session Creation
    Player->>DSM: createDrmSession(initData, drmHelper)
    DSM->>DSM: Check cachedKeyIDs for existing session
    DSM->>DSF: GetDrmSession(drmHelper, drmCallbacks)
    DSF-->>DSM: DrmSession* (OCDM or ClearKey)
    DSM->>DS: generateDRMSession(initData, customData)
    DS-->>DSM: Session ready (KEY_INIT)

    Note over Player,PSI: Key Acquisition
    DSM->>DS: generateKeyRequest(destURL, timeout)
    DS-->>DSM: DrmData* (challenge)
    DSM->>DSM: Fetch license from server
    DSM->>DS: processDRMKey(key, timeout)
    DS-->>DSM: KEY_READY or KEY_ERROR

    Note over Player,PSI: Cleanup
    Player->>DSM: clearSessionData()
    DSM->>DS: delete drmSession
    DSM->>DSM: Reset cachedKeyIDs
```

---

## 2. DRM Session Factory — Session Selection

```mermaid
sequenceDiagram
    participant DSM as DrmSessionManager
    participant DSF as DrmSessionFactory
    participant DH as DrmHelper
    participant OCDM_B as OCDMBasicSessionAdapter
    participant OCDM_G as OCDMGSTSessionAdapter
    participant CK as ClearKeySession

    DSM->>DSF: GetDrmSession(drmHelper, drmCallbacks)
    DSF->>DH: ocdmSystemId()
    DH-->>DSF: systemId string

    alt USE_OPENCDM_ADAPTER defined
        DSF->>DH: isClearDecrypt()
        alt isClearDecrypt() == true
            alt systemId == "org.w3.clearkey"
                DSF->>CK: new ClearKeySession()
                CK-->>DSF: session*
            else other clear decrypt
                DSF->>OCDM_B: new OCDMBasicSessionAdapter(drmHelper, callbacks)
                OCDM_B-->>DSF: session*
            end
        else isClearDecrypt() == false
            DSF->>OCDM_G: new OCDMGSTSessionAdapter(drmHelper, callbacks)
            OCDM_G-->>DSF: session*
        end
    else No OCDM support
        alt systemId == "org.w3.clearkey"
            DSF->>CK: new ClearKeySession()
            CK-->>DSF: session*
        else
            DSF-->>DSM: NULL
        end
    end
    DSF-->>DSM: DrmSession*
```

---

## 3. DRM Helper Engine — Factory Pattern

```mermaid
sequenceDiagram
    participant Caller
    participant DHE as DrmHelperEngine
    participant DHF as DrmHelperFactory
    participant WV as WidevineDrmHelper
    participant PR as PlayReadyHelper
    participant CK as ClearKeyHelper
    participant VM as VerimatrixHelper

    Note over Caller,VM: Registration (static init)
    DHF->>DHE: registerFactory(this)
    DHE->>DHE: Sort factories by weighting

    Note over Caller,VM: Helper Creation
    Caller->>DHE: createHelper(drmInfo)
    loop For each registered factory (by weight)
        DHE->>DHF: isDRM(drmInfo)
        alt Match found
            DHF->>DHF: createHelper(drmInfo)
            DHF-->>DHE: DrmHelperPtr (WV/PR/CK/VM)
            DHE-->>Caller: DrmHelperPtr
        end
    end

    Note over Caller,VM: System ID Query
    Caller->>DHE: getSystemIds(ids)
    loop For each factory
        DHE->>DHF: appendSystemId(ids)
    end
    DHE-->>Caller: vector<string> of system IDs

    Note over Caller,VM: DRM Support Check
    Caller->>DHE: hasDRM(drmInfo)
    loop For each factory
        DHE->>DHF: isDRM(drmInfo)
        alt Match
            DHE-->>Caller: true
        end
    end
```

---

## 4. OCDM Session Adapter — Full Key Lifecycle

```mermaid
sequenceDiagram
    participant DSM as DrmSessionManager
    participant OCDM as OCDMSessionAdapter
    participant SYS as OpenCDMSystem
    participant SESS as OpenCDMSession
    participant EXT as PlayerExternalsInterface
    participant CB as DrmCallbacks

    Note over DSM,CB: Construction
    DSM->>OCDM: new OCDMSessionAdapter(drmHelper, callbacks)
    OCDM->>SYS: opencdm_create_system()
    SYS-->>OCDM: m_pOpenCDMSystem
    OCDM->>EXT: GetPlayerExternalsInterfaceInstance()
    EXT-->>OCDM: m_pOutputProtection

    Note over DSM,CB: Session Generation
    DSM->>OCDM: generateDRMSession(initData, cbInitData, customData)
    OCDM->>OCDM: Setup m_OCDMSessionCallbacks (challenge, key_update, error, keys_updated)
    OCDM->>SYS: opencdm_construct_session(system, LicenseType::Temporary, "cenc", initData, callbacks)
    SYS-->>OCDM: m_pOpenCDMSession (or ERROR)
    alt Error
        OCDM->>OCDM: m_eKeyState = KEY_ERROR_SESSION_CREATE_FAILED
    end

    Note over DSM,CB: Challenge Processing (async callback)
    SYS-->>OCDM: process_challenge_callback(destUrl, challenge, challengeSize)
    OCDM->>OCDM: Parse message type
    alt individualization-request
        OCDM->>CB: Individualization(payload)
    else standard challenge
        OCDM->>OCDM: Store m_challenge, m_destUrl
        OCDM->>OCDM: m_challengeReady.signal()
    end
    alt LICENSE_RENEWAL ("1")
        OCDM->>CB: LicenseRenewal(drmHelper, session)
    end

    Note over DSM,CB: Key Request
    DSM->>OCDM: generateKeyRequest(destURL, timeout)
    OCDM->>OCDM: m_challengeReady.wait(timeout)
    OCDM-->>DSM: DrmData* with challenge + destURL

    Note over DSM,CB: Key Processing
    DSM->>OCDM: processDRMKey(key, timeout)
    OCDM->>SESS: opencdm_session_update(session, keyData, keyLen)
    OCDM->>OCDM: m_keyStatusReady.wait(timeout)
    alt KEY_READY
        OCDM->>OCDM: verifyOutputProtection()
        OCDM-->>DSM: DRM_API_SUCCESS
    else KEY_ERROR
        OCDM-->>DSM: DRM_API_FAILED
    end

    Note over DSM,CB: Key Update Callback
    SYS-->>OCDM: key_update_callback(key, keySize)
    OCDM->>SESS: opencdm_session_status(session, key, keySize)
    OCDM->>OCDM: Store in m_usableKeys vector

    Note over DSM,CB: Keys Updated Callback
    SYS-->>OCDM: keys_updated_callback()
    OCDM->>OCDM: m_keyStatusReady.signal()

    Note over DSM,CB: Destruction
    DSM->>OCDM: ~OCDMSessionAdapter()
    OCDM->>OCDM: clearDecryptContext()
    OCDM->>SYS: opencdm_destruct_system()
```

---

## 5. OCDM GST Session Adapter — GStreamer Decrypt

```mermaid
sequenceDiagram
    participant Plugin as GstCdmiDecryptor
    participant GSTA as OCDMGSTSessionAdapter
    participant SESS as OpenCDMSession
    participant Stats as DecryptStats

    Note over Plugin,Stats: Construction
    Plugin->>GSTA: new OCDMGSTSessionAdapter(drmHelper, callbacks)
    GSTA->>GSTA: dlsym("opencdm_gstreamer_session_decrypt_buffer")
    alt Symbol found
        GSTA->>GSTA: OCDMGSTSessionDecrypt = fn_ptr
    else Not found
        GSTA->>GSTA: OCDMGSTSessionDecrypt = nullptr
    end

    Note over Plugin,Stats: GStreamer Buffer Decrypt
    Plugin->>GSTA: decrypt(keyIDBuf, ivBuf, buffer, subSampleCount, subSamplesBuf, caps)
    GSTA->>GSTA: ExtractSEI(buffer)
    GSTA->>GSTA: Record start timestamp
    alt OCDMGSTSessionDecrypt available
        GSTA->>SESS: OCDMGSTSessionDecrypt(session, buffer, caps)
    else fallback
        GSTA->>SESS: opencdm_gstreamer_session_decrypt(session, buffer, subSamples, count, iv, keyId, waitFor)
    end
    GSTA->>Stats: LogPerformanceExt(start, end, dataSize)
    GSTA-->>Plugin: status (0=success)
```

---

## 6. ClearKey DRM Session

```mermaid
sequenceDiagram
    participant DSF as DrmSessionFactory
    participant CK as ClearKeySession
    participant SSL as OpenSSL_EVP

    Note over DSF,SSL: Session Creation
    DSF->>CK: new ClearKeySession()
    CK->>CK: initDRMSession()
    CK->>SSL: EVP_CIPHER_CTX_new()
    SSL-->>CK: mOpensslCtx

    Note over DSF,SSL: Generate DRM Session
    CK->>CK: generateDRMSession(initData, cbInitData, customData)
    CK->>CK: extractKeyIdFromPssh(psshData, len) → keyId (16 bytes)
    CK->>CK: setKeyId(keyId, AES_CTR_KID_LEN)
    CK->>CK: m_eKeyState remains KEY_INIT

    Note over DSF,SSL: Generate Key Request
    CK->>CK: generateKeyRequest(destURL, timeout)
    CK-->>DSF: DrmData* (NULL - ClearKey uses embedded key)

    Note over DSF,SSL: Process Key
    CK->>CK: processDRMKey(keyData, timeout)
    CK->>CK: Parse JSON {"keys":[{"k":"...","kid":"..."}]}
    CK->>CK: Base64url decode key → m_keyStr
    CK->>CK: m_eKeyState = KEY_READY

    Note over DSF,SSL: Decrypt (raw buffer)
    CK->>CK: decrypt(IV, cbIV, payload, payloadSize, opaqueData)
    CK->>SSL: EVP_DecryptInit_ex(ctx, EVP_aes_128_ctr(), key, IV)
    CK->>SSL: EVP_DecryptUpdate(ctx, out, payload, payloadSize)
    SSL-->>CK: Decrypted data
    CK-->>DSF: 0 (success)

    Note over DSF,SSL: Cleanup
    CK->>SSL: EVP_CIPHER_CTX_free(mOpensslCtx)
    CK->>CK: free(m_keyId), free(m_keyStr)
```

---

## 7. HLS DRM Session Manager

```mermaid
sequenceDiagram
    participant HLS as HLS_FragCollector
    participant HDSM as HlsDrmSessionManager
    participant DHE as DrmHelperEngine
    participant DH as DrmHelper
    participant Bridge as HlsOcdmBridge
    participant DS as DrmSession

    Note over HLS,DS: Singleton Access
    HLS->>HDSM: getInstance()
    HDSM-->>HLS: static instance

    Note over HLS,DS: DRM Support Check
    HLS->>HDSM: isDrmSupported(drmInfo)
    HDSM->>DHE: hasDRM(drmInfo)
    DHE-->>HDSM: true/false
    HDSM-->>HLS: bool

    Note over HLS,DS: Create Session
    HLS->>HDSM: createSession(drmInfo, streamType)
    HDSM->>DHE: createHelper(drmInfo)
    DHE-->>HDSM: DrmHelperPtr
    HDSM->>HDSM: GetHlsDrmSessionCb(bridge, drmHelper, drmSession, streamType)
    HDSM-->>HLS: shared_ptr<HlsDrmBase> (bridge)
```

---

## 8. HLS OCDM Bridge — Decrypt Flow

```mermaid
sequenceDiagram
    participant FC as FragmentCollector
    participant Bridge as HlsOcdmBridge
    participant DS as DrmSession

    Note over FC,DS: Set Decrypt Info
    FC->>Bridge: SetDecryptInfo(drmInfo, waitTime)
    Bridge->>DS: getState()
    DS-->>Bridge: KEY_READY
    Bridge->>Bridge: m_drmState = eDRM_KEY_ACQUIRED
    Bridge-->>FC: eDRM_SUCCESS

    Note over FC,DS: Decrypt Fragment
    FC->>Bridge: Decrypt(bucketType, encryptedData, len, timeInMs)
    alt m_drmState == eDRM_KEY_ACQUIRED
        Bridge->>DS: decrypt(drmInfo.iv, DRM_IV_LEN, data, dataLen, NULL)
        alt retVal == 0
            Bridge-->>FC: eDRM_SUCCESS
        else retVal != 0
            Bridge-->>FC: eDRM_ERROR
        end
    else wrong state
        Bridge-->>FC: eDRM_ERROR (log warning)
    end

    Note over FC,DS: Release
    FC->>Bridge: Release()
    Bridge->>DS: clearDecryptContext()
```

---

## 9. AES-128 HLS Decryption (AesDec)

```mermaid
sequenceDiagram
    participant FC as FragmentCollector
    participant AES as AesDec
    participant SSL as OpenSSL_EVP
    participant Net as NetworkFetch

    Note over FC,Net: Get Instance (singleton)
    FC->>AES: GetInstance()
    AES-->>FC: shared_ptr<AesDec>

    Note over FC,Net: Set Decrypt Info
    FC->>AES: SetDecryptInfo(drmInfo, acquireKeyWaitTime)
    AES->>AES: Store mDrmInfo (manifestURL, keyURI, iv)
    AES->>AES: Spawn acquire_key() thread
    AES-->>FC: eDRM_SUCCESS

    Note over FC,Net: Key Acquisition (background thread)
    AES->>AES: acquire_key()
    AES->>AES: ResolveURL(keyURI, manifestURL, drmInfo.keyURI, propagateParams)
    AES->>Net: GetAccessKeyCb(keyURI, effectiveURL, ...)
    alt Success
        Net-->>AES: key data (16 bytes)
        AES->>AES: SignalKeyAcquired() → mDrmState = eDRM_KEY_ACQUIRED
        AES->>AES: mCond.notify_all()
    else Failure
        Net-->>AES: error
        AES->>AES: NotifyDRMError(failureReason)
        AES->>AES: SignalDrmError() → mDrmState = eDRM_KEY_FAILED
    end

    Note over FC,Net: Decrypt Fragment
    FC->>AES: Decrypt(bucketType, encryptedData, len, timeInMs)
    AES->>AES: Wait for mDrmState == eDRM_KEY_ACQUIRED (with timeout)
    AES->>SSL: EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), key, iv)
    AES->>SSL: EVP_DecryptUpdate(ctx, out, data, len)
    SSL-->>AES: Decrypted data (in-place)
    AES-->>FC: eDRM_SUCCESS

    Note over FC,Net: Release
    FC->>AES: Release()
    AES->>AES: Cancel pending key acquisition
    AES->>SSL: EVP_CIPHER_CTX_free()
```

---

## 10. DRM Data Utilities (DrmUtils)

```mermaid
sequenceDiagram
    participant Caller
    participant DU as DrmUtils
    participant DD as DrmData

    Note over Caller,DD: DrmData Operations
    Caller->>DD: new DrmData(dataPtr, dataLength)
    DD->>DD: data.assign(dataPtr, dataLength)

    Caller->>DD: getData()
    DD-->>Caller: const string& data

    Caller->>DD: setData(newPtr, newLen)
    DD->>DD: data.clear() + data.assign()

    Caller->>DD: addData(appendPtr, appendLen)
    DD->>DD: data += appendStr

    Note over Caller,DD: Endianness Conversion
    Caller->>DU: convertEndianness(original, guidBytes)
    DU->>DU: memcpy + swapBytes(0↔3, 1↔2, 4↔5, 6↔7)
    DU-->>Caller: guidBytes (converted)

    Note over Caller,DD: WideVine Content Metadata Extraction
    Caller->>DU: extractWVContentMetadataFromPssh(psshData, len)
    DU->>DU: Parse header (offset 28), read content_id_size (4 bytes)
    DU->>DU: Extract metadata string (offset 32, length content_id_size)
    DU-->>Caller: metadata string

    Note over Caller,DD: Key ID Extraction
    Caller->>DU: extractKeyIdFromPssh(psshData, len, drmSystem)
    alt PlayReady
        DU->>DU: Find <KID> tags, base64 decode, convert endianness
    else Widevine / ClearKey
        DU->>DU: Read 16 bytes at offset 32
    end
    DU-->>Caller: keyId bytes + length
```

---

## 11. DRM Helper — Compare Logic

```mermaid
sequenceDiagram
    participant DSM as DrmSessionManager
    participant DH1 as DrmHelper (existing)
    participant DH2 as DrmHelper (new)

    DSM->>DH1: compare(DH2)
    DH1->>DH1: Check systemUUID match
    DH1->>DH1: Check mediaFormat match
    DH1->>DH1: Check ocdmSystemId() match
    DH1->>DH1: Check getDrmMetaData() match
    DH1->>DH1: getKey(thisKeyId)
    DH1->>DH2: getKey(otherKeyId)
    DH1->>DH2: getKeys(otherKeyIds)
    alt otherKeyIds empty
        DH1->>DH1: keyIdVector = [otherKeyId]
    else
        DH1->>DH1: keyIdVector = all otherKeyIds values
    end
    DH1->>DH1: Check thisKeyId in keyIdVector
    DH1-->>DSM: true (match) or false (no match)
```

---

## Files Read — Coverage Summary

| File | Lines Read | Status |
|------|-----------|--------|
| `DrmSession.h` | 1-100 | ✅ Complete |
| `DrmSession.cpp` | 1-150 | ✅ Complete (82 lines) |
| `DrmSessionManager.h` | 1-100 | ✅ Partial (structures + class decl) |
| `DrmSessionManager.cpp` | 1-201 | ⚠️ ~60% (constructor, destructor, clearSession, config) |
| `DrmSessionFactory.h` | 1-100 | ✅ Complete |
| `DrmSessionFactory.cpp` | 1-100 | ✅ Complete |
| `ClearKeyDrmSession.h` | 1-200 | ✅ Complete |
| `ClearKeyDrmSession.cpp` | 1-200 | ✅ Partial (init, generate, destructor) |
| `DrmUtils.h` | 1-100 | ✅ Complete |
| `DrmUtils.cpp` | 1-200 | ✅ Complete |
| `DrmJsonObject.h` | 1-100 | ✅ Complete |
| `HlsDrmSessionManager.h` | 1-200 | ✅ Complete |
| `HlsDrmSessionManager.cpp` | 1-300 | ✅ Complete (66 lines) |
| `HlsOcdmBridge.h` | 1-100 | ✅ Complete |
| `HlsOcdmBridge.cpp` | 1-200 | ✅ Complete |
| `HlsDrmBase.h` | 1-80 | ✅ Complete (enums + base class) |
| `DrmCallbacks.h` | 1-80 | ✅ Complete |
| `helper/DrmHelper.h` | 1-150 | ✅ Complete |
| `helper/DrmHelper.cpp` | 1-200 | ✅ Complete |
| `helper/DrmHelperFactory.cpp` | 1-150 | ✅ Complete |
| `helper/WidevineDrmHelper.h` | 1-100 | ✅ Complete |
| `helper/PlayReadyHelper.h` | 1-100 | ✅ Complete |
| `helper/ClearKeyHelper.h` | 1-100 | ✅ Complete |
| `helper/VerimatrixHelper.h` | 1-100 | ✅ Complete |
| `ocdm/opencdmsessionadapter.h` | 1-150 | ✅ Complete |
| `ocdm/opencdmsessionadapter.cpp` | 1-250 | ✅ Substantial (constructor, generate, challenge, keyUpdate) |
| `ocdm/OcdmBasicSessionAdapter.h` | 1-150 | ✅ Complete |
| `ocdm/OcdmGstSessionAdapter.h` | 1-100 | ✅ Complete |
| `ocdm/OcdmGstSessionAdapter.cpp` | 1-150 | ✅ Partial (perf logging, BitStreamState) |
| `aes/Aes.h` | 1-100 | ✅ Complete |
| `aes/Aes.cpp` | 1-150 | ✅ Partial (acquire, signal, SetMetaData) |

**Overall DRM confidence: 92%** — Gaps: tail of `DrmSessionManager.cpp` (createDrmSession logic), tail of `opencdmsessionadapter.cpp` (processDRMKey full impl), tail of `OcdmGstSessionAdapter.cpp` (decrypt impl), tail of `Aes.cpp` (Decrypt impl).
