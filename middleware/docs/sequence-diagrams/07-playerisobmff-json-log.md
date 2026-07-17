# Sequence Diagrams: playerisobmff / playerJsonObject / playerLogManager

## Module: playerisobmff

### 7.1 — ISO BMFF Buffer Parse Flow

```mermaid
sequenceDiagram
    participant Caller
    participant Buffer as PlayerIsoBmffBuffer
    participant Box as IsoBmffBox

    Caller->>Buffer: setBuffer(buf, sz)
    Caller->>Buffer: parseBuffer(correctBoxSize, newTrackId)
    loop while curOffset < bufSize
        Buffer->>Box: constructBox(buffer+curOffset, remaining, correctBoxSize, newTrackId)
        alt size > maxSz && correctBoxSize
            Box->>Box: Fix size (PLAYER_WRITE_U32), goto L_RESTART
        end
        alt type == MOOF/TRAK/MOOV (container)
            Box->>Box: Parse children recursively
        end
        Box-->>Buffer: return IsoBmffBox*
        Buffer->>Buffer: box->setOffset(curOffset), push_back
    end
    Buffer-->>Caller: true (boxes parsed)
```

### 7.2 — ISO BMFF Box Construction (Type Dispatch)

```mermaid
sequenceDiagram
    participant Factory as IsoBmffBox::constructBox
    participant TFDT as TfdtIsoBmffBox
    participant TRUN as TrunIsoBmffBox
    participant EMSG as EmsgIsoBmffBox
    participant Generic as IsoBmffBox

    Factory->>Factory: Read size (U32), type (4 bytes)
    alt type == "tfdt"
        Factory->>TFDT: new TfdtIsoBmffBox(hdr, size)
        TFDT->>TFDT: Parse version, baseMediaDecodeTime (U32 or U64)
    else type == "trun"
        Factory->>TRUN: new TrunIsoBmffBox(hdr, size)
        TRUN->>TRUN: Parse flags, sampleCount, durations, sizes
    else type == "emsg"
        Factory->>EMSG: new EmsgIsoBmffBox(hdr, size)
        EMSG->>EMSG: Parse scheme_id, value, timescale, duration, id, message
    else type == container (moof/trak/moov/mdia/traf)
        Factory->>Generic: new GenericContainerBox(size, type)
        Generic->>Factory: Recursively constructBox for children
    else other
        Factory->>Generic: new IsoBmffBox(size, type)
    end
```

### 7.3 — MDAT Extraction

```mermaid
sequenceDiagram
    participant Caller
    participant Buffer as PlayerIsoBmffBuffer

    Caller->>Buffer: getMdatBoxSize(size)
    Buffer->>Buffer: getBoxSizeInternal(boxes, "mdat", size)
    Buffer-->>Caller: true + size

    Caller->>Caller: malloc(size)
    Caller->>Buffer: parseMdatBox(buf, size)
    Buffer->>Buffer: parseBoxInternal(boxes, "mdat", buf, size)
    Note over Buffer: memcpy box payload (offset + BOX_HEADER_SIZE) into buf
    Buffer-->>Caller: true + filled buf
```

---

## Module: playerJsonObject

### 7.4 — PlayerJsonObject Construction & Serialization

```mermaid
sequenceDiagram
    participant Caller
    participant JSON as PlayerJsonObject
    participant cJSON

    alt Construct from scratch
        Caller->>JSON: new PlayerJsonObject()
        JSON->>cJSON: cJSON_CreateObject()
    else Parse from string
        Caller->>JSON: new PlayerJsonObject(jsonStr)
        JSON->>cJSON: cJSON_Parse(jsonStr)
        alt parse fails
            JSON-->>Caller: throw PlayerJsonParseException
        end
    end

    Caller->>JSON: add("key", "value", ENCODING_STRING)
    JSON->>cJSON: cJSON_CreateString(value)
    JSON->>cJSON: cJSON_AddItemToObject(obj, name, item)

    Caller->>JSON: add("key", byteVector, ENCODING_BASE64)
    JSON->>JSON: base64_Encode(data, len)
    JSON->>cJSON: cJSON_CreateString(encoded)

    Caller->>JSON: add("key", byteVector, ENCODING_BASE64_URL)
    JSON->>JSON: player_Base64_URL_Encode(data, len)
    JSON->>cJSON: cJSON_CreateString(encoded)

    Caller->>JSON: print()
    JSON->>cJSON: cJSON_PrintUnformatted(mJsonObj)
    JSON-->>Caller: JSON string
```

### 7.5 — PlayerJsonObject Array & Nested Object

```mermaid
sequenceDiagram
    participant Caller
    participant JSON as PlayerJsonObject
    participant cJSON

    Caller->>JSON: add("arr", vector<string>)
    JSON->>cJSON: cJSON_CreateArray()
    loop each string
        JSON->>cJSON: cJSON_CreateString(val)
        JSON->>cJSON: cJSON_AddItemToArray(arr, item)
    end
    JSON->>cJSON: cJSON_AddItemToObject(obj, "arr", arr)

    Caller->>JSON: add("nested", vector<PlayerJsonObject*>)
    JSON->>cJSON: cJSON_CreateArray()
    loop each child
        JSON->>JSON: child->mParent = this (ownership transfer)
        JSON->>cJSON: cJSON_AddItemToArray(arr, child->mJsonObj)
    end
```

---

## Module: playerLogManager

### 7.6 — Log Dispatch Flow (logprintf)

```mermaid
sequenceDiagram
    participant Source as Any Module
    participant LogMgr as PlayerLogManager
    participant Journal as sd_journal_printv
    participant Ethan as vethanlog
    participant Console as vprintf

    Source->>LogMgr: MW_LOG_WARN(format, args...)
    Note over LogMgr: Expands to logprintf(mLOGLEVEL_WARN, __func__, __LINE__, format, ...)

    LogMgr->>LogMgr: isLogLevelAllowed(level) ?
    alt level < mwLoglevel
        LogMgr-->>Source: (suppressed)
    else allowed
        LogMgr->>LogMgr: gMwLogCounter++ (atomic, mod 1000)
        LogMgr->>LogMgr: Format: [PLAYER_IF][seq][LEVEL][threadId][func][line]msg
        alt disableLogRedirection (CLI/simulator)
            LogMgr->>LogMgr: Add timestamp prefix
            LogMgr->>Console: vprintf(formatted, args)
        else enableEthanLogRedirection
            LogMgr->>LogMgr: Map MW level → Ethan level
            LogMgr->>Ethan: vethanlog(ethanLevel, format, args)
        else default (device)
            LogMgr->>Journal: sd_journal_printv(LOG_NOTICE, format, args)
        end
    end
```

### 7.7 — Log Level Configuration

```mermaid
sequenceDiagram
    participant Externals as InterfacePlayerRDK
    participant LogMgr as PlayerLogManager

    Externals->>LogMgr: SetLoggerInfo(disableRedirect, ethanLog, level, lock)
    LogMgr->>LogMgr: disableLogRedirection = disableRedirect
    LogMgr->>LogMgr: enableEthanLogRedirection = ethanLog
    LogMgr->>LogMgr: setLogLevel(level)
    alt !locked
        LogMgr->>LogMgr: mwLoglevel = newLevel
    end
    LogMgr->>LogMgr: lockLogLevel(lock)
```

---

## Coverage Summary

| File | Lines Read | Confidence |
|------|-----------|------------|
| playerisobmffbox.h | 1–150 | 100% |
| playerisobmffbox.cpp | 1–200 | 85% (large file, box construction logic captured) |
| playerisobmffbuffer.h | 1–150 | 100% |
| playerisobmffbuffer.cpp | 1–200 | 90% (core parse/extract logic captured) |
| PlayerJsonObject.h | 1–100 | 100% |
| PlayerJsonObject.cpp | 1–200 | 95% |
| PlayerLogManager.h | 1–100 | 100% |
| PlayerLogManager.cpp | 1–200 | 100% |

**Overall Confidence: 95%**
