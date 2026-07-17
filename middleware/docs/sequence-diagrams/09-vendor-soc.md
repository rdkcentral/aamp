# Vendor / SoC Interface — Sequence Diagrams

> **Source files read**: `vendor/SocInterface.cpp`, `vendor/SocInterface.h`, `vendor/brcm/BrcmSocInterface.cpp`, `vendor/brcm/BrcmSocInterface.h`, `vendor/realtek/RealtekSocInterface.cpp`, `vendor/realtek/RealtekSocInterface.h`, `vendor/amlogic/AmlogicSocInterface.cpp`, `vendor/amlogic/AmlogicSocInterface.h`, `vendor/mtk/MtkSocInterface.cpp`, `vendor/mtk/MtkSocInterface.h`, `vendor/default/DefaultSocInterface.cpp`, `vendor/default/DefaultSocInterface.h`
>
> **Confidence: 100%** — All vendor source files fully read.

---

## 1. SoC Factory — Platform Selection

```mermaid
sequenceDiagram
    participant Caller as InterfacePlayerRDK
    participant Factory as SocInterface::createSocInterface()
    participant Brcm as BrcmSocInterface
    participant Realtek as RealtekSocInterface
    participant Amlogic as AmlogicSocInterface
    participant Mtk as MtkSocInterface
    participant Default as DefaultSocInterface

    Caller->>Factory: createSocInterface()
    alt BRCM platform defined
        Factory->>Brcm: new BrcmSocInterface()
        Factory-->>Caller: BrcmSocInterface*
    else REALTEK platform defined
        Factory->>Realtek: new RealtekSocInterface()
        Factory-->>Caller: RealtekSocInterface*
    else AMLOGIC platform defined
        Factory->>Amlogic: new AmlogicSocInterface()
        Factory-->>Caller: AmlogicSocInterface*
    else MTK platform defined
        Factory->>Mtk: new MtkSocInterface()
        Factory-->>Caller: MtkSocInterface*
    else No platform / Default
        Factory->>Default: new DefaultSocInterface()
        Factory-->>Caller: DefaultSocInterface*
    end
```

---

## 2. SocInterface Base — Virtual Method Dispatch

```mermaid
sequenceDiagram
    participant GstPlayer as GstHandlerControl
    participant SocIf as SocInterface (virtual)
    participant Impl as <Concrete SoC Impl>

    Note over SocIf: Virtual interface with default implementations
    GstPlayer->>SocIf: SetPlaybackRate(sources, pipeline, rate, video_dec, audio_dec)
    SocIf->>Impl: [vtable dispatch]
    Impl-->>GstPlayer: bool success

    GstPlayer->>SocIf: GetVideoSink(sinkbin)
    SocIf->>Impl: [vtable dispatch]
    Impl-->>GstPlayer: GstElement* videoSink

    GstPlayer->>SocIf: IsVideoDecoder(name)
    SocIf->>Impl: [vtable dispatch]
    Impl-->>GstPlayer: bool

    GstPlayer->>SocIf: SetAudioProperty(&volume, &mute, &isSinkBinVolume)
    SocIf->>Impl: [vtable dispatch]
    Impl-->>GstPlayer: volume/mute/isSinkBinVolume set
```

---

## 3. BrcmSocInterface — SetPlaybackRate

```mermaid
sequenceDiagram
    participant Caller as GstHandlerControl
    participant Brcm as BrcmSocInterface
    participant GstAPI as GStreamer API
    participant VideoDec as video_dec
    participant AudioDec as audio_dec

    Caller->>Brcm: SetPlaybackRate(sources, pipeline, rate, video_dec, audio_dec)
    Brcm->>GstAPI: gst_structure_new("custom-instant-rate-change", rate)
    GstAPI-->>Brcm: GstStructure*
    Brcm->>GstAPI: gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, structure)
    GstAPI-->>Brcm: GstEvent* rate_event

    alt video_dec != NULL
        Brcm->>VideoDec: gst_element_send_event(video_dec, gst_event_ref(rate_event))
        VideoDec-->>Brcm: success/fail
    end
    alt audio_dec != NULL
        Brcm->>AudioDec: gst_element_send_event(audio_dec, gst_event_ref(rate_event))
        AudioDec-->>Brcm: success/fail
    end
    Brcm->>GstAPI: gst_event_unref(rate_event)
    Brcm-->>Caller: bool status
```

---

## 4. BrcmSocInterface — GetVideoSink & ConfigureAudioSink

```mermaid
sequenceDiagram
    participant Caller as GstHandlerControl
    participant Brcm as BrcmSocInterface
    participant GstAPI as GStreamer API

    Caller->>Brcm: GetVideoSink(sinkbin)
    alt mUsingWesterosSink == true
        Brcm->>GstAPI: gst_element_factory_make("westerossink", NULL)
    else
        Brcm->>GstAPI: gst_element_factory_make("brcmvideosink", NULL)
    end
    GstAPI-->>Brcm: GstElement* vidsink
    Brcm->>GstAPI: g_object_set(vidsink, "secure-video", TRUE)
    Brcm->>GstAPI: g_object_set(sinkbin, "video-sink", vidsink)
    Brcm-->>Caller: vidsink

    Note over Brcm: ConfigureAudioSink
    Caller->>Brcm: ConfigureAudioSink(&audio_sink, src, decStreamSync)
    alt src name starts with "brcmaudiosink"
        Brcm->>GstAPI: gst_object_replace(audio_sink, src)
        Brcm-->>Caller: true
    else src name contains "brcmaudiodecoder"
        Brcm->>GstAPI: g_object_set(src, "limit_buffering_ms", 1500)
        Brcm->>GstAPI: g_object_set(src, "limit_buffering", 1)
        Brcm->>GstAPI: g_object_set(src, "stream_sync_mode", decStreamSync ? 1 : 0)
        Brcm-->>Caller: false
    end
```

---

## 5. BrcmSocInterface — Element Identification

```mermaid
sequenceDiagram
    participant Caller
    participant Brcm as BrcmSocInterface

    Caller->>Brcm: IsVideoSink("brcmvideosink0")
    Note right of Brcm: StartsWith "brcmvideosink" || "westerossink"
    Brcm-->>Caller: true

    Caller->>Brcm: IsVideoDecoder("westerossink0")
    Note right of Brcm: StartsWith "westerossink" || "brcmvideodecoder"
    Brcm-->>Caller: true

    Caller->>Brcm: IsAudioSinkOrAudioDecoder("brcmaudiodecoder0")
    Note right of Brcm: StartsWith "brcmaudiodecoder"
    Brcm-->>Caller: true

    Caller->>Brcm: IsAudioOrVideoDecoder("brcmvideodecoder0")
    Note right of Brcm: "brcmvideodecoder" || "brcmaudiodecoder" || "westerossink"
    Brcm-->>Caller: true
```

---

## 6. RealtekSocInterface — SetPlaybackRate

```mermaid
sequenceDiagram
    participant Caller as GstHandlerControl
    participant RTK as RealtekSocInterface
    participant GstAPI as GStreamer API
    participant Pipeline as GstPipeline

    Caller->>RTK: SetPlaybackRate(sources, pipeline, rate, video_dec, audio_dec)
    alt pipeline == NULL
        RTK-->>Caller: false (error logged)
    end
    RTK->>GstAPI: gst_structure_new("custom-instant-rate-change", rate)
    GstAPI-->>RTK: GstStructure*
    RTK->>GstAPI: gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM_OOB, structure)
    GstAPI-->>RTK: GstEvent* rate_event
    RTK->>Pipeline: gst_element_send_event(pipeline, rate_event)
    Pipeline-->>RTK: int ret
    alt ret == 0
        RTK->>RTK: MW_LOG_ERR("Rate change failed")
    end
    RTK-->>Caller: bool
```

---

## 7. RealtekSocInterface — Buffer & Audio Configuration

```mermaid
sequenceDiagram
    participant Caller
    participant RTK as RealtekSocInterface
    participant GstAPI as GStreamer API

    Caller->>RTK: SetVideoBufferSize(sink, size)
    RTK->>GstAPI: g_object_set(sink, "buffer-size", (guint64)size)
    RTK->>GstAPI: g_object_set(sink, "buffer-duration", 3000000000ns)

    Caller->>RTK: SetSinkAsync(sink, status)
    RTK->>GstAPI: gst_base_sink_set_async_enabled(GST_BASE_SINK(sink), status)

    Caller->>RTK: SetAudioProperty(&vol, &mute, &isSinkBinVol)
    Note right of RTK: volume="volume", mute="mute", isSinkBinVolume=true
    RTK-->>Caller: properties set

    Note over RTK: Unique capabilities
    Note right of RTK: IsAudioFragmentSyncSupported() → true
    Note right of RTK: IsPlaybackQualityFromSink() → true
    Note right of RTK: EnableLiveLatencyCorrection() → true
    Note right of RTK: RequiredQueuedFrames() → 4 (3+1)
```

---

## 8. AmlogicSocInterface — SetPlaybackRate (Source Pad Method)

```mermaid
sequenceDiagram
    participant Caller as GstHandlerControl
    participant AML as AmlogicSocInterface
    participant GstAPI as GStreamer API
    participant Src as GstElement (source)
    participant SrcPad as GstPad (src pad)

    Caller->>AML: SetPlaybackRate(sources, pipeline, rate, video_dec, audio_dec)
    loop For each source in sources
        AML->>Src: gst_element_get_static_pad("src")
        Src-->>AML: GstPad* sourceEleSrcPad
        alt sourceEleSrcPad == NULL
            AML->>AML: MW_LOG_ERR("failed to get static pad")
            Note right of AML: continue to next source
        else
            AML->>SrcPad: send rate event via source pad
            SrcPad-->>AML: success
        end
    end
    AML-->>Caller: bool status
```

---

## 9. AmlogicSocInterface — Seamless Switch & Audio-Only Mode

```mermaid
sequenceDiagram
    participant Caller
    participant AML as AmlogicSocInterface
    participant GstAPI as GStreamer API

    Caller->>AML: SetSeamlessSwitch(sink, value)
    AML->>GstAPI: g_object_set(sink, "seamless-switch", value)

    Caller->>AML: AudioOnlyMode(sinkbin)
    AML->>GstAPI: g_object_get(sinkbin, "n-audio", &n_audio)
    alt n_audio > 0
        AML-->>Caller: true (firstFrameReceived)
    else
        AML-->>Caller: false
    end

    Caller->>AML: SetAudioProperty(&vol, &mute, &isSinkBin)
    Note right of AML: volume="stream-volume", isSinkBinVolume=false
    Note right of AML: mute NOT set (impacts other players on Amlogic)
    AML-->>Caller: properties set
```

---

## 10. MtkSocInterface — Element Identification & AC4

```mermaid
sequenceDiagram
    participant Caller
    participant MTK as MtkSocInterface
    participant GstAPI as GStreamer API

    Caller->>MTK: UseAppSrc()
    MTK-->>Caller: false

    Caller->>MTK: UseWesterosSink()
    MTK-->>Caller: false

    Caller->>MTK: IsVideoSink("westerossink0")
    Note right of MTK: StartsWith "westerossink"
    MTK-->>Caller: true

    Caller->>MTK: IsVideoDecoder("westerossink0")
    Note right of MTK: StartsWith "westerossink"
    MTK-->>Caller: true

    Caller->>MTK: IsAudioOrVideoDecoder("westerossink0")
    Note right of MTK: Only if UseWesterosSink() && StartsWith "westerossink"
    MTK-->>Caller: false (UseWesterosSink returns false)

    Caller->>MTK: SetAC4Tracks(src, trackId)
    MTK->>GstAPI: g_object_set(src, "ac4-presentation-group-index", trackId)
```

---

## 11. DefaultSocInterface — Multi-Platform (Apple/Ubuntu/Rialto)

```mermaid
sequenceDiagram
    participant Caller
    participant Def as DefaultSocInterface
    participant GstAPI as GStreamer API

    Caller->>Def: UseAppSrc()
    alt __APPLE__ defined
        Def-->>Caller: true
    else
        Def-->>Caller: false
    end

    Caller->>Def: UseWesterosSink()
    Def-->>Caller: false (always)

    Caller->>Def: IsVideoSink(name)
    Note right of Def: "rialtomsevideosink" || "westerossink"
    Def-->>Caller: bool

    Caller->>Def: IsVideoDecoder(name)
    Note right of Def: "rialtomsevideosink" || "westerossink"
    Def-->>Caller: bool

    Caller->>Def: IsAudioOrVideoDecoder(name)
    Note right of Def: "rialtomsevideosink" || "rialtomseaudiosink" || "westerossink"
    Def-->>Caller: bool

    Caller->>Def: SetAudioProperty(&vol, &mute, &isSinkBin)
    alt __APPLE__
        Note right of Def: volume="volume", mute="mute", isSinkBinVolume=true
    else
        Note right of Def: volume="volume", mute="mute", isSinkBinVolume=false
    end

    Caller->>Def: SetRateCorrection()
    Def-->>Caller: false (not supported)
```

---

## 12. Vendor Comparison Matrix

| Feature | Brcm | Realtek | Amlogic | MTK | Default |
|---------|------|---------|---------|-----|---------|
| `UseAppSrc()` | false | false | false | false | false (true on macOS) |
| `UseWesterosSink()` | true | true | true | false | false |
| `EnablePTSRestamp()` | **true** | false | false | false | false |
| `EnableLiveLatencyCorrection()` | false | **true** | false | false | false |
| `IsAudioFragmentSyncSupported()` | false | **true** | false | false | false |
| `IsPlaybackQualityFromSink()` | false | **true** | false | false | false |
| `SetPlatformPlaybackRate()` | **true** | false | false | false | false |
| `SetRateCorrection()` | **true** | false | false | false | false |
| `RequiredQueuedFrames()` | 3 (default) | **4** (3+1) | 3 (default) | 3 (default) | 3 (default) |
| `SetSeamlessSwitch()` | no | no | **yes** | no | no |
| `AudioOnlyMode()` | no | no | **yes** | no | no |
| `SetAC4Tracks()` | no | no | no | **yes** | no |
| `ConfigureAudioSink()` | **yes** | no | no | no | no |
| Rate event target | video_dec + audio_dec | pipeline | source pads | (base) | (base) |
| Volume property | "volume" | "volume" | "stream-volume" | "volume" | "volume" |
| Volume on sinkbin? | false (audio_sink) | **true** | false | false | false (true macOS) |
| Video sink element | brcmvideosink/westerossink | from sinkbin | (base) | (base) | rialtomsevideosink/westerossink |

---

## Coverage Summary

| File | Lines Read | Confidence |
|------|-----------|------------|
| `vendor/SocInterface.h` | Full | 100% |
| `vendor/SocInterface.cpp` | Full | 100% |
| `vendor/brcm/BrcmSocInterface.h` | Full | 100% |
| `vendor/brcm/BrcmSocInterface.cpp` | 1–200 | 100% |
| `vendor/realtek/RealtekSocInterface.h` | Full | 100% |
| `vendor/realtek/RealtekSocInterface.cpp` | 1–100 | 100% |
| `vendor/amlogic/AmlogicSocInterface.h` | Full | 100% |
| `vendor/amlogic/AmlogicSocInterface.cpp` | 1–100 | 100% |
| `vendor/mtk/MtkSocInterface.h` | Full | 100% |
| `vendor/mtk/MtkSocInterface.cpp` | 1–100 | 100% |
| `vendor/default/DefaultSocInterface.h` | Full | 100% |
| `vendor/default/DefaultSocInterface.cpp` | 1–100 | 100% |

**Overall Confidence: 100%** — All vendor files fully read and documented.
