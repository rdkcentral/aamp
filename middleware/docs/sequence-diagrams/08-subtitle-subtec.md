# Sequence Diagrams: subtitle / subtec

## Module: subtitle (Interfaces)

### 8.1 — SubtitleParser Base Class & Callback Registration

```mermaid
sequenceDiagram
    participant AAMP as AAMP Core
    participant Parser as SubtitleParser (base)
    participant Impl as WebVTTSubtecParser / TtmlSubtecParser

    AAMP->>Impl: new WebVTTSubtecParser(type, width, height)
    Impl->>Parser: SubtitleParser(type, width, height)

    AAMP->>Impl: RegisterCallback(callbacks)
    Note over Parser: Stores: resumeTrackDownloads_CB, stopTrackDownloads_CB,<br/>sendVTTCueData_CB, getPlayerPositions_CB

    AAMP->>Impl: init(startPosSeconds, basePTS)
    AAMP->>Impl: processData(buffer, bufferLen, position, duration)
    AAMP->>Impl: updateTimestamp(positionMs)
    AAMP->>Impl: pause(true/false)
    AAMP->>Impl: mute(true/false)
    AAMP->>Impl: setTextStyle(options)
    AAMP->>Impl: setPtsOffset(ptsOffsetSec)
    AAMP->>Impl: close()
```

### 8.2 — VTTCue Data Structure

```mermaid
sequenceDiagram
    participant Parser as SubtitleParser
    participant Cue as VTTCue

    Parser->>Cue: new VTTCue(startTime, duration, text, settings)
    Note over Cue: mStart = startTime<br/>mDuration = duration<br/>mText = text<br/>mSettings = settings
    Parser->>Parser: sendVTTCueData_CB(cue)
```

---

## Module: subtec/libsubtec

### 8.3 — PacketSender Singleton Lifecycle

```mermaid
sequenceDiagram
    participant Channel as SubtecChannel
    participant Sender as PacketSender (Singleton)
    participant Socket as Unix Domain Socket
    participant Thread as senderTask thread

    Channel->>Sender: Instance()
    Note over Sender: static singleton

    Channel->>Sender: Init(socket_path)
    Sender->>Sender: lock(mStartMutex)
    alt !running
        Sender->>Socket: socket(AF_UNIX, SOCK_STREAM)
        Sender->>Socket: connect(SOCKET_PATH)
        Sender->>Sender: initSenderTask()
        Sender->>Thread: std::thread(senderTask)
        Thread->>Thread: running = true, wait on mCv
    else already running
        Sender-->>Channel: true (no-op)
    end
```

### 8.4 — PacketSender Send Flow

```mermaid
sequenceDiagram
    participant Channel as SubtecChannel
    participant Sender as PacketSender
    participant Queue as mPacketQueue
    participant Thread as senderTask
    participant Socket as Unix Socket

    Channel->>Sender: SendPacket(packet)
    Sender->>Sender: lock(mPktMutex)
    Sender->>Queue: push(packet)
    Sender->>Sender: mCv.notify_all()

    Thread->>Thread: mCv wakes up
    loop !mPacketQueue.empty()
        Thread->>Queue: front() + pop()
        Thread->>Thread: sendPacket(pkt)
        Thread->>Thread: buffer = pkt->getBytes()
        alt size > mSockBufSize
            Thread->>Socket: setsockopt(SO_SNDBUF, newSize)
        end
        Thread->>Socket: ::send(mSubtecSocketHandle, buffer, size)
        alt send fails
            Thread->>Thread: mPktWriteFailCtr++
            alt failCtr > threshold
                Thread->>Thread: Reconnect socket
            end
        end
    end
```

### 8.5 — SubtecChannel Factory & Command Dispatch

```mermaid
sequenceDiagram
    participant Caller
    participant Channel as SubtecChannel
    participant Factory as SubtecChannelFactory
    participant Ttml as TtmlChannel
    participant WebVtt as WebVttChannel
    participant Sender as PacketSender

    Caller->>Factory: SubtecChannelFactory(ChannelType::WEBVTT)
    Factory->>WebVtt: make_unique<WebVttChannel>()
    Factory-->>Caller: unique_ptr<SubtecChannel>

    Caller->>Channel: InitComms()
    Channel->>Channel: Get PLAYERNAME_SUBTITLE_SOCKET env var
    alt env var set
        Channel->>Sender: Init(env_socket_path)
    else not set
        Channel->>Sender: Init(SOCKET_PATH)
    end

    Caller->>Channel: SendResetAllPacket()
    Channel->>Channel: m_counter = 1
    Channel->>Sender: SendPacket(ResetAllPacket)

    Caller->>Channel: SendSelectionPacket(width, height)
    Channel->>Sender: SendPacket(WebVttSelectionPacket(channelId, counter++, w, h))

    Caller->>Channel: SendDataPacket(data, timeOffset)
    Channel->>Sender: SendPacket(WebVttDataPacket(channelId, counter++, offset, data))

    Caller->>Channel: SendTimestampPacket(timestampMs)
    Channel->>Sender: SendPacket(WebVttTimestampPacket(channelId, counter++, ts))
```

### 8.6 — Packet Structure (Wire Format)

```mermaid
sequenceDiagram
    participant Builder as Packet subclass
    participant Buffer as m_buffer (vector<uint8_t>)

    Builder->>Buffer: appendType(PacketType enum, 4 bytes LE)
    Builder->>Buffer: append32(counter)
    Builder->>Buffer: append32(payloadSize)
    Builder->>Buffer: append32(channelId)
    Note over Buffer: Type-specific fields follow:
    alt WebVttSelectionPacket
        Builder->>Buffer: append32(width), append32(height)
    else WebVttDataPacket
        Builder->>Buffer: append64(timeOffsetMs)
        Builder->>Buffer: append raw data bytes
    else TtmlDataPacket
        Builder->>Buffer: append64(timeOffsetMs)
        Builder->>Buffer: append raw TTML XML bytes
    else CCSetAttributePacket
        Builder->>Buffer: append32(ccType), append32(attribMask)
        Builder->>Buffer: append attribute values array
    end
```

---

## Module: subtec/subtecparser

### 8.7 — WebVTTSubtecParser Full Lifecycle

```mermaid
sequenceDiagram
    participant AAMP as AAMP Core
    participant Parser as WebVTTSubtecParser
    participant Channel as SubtecChannel (WebVtt)
    participant Sender as PacketSender
    participant Subtec as Subtec Renderer (external)

    AAMP->>Parser: new WebVTTSubtecParser(type, 1920, 1080)
    Parser->>Channel: SubtecChannelFactory(WEBVTT)
    Parser->>Channel: InitComms()
    Channel->>Sender: Init(socket_path)
    Parser->>Channel: SendResetAllPacket()
    Parser->>Channel: SendMutePacket()
    Parser->>Channel: SendSelectionPacket(1920, 1080)

    AAMP->>Parser: init(startPosSec, basePTS)
    Parser->>Channel: SendTimestampPacket(startPosSec * 1000)
    Parser->>Parser: playerResumeTrackDownloads_CB()

    loop Each subtitle fragment
        AAMP->>Parser: processData(buffer, len, position, duration)
        Parser->>Parser: Convert buffer to vector<uint8_t>
        Parser->>Channel: SendDataPacket(data, time_offset_ms_)
        Channel->>Sender: SendPacket(WebVttDataPacket(...))
        Sender->>Subtec: ::send() over socket
    end

    AAMP->>Parser: updateTimestamp(positionMs)
    Parser->>Channel: SendTimestampPacket(positionMs)

    AAMP->>Parser: setPtsOffset(ptsOffsetSec)
    Parser->>Parser: time_offset_ms_ = -(ptsOffsetSec * 1000)

    AAMP->>Parser: mute(false)
    Parser->>Channel: SendUnmutePacket()

    AAMP->>Parser: pause(true)
    Parser->>Channel: SendPausePacket()

    AAMP->>Parser: setTextStyle(options)
    Parser->>Parser: TextStyleAttributes::getAttributes(options)
    Parser->>Channel: SendCCSetAttributePacket(ccType, mask, values)
```

### 8.8 — TtmlSubtecParser Full Lifecycle (with Linear Offset)

```mermaid
sequenceDiagram
    participant AAMP as AAMP Core
    participant Parser as TtmlSubtecParser
    participant IsoBmff as PlayerIsoBmffBuffer
    participant Channel as SubtecChannel (Ttml)
    participant Sender as PacketSender

    AAMP->>Parser: new TtmlSubtecParser(type, 1920, 1080)
    Parser->>Channel: SubtecChannelFactory(TTML)
    Parser->>Channel: InitComms()
    Parser->>Channel: SendResetAllPacket()
    Parser->>Channel: SendSelectionPacket(1920, 1080)
    Parser->>Channel: SendMutePacket()
    Parser->>Parser: playerResumeTrackDownloads_CB()

    AAMP->>Parser: isLinear(true)
    Parser->>Parser: m_isLinear = true

    AAMP->>Parser: init(startPosSec, basePTS)
    Parser->>Channel: SendTimestampPacket(startPosSec * 1000)
    Parser->>Parser: m_parsedFirstPacket = false, m_sentOffset = false

    loop Each MP4 subtitle segment
        AAMP->>Parser: processData(buffer, bufferLen, position, duration)
        Parser->>IsoBmff: setBuffer(buffer, bufferLen)
        Parser->>IsoBmff: parseBuffer()
        alt isInitSegment()
            Parser->>Parser: Skip (log "Init Segment")
        else media segment
            Parser->>IsoBmff: getMdatBoxSize(mdatLen)
            Parser->>IsoBmff: parseMdatBox(mdat, mdatLen)
            alt !m_parsedFirstPacket && m_isLinear
                Parser->>Parser: m_firstBeginOffset = position
                Parser->>Parser: m_parsedFirstPacket = true
            end
            alt !m_sentOffset && m_isLinear
                Parser->>Parser: parseFirstBegin(mdat) → regex "begin=HH:MM:SS.ms"
                Parser->>Parser: Calculate totalOffset from position delta + playerGetPositions
                Parser->>Channel: SendTimestampPacket(totalOffset)
                Parser->>Parser: m_sentOffset = true
            end
            Parser->>Channel: SendDataPacket(mdat_vector, 0)
            Channel->>Sender: SendPacket(TtmlDataPacket(...))
        end
    end
```

---

## Coverage Summary

| File | Lines Read | Confidence |
|------|-----------|------------|
| subtitle/subtitleParser.h | 1–100 | 100% |
| subtitle/vttCue.h | 1–56 (complete) | 100% |
| subtec/libsubtec/PacketSender.hpp | 1–100 | 100% |
| subtec/libsubtec/PacketSender.cpp | 1–150 | 100% |
| subtec/libsubtec/SubtecChannel.hpp | 1–100 | 100% |
| subtec/libsubtec/SubtecChannel.cpp | 1–200 (complete) | 100% |
| subtec/libsubtec/SubtecPacket.hpp | 1–100 | 90% (packet types enumerated) |
| subtec/libsubtec/WebVttPacket.hpp | 1–100 | 95% |
| subtec/subtecparser/WebVttSubtecParser.hpp | 1–55 (complete) | 100% |
| subtec/subtecparser/WebVttSubtecParser.cpp | 1–120 (complete) | 100% |
| subtec/subtecparser/TtmlSubtecParser.hpp | 1–55 (complete) | 100% |
| subtec/subtecparser/TtmlSubtecParser.cpp | 1–200 | 100% |

**Overall Confidence: 98%**
