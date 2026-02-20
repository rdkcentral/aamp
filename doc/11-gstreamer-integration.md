# GStreamer Integration

## Overview

AAMP integrates with GStreamer, an open-source multimedia framework, to provide media pipeline management, hardware-accelerated decoding, and platform-specific rendering. GStreamer serves as the low-level media processing engine that handles demuxing, decoding, and rendering of media fragments injected by AAMP's fragment collection system.

The GStreamer integration abstracts platform-specific details through `InterfacePlayerRDK`, providing a unified interface for pipeline creation, fragment injection, and event handling across different platforms (RDK, Linux, macOS). The integration supports multiple media formats (ISO BMFF/MP4, MPEG-TS, HLS TS), various codecs (H.264, H.265, AAC, AC3, etc.), and platform-specific optimizations (hardware decoders, Westeros sink, Rialto sink).

The architecture separates AAMP's high-level playback logic (`AAMPGstPlayer`) from platform-specific GStreamer implementations (`InterfacePlayerRDK`), enabling portable player code while leveraging platform capabilities for optimal performance.

## Architecture

**Files**: `aampgstplayer.h/cpp`, `gstaamptaskpool.h/cpp`, `middleware/InterfacePlayerRDK.cpp`, `middleware/gst-plugins/`

**Key Classes**:
- **`AAMPGstPlayer`**: High-level GStreamer player wrapper that manages pipeline lifecycle, fragment injection, event handling, and buffer control. Provides AAMP-specific abstractions over GStreamer operations, including buffer management, PTS/DTS handling, and playback state coordination. Maintains references to `PrivateInstanceAAMP` for event reporting and configuration access.

- **`InterfacePlayerRDK`**: Platform-specific GStreamer interface that handles actual pipeline creation, element configuration, and platform optimizations. Provides callbacks for buffer underflow, bus events, decode errors, and other GStreamer events. Abstracts platform differences (RDK vs. Linux vs. macOS) and enables platform-specific features (hardware decoders, custom sinks, SOC optimizations).

## Pipeline Creation

### Pipeline Elements

The GStreamer pipeline structure varies based on media format and platform capabilities, but follows a general pattern:

**Typical Pipeline Structure**:
```
appsrc → parser → decoder → sink
```

**Element Descriptions**:
- **`appsrc`**: GStreamer source element that receives media data from AAMP. AAMP injects fragments into `appsrc` via `gst_app_src_push_buffer()`, providing PTS/DTS timestamps and duration information. `appsrc` provides flow control through `need-data` and `enough-data` signals, allowing AAMP to pace fragment injection based on pipeline consumption.

- **Parser Elements**: Format-specific parsers that extract elementary streams from container formats:
  - **ISO BMFF (MP4)**: `qtdemux` parser extracts video, audio, and subtitle tracks from MP4 containers. Handles fragmented MP4 (fMP4) used in DASH and HLS, extracting individual fragments and providing track information.
  - **MPEG-TS**: `tsdemux` parser extracts elementary streams from MPEG transport streams. Used for HLS TS content and broadcast streams, handling program selection and track extraction.
  - **HLS TS**: For HLS transport streams, `tsdemux` is configured with demuxing options to extract video and audio tracks separately, enabling independent track management.

- **Decoder Elements**: Codec-specific decoders that decode compressed media:
  - **Video Decoders**: `avdec_h264` (software H.264), `omxh264dec` (hardware H.264 on platforms with OpenMAX), platform-specific hardware decoders for SOC optimization.
  - **Audio Decoders**: `avdec_aac` (AAC), `avdec_ac3` (AC3), `avdec_eac3` (E-AC3), platform-specific audio decoders for hardware acceleration.

- **Sink Elements**: Output elements that render decoded media:
  - **Video Sinks**: `westerossink` (Westeros compositor for RDK platforms), `rialtosink` (Rialto sink for platform integration), `autovideosink` (automatic video sink selection), `tcpserversink` (TCP server sink for testing).
  - **Audio Sinks**: `autoaudiosink` (automatic audio sink selection), platform-specific audio sinks for hardware audio output.

### Pipeline Configuration

Pipeline configuration occurs during `AAMPGstPlayer` initialization and involves setting up elements, properties, and callbacks:

```cpp
void AAMPGstPlayer::Configure(
    StreamOutputFormat format,
    StreamOutputFormat audioFormat,
    ...)
{
    // Create pipeline
    // Add elements based on format
    // Link elements
    // Set to READY state
}
```

**Configuration Process**:
- **Format Detection**: Based on `StreamOutputFormat` (detected from manifest or specified), the system selects appropriate parser elements. ISO BMFF format uses `qtdemux`, MPEG-TS uses `tsdemux`, with format-specific configuration options.

- **Element Creation**: `InterfacePlayerRDK` creates GStreamer elements (`gst_element_factory_make()`) based on format and platform capabilities. Platform detection determines whether to use hardware decoders (available on RDK platforms) or software decoders (fallback for Linux/macOS).

- **Property Configuration**: Elements are configured with properties via `gst_element_set_property()`:
  - **appsrc**: `caps` (media capabilities), `format` (time format), `is-live` (live stream flag), `do-timestamp` (timestamp handling).
  - **Decoders**: Codec-specific properties, hardware acceleration flags, output format settings.
  - **Sinks**: Display properties (video rectangle, zoom mode), audio properties (volume, mute), platform-specific sink configuration.

- **Element Linking**: Elements are linked via `gst_element_link()` to form the pipeline. Linking order: `appsrc → parser → decoder → sink`. Some pipelines include additional elements (converters, scalers) between decoder and sink for format conversion or processing.

- **State Transition**: Pipeline is set to `GST_STATE_READY` after configuration, preparing it for data injection. Transition to `GST_STATE_PLAYING` occurs when playback starts, after initial buffering is complete.

**Platform-Specific Configuration**: `InitializePlayerConfigs()` maps AAMP configuration parameters to `InterfacePlayerRDK` GStreamer configuration, including buffer sizes (`videoBufBytes`, `audioBufBytes`), trick play settings (`vodTrickModeFPS`), PTS restamping (`enablePTSReStamp`), and platform-specific features (`useWesterosSink`, `useRialtoSink`).

## Fragment Injection

Fragment injection is the process of transferring decrypted media fragments from AAMP's cache into the GStreamer pipeline for decoding and rendering.

### Injection Methods

AAMP supports two primary injection methods, with `appsrc` being the standard approach:

1. **appsrc Push-Based Injection**: The primary injection method uses GStreamer's `appsrc` element, which provides a push-based interface for injecting media data. AAMP creates GStreamer buffers (`GstBuffer`) containing fragment data, sets PTS/DTS timestamps and duration, and pushes buffers to `appsrc` via `gst_app_src_push_buffer()`. The `appsrc` element handles buffer queuing and provides flow control through `need-data` and `enough-data` signals, allowing AAMP to pace injection based on pipeline consumption rate.

2. **Stream Sink Custom Interface**: An alternative injection method uses custom sink interfaces (`StreamSink`) that provide platform-specific injection paths. This method bypasses standard GStreamer pipeline elements and injects data directly into platform-specific rendering components. Used for specialized scenarios requiring platform integration or hardware-accelerated injection paths.

**Injection Implementation**:
```cpp
bool AAMPGstPlayer::SendTransfer(
    AampMediaType mediaType,
    std::vector<uint8_t>&& buffer,
    double fpts, double fdts, double fDuration,
    ...)
{
    // Create GStreamer buffer
    // Set PTS/DTS
    // Push to appsrc
}
```

**Injection Process**:
- **Buffer Creation**: `SendTransfer()` creates a `GstBuffer` from the fragment data buffer using `gst_buffer_new_allocate()` or `gst_buffer_new_wrapped()`. The buffer contains the raw media data (video NAL units, audio samples) ready for pipeline processing.

- **Timestamp Setting**: PTS (Presentation Time Stamp) and DTS (Decode Time Stamp) are set on the buffer via `GST_BUFFER_PTS()` and `GST_BUFFER_DTS()` macros. These timestamps are extracted from fragment metadata (ISO BMFF boxes, TS packets) and converted to GStreamer's `GST_CLOCK_TIME` format (nanoseconds). Duration is set via `GST_BUFFER_DURATION()` to enable accurate timing and synchronization.

- **Buffer Metadata**: Additional metadata may be attached to buffers, including:
  - **Discontinuity Flags**: `GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DISCONT)` marks buffer boundaries where PTS discontinuities occur (e.g., stream switches, ad insertions).
  - **Keyframe Flags**: `GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT)` indicates non-keyframe buffers, while absence indicates keyframes.
  - **Stream Information**: Track-specific information (codec, bitrate, resolution) may be attached for pipeline configuration or debugging.

- **Push to Pipeline**: The buffer is pushed to the appropriate `appsrc` element (video, audio, or subtitle track) via `gst_app_src_push_buffer()`. The `appsrc` queues the buffer internally and feeds it to downstream pipeline elements when ready. Push operations are non-blocking, allowing AAMP to continue fragment processing while GStreamer handles pipeline operations.

**Flow Control**: `appsrc` provides flow control through signals:
- **`need-data`**: Emitted when `appsrc` needs more data to continue playback. AAMP responds by injecting the next fragment from cache.
- **`enough-data`**: Emitted when `appsrc` has sufficient buffered data. AAMP may pause injection to prevent excessive buffering that increases latency.

The flow control mechanism ensures optimal buffering: enough data to prevent underruns while minimizing latency and memory usage.

## Stream Sink

The Stream Sink interface provides a custom injection path for platform-specific integration:

- **`StreamSink` Base Interface**: Abstract interface (`StreamSink` class) that defines methods for media injection, frame export, and sink configuration. Applications can provide custom `StreamSink` implementations to integrate with platform-specific rendering systems or enable frame-level access for processing.

- **Platform-Specific Implementations**: Different platforms may provide specialized `StreamSink` implementations:
  - **RDK Platforms**: Integration with RDK rendering systems, hardware compositors, or display managers.
  - **Testing/Development**: Custom sinks for frame export, video capture, or testing scenarios (`exportFrames` callback in `PlayerInstanceAAMP` constructor).

- **Frame Export Support**: `StreamSink` implementations may support frame export callbacks, allowing applications to receive decoded video frames for processing, recording, or display in custom UI frameworks. Frame export provides access to decoded pixel data before rendering, enabling advanced use cases like video analysis, overlays, or custom rendering.

**Usage**: Custom `StreamSink` implementations are provided to `PlayerInstanceAAMP` constructor via `streamSink` parameter. When provided, AAMP uses the custom sink instead of standard GStreamer sinks, enabling platform-specific integration or custom processing pipelines.

## Summary

GStreamer integration provides a robust, flexible foundation for media playback:

- **Flexible Pipeline Configuration**: Format-specific pipeline creation adapts to different media types (ISO BMFF, MPEG-TS, HLS TS) and codecs (H.264, H.265, AAC, AC3). Platform detection enables automatic selection of hardware decoders when available, falling back to software decoders for portability.

- **Multiple Format Support**: Unified interface supports HLS, DASH, and progressive formats through format-specific parsers and decoders. The abstraction layer (`InterfacePlayerRDK`) hides format differences from AAMP's high-level logic, enabling consistent playback behavior across formats.

- **Platform-Specific Rendering**: Integration with platform rendering systems (Westeros, Rialto, standard GStreamer sinks) enables optimal performance on each platform. Hardware-accelerated decoding and rendering leverage platform capabilities for efficient media processing.

- **Efficient Fragment Injection**: Push-based injection via `appsrc` provides low-latency fragment transfer with flow control. Timestamp handling and buffer metadata enable accurate synchronization and discontinuity handling, ensuring smooth playback across stream transitions and ad insertions.


