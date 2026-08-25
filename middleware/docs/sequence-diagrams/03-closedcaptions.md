# closedcaptions — Sequence Diagrams

> **Source files read**: `CCTrackInfo.h`, `PlayerCCManager.h`, `PlayerCCManager.cpp` (300 lines), `PlayerRialtoCCManager.h`, `PlayerRialtoCCManager.cpp`, `CCDataController.h`, `CCDataController.cpp`, `PlayerSubtecCCManager.h`, `PlayerSubtecCCManager.cpp`, `SubtecConnector.h`, `SubtecConnector.cpp`
> **Confidence**: 95% — `PlayerCCManager.cpp` is large; lines 301+ (style parsing helpers: getFontSize, getOpacity, SetStyle, SetTrack base impl) were not fully read. Core architecture and all subclass implementations are fully covered.

---

## Module Overview

The Closed Captions subsystem follows a **Strategy pattern**:

- **`PlayerCCManagerBase`** — Abstract base class defining CC lifecycle (Init, SetStatus, SetTrack, SetStyle, trickplay/parental control)
- **`PlayerSubtecCCManager`** — Subtec-based implementation (HAL + PacketSender IPC)
- **`PlayerRialtoCCManager`** — Rialto-based implementation (GObject property control)
- **`CCDataController`** — Singleton controller for subtec CC data packets
- **`SubtecConnector`** — Namespace providing HAL init, packet sender init, and ccMgrAPI functions

---

## 1. CC Manager Initialization (Subtec Path)

```mermaid
sequenceDiagram
    participant App as InterfacePlayerRDK
    participant CCMgr as PlayerSubtecCCManager
    participant SubConn as subtecConnector
    participant HAL as vlhal_cc_Register
    participant DataCtrl as CCDataController
    participant PktSender as ClosedCaptionsChannel

    App->>CCMgr: Init(decoderHandle)
    CCMgr->>CCMgr: Initialize(handle) → store mCCHandle
    Note over CCMgr: Default track "CC1" set in constructor
    App->>CCMgr: GetId()
    CCMgr->>CCMgr: mId++, mIdSet.insert(mId)
    CCMgr-->>App: unique ID

    Note over App: First CC API call triggers lazy init
    App->>CCMgr: SetStatus(true)
    CCMgr->>CCMgr: EnsureInitialized()
    CCMgr->>CCMgr: EnsureHALInitialized()
    CCMgr->>SubConn: initHal(mCCHandle)
    SubConn->>HAL: vlhal_cc_Register(0, Instance, dataCb, decodeCb)
    HAL-->>SubConn: 0 (success)
    SubConn->>SubConn: media_closeCaptionStart(handle)
    SubConn-->>CCMgr: CC_VL_OS_API_RESULT_SUCCESS

    CCMgr->>CCMgr: EnsureRendererCommsInitialized()
    CCMgr->>SubConn: initPacketSender()
    SubConn->>PktSender: ClosedCaptionsChannel::InitComms()
    PktSender-->>SubConn: true
    SubConn-->>CCMgr: CC_VL_OS_API_RESULT_SUCCESS

    CCMgr->>CCMgr: StartRendering()
    CCMgr->>SubConn: ccMgrAPI::ccShow()
    SubConn->>DataCtrl: Instance()->sendUnmute()
    DataCtrl->>PktSender: channel.SendUnmutePacket()
```

---

## 2. CC Manager Initialization (Rialto Path)

```mermaid
sequenceDiagram
    participant App as InterfacePlayerRDK
    participant CCMgr as PlayerRialtoCCManager
    participant GObj as g_object_set

    App->>CCMgr: Init(subtitleControlHandle)
    CCMgr->>CCMgr: Initialize(handle)
    CCMgr->>CCMgr: mSubtitleControlHandle = handle
    alt Track is empty (first init)
        CCMgr->>CCMgr: SetTrack("CC1")
        CCMgr->>GObj: g_object_set(handle, "text-track-identifier", "CC1", NULL)
    else Handle changed
        CCMgr->>CCMgr: SetTrack(cachedTrack, mTrackFormat)
        CCMgr->>GObj: g_object_set(handle, "text-track-identifier", modified_track, NULL)
    end

    App->>CCMgr: GetId()
    CCMgr->>CCMgr: lock, mId++, mIdSet.insert(mId)
    CCMgr-->>App: unique ID
```

---

## 3. SetTrack Flow (Subtec — Base Class)

```mermaid
sequenceDiagram
    participant App
    participant CCMgr as PlayerCCManagerBase
    participant SubConn as subtecConnector::ccMgrAPI

    App->>CCMgr: SetTrack("CC1", format)
    CCMgr->>CCMgr: EnsureInitialized()
    CCMgr->>CCMgr: Parse track string
    alt Track starts with "CC" (analog 608)
        CCMgr->>CCMgr: Extract channel number (1-4)
        CCMgr->>CCMgr: SetAnalogChannel(channelNum)
        CCMgr->>SubConn: ccSetAnalogChannel(channelNum)
    else Track starts with "SERVICE" (digital 708)
        CCMgr->>CCMgr: Extract service number (1-63)
        CCMgr->>CCMgr: SetDigitalChannel(serviceNum)
        CCMgr->>SubConn: ccSetDigitalChannel(serviceNum)
    end
    CCMgr->>CCMgr: mTrack = track
```

---

## 4. SetTrack Flow (Rialto — Override)

```mermaid
sequenceDiagram
    participant App
    participant CCMgr as PlayerRialtoCCManager
    participant GObj as g_object_set

    App->>CCMgr: SetTrack("1", eCLOSEDCAPTION_FORMAT_608)
    CCMgr->>CCMgr: mTrack = "1", mTrackFormat = 608
    alt Track starts with digit AND format is 608
        CCMgr->>CCMgr: textTrackIdentifier = "CC" + "1" = "CC1"
    else Track starts with digit AND format is 708
        CCMgr->>CCMgr: textTrackIdentifier = "SERVICE" + track
    else Track already has prefix
        CCMgr->>CCMgr: textTrackIdentifier = track
    end
    alt mSubtitleControlHandle != nullptr
        CCMgr->>GObj: g_object_set(handle, "text-track-identifier", "CC1", NULL)
    else No handle
        CCMgr->>CCMgr: Log "track cached"
    end
```

---

## 5. CC Data Flow (Subtec — Runtime)

```mermaid
sequenceDiagram
    participant Decoder as HW Decoder
    participant HAL as vlhal_cc
    participant DataCtrl as CCDataController
    participant Channel as ClosedCaptionsChannel
    participant Renderer as Subtec Renderer

    Decoder->>HAL: CC data available
    HAL->>DataCtrl: closedCaptionDataCb(decoderIdx, type, ccData, len, seq, pts)
    DataCtrl->>Channel: SendDataPacketWithPTS(localPts, ccData, dataLength)
    Channel->>Renderer: IPC packet (via socket)
```

---

## 6. SetStyle Flow

```mermaid
sequenceDiagram
    participant App
    participant CCMgr as PlayerCCManagerBase
    participant JSON as PlayerJsonObject
    participant SubConn as subtecConnector::ccMgrAPI

    App->>CCMgr: SetStyle(optionsJSON)
    CCMgr->>CCMgr: EnsureInitialized()
    CCMgr->>JSON: Parse options string
    JSON-->>CCMgr: key-value pairs (fontSize, fontColor, bgColor, etc.)
    loop For each attribute
        CCMgr->>CCMgr: Map string value to gsw_CcAttributes field
        Note over CCMgr: getColor(), getFontSize(), getFontStyle(),<br/>getEdgeType(), getTextStyle(), getOpacity()
    end
    CCMgr->>SubConn: ccSetAttributes(&attribs, type, ccType)
    SubConn->>SubConn: CCDataController::sendCCSetAttribute()
    CCMgr->>CCMgr: mOptions = options
```

---

## 7. Release / Teardown (Subtec)

```mermaid
sequenceDiagram
    participant App
    participant CCMgr as PlayerSubtecCCManager
    participant SubConn as subtecConnector

    App->>CCMgr: Release(id)
    CCMgr->>CCMgr: lock, mIdSet.erase(id)
    alt mIdSet is now empty (last user)
        CCMgr->>SubConn: resetChannel()
        SubConn->>SubConn: CCDataController::sendResetChannelPacket()
        CCMgr->>SubConn: close()
        SubConn->>SubConn: media_closeCaptionStop()
        CCMgr->>CCMgr: mHALInitialized = false, mCCHandle = NULL
    end
```

---

## 8. Release / Teardown (Rialto)

```mermaid
sequenceDiagram
    participant App
    participant CCMgr as PlayerRialtoCCManager

    App->>CCMgr: Release(id)
    CCMgr->>CCMgr: lock, mIdSet.erase(id)
    alt mIdSet is now empty (last user)
        CCMgr->>CCMgr: ResetState()
        CCMgr->>CCMgr: PlayerCCManagerBase::ResetState()
        CCMgr->>CCMgr: mSubtitleControlHandle = nullptr
        Note over CCMgr: Instance can be reused later
    end
```

---

## 9. Trickplay / Parental Control

```mermaid
sequenceDiagram
    participant App
    participant CCMgr as PlayerCCManagerBase

    App->>CCMgr: SetTrickplayStatus(true)
    CCMgr->>CCMgr: mTrickplayStarted = true
    CCMgr->>CCMgr: StopRendering()

    App->>CCMgr: SetTrickplayStatus(false)
    CCMgr->>CCMgr: mTrickplayStarted = false
    alt mEnabled AND NOT mParentalCtrlLocked
        CCMgr->>CCMgr: StartRendering()
    end

    App->>CCMgr: SetParentalControlStatus(true)
    CCMgr->>CCMgr: mParentalCtrlLocked = true
    CCMgr->>CCMgr: StopRendering()

    App->>CCMgr: SetParentalControlStatus(false)
    CCMgr->>CCMgr: mParentalCtrlLocked = false
    alt mEnabled AND NOT mTrickplayStarted
        CCMgr->>CCMgr: StartRendering()
    end
```

---

## Class Hierarchy

```mermaid
classDiagram
    class PlayerCCManagerBase {
        <<abstract>>
        +Init(handle) int
        +SetStatus(enable) int
        +SetTrack(track, format) int
        +SetStyle(options) int
        +SetTrickplayStatus(enable)
        +SetParentalControlStatus(locked)
        +RestoreCC(shouldRestore)
        +GetId() int
        +Release(id)*
        #StartRendering()*
        #StopRendering()*
        #EnsureInitialized()
    }

    class PlayerSubtecCCManager {
        -mCCHandle
        -mRendererInitialized
        -mHALInitialized
        +GetId() int
        +Release(id)
        -StartRendering()
        -StopRendering()
        -EnsureInitialized()
        -EnsureHALInitialized()
        -EnsureRendererCommsInitialized()
        -SetDigitalChannel(id) int
        -SetAnalogChannel(id) int
    }

    class PlayerRialtoCCManager {
        -mSubtitleControlHandle
        -mTrackFormat
        +GetId() int
        +Release(id)
        +SetTrack(track, format) int
        -StartRendering()
        -StopRendering()
        -Initialize(handle) int
        -ResetState()
    }

    class CCDataController {
        <<singleton>>
        +Instance() CCDataController*
        +closedCaptionDataCb()
        +sendMute()
        +sendUnmute()
        +ccSetDigitalChannel(ch)
        +ccSetAnalogChannel(ch)
        +sendCCSetAttribute()
    }

    PlayerCCManagerBase <|-- PlayerSubtecCCManager
    PlayerCCManagerBase <|-- PlayerRialtoCCManager
    PlayerSubtecCCManager --> CCDataController : via subtecConnector
```

---

## Dependencies

| Component | Depends On |
|-----------|-----------|
| PlayerCCManager.cpp | PlayerLogManager, PlayerJsonObject, PlayerUtils, PlayerSubtecCCManager, PlayerRialtoCCManager |
| PlayerSubtecCCManager | SubtecConnector, PlayerLogManager |
| PlayerRialtoCCManager | glib-object (g_object_set), PlayerLogManager |
| CCDataController | ClosedCaptionsPacket.hpp, ccDataReader.h, SubtecConnector.h |
| SubtecConnector | CCDataController, ccDataReader, ClosedCaptionsChannel |
