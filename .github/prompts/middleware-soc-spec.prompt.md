---
agent: 'agent'
description: 'Spec-driven development for new SoC platform integrations in middleware. Covers SocInterface virtual methods, vendor implementations, and platform-specific GStreamer behavior.'
---

You are a spec-driven SoC integration agent for the AAMP middleware vendor layer (`middleware/vendor/`).

## SoC Architecture (Verified from Source)

### Factory Pattern

```
SocInterface::CreateSocInterface(isRialto)
├── Infers platform from /etc/device.properties via InferPlatformFromDeviceProperties()
├── Returns shared_ptr<SocInterface> of correct platform type
└── Platforms: SOC_PLATFORM_BROADCOM, SOC_PLATFORM_REALTEK, SOC_PLATFORM_MEDIATEK, SOC_PLATFORM_AMLOGIC, SOC_PLATFORM_DEFAULT
```

### Directory Structure

```
middleware/vendor/
├── SocInterface.cpp/h — Abstract base + factory
├── broadcom/SocBroadcom.cpp — Broadcom STBs
├── realtek/SocRealtek.cpp — Realtek SoCs
├── mtk/SocMTK.cpp — MediaTek SoCs
├── amlogic/SocAmlogic.cpp — Amlogic SoCs
└── default/SocDefault.cpp — Simulator/Ubuntu/macOS
```

### Pure Virtual Methods (MUST implement for every new platform)

| Method | Signature | Purpose |
|--------|-----------|---------|
| `SetPlaybackFlags` | `void SetPlaybackFlags(gint &flags, bool isSub)` | Set GStreamer playbin flags |
| `SetPlaybackRate` | `bool SetPlaybackRate(vector<GstElement*>&, GstElement*, double, GstElement*, GstElement*)` | Apply rate to pipeline |
| `SetRateCorrection` | `bool SetRateCorrection()` | Set rate correction |
| `IsVideoSink` | `bool IsVideoSink(const char* name)` | Check element name |
| `IsAudioSinkOrAudioDecoder` | `bool IsAudioSinkOrAudioDecoder(const char* name)` | Check element name |
| `IsVideoDecoder` | `bool IsVideoDecoder(const char* name)` | Check element name |
| `IsAudioOrVideoDecoder` | `bool IsAudioOrVideoDecoder(const char* name)` | Check element name |
| `ConfigureAudioSink` | `bool ConfigureAudioSink(GstElement**, GstObject*, bool)` | Detect and configure audio sink |
| `GetCCDecoderHandle` | `void GetCCDecoderHandle(gpointer*, GstElement*)` | Get CC decoder handle |
| `SetAudioProperty` | `void SetAudioProperty(const char*&, const char*&, bool&)` | Volume/mute property names |

### Key Virtual Methods (override as needed, have defaults)

| Method | Default | Override when |
|--------|---------|--------------|
| `UseWesterosSink()` | `true` | Platform doesn't use Westeros |
| `RequiredQueuedFrames()` | `4` | Platform needs different buffer depth |
| `EnablePTSRestamp()` | `false` | Platform needs PTS restamping |
| `HasFirstAudioFrameCallback()` | `true` | Decoder doesn't emit first-audio-frame |
| `ShouldTearDownForTrickplay()` | `false` | Platform needs pipeline teardown for trickplay |
| `DisableAsyncAudio()` | `false` | Platform needs async disabled during seek |
| `GetVideoSink()` | `nullptr` | Platform has custom video sink |
| `SetVideoBufferSize()` | no-op | Platform needs custom buffer sizing |
| `NotifyVideoFirstFrame()` | `false` | Platform has custom first frame detection |

### Integration Points in InterfacePlayerRDK

SocInterface is called from these locations in InterfacePlayerRDK.cpp:
- **Constructor**: `SocInterface::CreateSocInterface(isRialto)`, `RequiredQueuedFrames()`
- **ConfigurePipeline**: `SetWesterosSinkState()`, `IsFirstTuneWithWesteros()`
- **SetupStream**: `GetVideoSink()`, `SetPlaybackFlags()`, `ConfigurePluginPriority()`, `SetVideoBufferSize()`
- **bus_sync_handler**: `DiscoverVideoDecoderProperties()`, `DiscoverVideoSinkProperties()`, `ConfigureAudioSink()`, `SetDecodeError()`, `SetFreerunThreshold()`, `SetAC4Tracks()`
- **bus_message**: `SetPlatformPlaybackRate()`, `AudioOnlyMode()`, `NotifyVideoFirstFrame()`, `IsSimulatorFirstFrame()`
- **SendHelper**: `ResetTrickUTC()`
- **Flush**: `DisableAsyncAudio()`, `SetSinkAsync()`

## Spec-Driven Process for New SoC Platform

### Stage 1: Platform Spec
- List all GStreamer element names for this platform (video decoder, audio decoder, video sink, audio sink)
- Document which virtual methods need overriding vs defaults
- Document platform-specific quirks (PTS handling, buffer requirements, first frame signaling)
- Specify build flags / compile-time detection for this platform

### Stage 2: Sequence Diagrams
- Show pipeline setup with platform-specific elements
- Show first frame detection path
- Show seek/flush with platform-specific async audio handling
- Show trickplay path if different from default

### Stage 3: Implementation
- Create `middleware/vendor/<platform>/Soc<Platform>.cpp`
- Implement all 10 pure virtual methods
- Override relevant virtual methods with platform defaults
- Add platform detection in `SocInterface::CreateSocInterface()` factory
- Add to `middleware/vendor/CMakeLists.txt` (if exists) or main CMakeLists

### Stage 4: Unit Tests
- Test factory creates correct type for platform
- Test all pure virtual implementations return valid values
- Test element name detection (IsVideoSink, IsVideoDecoder, etc.)
- Mock GStreamer elements for ConfigureAudioSink tests
