# 13. Stream Sink Manager — Sequence Diagrams

**Source Files Read**:
- `AampStreamSinkManager.h` (complete)
- `AampStreamSinkManager.cpp` (complete — 500+ lines)
- `AampStreamSinkInactive.h` (complete)
- `StreamSink.h` (complete)
- `priv_aamp.cpp` all StreamSink usage (complete)

**Confidence: 100%**

---

## 1. Sink Acquisition and Pipeline Configuration

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant SinkMgr as AampStreamSinkManager
    participant Sink as AAMPGstPlayer
    participant Inactive as AampStreamSinkInactive

    Priv->>SinkMgr: GetInstance() [singleton]
    Priv->>SinkMgr: GetStreamSink(this)
    alt Player is active & sink exists
        SinkMgr-->>Priv: Return existing AAMPGstPlayer
    else Player inactive
        SinkMgr-->>Priv: Return AampStreamSinkInactive (no-op)
    end
    Priv->>Sink: Configure(videoFormat, audioFormat, subtitleFormat, forwardAudioToAux)
    Sink->>Sink: Create/reconfigure GStreamer pipeline elements
    Sink-->>Priv: Pipeline ready
`

## 2. Player Activation/Deactivation

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant SinkMgr as AampStreamSinkManager
    participant Sink as AAMPGstPlayer

    Note over SinkMgr: On Tune:
    Priv->>SinkMgr: ActivatePlayer(this)
    SinkMgr->>SinkMgr: Check single pipeline mode
    alt Single pipeline mode
        SinkMgr->>SinkMgr: Reuse shared pipeline
    else Multi-pipeline mode
        SinkMgr->>Sink: new AAMPGstPlayer(this)
    end
    SinkMgr->>SinkMgr: Map player → sink
    SinkMgr-->>Priv: Sink activated

    Note over SinkMgr: On Stop:
    Priv->>SinkMgr: DeactivatePlayer(this, fullStop)
    alt fullStop = true
        SinkMgr->>Sink: Stop(keepPipeline=false)
        SinkMgr->>SinkMgr: Remove player → sink mapping
        SinkMgr->>Sink: delete sink
    else fullStop = false (detach)
        SinkMgr->>Sink: Stop(keepPipeline=true)
        SinkMgr->>SinkMgr: Mark player as background
    end
`

## 3. Single Pipeline Mode

`mermaid
sequenceDiagram
    participant App as Application
    participant SinkMgr as AampStreamSinkManager
    participant FG as ForegroundPlayer
    participant BG as BackgroundPlayer
    participant Sink as SharedPipeline

    App->>SinkMgr: SetSinglePipelineMode(foregroundPlayer)
    SinkMgr->>SinkMgr: singlePipelineMode = true
    SinkMgr->>SinkMgr: Assign shared sink to foreground

    Note over SinkMgr: On player switch:
    App->>FG: detach()
    FG->>SinkMgr: DeactivatePlayer(FG, false)
    SinkMgr->>SinkMgr: FG becomes background

    App->>BG: Tune(url)
    BG->>SinkMgr: ActivatePlayer(BG)
    SinkMgr->>SinkMgr: Transfer shared sink to BG
    SinkMgr->>Sink: Reconfigure for new stream
    BG->>Sink: Configure(newFormats)
`

## 4. Sink Interface Methods (StreamSink.h)

`mermaid
sequenceDiagram
    participant SA as StreamAbstractionAAMP
    participant Priv as PrivateInstanceAAMP
    participant Sink as StreamSink

    SA->>Priv: SendStreamCopy(mediaType, data, pts, duration)
    Priv->>Sink: SendCopy(mediaType, data, pts, duration)
    Sink->>Sink: Queue buffer for GStreamer injection

    SA->>Priv: EndOfStreamReached(mediaType)
    Priv->>Sink: EndOfStreamReached(mediaType)
    Sink->>Sink: Push EOS event to pipeline

    Priv->>Sink: Flush(position, rate)
    Sink->>Sink: Flush GStreamer pipeline, seek to position

    Priv->>Sink: SetVideoRectangle(x, y, w, h)
    Priv->>Sink: SetVideoMute(muted)
    Priv->>Sink: SetAudioVolume(volume)
    Priv->>Sink: Stop(keepPipeline)
`

---

## Key Implementation Details

| Aspect | Implementation |
|--------|---------------|
| **Pattern** | Singleton manager with player→sink mapping |
| **Inactive sink** | No-op implementation (AampStreamSinkInactive) for stopped players |
| **Single pipeline** | Shared GStreamer pipeline across foreground/background players |
| **Thread safety** | Mutex-protected sink access in all PrivateInstanceAAMP methods |
| **Sink types** | AAMPGstPlayer (primary), AampStreamSinkInactive (no-op) |
| **Lifecycle** | Activate on Tune, Deactivate on Stop/Detach |
