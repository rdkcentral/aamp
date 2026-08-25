# 09 - Curl Network & Download Subsystem

## Module: `downloader/` + `AampCMCDCollector`

**Source Files Read:**
- `downloader/AampCurlStore.h` (complete - 280 lines)
- `downloader/AampCurlDownloader.h` (complete - 250 lines)
- `downloader/AampCurlDefine.h` (complete - 140 lines)
- `AampCMCDCollector.h` (complete - 130 lines)

**Confidence: 90%**
- Gap: `AampCurlStore.cpp` and `AampCurlDownloader.cpp` implementation bodies not read

---

## 9.1 Curl Handle Lifecycle (CurlStore Singleton)

```mermaid
sequenceDiagram
    participant PA as PrivateInstanceAAMP
    participant CS as CurlStore (Singleton)
    participant CSS as CurlSocketStoreStruct
    participant CURL as libcurl

    Note over PA,CURL: CurlStore manages per-host connection pooling

    PA->>CS: GetCurlStoreInstance(pAamp)
    CS-->>PA: Singleton reference

    PA->>CS: CurlInit(pAamp, startIdx, count, proxy, remotehost)
    CS->>CS: GetFromCurlStoreBulk(hostname, idx, count)
    alt Host exists in umCurlSockDataStore
        CS->>CSS: GetCurlHandleFromFreeQ(CurlSock, instId)
        alt FreeQ has handle & age < 300s
            CSS-->>CS: Reuse existing CURL* handle
        else FreeQ empty or aged out
            CS->>CS: CurlEasyInitWithOpt(pAamp, proxy, instId)
            CS->>CURL: curl_easy_init()
            CURL-->>CS: New CURL* handle
        end
    else Host not found
        CS->>CS: CreateCurlStore(hostname)
        CS->>CURL: curl_share_init()
        CURL-->>CS: CURLSH* (shared DNS/SSL)
        CS->>CS: CurlEasyInitWithOpt(pAamp, proxy, instId)
        CS->>CURL: curl_easy_init()
        CURL-->>CS: New CURL* handle
    end
    CS-->>PA: Handles assigned to pAamp->curl[startIdx..startIdx+count]

    Note over PA,CURL: After download completes

    PA->>CS: CurlTerm(pAamp, startIdx, count, isFlush, remotehost)
    CS->>CS: KeepInCurlStoreBulk(hostname, idx, count)
    CS->>CSS: Push handle back to mFreeQ (with timestamp)
    alt isFlushFds == true
        CS->>CS: FlushCurlSockForHost(hostname)
        CS->>CURL: curl_easy_cleanup() for all handles
        CS->>CURL: curl_share_cleanup()
    end
```

---

## 9.2 Download Flow (AampCurlDownloader)

```mermaid
sequenceDiagram
    participant Caller as FragmentCollector/ManifestFetch
    participant DL as AampCurlDownloader
    participant CURL as libcurl
    participant Server as CDN/Origin

    Caller->>DL: Initialize(downloadConfig)
    DL->>DL: Store config (timeout, TLS, proxy, headers, retry)
    DL->>DL: updateCurlParams()
    DL->>CURL: curl_easy_setopt(URL, TIMEOUT, SSL, PROXY, etc.)
    DL->>CURL: curl_easy_setopt(WRITEFUNCTION, WriteCallback)
    DL->>CURL: curl_easy_setopt(HEADERFUNCTION, HeaderCallback)
    DL->>CURL: curl_easy_setopt(PROGRESSFUNCTION, ProgressCallback)

    Caller->>DL: Download(url, downloadResponse)
    DL->>DL: mDownloadActive = true
    DL->>CURL: curl_easy_perform()
    
    loop Data arrives
        CURL->>Server: HTTP GET/POST
        Server-->>CURL: Response chunks
        CURL->>DL: WriteCallback(contents, size)
        DL->>DL: Append to mDownloadResponse->mDownloadData
        CURL->>DL: ProgressCallback(dltotal, dlnow)
        alt stallTimeout exceeded
            DL->>DL: abortReason = eCURL_ABORT_REASON_STALL_TIMEDOUT
            DL-->>CURL: Return non-zero (abort)
        else startTimeout exceeded
            DL->>DL: abortReason = eCURL_ABORT_REASON_START_TIMEDOUT
            DL-->>CURL: Return non-zero (abort)
        end
        CURL->>DL: HeaderCallback(header line)
        DL->>DL: Parse Set-Cookie, X-Bitrate, Content-Length, Location
    end

    CURL-->>DL: CURLcode result
    DL->>DL: updateResponseParams() (metrics: total, connect, resolve, appConnect)
    DL->>DL: mDownloadActive = false
    DL-->>Caller: HTTP response code

    opt Retry on failure
        alt retryCount > 0 && retriable error
            Caller->>DL: Download(url, downloadResponse) [retry]
        end
    end

    Caller->>DL: Release()
    DL->>DL: Reset state
    Caller->>DL: CleanupCurlHeaderResources()
    DL->>CURL: curl_slist_free_all(mHeaders)
```

---

## 9.3 CurlCallbackContext — Chunked Transfer Handling

```mermaid
sequenceDiagram
    participant CURL as libcurl
    participant CTX as CurlCallbackContext
    participant BUF as Download Buffer

    Note over CURL,BUF: HTTP/1.1 chunked transfer-encoding state machine

    CURL->>CTX: WriteCallback(data chunk)
    
    alt chunkedDownload == true
        loop Parse chunked encoding
            alt state == READING_CHUNK_SIZE
                CTX->>CTX: Parse hex chunk size from data
                CTX->>CTX: m_ChunkedBytesRemaining = parsed size
                alt size == 0
                    CTX->>CTX: state = DONE
                else size > 0
                    CTX->>CTX: state = PENDING_CHUNK_START_LF
                end
            else state == READING_CHUNK_DATA
                CTX->>BUF: Append min(available, m_ChunkedBytesRemaining) to buffer
                CTX->>CTX: m_ChunkedBytesRemaining -= copied
                alt m_ChunkedBytesRemaining == 0
                    CTX->>CTX: state = PENDING_CHUNK_END_CR
                    CTX->>CTX: Update chunkBoundary offset
                end
            else state == ERROR
                CTX->>CTX: abortReason = eCURL_ABORT_REASON_CHUNKED_PARSER_ERROR
                CTX-->>CURL: Return 0 (abort transfer)
            end
        end
    else Non-chunked
        CTX->>BUF: Direct append to buffer
    end
```

---

## 9.4 CMCD (Common Media Client Data) Header Injection

```mermaid
sequenceDiagram
    participant FC as FragmentCollector
    participant CMCD as AampCMCDCollector
    participant HDR as CMCDHeaders (per-type)
    participant DL as AampCurlDownloader

    Note over FC,DL: CMCD headers added per CTA-5004 spec

    FC->>CMCD: Initialize(enabled, traceId)
    CMCD->>CMCD: Create StreamTypeCMCD map (Video/Audio/Subtitle/Manifest)
    CMCD->>HDR: new VideoCMCDHeaders / AudioCMCDHeaders / etc.

    FC->>CMCD: SetBitrates(mediaType, bitrateList)
    CMCD->>HDR: Store available bitrate ladder

    FC->>CMCD: SetTrackData(mediaType, bufferRed, bufferedDuration, currentBitrate)
    CMCD->>HDR: Update buffer status, duration, active bitrate

    FC->>CMCD: CMCDSetNextObjectRequest(url, bandwidth, mediaType)
    CMCD->>HDR: Store next object request (nor=<url>)

    FC->>CMCD: CMCDSetNetworkMetrics(mediaType, startTransfer, total, dnsLookup)
    CMCD->>HDR: Store network timing for header generation

    Note over FC,DL: Before each download request

    FC->>CMCD: CMCDGetHeaders(mediaType, customHeaders)
    CMCD->>HDR: Generate CMCD-Object, CMCD-Request, CMCD-Session, CMCD-Status
    HDR-->>CMCD: Header strings (bl=, br=, d=, nor=, sid=, etc.)
    CMCD-->>FC: Appended to customHeaders vector

    FC->>DL: Download(url) with customHeaders containing CMCD
    DL->>DL: curl_slist_append(CMCD headers)
```

---

## 9.5 Curl Instance Assignment (Per Media Type)

```mermaid
sequenceDiagram
    participant PA as PrivateInstanceAAMP
    participant CS as CurlStore

    Note over PA,CS: Dedicated curl instances per stream type

    PA->>CS: CurlInit(VIDEO, eCURLINSTANCE_VIDEO)
    PA->>CS: CurlInit(AUDIO, eCURLINSTANCE_AUDIO)
    PA->>CS: CurlInit(SUBTITLE, eCURLINSTANCE_SUBTITLE)
    PA->>CS: CurlInit(MANIFEST_MAIN, eCURLINSTANCE_MANIFEST_MAIN)
    PA->>CS: CurlInit(PLAYLIST_VIDEO, eCURLINSTANCE_MANIFEST_PLAYLIST_VIDEO)
    PA->>CS: CurlInit(PLAYLIST_AUDIO, eCURLINSTANCE_MANIFEST_PLAYLIST_AUDIO)
    PA->>CS: CurlInit(DAI, eCURLINSTANCE_DAI)
    PA->>CS: CurlInit(AES, eCURLINSTANCE_AES)

    Note over PA,CS: Each instance gets per-host shared DNS/SSL via CURLSH
    Note over PA,CS: Max age per connection: 300s (eCURL_MAX_AGE_TIME)
    Note over PA,CS: Handles recycled via FreeQ per host
```

---

## Key Classes

| Class | File | Role |
|-------|------|------|
| `CurlStore` | `downloader/AampCurlStore.h` | Singleton, per-host connection pool, shared DNS/SSL |
| `CurlSocketStoreStruct` | `downloader/AampCurlStore.h` | Per-host storage (FreeQ + CURLSH + locks) |
| `CurlCallbackContext` | `downloader/AampCurlStore.h` | Per-download state (chunked parsing, buffer, abort) |
| `CurlProgressCbContext` | `downloader/AampCurlStore.h` | Progress tracking (stall/start timeout detection) |
| `AampCurlDownloader` | `downloader/AampCurlDownloader.h` | Download executor (init, download, retry, metrics) |
| `DownloadConfig` | `downloader/AampCurlDownloader.h` | Download parameters (timeout, TLS, proxy, headers) |
| `DownloadResponse` | `downloader/AampCurlDownloader.h` | Response container (data, metrics, headers) |
| `AampCMCDCollector` | `AampCMCDCollector.h` | CMCD header generation per CTA-5004 |

## Enums

| Enum | Values |
|------|--------|
| `AampCurlInstance` | VIDEO, AUDIO, SUBTITLE, MANIFEST_MAIN, PLAYLIST_VIDEO/AUDIO/SUBTITLE, DAI, AES, PRECACHE (10 total) |
| `CurlAbortReason` | NONE, STALL_TIMEDOUT, START_TIMEDOUT, LOW_BANDWIDTH_TIMEDOUT, CHUNKED_PARSER_ERROR, FIRST_CHUNK_SLOW, INVALID_CHUNK_BOUNDARY, BUFFER_ALLOC_FAILURE |
| `AampCurlStoreErrorCode` | HOST_NOT_AVAILABLE, SOCK_NOT_AVAILABLE, HOST_SOCK_AVAILABLE |
| `CurlRequest` | GET, POST, DELETE |
