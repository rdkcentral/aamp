# Root-Level Middleware — Sequence Diagrams

> **Source files read**: InterfacePlayerRDK.cpp (~4600 lines), InterfacePlayerRDK.h, InterfacePlayerPriv.h, GstUtils.h/cpp, GstHandlerControl.h/cpp, PlayerScheduler.h/cpp, PlayerUtils.h/cpp, ProcessHandler.h/cpp, SocUtils.h/cpp, MediaSample.h, DemuxDataTypes.h, PlayerMetadata.hpp, gstplayertaskpool.h  
> **Confidence**: 95% (remaining ~200 lines of InterfacePlayerRDK.cpp not yet read: SetVolumeOrMuteUnMute tail, bus_sync_handler, DumpDiagnostics, SetVideoZoom/Mute, SetTextStyle, NotifyEOS/FragmentCaching, EndOfStreamReached, SetStreamCaps, DecorateGstBufferWithDrmMetadata)

---

## 1. Pipeline Construction & Configuration

```mermaid
sequenceDiagram
    participant App as Application (AAMP Core)
    participant RDK as InterfacePlayerRDK
    participant Priv as InterfacePlayerPriv
    participant GstPriv as GstPlayerPriv
    participant Soc as SocInterface
    participant Sched as PlayerScheduler
    participant GStreamer as GStreamer

    App->>RDK: new InterfacePlayerRDK(isRialto)
    RDK->>Priv: new InterfacePlayerPriv(isRialto)
    Priv->>GstPriv: new GstPlayerPriv()
    Priv->>Soc: CreateSocInterface(isRialto)
    RDK->>Sched: StartScheduler()
    Sched->>Sched: spawn mSchedulerThread

    App->>RDK: ConfigurePipeline(format, audioFormat, subFormat, ...)
    RDK->>Soc: ShouldTearDownForTrickplay()
    RDK->>RDK: CreatePipeline(pipelineName, priority)
    RDK->>GStreamer: gst_pipeline_new(pipelineName)
    RDK->>GStreamer: gst_pipeline_get_bus()
    RDK->>GStreamer: gst_bus_add_watch(bus_message)
    RDK->>GStreamer: gst_bus_set_sync_handler(bus_sync_handler)
    RDK->>RDK: InterfacePlayer_SetupStream(video)
    RDK->>RDK: InterfacePlayer_SetupStream(audio)
    alt Subtitles enabled
        RDK->>RDK: InterfacePlayer_SetupStream(subtitle)
    else CC Control (Rialto, no subs)
        RDK->>RDK: SetupClosedCaptionControlStream()
    end
    alt Rialto Sink
        RDK->>GStreamer: gst_context_new("streams-info")
        RDK->>GStreamer: gst_element_set_context(pipeline, context)
    end
    RDK->>GStreamer: SetStateWithWarnings(pipeline, PLAYING/PAUSED)
```

---

## 2. Stream Setup (SetupStream / InterfacePlayer_SetupStream)

```mermaid
sequenceDiagram
    participant RDK as InterfacePlayerRDK
    participant Priv as InterfacePlayerPriv
    participant GstPriv as GstPlayerPriv
    participant Soc as SocInterface
    participant GStreamer as GStreamer

    RDK->>RDK: SetupStream(streamId, playerInstance, manifest)
    alt Subtitle with Rialto
        RDK->>GStreamer: gst_element_factory_make("playbin")
        RDK->>GStreamer: gst_element_factory_make("rialtomsesubtitlesink")
        RDK->>GStreamer: gst_element_factory_make("vipertransform")
        RDK->>GStreamer: gst_element_link(vipertransform, textsink)
    else Subtitle with subtecbin
        RDK->>GStreamer: gst_element_factory_make("subtecbin")
        RDK->>RDK: InterfacePlayerRDK_GetAppSrc(SUBTITLE)
    else Video/Audio
        RDK->>GStreamer: gst_element_factory_make("playbin")
        alt Rialto Video
            RDK->>GStreamer: gst_element_factory_make("rialtomsevideosink")
        else Rialto Audio
            RDK->>GStreamer: gst_element_factory_make("rialtomseaudiosink")
        else Westeros Video
            RDK->>Soc: GetVideoSink(sinkbin)
        end
    end
    RDK->>GStreamer: gst_bin_add(pipeline, sinkbin)
    RDK->>Soc: SetPlaybackFlags(flags, isSub)
    alt Non-progressive or AppSrc
        RDK->>GStreamer: g_object_set(sinkbin, "uri", "appsrc://")
        RDK->>Priv: SignalConnect("deep-notify::source", gst_found_source)
    else Progressive HTTP
        RDK->>GStreamer: g_object_set(sinkbin, "uri", manifestUrl)
        RDK->>Priv: SignalConnect("source-setup", httpsoup_source_setup)
    end
    RDK->>GStreamer: gst_element_sync_state_with_parent(sinkbin)
```

---

## 3. Buffer Injection (SendHelper)

```mermaid
sequenceDiagram
    participant App as AAMP Core
    participant RDK as InterfacePlayerRDK
    participant Priv as InterfacePlayerPriv
    participant GStreamer as GStreamer

    App->>RDK: SendHelper(mediaType, MediaSample&&, initFragment, ...)
    RDK->>RDK: pthread_mutex_lock(sourceLock)
    alt Source not configured
        RDK->>RDK: WaitForSourceSetup(mediaType)
    end
    alt First buffer (resetPosition)
        RDK->>Priv: SendGstEvents(mediaType, pts, ...)
        Priv->>GStreamer: gst_element_seek_simple(source, seekPosition)
        Priv->>Priv: SendQtDemuxOverrideEvent(mediaType, pts)
        Priv->>GStreamer: gst_pad_push_event(protectionEvent)
        alt Need new segment
            RDK->>Priv: SendNewSegmentEvent(mediaType, pts, 0)
            alt Rialto
                Priv->>GStreamer: gst_app_src_push_sample(segment)
            else Non-Rialto
                Priv->>GStreamer: gst_pad_push_event(segment_event)
            end
        end
    end
    RDK->>RDK: Create GstBuffer (zero-copy via shared_ptr)
    RDK->>GStreamer: GST_BUFFER_PTS/DTS/DURATION = ...
    alt Encrypted
        RDK->>RDK: DecorateGstBufferWithDrmMetadata(buffer, drmMetadata)
    end
    RDK->>GStreamer: gst_app_src_push_buffer(source, buffer)
    RDK->>RDK: pthread_mutex_unlock(sourceLock)
```

---

## 4. Pipeline Stop & Teardown

```mermaid
sequenceDiagram
    participant App as AAMP Core
    participant RDK as InterfacePlayerRDK
    participant Priv as InterfacePlayerPriv
    participant GstPriv as GstPlayerPriv
    participant HC as GstHandlerControl
    participant Sched as PlayerScheduler
    participant GStreamer as GStreamer

    App->>RDK: Stop(keepLastFrame)
    RDK->>RDK: lock(mMutex)
    RDK->>HC: syncControl.disable()
    RDK->>HC: aSyncControl.disable()
    RDK->>RDK: mSourceSetupCV.notify_all()
    RDK->>GStreamer: gst_bus_remove_watch(bus)
    RDK->>Sched: RemoveTask(progressCallbackId)
    RDK->>Sched: RemoveTask(eosCallbackId)
    RDK->>Sched: RemoveTask(firstFrameCallbackId)
    RDK->>HC: syncControl.waitForDone(50ms)
    RDK->>HC: aSyncControl.waitForDone(50ms)
    RDK->>HC: callbackControl.disable()
    RDK->>RDK: DisconnectSignals()
    RDK->>HC: callbackControl.waitForDone(100ms)
    RDK->>RDK: RemoveProbes()
    alt EOS injection mode == STOP_ONLY
        RDK->>GStreamer: GstPlayer_SignalEOS(all streams)
    end
    RDK->>GStreamer: SetStateWithWarnings(pipeline, NULL)
    loop For each track
        RDK->>RDK: TearDownStream(track)
        RDK->>GStreamer: SetStateWithWarnings(sinkbin, NULL)
        RDK->>GStreamer: gst_bin_remove(pipeline, sinkbin)
    end
    RDK->>RDK: DestroyPipeline()
    RDK->>GStreamer: gst_object_unref(pipeline)
    RDK->>GStreamer: gst_object_unref(bus)
```

---

## 5. Flush / Seek

```mermaid
sequenceDiagram
    participant App as AAMP Core
    participant RDK as InterfacePlayerRDK
    participant Priv as InterfacePlayerPriv
    participant Soc as SocInterface
    participant GStreamer as GStreamer

    App->>RDK: Flush(position, rate, shouldTearDown, isAppSeek)
    RDK->>RDK: SetSeekPosition(position)
    RDK->>GStreamer: gst_element_get_state(pipeline) → current
    alt Pipeline not PLAYING/PAUSED
        alt shouldTearDown
            RDK->>App: stopCallback(true)
        end
        RDK-->>App: return false
    end
    RDK->>RDK: ResetGstEvents()
    alt Non-Rialto
        RDK->>Soc: DisableAsyncAudio(audio_sink, rate)
        RDK->>GStreamer: GstPlayer_SignalEOS(audio stream)
    end
    RDK->>GStreamer: gst_element_seek(pipeline, rate, FLUSH, position*GST_SECOND)
    alt Rialto + trickplay
        RDK->>GStreamer: GstPlayer_SignalEOS(audio stream)
    end
```

---

## 6. First Frame Notification Flow

```mermaid
sequenceDiagram
    participant GStreamer as GStreamer
    participant RDK as InterfacePlayerRDK
    participant Priv as InterfacePlayerPriv
    participant Sched as PlayerScheduler
    participant App as AAMP Core

    GStreamer->>RDK: GstPlayer_OnFirstVideoFrameCallback()
    Note over RDK: HANDLER_CONTROL_HELPER_CALLBACK_VOID
    RDK->>RDK: firstVideoFrameReceived = true
    RDK->>RDK: NotifyFirstFrame(VIDEO)
    RDK->>App: notifyFirstFrameCallback(VIDEO, ...)
    RDK->>Sched: ScheduleTask(IdleCallbackOnFirstFrame)
    RDK->>Sched: ScheduleTask(IdleCallback → progress timer)

    Sched->>RDK: IdleCallbackOnFirstFrame()
    RDK->>App: TriggerEvent(firstVideoFrameReceived)

    Sched->>RDK: IdleCallback()
    RDK->>App: TriggerEvent(idleCb)
    RDK->>RDK: TimerAdd(ProgressCallbackOnTimeout, interval)
```

---

## 7. Bus Message Handling

```mermaid
sequenceDiagram
    participant GStreamer as GStreamer Bus
    participant RDK as InterfacePlayerRDK
    participant Priv as InterfacePlayerPriv
    participant Soc as SocInterface
    participant App as AAMP Core

    GStreamer->>RDK: bus_message(bus, msg)
    Note over RDK: HANDLER_CONTROL_HELPER(aSyncControl)
    alt GST_MESSAGE_ERROR
        RDK->>App: busMessageCallback(MESSAGE_ERROR, msg, dbg)
    else GST_MESSAGE_STATE_CHANGED (pipeline→PLAYING)
        RDK->>Soc: SetPlatformPlaybackRate()
        alt Rialto
            RDK->>RDK: NotifyFirstFrame(VIDEO)
        end
        RDK->>RDK: IdleTaskAdd(firstProgressCallback)
        alt First tune with westeros off
            RDK->>RDK: NotifyFirstFrame(VIDEO)
        end
        RDK->>App: busMessageCallback(MESSAGE_STATE_CHANGE)
    else GST_MESSAGE_EOS
        RDK->>App: busMessageCallback(MESSAGE_EOS)
        RDK->>RDK: NotifyEOS()
    else GST_MESSAGE_CLOCK_LOST (non-DASH)
        RDK->>GStreamer: SetState(PAUSED) then SetState(PLAYING)
    end
```

---

## 8. EOS / Underflow Handling

```mermaid
sequenceDiagram
    participant GStreamer as GStreamer
    participant RDK as InterfacePlayerRDK
    participant Priv as InterfacePlayerPriv
    participant App as AAMP Core

    Note over GStreamer: Underflow signal from decoder/sink
    GStreamer->>RDK: GstPlayer_OnGstBufferUnderflowCb()
    RDK->>RDK: stream[type].bufferUnderrun = true
    alt EOS reached & normal rate
        RDK->>RDK: GetVideoPTS() → lastKnownPTS
        RDK->>GStreamer: g_timeout_add(500ms, VideoDecoderPtsCheckerForEOS)
        Note over GStreamer: After 500ms...
        GStreamer->>RDK: VideoDecoderPtsCheckerForEOS()
        alt PTS unchanged
            RDK->>RDK: NotifyEOS()
            RDK->>App: TriggerEvent(notifyEOS)
        end
    end
    RDK->>App: OnGstBufferUnderflowCb(mediaType)
```

---

## 9. Pause / Resume

```mermaid
sequenceDiagram
    participant App as AAMP Core
    participant RDK as InterfacePlayerRDK
    participant GStreamer as GStreamer

    App->>RDK: Pause(true, forceStop)
    RDK->>GStreamer: SetStateWithWarnings(pipeline, PAUSED)
    alt GST_STATE_CHANGE_ASYNC
        RDK->>RDK: validateStateWithMsTimeout(PAUSED, 100ms)
    end
    RDK->>RDK: paused = true

    App->>RDK: Pause(false, forceStop)
    RDK->>GStreamer: SetStateWithWarnings(pipeline, PLAYING)
    RDK->>RDK: paused = false
```

---

## 10. GstHandlerControl Pattern

```mermaid
sequenceDiagram
    participant Caller as Any Thread
    participant HC as GstHandlerControl
    participant Handler as GStreamer Callback

    Note over HC: mEnabled=true, mInstanceCount=0

    Caller->>HC: disable()
    Note over HC: mEnabled=false

    Handler->>HC: getScopeHelper()
    HC-->>Handler: ScopeHelper(this), mInstanceCount++
    Handler->>HC: returnStraightAway() → true (disabled)
    Handler-->>Handler: return immediately
    Note over HC: ~ScopeHelper() → handlerEnd() → mInstanceCount--

    Caller->>HC: waitForDone(50ms, "bus_sync_handler")
    HC->>HC: disable() + wait(mInstanceCount==0)
    HC-->>Caller: true (all done)
```

---

## 11. PlayerScheduler Task Lifecycle

```mermaid
sequenceDiagram
    participant Client as InterfacePlayerRDK
    participant Sched as PlayerScheduler
    participant Worker as SchedulerThread

    Client->>Sched: ScheduleTask(taskObj)
    Sched->>Sched: lock(mQMutex), push_back(task), notify
    Sched-->>Client: taskId

    Worker->>Sched: wait(mQCond)
    Note over Worker: wakes up
    Worker->>Sched: lock(mExMutex)
    Worker->>Sched: pop_front() → task
    Worker->>Worker: task.mTask(task.mData)
    Note over Worker: unlock mExMutex

    Client->>Sched: SuspendScheduler()
    Sched->>Sched: mExLock.lock()
    Note over Worker: blocked on mExMutex

    Client->>Sched: RemoveAllTasks()
    Sched->>Sched: mTaskQueue.clear()

    Client->>Sched: ResumeScheduler()
    Sched->>Sched: mExLock.unlock()
    Note over Worker: resumes
```

---

## 12. Protection Event / DRM Queue

```mermaid
sequenceDiagram
    participant App as AAMP Core
    participant RDK as InterfacePlayerRDK
    participant GstPriv as GstPlayerPriv
    participant GStreamer as GStreamer

    App->>RDK: QueueProtectionEvent(formatType, systemId, initData, size, mediaType)
    RDK->>RDK: lock(mProtectionLock)
    RDK->>GStreamer: gst_buffer_new_wrapped(initData copy)
    RDK->>GStreamer: gst_event_new_protection(systemId, pssi, format)
    RDK->>GstPriv: protectionEvent[type] = event
    RDK->>RDK: unlock(mProtectionLock)

    Note over RDK: Later, during SendHelper (first buffer)...
    RDK->>Priv: SendGstEvents() 
    Priv->>GStreamer: gst_pad_push_event(protectionEvent)
```

---

## Module Dependency Summary

```mermaid
graph TD
    A[InterfacePlayerRDK] --> B[InterfacePlayerPriv]
    B --> C[GstPlayerPriv]
    B --> D[SocInterface]
    A --> E[PlayerScheduler]
    A --> F[GstHandlerControl]
    A --> G[GstUtils]
    A --> H[PlayerUtils]
    A --> I[SocUtils]
    I --> D
    A --> J[MediaSample]
    J --> K[DemuxDataTypes]
    A --> L[PlayerLogManager]
    A --> M[gstplayertaskpool]
    A --> N[PlayerExternalsInterface]
```
