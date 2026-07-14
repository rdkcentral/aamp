# Middleware E2E Architecture — 100% Verified

## Table of Contents
1. [System Overview](#1-system-overview)
2. [Middleware Module Map](#2-middleware-module-map)
3. [Core Player Lifecycle](#3-core-player-lifecycle)
4. [Data Flow Architecture](#4-data-flow-architecture)
5. [DRM Subsystem](#5-drm-subsystem)
6. [Vendor SoC Abstraction](#6-vendor-soc-abstraction)
7. [Externals Subsystem](#7-externals-subsystem)
8. [GStreamer Plugin Architecture](#8-gstreamer-plugin-architecture)
9. [Subtitle & Closed Captions](#9-subtitle--closed-captions)
10. [Threading & Synchronization](#10-threading--synchronization)
11. [E2E Playback Flow](#11-e2e-playback-flow)

---

## 1. System Overview

```mermaid
graph TB
    subgraph "Application Layer"
        APP[Application / RDK Shell]
    end

    subgraph "AAMP Core"
        AAMP[PrivateInstanceAAMP]
        GSP[AAMPGstPlayer - Bridge]
    end

    subgraph "Middleware Layer"
        IRDK[InterfacePlayerRDK]
        subgraph "Core Modules"
            PRIV[InterfacePlayerPriv]
            GPP[GstPlayerPriv]
            SCHED[PlayerScheduler]
            HC[GstHandlerControl]
        end
        subgraph "DRM"
            DSM[DrmSessionManager]
            DS[DrmSession]
            DH[DrmHelper]
            OCDM[OCDM Adapters]
        end
        subgraph "Vendor"
            SI[SocInterface]
            BRCM[SocBroadcom]
            RTK[SocRealtek]
            MTK[SocMTK]
            AML[SocAmlogic]
            DEF[SocDefault]
        end
        subgraph "Externals"
            THD[PlayerThunderInterface]
            RFC[RFCSettings namespace]
            FB[FireboltInterface]
            CSM[ContentSecurityManager]
            PEI[PlayerExternalsInterface]
        end
        subgraph "GStreamer Plugins"
            DECRYPT[gstcdmidecryptor]
            SUBTEC[gst-subtec plugins]
        end
        subgraph "Subtitles"
            SUBP[SubtitleParser]
            LIBSUB[libsubtec packets]
        end
        subgraph "Utilities"
            GU[GstUtils]
            PU[PlayerUtils]
            PH[ProcessHandler]
            SU[SocUtils]
            BLM[PlayerLogManager]
            ISOBMFF[PlayerISOBMFF]
            PJSON[PlayerJsonObject]
            BC[BaseConversion]
        end
    end

    subgraph "GStreamer Framework"
        PIPE[GstPipeline]
        APPSRC[GstAppSrc]
        DEC[Decoder Elements]
        SINK[Sink Elements]
    end

    subgraph "Hardware"
        HW[SoC Hardware - Video/Audio Decoder + Display]
    end

    APP --> AAMP
    AAMP --> GSP
    GSP --> IRDK
    IRDK --> PRIV
    PRIV --> GPP
    IRDK --> SCHED
    IRDK --> HC
    IRDK --> SI
    DSM --> CSM
    IRDK --> GU
    SI --> BRCM
    SI --> RTK
    SI --> MTK
    SI --> AML
    SI --> DEF
    DSM --> DS
    DS --> DH
    DS --> OCDM
    IRDK --> PIPE
    PIPE --> APPSRC
    PIPE --> DEC
    PIPE --> SINK
    SINK --> HW
```

---

## 2. Middleware Module Map

```mermaid
graph LR
    subgraph "middleware/ Root Files"
        IRDK_CPP[InterfacePlayerRDK.cpp - 211KB - Main Player]
        IRDK_H[InterfacePlayerRDK.h - 30KB - Public API]
        PRIV_H[InterfacePlayerPriv.h - 17KB - Private Impl]
        GU_CPP[GstUtils.cpp/h - Caps/Buffer Utils]
        HC_CPP[GstHandlerControl.cpp/h - RAII Safety]
        SCHED[PlayerScheduler.cpp/h - Async Task Queue]
        PU[PlayerUtils.cpp/h - String/Base64/URL Utils]
        PH[ProcessHandler.cpp/h - Process Kill]
        SU[SocUtils.cpp/h - SoC Query Facade]
        MS[MediaSample.h - Zero-Copy Data Transport]
        PM[PlayerMetadata.hpp - Player Name]
        TP[gstplayertaskpool.cpp/h - Custom Thread Pool]
        DMX[DemuxDataTypes.h - Demux Type Definitions]
    end

    subgraph "middleware/drm/"
        DSM[DrmSessionManager.cpp/h]
        DS[DrmSession.cpp/h]
        DH_BASE[helper/DrmHelper.cpp/h - Base]
        DH_FACTORY[helper/DrmHelperFactory.cpp - Factory]
        DH_WV[helper/WidevineDrmHelper.cpp/h]
        DH_PR[helper/PlayReadyHelper.cpp/h]
        DH_CK[helper/ClearKeyHelper.cpp/h]
        DH_VMX[helper/VerimatrixHelper.cpp/h]
        OCDM_BASIC[ocdm/OcdmBasicSessionAdapter.cpp/h]
        OCDM_GST[ocdm/OcdmGstSessionAdapter.cpp/h]
        OCDM_ADAPT[ocdm/opencdmsessionadapter.cpp/h]
        AES[aes/AesDec.cpp/h]
        HLS_DRM[HlsDrmBase/HlsOcdmBridge]
    end

    subgraph "middleware/vendor/"
        SI_BASE[SocInterface.cpp/h - Factory + Base]
        SI_BRCM[broadcom/SocBroadcom.cpp]
        SI_RTK[realtek/SocRealtek.cpp]
        SI_MTK[mtk/SocMTK.cpp]
        SI_AML[amlogic/SocAmlogic.cpp]
        SI_DEF[default/SocDefault.cpp]
    end

    subgraph "middleware/externals/"
        THD_F[PlayerThunderInterface.cpp/h - Thunder JSON-RPC]
        RFC_F[PlayerRfc.cpp/h - RFC parameter read]
        FB_F[IFirebolt/FireboltInterface.cpp/h - Firebolt SDK]
        PEI_F[PlayerExternalsInterface.cpp/h - HDCP/Display]
        PEU_F[PlayerExternalUtils.cpp/h - Utility functions]
        CSM_DIR[contentsecuritymanager/ - License acquisition]
        RDK_DIR[rdk/ - RDK platform externals]
    end

    subgraph "middleware/gst-plugins/"
        GST_CDMI[drm/gst/gstcdmidecryptor.cpp - Base Decryptor]
        GST_PR[drm/gst/gstplayreadydecryptor.cpp]
        GST_WV[drm/gst/gstwidevinedecryptor.cpp]
        GST_CK[drm/gst/gstclearkeydecryptor.cpp]
        GST_VMX[drm/gst/gstverimatrixdecryptor.cpp]
        GST_SBIN[gst_subtec/gstsubtecbin.cpp]
        GST_SSINK[gst_subtec/gstsubtecsink.cpp]
        GST_SMP4[gst_subtec/gstsubtecmp4transform.cpp]
        GST_VIPER[gst_subtec/gstvipertransform.cpp]
    end

    subgraph "middleware/subtec/"
        SP_WV[subtecparser/WebVttSubtecParser.cpp/hpp]
        SP_TT[subtecparser/TtmlSubtecParser.cpp/hpp]
        SP_DEV[subtecparser/WebvttSubtecDevInterface.cpp/hpp]
        LP_CC[libsubtec/ClosedCaptionsPacket.hpp]
        LP_WV[libsubtec/WebVttPacket.hpp]
        LP_TT[libsubtec/TtmlPacket.hpp]
        LP_PKT[libsubtec/SubtecPacket.hpp]
        LP_SEND[libsubtec/PacketSender.cpp/hpp]
        LP_CH[libsubtec/SubtecChannel.cpp/hpp]
    end

    subgraph "middleware/playerisobmff/"
        ISO[PlayerISOBMFF - MP4 Box Parser]
    end

    subgraph "middleware/playerJsonObject/"
        JSON[PlayerJsonObject - JSON Wrapper]
    end

    subgraph "middleware/playerLogManager/"
        LOG[PlayerLogManager - Log Control]
    end

    subgraph "middleware/baseConversion/"
        BCONV[base16.cpp/h + _base64.cpp/h]
    end

    subgraph "middleware/closedcaptions/"
        CCMGR[PlayerCCManager.cpp/h - CC Manager Factory]
        CC_SUBTEC[subtec/PlayerSubtecCCManager.cpp/h]
        CC_RIALTO[rialto/PlayerRialtoCCManager.cpp/h]
        CCDC[subtec/CCDataController.cpp/h - Inband CC]
        CCCONN[subtec/SubtecConnector.cpp/h]
    end

    subgraph "middleware/subtitle/"
        SUBPARSE[subtitleParser.h - Parser Interface]
        VTTCUE[vttCue.h - WebVTT Cue]
    end
```

---

## 3. Core Player Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Constructed : new InterfacePlayerRDK
    Constructed --> PipelineCreated : CreatePipeline
    PipelineCreated --> StreamsConfigured : ConfigurePipeline
    StreamsConfigured --> SourcesDiscovered : deep-notify source fires
    SourcesDiscovered --> ElementsDiscovered : bus_sync_handler STATE_CHANGED
    ElementsDiscovered --> Injecting : SendHelper loop begins
    Injecting --> FirstFrame : first-video-frame-callback
    FirstFrame --> SteadyState : Progress timer starts
    SteadyState --> Seeking : Flush
    Seeking --> Injecting : ResetGstEvents resume inject
    SteadyState --> EOS : NotifyEOS
    SteadyState --> Stopped : Stop
    Seeking --> Stopped : Stop
    EOS --> Stopped : Stop
    Stopped --> PipelineCreated : Re-tune
    Stopped --> Destroyed : Destructor
    Destroyed --> [*]

    SteadyState --> Paused : SetPlayerState PAUSED
    Paused --> SteadyState : SetPlayerState PLAYING
    SteadyState --> RateChange : Flush with new rate
    RateChange --> Injecting : Trickplay inject
```

---

## 4. Data Flow Architecture

```mermaid
sequenceDiagram
    participant APP as AAMP Core
    participant GSP as AAMPGstPlayer
    participant IRDK as InterfacePlayerRDK
    participant SRC as GstAppSrc
    participant DRM as DRM Decryptor Plugin
    participant DEC as Decoder
    participant SINK as Video/Audio Sink
    participant HW as Hardware

    Note over APP,HW: === Fragment Download to Display ===

    APP->>APP: Download fragment from CDN

    alt HLS TS segments (FORMAT_MPEGTS source, ES output)
        APP->>APP: TSProcessor::demuxAndSend() parses PAT/PMT, extracts PES
        APP->>APP: Strips PES headers, outputs raw ES (H264/AAC/AC3/HEVC)
        APP->>GSP: Send ES samples (FORMAT_VIDEO_ES_H264, FORMAT_AUDIO_ES_AAC, etc)
        Note over GSP,SRC: No tsdemux in pipeline - AAMP demuxes TS to ES
    else DASH/HLS fMP4 with UseMp4Demux=false (FORMAT_ISO_BMFF, default)
        APP->>GSP: Send raw fMP4 segment (moof+mdat)
        Note over GSP,SRC: GStreamer qtdemux handles demuxing in-pipeline
        Note over GSP,SRC: SendQtDemuxOverrideEvent sent for PTS restamping
    else DASH/HLS fMP4 with UseMp4Demux=true (ES output)
        APP->>APP: AAMP Mp4Demux parses ISOBMFF boxes
        APP->>GSP: SetStreamCaps(type, codecInfo) sets isMp4DemuxPlayback=true
        APP->>GSP: Send individual ES samples (H264/AAC/HEVC NALUs)
        Note over GSP,SRC: No qtdemux in pipeline - raw ES to decoder directly
    else Progressive MP4
        Note over GSP,SRC: GStreamer souphttpsrc + qtdemux handle everything
        Note over GSP,SRC: AAMP does not download/inject fragments
    end

    GSP->>IRDK: SendHelper(type, sample, ...)
    IRDK->>IRDK: pthread_mutex_lock(sourceLock)
    IRDK->>IRDK: WaitForSourceSetup if !sourceConfigured

    alt First buffer after seek/start
        IRDK->>IRDK: SendGstEvents(mediaType, pts)
        Note over IRDK: pendingSeek, protectionEvent, qtdemux override
    end

    IRDK->>IRDK: lifetimeRef = new shared_ptr(move(sample.mData))
    IRDK->>SRC: gst_buffer_new_wrapped_full(READONLY, rawPtr, size, lifetimeRef)
    Note over IRDK,SRC: ZERO-COPY: GstBuffer aliases sample memory

    IRDK->>SRC: Set PTS, DTS, DURATION on buffer
    alt Encrypted content
        IRDK->>SRC: DecorateGstBufferWithDrmMetadata(buffer, metadata)
        Note over SRC: Attaches KID, IV, subsample info as GstProtectionMeta
    end
    IRDK->>SRC: gst_app_src_push_buffer(source, buffer)

    SRC->>DRM: Buffer flows through pipeline
    alt Has GstProtectionMeta
        DRM->>DRM: Extract KID, IV, subsamples
        DRM->>DRM: DrmSessionManager->GetSession(KID)
        DRM->>DRM: OpenCDMSession->Decrypt(buffer)
        DRM->>DEC: Clear buffer to decoder
    else Clear content
        SRC->>DEC: Buffer directly to decoder
    end

    DEC->>DEC: Decode video/audio frame
    DEC->>SINK: Decoded frame to sink
    SINK->>HW: Render to display/speakers

    Note over APP,HW: === Flow Control ===
    SRC-->>IRDK: "need-data" signal (buffer < 50%)
    IRDK-->>GSP: NeedDataCb(mediaType)
    GSP-->>APP: Resume fragment download

    SRC-->>IRDK: "enough-data" signal (buffer full)
    IRDK-->>GSP: EnoughDataCb(mediaType)
    GSP-->>APP: Pause fragment download
```

---

## 5. DRM Subsystem

```mermaid
sequenceDiagram
    participant IRDK as InterfacePlayerRDK
    participant SYNC as bus_sync_handler
    participant PLUGIN as DRM Decryptor Plugin
    participant DSM as DrmSessionManager
    participant DS as DrmSession
    participant DH as DrmHelper (WV/PR/CK/VMX)
    participant OCDM as OpenCDMSession
    participant CSM as ContentSecurityManager
    participant LS as License Server

    Note over IRDK,LS: === DRM Initialization (bus_sync_handler) ===

    SYNC->>SYNC: GST_MESSAGE_NEED_CONTEXT("drm-preferred-decryption-system-id")
    SYNC->>SYNC: gst_context_new("drm-preferred-decryption-system-id")
    SYNC->>SYNC: gst_structure_set("decryption-system-id", mDrmSystem)
    SYNC->>PLUGIN: gst_element_set_context(src, context)

    SYNC->>SYNC: STATE_CHANGED NULL->READY on DRM decryptor
    SYNC->>PLUGIN: g_object_set_property(src, playerName, mDRMSessionManager)
    SYNC->>PLUGIN: g_object_set_property(src, "drm-session-manager", mEncrypt)

    Note over IRDK,LS: === License Acquisition ===

    PLUGIN->>DSM: GetSession(keyId)
    alt Session exists for KID
        DSM-->>PLUGIN: return existing DrmSession
    else New session needed
        DSM->>DH: DrmHelperEngine::getInstance().createHelper(drmInfo)
        DH-->>DSM: DrmHelper instance (WV/PR/CK/VMX/Vanilla)
        DSM->>DS: new DrmSession(helper)
        DS->>OCDM: generateDRMSession(initData, size, customData)
        OCDM-->>DS: session handle (OpenCDM)
        DS->>DS: generateKeyRequest(destinationURL, timeout)
        DS-->>DSM: challenge data (DrmData*)
        alt SecManagerThunder enabled
            DSM->>CSM: AcquireLicense(challenge, url, accessToken, ...)
            CSM->>CSM: Route to SecManagerThunder subclass
            CSM->>LS: AcquireLicenseOpenOrUpdate via Thunder org.rdk.SecManager.1
            LS-->>CSM: License response + statusCode + reasonCode + businessStatus
            CSM-->>DSM: License response
            DSM-->>DS: License applied
        else ContentProtectionFirebolt enabled
            DSM->>CSM: AcquireLicense(challenge, url, accessToken, ...)
            CSM->>CSM: Route to ContentProtectionFirebolt subclass
            CSM->>LS: Firebolt SDK Content Protection API
            LS-->>CSM: License response
            CSM-->>DSM: License response
            DSM-->>DS: License applied
        else Direct license fetch (no CSM)
            DSM->>LS: HTTP POST challenge directly
            LS-->>DSM: License response
        end
        DS->>DS: processDRMKey(licenseResponse, timeout)
        DS->>OCDM: opencdm_session_update(response) internally
        OCDM-->>DS: Key ready (KEY_READY state)
        DSM-->>PLUGIN: return DrmSession
    end

    Note over IRDK,LS: === Decryption (per buffer) ===

    PLUGIN->>PLUGIN: Extract GstProtectionMeta from buffer
    PLUGIN->>DS: decrypt(keyIDBuffer, ivBuffer, buffer, subSampleCount, subSamplesBuffer, caps)
    DS->>OCDM: opencdm_gstreamer_session_decrypt via OcdmGstSessionAdapter
    OCDM-->>DS: Decrypted buffer (in-place)
    DS-->>PLUGIN: Success (0)
    PLUGIN->>PLUGIN: Remove GstProtectionMeta
    PLUGIN->>PLUGIN: Push clear buffer downstream
```

```mermaid
graph TB
    subgraph "DRM Helper Factory - helper/DrmHelperFactory.cpp"
        DHF[DrmHelperEngine::createHelper - singleton factory engine]
        DHF --> DH_WV2[WidevineDrmHelper]
        DHF --> DH_PR2[PlayReadyHelper]
        DHF --> DH_CK2[ClearKeyHelper]
        DHF --> DH_VMX2[VerimatrixHelper]
        DHF --> DH_VAN[VanillaDrmHelper]
    end

    subgraph "DRM Session Manager"
        DSM[DrmSessionManager]
        DSM --> DS1[DrmSession - KID1]
        DSM --> DS2[DrmSession - KID2]
        DSM --> DSN[DrmSession - KIDN]
    end

    subgraph "OCDM Layer - ocdm/"
        OCDM_A[opencdmsessionadapter - Base Adapter]
        OCDM_A --> OCDM_B[OcdmBasicSessionAdapter - Non-GStreamer]
        OCDM_A --> OCDM_G[OcdmGstSessionAdapter - GStreamer Decrypt]
    end

    subgraph "HLS-Specific DRM"
        HLS_BASE[HlsDrmBase - Interface]
        HLS_BASE --> HLS_OCDM[HlsOcdmBridge - SAMPLE-AES/OCDM]
        HLS_BASE --> AES_DEC[AesDec - AES-128-CBC vanilla]
        HLS_IFACE[PlayerHlsDrmSessionInterface]
    end
```

---

## 6. Vendor SoC Abstraction

```mermaid
graph TB
    subgraph "Factory Pattern"
        CREATE[SocInterface::CreateSocInterface]
    end

    subgraph "Base Interface"
        SI[SocInterface - Pure Virtual Base]
    end

    subgraph "Platform Implementations"
        BRCM[SocBroadcom - Broadcom SoCs]
        RTK[SocRealtek - Realtek SoCs]
        MTK[SocMTK - MediaTek SoCs]
        AML[SocAmlogic - Amlogic SoCs]
        DEF[SocDefault - Simulator/Generic]
    end

    CREATE --> SI
    SI --> BRCM
    SI --> RTK
    SI --> MTK
    SI --> AML
    SI --> DEF
```

### SocInterface Key Virtual Methods (from vendor/SocInterface.h):

| Method | Type | Purpose |
|--------|------|---------|
| `UseWesterosSink()` | virtual | Whether platform uses Westeros video sink (default: true) |
| `UseAppSrc()` | virtual | Whether AppSrc element should be used (default: false) |
| `RequiredQueuedFrames()` | virtual | Min frames to queue before decode (default: 4) |
| `EnablePTSRestamp()` | virtual | Whether platform supports PTS restamping (default: false) |
| `IsFirstTuneWithWesteros()` | virtual | First-tune detection without Westeros (default: false) |
| `HasFirstAudioFrameCallback()` | virtual | Whether first audio frame callback exists (default: true) |
| `ShouldTearDownForTrickplay()` | virtual | Whether trickplay needs pipeline teardown (default: false) |
| `IsDecryptRequired()` | virtual | Whether platform needs explicit decryption (default: false) |
| `IsTransformCapsRequired()` | virtual | Whether transform caps are needed (default: false) |
| `AudioOnlyMode()` | virtual | Handle audio-only first frame detection (default: false) |
| `SetPlatformPlaybackRate()` | virtual | Apply rate to platform elements (default: false) |
| `DisableAsyncAudio()` | virtual | Disable async on audio sink during seek (default: false) |
| `ResetTrickUTC()` | virtual | Reset UTC reference for trickplay (default: false) |
| `NotifyVideoFirstFrame()` | virtual | Platform-specific first frame notification (default: false) |
| `IsSimulatorFirstFrame()` | virtual | Simulator first frame detection (default: false) |
| `SetVideoBufferSize()` | virtual | Platform buffer size configuration |
| `SetSinkAsync()` | virtual | Re-enable async on audio sink post-seek |
| `SetFreerunThreshold()` | virtual | Set AV sync freerun threshold |
| `SetSeamlessSwitch()` | virtual | Enable/disable seamless audio switch |
| `ConfigurePluginPriority()` | virtual | Set audio decoder plugin priorities |
| `SetH264Caps()` | virtual | Platform-specific H264 caps adjustments |
| `SetHevcCaps()` | virtual | Platform-specific HEVC caps adjustments |
| `SetDecodeError()` | virtual | Connect decode error callback |
| `GetVideoPts()` | virtual | Get current video PTS (90kHz ticks) |
| `DiscoverVideoDecoderProperties()` | virtual | Find decoder signals at NULL->READY |
| `DiscoverVideoSinkProperties()` | virtual | Find sink properties at READY->PAUSED |
| `SetAC4Tracks()` | virtual | AC4 audio track selection |
| `SetPlaybackFlags()` | **pure virtual** | Platform-specific GStreamer playbin flags |
| `SetPlaybackRate()` | **pure virtual** | Set rate on sources/pipeline/decoders |
| `SetRateCorrection()` | **pure virtual** | Set rate correction |
| `IsVideoSink()` | **pure virtual** | Check if element name is video sink |
| `IsAudioSinkOrAudioDecoder()` | **pure virtual** | Check if element is audio sink/decoder |
| `IsVideoDecoder()` | **pure virtual** | Check if element is video decoder |
| `IsAudioOrVideoDecoder()` | **pure virtual** | Check if element is audio or video decoder |
| `ConfigureAudioSink()` | **pure virtual** | Detect and configure platform audio sink |
| `GetCCDecoderHandle()` | **pure virtual** | Get closed caption decoder handle |
| `SetAudioProperty()` | **pure virtual** | Get volume/mute property names for platform |
| `GetVideoSink()` | virtual | Get/create platform video sink element |

---

## 7. Externals Subsystem

```mermaid
graph TB
    subgraph "PlayerThunderInterface"
        THD[PlayerThunderInterface]
        THD --> JSONRPC[JSON-RPC via Thunder WPEFramework]
        JSONRPC --> HDMI[DisplayInfo Plugin]
        JSONRPC --> SYSTEM[System Plugin]
    end

    subgraph "RFCSettings"
        RFC[RFCSettings::readRFCValue]
        RFC --> TR181[TR-181 DataModel via tr181api]
    end

    subgraph "FireboltInterface"
        FB[FireboltInterface - Singleton]
        FB --> FBSDK[Firebolt SDK - fireboltaamp.h]
        FBSDK --> CAPS[Device Capabilities]
    end

    subgraph "PlayerExternalsInterface"
        PEI[PlayerExternalsInterface]
        PEI --> HDCP[HDCP Status - dsHdcpProtocolVersion]
        PEI --> DISPLAY[Display Resolution]
        subgraph "RDK Implementation"
            RDKEXT[PlayerExternalsRdkInterface]
            RDKEXT --> DEVFB[DeviceFireboltInterface]
            RDKEXT --> DEVIARM[DeviceIARMInterface]
        end
    end

    subgraph "ContentSecurityManager - License Acquisition"
        CSM[ContentSecurityManager - Base Class<br/>ContentSecurityManager.h<br/>Extends PlayerScheduler]
        SMT[SecManagerThunder - Subclass<br/>SecManagerThunder.h<br/>Thunder Plugin: org.rdk.SecManager.1]
        CPF[ContentProtectionFirebolt - Subclass<br/>IFirebolt/ContentProtectionFirebolt.h<br/>Firebolt Content Protection SDK]
        CSMS[ContentSecurityManagerSession<br/>ContentSecurityManagerSession.h<br/>Per-Playback Session State]
        CSM --> SMT
        CSM --> CPF
        CSM --> CSMS
        SMT --> THUNDER_SM[Thunder SecManager Plugin]
        SMT --> THUNDER_WM[Thunder Watermark Plugin]
        SMT --> THUNDER_AUTH[Thunder AuthService Plugin]
        CPF --> FB_SDK[Firebolt SDK]
    end
```

---

## 8. GStreamer Plugin Architecture

```mermaid
graph LR
    subgraph "DRM Decryptor Plugins - gst-plugins/drm/gst/"
        PR_DEC[gstplayreadydecryptor]
        WV_DEC[gstwidevinedecryptor]
        CK_DEC[gstclearkeydecryptor]
        VMX_DEC[gstverimatrixdecryptor]
    end

    subgraph "Subtitle Plugins - gst-plugins/gst_subtec/"
        SUBTEC_BIN[gstsubtecbin - Container bin]
        SUBTEC_SINK[gstsubtecsink - Subtitle render]
        SUBTEC_MP4[gstsubtecmp4transform - MP4 sub transform]
        SUBTEC_VIPER[gstvipertransform - Viper transform]
    end

    subgraph "Common Base"
        GST_BASE[gstcdmidecryptor - Base CDMI Decryptor]
        PR_DEC --> GST_BASE
        WV_DEC --> GST_BASE
        CK_DEC --> GST_BASE
        VMX_DEC --> GST_BASE
    end

    subgraph "Pipeline Integration"
        PIPE[GstPipeline]
        PIPE --> APPSRC[appsrc]
        APPSRC --> DEMUX[qtdemux/tsdemux]
        DEMUX -->|"protection-system-id match"| DRM_SLOT[One DRM Decryptor - GstBaseTransform in-place]
        DRM_SLOT --> VDEC[Video Decoder]
        DRM_SLOT --> ADEC[Audio Decoder]
        VDEC --> VSINK[Video Sink]
        ADEC --> ASINK[Audio Sink]
    end

    Note_DRM["Only ONE decryptor active per stream.<br/>GStreamer auto-selects based on protection-system-id:<br/>PR: 9a04f079-9840-4286-ab92-e65be0885f95<br/>WV: edef8ba9-79d6-4ace-a3c8-27dcd51d21ed<br/>CK: 1077efec-c0b2-4d02-ace3-3c1e52e2fb4b<br/>VMX: 9a27dd82-fde2-4725-8cbc-4234aa06ec09"]
```

---

## 9. Subtitle & Closed Captions

```mermaid
sequenceDiagram
    participant APP as AAMP Core
    participant IRDK as InterfacePlayerRDK
    participant GST as GStreamer Pipeline
    participant SUBTEC as SubtecBin/SubtecSink
    participant PARSER as SubtitleParser (WebVTT/TTML)
    participant RENDER as Subtitle Renderer

    Note over APP,RENDER: === Embedded Subtitles (GStreamer path) ===

    APP->>IRDK: SendHelper(SUBTITLE, sample)
    IRDK->>GST: gst_app_src_push_buffer(subtitle_source, buffer)
    GST->>SUBTEC: Buffer flows to subtecbin
    SUBTEC->>PARSER: Parse subtitle data
    PARSER->>RENDER: Rendered subtitle cue

    Note over APP,RENDER: === Inband Closed Captions (CEA-608/708) ===
    Note over APP,RENDER: CC data embedded in video ES, extracted by decoder

    APP->>IRDK: Video fragments injected normally
    IRDK->>GST: Video buffer to pipeline
    GST->>GST: Decoder extracts CC from video ES (CEA-608/708)
    GST->>GST: closedCaptionDataCb(decoderIndex, eType, ccData, len, seqNum, localPts)
    GST->>RENDER: CCDataController -> ClosedCaptionsPacket -> SubtecChannel -> Renderer

    Note over APP,RENDER: === Out-of-Band CC (Rialto CC Control Stream) ===
    Note over APP,RENDER: CC as separate track via dedicated appsrc

    APP->>IRDK: ConfigurePipeline with usingClosedCaptionsControl=true
    IRDK->>IRDK: SetupClosedCaptionControlStream()
    IRDK->>GST: gst_element_factory_make("playbin") for CC sinkbin
    IRDK->>GST: caps = gst_caps_new_simple("application/x-subtitle-cc")
    IRDK->>GST: gst_app_src_set_caps(source, caps)
    APP->>IRDK: SendHelper(SUBTITLE, cc_data)
    IRDK->>GST: gst_app_src_push_buffer(source, cc_buffer)
    GST->>RENDER: CC data to Rialto subtitle sink for rendering
```

### CC Manager Class Hierarchy (from closedcaptions/PlayerCCManager.h):

```mermaid
graph TB
    subgraph "CC Manager Factory - PlayerCCManager singleton"
        FACTORY[PlayerCCManager::GetInstance]
        FACTORY -->|"mIsRialto=false"| SUBTEC_CC[PlayerSubtecCCManager]
        FACTORY -->|"mIsRialto=true"| RIALTO_CC[PlayerRialtoCCManager]
        FACTORY -->|"simulator"| FAKE_CC[PlayerFakeCCManager - stub]
    end

    subgraph "PlayerCCManagerBase - base class"
        BASE[PlayerCCManagerBase]
        BASE --> INIT[Init - decoder handle]
        BASE --> SET_STATUS[SetStatus - enable/disable]
        BASE --> SET_TRACK[SetTrack - 608/708/default]
        BASE --> SET_STYLE[SetStyle - rendering options]
        BASE --> TRICKPLAY[SetTrickplayStatus]
        BASE --> PARENTAL[SetParentalControlStatus]
        BASE --> OOB[IsOOBCCRenderingSupported]
    end

    SUBTEC_CC --> BASE
    RIALTO_CC --> BASE
    FAKE_CC --> BASE

    subgraph "Subtec CC Path - closedcaptions/subtec/"
        SUBTEC_CC --> CCDC[CCDataController - singleton]
        CCDC --> CC_DATA_CB[closedCaptionDataCb - inband CC from decoder]
        CCDC --> CC_DECODE_CB[closedCaptionDecodeCb - decode events]
        CCDC --> CC_MUTE[sendMute/sendUnmute]
        CCDC --> CC_PKT[ClosedCaptionsPacket]
        SUBTEC_CC --> SC[SubtecConnector]
    end

    subgraph "Rialto CC Path - closedcaptions/rialto/"
        RIALTO_CC --> RIALTO_IMPL[PlayerRialtoCCManager]
        RIALTO_IMPL --> RIALTO_RENDER[Rialto CC Rendering]
    end
```

### Subtitle Parser Types (from subtec/subtecparser/):

```mermaid
graph TB
    subgraph "Subtitle Parsers - subtec/subtecparser/"
        SP_BASE[SubtecParser Base]
        SP_BASE --> WEBVTT[WebVttSubtecParser.cpp/hpp]
        SP_BASE --> TTML[TtmlSubtecParser.cpp/hpp]
        SP_BASE --> DEVIF[WebvttSubtecDevInterface.cpp/hpp]
    end

    subgraph "libsubtec Packets - subtec/libsubtec/"
        PKT[Packet - Base class in SubtecPacket.hpp]
        PKT --> PKT_CC[ClosedCaptionsPacket.hpp]
        PKT --> PKT_WV[WebVttPacket.hpp]
        PKT --> PKT_TT[TtmlPacket.hpp]
        SEND[PacketSender.cpp/hpp - Sends packets via Unix socket]
        CHAN[SubtecChannel.cpp/hpp - Channel ID mgmt + sendPacket template]
        SEND -->|"sends PacketPtr"| PKT
        CHAN -->|"creates and sends"| PKT
    end
```

### Key CC Concepts:

| Type | Description | Data Source | Rendering Path |
|------|-------------|------------|----------------|
| **Inband CC** | CEA-608/708 embedded in video ES | Decoder extracts from video PES | `closedCaptionDataCb` -> `CCDataController` -> `ClosedCaptionsPacket` -> `SubtecChannel` |
| **Out-of-Band CC** | Separate subtitle track (WebVTT/TTML) | AAMP downloads and injects via separate appsrc | `SetupClosedCaptionControlStream` -> `application/x-subtitle-cc` caps -> Rialto subtitle sink |
| **OOB Check** | `IsOOBCCRenderingSupported()` | `PlayerCCManagerBase` virtual | Returns whether platform supports OOB CC rendering |
| **CC Formats** | `eCLOSEDCAPTION_FORMAT_608`, `eCLOSEDCAPTION_FORMAT_708`, `eCLOSEDCAPTION_FORMAT_DEFAULT` | `PlayerCCManager.h` enum | Used in `SetTrack(track, format)` |

---

## 10. Threading & Synchronization

```mermaid
graph TB
    subgraph "Thread Pool"
        TP[GstPlayerTaskPool - Custom pthread pool]
        TP --> T1[GStreamer streaming thread 1]
        TP --> T2[GStreamer streaming thread 2]
        TP --> TN[GStreamer streaming thread N]
    end

    subgraph "Scheduler"
        SCHED[PlayerScheduler]
        SCHED --> WT[Worker Thread - single]
        WT --> TQ[Task Queue - deque]
    end

    subgraph "GStreamer Threads"
        BUS_SYNC[Bus Sync Handler - streaming thread]
        BUS_ASYNC[Bus Async Handler - main loop thread]
        NEED_DATA[need-data callback - streaming thread]
        ENOUGH_DATA[enough-data callback - streaming thread]
    end

    subgraph "Application Threads"
        INJECT[SendHelper - caller thread]
        CONTROL[Stop/Flush - caller thread]
    end

    subgraph "Synchronization Primitives"
        HC1[syncControl - GstHandlerControl]
        HC2[aSyncControl - GstHandlerControl]
        HC3[callbackControl - GstHandlerControl]
        MTX1[mMutex - Stop/Configure serialization]
        MTX2[sourceLock per track - SendHelper serialization]
        MTX3[mProtectionLock - DRM event protection]
        MTX4[mQMutex - Scheduler queue access]
        MTX5[mExMutex - Scheduler execution lock]
        MTX6[mSignalVectorAccessMutex - Signal list access]
        CV1[mSourceSetupCV - Source ready notification]
        CV2[mQCond - Scheduler task notification]
        CV3[mDoneCond - Handler completion wait]
    end
```

### Handler Control Pattern (RAII Safety):

```mermaid
sequenceDiagram
    participant CB as GStreamer Callback Thread
    participant HC as GstHandlerControl
    participant STOP as Stop Thread

    Note over CB,STOP: Normal operation - handler enabled

    CB->>HC: getScopeHelper()
    HC->>HC: lock(mSync), mInstanceCount++
    HC-->>CB: ScopeHelper created

    CB->>HC: returnStraightAway() checks isEnabled()
    HC-->>CB: false (enabled, proceed)
    CB->>CB: Execute handler logic

    Note over CB: ScopeHelper destructor (RAII)
    CB->>HC: handlerEnd()
    HC->>HC: lock(mSync), mInstanceCount--, notify_one

    Note over CB,STOP: Teardown - handler disabled

    STOP->>HC: waitForDone(50ms, "bus_sync_handler")
    HC->>HC: disable() sets mEnabled=false
    HC->>HC: wait on mDoneCond until mInstanceCount==0 or timeout
    HC-->>STOP: true (all handlers exited)

    Note over CB: Late callback arrives
    CB->>HC: getScopeHelper()
    HC->>HC: lock(mSync), mInstanceCount++
    CB->>HC: returnStraightAway() checks isEnabled()
    HC-->>CB: true (disabled, exit immediately)
    CB->>HC: ~ScopeHelper -> handlerEnd(), mInstanceCount--
```

---

## 11. E2E Playback Flow

```mermaid
sequenceDiagram
    participant APP as Application
    participant AAMP as PrivateInstanceAAMP
    participant GSP as AAMPGstPlayer
    participant IRDK as InterfacePlayerRDK
    participant PRIV as InterfacePlayerPriv
    participant GPP as GstPlayerPriv
    participant SI as SocInterface
    participant SCHED as PlayerScheduler
    participant GST as GStreamer Pipeline
    participant DRM as DRM Plugin
    participant DEC as Decoder
    participant SINK as Sink
    participant HW as Hardware

    Note over APP,HW: ═══ TUNE REQUEST ═══

    APP->>AAMP: Tune(url)
    AAMP->>GSP: Configure(format, audioFormat, ...)
    GSP->>IRDK: ConfigurePipeline(format, audioFormat, subFormat, rate, ...)

    Note over IRDK: Create pipeline if needed
    IRDK->>GST: gst_pipeline_new(name)
    IRDK->>GST: gst_bus_add_watch + set_sync_handler

    Note over IRDK: Setup streams
    loop VIDEO, AUDIO, SUBTITLE
        IRDK->>GST: gst_element_factory_make("playbin") for sinkbin
        IRDK->>SI: SetPlaybackFlags, GetVideoSink
        IRDK->>GST: g_object_set(sinkbin, "uri", "appsrc://")
        IRDK->>GST: gst_bin_add + sync_state_with_parent
    end

    IRDK->>GST: SetStateWithWarnings(pipeline, PLAYING)

    Note over APP,HW: ═══ PIPELINE RAMP-UP ═══

    GST-->>IRDK: "deep-notify::source" (VIDEO)
    IRDK->>GST: Configure appsrc (max-bytes, caps, signals)
    GST-->>IRDK: "deep-notify::source" (AUDIO)
    IRDK->>GST: Configure appsrc (max-bytes, caps, signals)

    GST-->>IRDK: bus_sync: STATE_CHANGED NULL->READY (video_dec)
    IRDK->>SI: DiscoverVideoDecoderProperties
    IRDK->>GST: SignalConnect("first-video-frame-callback")

    GST-->>IRDK: bus_sync: STATE_CHANGED NULL->READY (drm_decryptor)
    IRDK->>DRM: Set mDRMSessionManager + mEncrypt

    GST-->>IRDK: bus_sync: STATE_CHANGED READY->PAUSED (video_sink)
    IRDK->>SI: DiscoverVideoSinkProperties
    IRDK->>GST: Set rectangle, zoom, show-video-window

    Note over APP,HW: ═══ DATA INJECTION ═══

    loop Fragment download loop
        AAMP->>AAMP: Download fragment from CDN
        AAMP->>AAMP: Demux into MediaSample (zero-copy)
        AAMP->>GSP: Send(mediaType, sample)
        GSP->>IRDK: SendHelper(type, sample, ...)

        alt First buffer
            IRDK->>PRIV: SendGstEvents(pts, protectionEvent)
        end

        IRDK->>GST: gst_buffer_new_wrapped_full (zero-copy)
        IRDK->>GST: Set PTS/DTS/Duration
        alt Encrypted
            IRDK->>GST: DecorateGstBufferWithDrmMetadata
        end
        IRDK->>GST: gst_app_src_push_buffer

        GST->>DRM: Buffer with GstProtectionMeta
        DRM->>DRM: Decrypt in-place via OCDM
        DRM->>DEC: Clear buffer
        DEC->>SINK: Decoded frame
        SINK->>HW: Display/Play
    end

    Note over APP,HW: ═══ FIRST FRAME ═══

    DEC-->>IRDK: "first-video-frame-callback"
    IRDK->>GPP: firstVideoFrameReceived = true
    IRDK->>IRDK: NotifyFirstFrame(VIDEO)
    IRDK->>SCHED: ScheduleTask(IdleCallbackOnFirstFrame)
    SCHED->>GSP: TriggerEvent(firstVideoFrameReceived)
    GSP->>AAMP: First frame notification
    AAMP->>APP: AAMP_EVENT_TUNED

    IRDK->>IRDK: IdleTaskAdd(IdleCallback)
    IRDK->>IRDK: TimerAdd(ProgressCallbackOnTimeout)

    Note over APP,HW: ═══ STEADY STATE ═══

    loop Every progressInterval ms
        IRDK->>IRDK: MonitorAV (query positions, detect stall/freeze/avsync)
        IRDK->>GSP: TriggerEvent(progressCb)
        GSP->>AAMP: Progress update
    end

    Note over APP,HW: ═══ SEEK ═══

    APP->>AAMP: Seek(position)
    AAMP->>GSP: Flush(position, rate)
    GSP->>IRDK: Flush(position, rate, shouldTearDown, isAppSeek)
    IRDK->>SCHED: RemoveTask(eosCallback) if pending
    IRDK->>IRDK: SetSeekPosition(position)
    alt !Rialto
        IRDK->>SI: DisableAsyncAudio
        IRDK->>GST: GstPlayer_SignalEOS(AUDIO)
    end
    IRDK->>IRDK: ResetGstEvents() [resetPosition=true]
    IRDK->>GST: gst_element_seek(pipeline, FLUSH, position)
    IRDK->>GPP: eosSignalled=false, numberOfVideoBuffersSent=0

    Note over APP,HW: ═══ END OF STREAM ═══

    GST-->>IRDK: bus_message(GST_MESSAGE_EOS)
    IRDK->>IRDK: NotifyEOS()
    IRDK->>SCHED: ScheduleTask(IdleCallbackOnEOS)
    SCHED->>GSP: TriggerEvent(notifyEOS)
    GSP->>AAMP: EOS notification
    AAMP->>APP: AAMP_EVENT_EOS

    Note over APP,HW: ═══ STOP ═══

    APP->>AAMP: Stop()
    AAMP->>GSP: Stop(keepLastFrame)
    GSP->>IRDK: Stop(keepLastFrame)
    IRDK->>GPP: syncControl.disable(), aSyncControl.disable()
    IRDK->>IRDK: mSourceSetupCV.notify_all()
    IRDK->>GST: gst_bus_remove_watch
    IRDK->>IRDK: Remove all timers/idle tasks
    IRDK->>GPP: waitForDone on all handler controls
    IRDK->>IRDK: DisconnectSignals()
    IRDK->>IRDK: RemoveProbes()
    IRDK->>GST: SetStateWithWarnings(pipeline, NULL)
    loop Each track
        IRDK->>IRDK: TearDownStream(i)
    end
    IRDK->>IRDK: DestroyPipeline()
    IRDK->>GPP: Reset all state
```

---

## Summary

| Component | Files | Responsibility |
|-----------|-------|---------------|
| **InterfacePlayerRDK** | InterfacePlayerRDK.cpp/h | Main GStreamer player - pipeline management, data injection, bus handling |
| **InterfacePlayerPriv** | InterfacePlayerPriv.h | Private implementation - GstEvents, segments, protection events |
| **GstPlayerPriv** | (in InterfacePlayerPriv.h) | State structure - pipeline, bus, sinks, decoders, flags |
| **PlayerScheduler** | PlayerScheduler.cpp/h | Single-threaded async task queue for callbacks |
| **GstHandlerControl** | GstHandlerControl.h | RAII safety - prevent callbacks during teardown |
| **SocInterface** | vendor/SocInterface.cpp/h + platforms | Hardware abstraction factory |
| **DrmSessionManager** | drm/DrmSessionManager.cpp/h | DRM session lifecycle management |
| **DrmSession** | drm/DrmSession.cpp/h | Individual DRM session + OCDM interaction |
| **DrmHelper** | drm/helper/DrmHelper*.cpp/h | DRM system-specific logic (WV/PR/CK/VMX/Vanilla) |
| **DrmHelperFactory** | drm/helper/DrmHelperFactory.cpp | Creates correct helper by key system |
| **OCDM Adapters** | drm/ocdm/Ocdm*SessionAdapter.cpp/h | OpenCDM session wrappers (Basic + GStreamer) |
| **gstcdmidecryptor** | gst-plugins/drm/gst/gstcdmidecryptor.cpp | GStreamer CDMI decrypt element base |
| **GstUtils** | GstUtils.cpp | Caps creation, buffer utilities |
| **PlayerUtils** | PlayerUtils.cpp | Base64, URL resolution, time utils |
| **SocUtils** | SocUtils.cpp | Static facade over SocInterface |
| **SubtitleParser** | subtec/subtecparser/ | WebVTT (WebVttSubtecParser), TTML (TtmlSubtecParser) |
| **libsubtec** | subtec/libsubtec/ | Subtitle packet protocol (SubtecPacket, CC, WebVtt, Ttml) |
| **PlayerISOBMFF** | playerisobmff/ | MP4 box parsing utilities |
| **PlayerJsonObject** | playerJsonObject/ | JSON wrapper for DRM challenges |
| **PlayerLogManager** | playerLogManager/ | Log level control |
| **PlayerCCManager** | closedcaptions/PlayerCCManager.cpp/h | CC manager factory (Subtec/Rialto/Fake) |
| **PlayerSubtecCCManager** | closedcaptions/subtec/PlayerSubtecCCManager.cpp/h | Subtec-based CC with CCDataController |
| **PlayerRialtoCCManager** | closedcaptions/rialto/PlayerRialtoCCManager.cpp/h | Rialto-based CC rendering |
| **Externals** | externals/ | PlayerThunderInterface, RFCSettings, FireboltInterface, PlayerExternalsInterface, ContentSecurityManager (SecManagerThunder + ContentProtectionFirebolt) |
