# 02 — GStreamer Pipeline (AAMPGstPlayer)

> **Source files read**: `aampgstplayer.h` (lines 1-400), `aampgstplayer.cpp` (lines 1-1500 — complete)
> **Confidence**: 100%

## Overview

`AAMPGstPlayer` is the StreamSink implementation that manages the GStreamer pipeline via `InterfacePlayerRDK`. It handles:
- Pipeline creation and configuration
- Buffer injection (Send/SendHelper)
- Playback state transitions (Play/Pause/Stop/Flush)
- Buffer flow control (NeedData/EnoughData/Underflow)
- Error handling (decode errors, PTS errors, bus messages)
- DRM protection events
- First frame notification chain
- AV monitoring

## 1. Pipeline Construction & Configuration

```mermaid
sequenceDiagram
    participant AAMP as PrivateInstanceAAMP
    participant GstPlayer as AAMPGstPlayer
    participant Interface as InterfacePlayerRDK
    participant GstPipeline as GStreamer Pipeline

    Note over AAMP,GstPipeline: Constructor Flow
    AAMP->>GstPlayer: new AAMPGstPlayer(aamp, id3Callback, exportFrames)
    GstPlayer->>GstPlayer: privateContext = new AAMPGstPlayerPriv()
    GstPlayer->>Interface: new InterfacePlayerRDK(useRialtoSink)
    GstPlayer->>Interface: RegisterBusCb() [underflow, bus, decode error, PTS error, buffering timeout, red button, needData, enoughData]
    GstPlayer->>Interface: EnableGstDebugLogging(debugLevel)
    GstPlayer->>Interface: InitializePlayerConfigs() [media format, proxy, TCP sink, PTS restamp, video/audio buf bytes, westeros, rialto, etc.]
    GstPlayer->>Interface: SetPlayerName("AAMPGstPlayerPipeline")
    GstPlayer->>Interface: setEncryption(aamp, drmSessionManager)
    GstPlayer->>GstPlayer: RegisterFirstFrameCallbacks()

    Note over AAMP,GstPipeline: Configure Pipeline (called from TuneHelper)
    AAMP->>GstPlayer: Configure(videoFormat, audioFormat, subFormat, bESChange, setReady)
    GstPlayer->>Interface: SetPreferredDRM(drmSystemId)
    GstPlayer->>Interface: InitializePlayerConfigs()
    GstPlayer->>Interface: ConfigurePipeline(video, audio, sub, ESChange, setReady, subEnabled, trackId, rate, name, priority, firstFrameFlag, manifestUrl, liveRateCorrection)
    Interface->>GstPipeline: Create/configure elements
    GstPlayer->>GstPlayer: StartMonitorAvTimer()
```

## 2. Buffer Injection Flow

```mermaid
sequenceDiagram
    participant Collector as FragmentCollector
    participant GstPlayer as AAMPGstPlayer
    participant Interface as InterfacePlayerRDK
    participant BufferCtrl as BufferControl

    Note over Collector,BufferCtrl: Fragment Injection (MP4 path)
    Collector->>GstPlayer: SendTransfer(mediaType, buffer, pts, dts, duration, ptsOffset, initFragment, discontinuity)
    GstPlayer->>GstPlayer: Create MediaSample(buffer, pts, dts, duration, ptsOffset)
    GstPlayer->>GstPlayer: SendHelper(mediaType, sample, initFragment, discontinuity)

    Note over GstPlayer: Validation
    GstPlayer->>GstPlayer: Check sample.data() != null && sample.size() > 0
    GstPlayer->>GstPlayer: Check SuppressDecode config

    Note over GstPlayer: ID3 metadata check
    GstPlayer->>GstPlayer: IsValidMediaType(mediaType) && IsValidHeader(data, size)
    GstPlayer->>GstPlayer: m_ID3MetadataHandler(mediaType, data, size, timestamps)

    Note over GstPlayer: New segment event
    GstPlayer->>GstPlayer: Check mbNewSegmentEvtSent[mediaType]
    GstPlayer->>Interface: SendHelper(mediaType, sample, initFragment, discontinuity, ...)
    Interface-->>GstPlayer: bPushBuffer = true

    GstPlayer->>GstPlayer: aamp->mbNewSegmentEvtSent[mediaType] = true
    GstPlayer->>GstPlayer: aamp->profiler.ProfilePerformed(FIRST_BUFFER) [if firstBufferPushed]
    GstPlayer->>BufferCtrl: notifyFragmentInject(mediaType, pts, dts, duration, discontinuity)

    Note over GstPlayer: Video-specific post-injection
    GstPlayer->>GstPlayer: NotifyFirstBufferProcessed(videoRect) [if notifyFirstBufferProcessed]
    GstPlayer->>GstPlayer: ResetTrickStartUTCTime() [if resetTrickUTC]
    GstPlayer->>GstPlayer: StopBuffering(false) [if underflow monitor disabled]
```

## 3. Buffer Flow Control (NeedData / EnoughData / Underflow)

```mermaid
sequenceDiagram
    participant GstPipeline as GStreamer Pipeline
    participant Interface as InterfacePlayerRDK
    participant GstPlayer as AAMPGstPlayer
    participant BufferCtrl as BufferControl
    participant AAMP as PrivateInstanceAAMP

    Note over GstPipeline,AAMP: AppSrc needs data
    GstPipeline->>Interface: need-data signal
    Interface->>GstPlayer: NeedData(mediaType)
    GstPlayer->>BufferCtrl: mBufferControl[media].needData(player, media)
    BufferCtrl-->>AAMP: Resume fragment fetching

    Note over GstPipeline,AAMP: AppSrc has enough data
    GstPipeline->>Interface: enough-data signal
    Interface->>GstPlayer: EnoughData(mediaType)
    GstPlayer->>BufferCtrl: mBufferControl[media].enoughData(player, media)
    BufferCtrl-->>AAMP: Pause fragment fetching

    Note over GstPipeline,AAMP: Buffer Underflow
    GstPipeline->>Interface: underflow signal
    Interface->>GstPlayer: HandleOnGstBufferUnderflowCb(mediaType)
    GstPlayer->>BufferCtrl: isBufferFull(type)
    GstPlayer->>BufferCtrl: underflow(player, type)
    alt AampUnderflowMonitor enabled
        GstPlayer->>GstPlayer: Skip retune (handled by monitor)
    else
        GstPlayer->>AAMP: ScheduleRetune(eGST_ERROR_UNDERFLOW, type, isBufferFull)
    end
```

## 4. Playback State Transitions

```mermaid
sequenceDiagram
    participant AAMP as PrivateInstanceAAMP
    participant GstPlayer as AAMPGstPlayer
    participant Interface as InterfacePlayerRDK

    Note over AAMP,Interface: Pause
    AAMP->>GstPlayer: Pause(pause, forceStopPreBuffering)
    GstPlayer->>GstPlayer: aamp->SyncLock()
    GstPlayer->>Interface: Pause(pause, forceStopPreBuffering)
    Interface-->>GstPlayer: success
    GstPlayer->>AAMP: PauseSubtitleParser(pause) [if !gstreamerSubsEnabled]

    Note over AAMP,Interface: Flush (Seek)
    AAMP->>GstPlayer: Flush(position, rate, shouldTearDown)
    GstPlayer->>Interface: Flush(position, rate, shouldTearDown, isAppSeek)
    Interface-->>GstPlayer: success
    GstPlayer->>GstPlayer: mBufferControl[i].flush() [all tracks]

    Note over AAMP,Interface: Stop
    AAMP->>GstPlayer: Stop(keepLastFrame)
    GstPlayer->>GstPlayer: aamp->SyncLock()
    GstPlayer->>GstPlayer: StopMonitorAvTimer()
    GstPlayer->>Interface: Stop(keepLastFrame)
    GstPlayer->>GstPlayer: aamp->seiTimecode = ""

    Note over AAMP,Interface: Destructor
    AAMP->>GstPlayer: ~AAMPGstPlayer()
    GstPlayer->>GstPlayer: UnregisterBusCb()
    GstPlayer->>GstPlayer: UnregisterFirstFrameCallbacks()
    GstPlayer->>Interface: DestroyPipeline()
    GstPlayer->>GstPlayer: delete playerInstance
    GstPlayer->>GstPlayer: delete privateContext
```

## 5. Error Handling (Bus Messages)

```mermaid
sequenceDiagram
    participant GstPipeline as GStreamer Pipeline
    participant Interface as InterfacePlayerRDK
    participant GstPlayer as AAMPGstPlayer
    participant AAMP as PrivateInstanceAAMP

    GstPipeline->>Interface: Bus message
    Interface->>GstPlayer: HandleBusMessage(busEvent)

    alt MESSAGE_ERROR
        alt "video decode error"
            GstPlayer->>AAMP: SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, desc, false)
        else "HDCP Compliance Check Failure"
            GstPlayer->>AAMP: SendErrorEvent(AAMP_TUNE_HDCP_COMPLIANCE_ERROR, desc, false)
        else "Internal data stream error" + RetuneForGSTError
            GstPlayer->>AAMP: ScheduleRetune(eGST_ERROR_GST_PIPELINE_INTERNAL, VIDEO)
        else "corrupt file"
            GstPlayer->>AAMP: SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, desc, false)
        else other
            GstPlayer->>AAMP: SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, desc)
        end
    else MESSAGE_WARNING + "No decoder available" + DecoderUnavailableStrict
        GstPlayer->>AAMP: SendErrorEvent(AAMP_TUNE_GST_PIPELINE_ERROR, desc, false)
    else MESSAGE_STATE_CHANGE
        GstPlayer->>AAMP: NotifyFirstBufferProcessed() [if firstBufferProcessed]
        GstPlayer->>AAMP: LogFirstFrame() + LogTuneComplete() [if receivedFirstFrame]
        GstPlayer->>GstPlayer: SetPlayBackRate() [if rate pending after first frame]
    else MESSAGE_APPLICATION + "HDCPProtectionFailure"
        GstPlayer->>GstPlayer: SetVideoMute(true)
        GstPlayer->>AAMP: ScheduleRetune(eGST_ERROR_OUTPUT_PROTECTION_ERROR, VIDEO)
    end
```

## 6. First Frame Notification Chain

```mermaid
sequenceDiagram
    participant Interface as InterfacePlayerRDK
    participant GstPlayer as AAMPGstPlayer
    participant AAMP as PrivateInstanceAAMP

    Interface->>GstPlayer: FirstFrameCallback(mediatype, notifyFirstBuffer, initCC, ...)
    GstPlayer->>GstPlayer: NotifyFirstFrame(mediatype, ...)

    alt notifyFirstBuffer && !flushInProgress
        GstPlayer->>AAMP: LogFirstFrame()
        GstPlayer->>AAMP: LogTuneComplete()
        GstPlayer->>AAMP: NotifyFirstBufferProcessed(videoRect)
    end

    alt eMEDIATYPE_VIDEO
        GstPlayer->>AAMP: SetDiscontinuityParam() [if telemetry + discontinuity]
        GstPlayer->>AAMP: NotifyFirstBufferProcessed(videoRect) [if not already notified]
        GstPlayer->>AAMP: InitializeCC(ccDecoderHandle) [if initCC]
    end

    Note over Interface,AAMP: Other registered callbacks
    Interface->>GstPlayer: firstVideoFrameDisplayed → aamp->NotifyFirstVideoFrameDisplayed()
    Interface->>GstPlayer: firstVideoFrameReceived → aamp->NotifyFirstFrameReceived(ccHandle)
    Interface->>GstPlayer: notifyEOS → aamp->NotifyEOSReached()
    Interface->>GstPlayer: progressCb → mBufferControl[i].update() + MonitorProgress()
    Interface->>GstPlayer: idleCb → MonitorProgress()
```

## 7. DRM Protection Events

```mermaid
sequenceDiagram
    participant AAMP as PrivateInstanceAAMP
    participant GstPlayer as AAMPGstPlayer
    participant Interface as InterfacePlayerRDK

    AAMP->>GstPlayer: QueueProtectionEvent(protSystemId, initData, size, type)
    GstPlayer->>GstPlayer: formatType = IsDashAsset() ? "dash/mpd" : "hls/m3u8"
    GstPlayer->>Interface: QueueProtectionEvent(formatType, protSystemId, initData, size, type)

    AAMP->>GstPlayer: ClearProtectionEvent()
    GstPlayer->>Interface: ClearProtectionEvent()

    AAMP->>GstPlayer: SetEncryptedAamp(aamp)
    GstPlayer->>GstPlayer: mEncryptedAamp = aamp
    GstPlayer->>Interface: setEncryption(aamp, drmSessionManager)
```

## 8. Discontinuity Handling

```mermaid
sequenceDiagram
    participant AAMP as PrivateInstanceAAMP
    participant GstPlayer as AAMPGstPlayer
    participant Interface as InterfacePlayerRDK

    AAMP->>GstPlayer: Discontinuity(type)
    GstPlayer->>Interface: CheckDiscontinuity(type, videoFormat, reconfigure, ...)
    Interface-->>GstPlayer: result

    alt PTS-RESTAMP enabled + codec unchanged
        GstPlayer->>AAMP: CompleteDiscontinuityDataDeliverForPTSRestamp(type)
    else shouldHaltBuffering (codec change)
        GstPlayer->>GstPlayer: StopBuffering(true)
        alt AampUnderflowMonitor enabled
            GstPlayer->>AAMP: mpStreamAbstractionAAMP->NotifyPipelinePausedToUnderflowMonitor()
        end
    end
```

## Key Interfaces

| Method | Purpose |
|--------|---------|
| `Configure()` | Create/configure pipeline based on A/V/Sub formats |
| `SendTransfer()` / `SendCopy()` | Inject fragments (MP4 / TS elementary) |
| `Flush()` | Seek — flush buffers, reset buffer control |
| `Pause()` | Pause/resume pipeline |
| `Stop()` | Stop pipeline, clear timers |
| `EndOfStreamReached()` | Signal EOS for a track |
| `Discontinuity()` | Handle stream discontinuity |
| `SetPlayBackRate()` | Trick play / live latency correction |
| `StopBuffering()` | Un-pause after buffer recovery |
| `GetPositionMilliseconds()` | Query playback position |
| `SetVideoRectangle()` | Set display coordinates |
