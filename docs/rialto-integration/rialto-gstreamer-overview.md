# Rialto-GStreamer Overview and AAMP Integration

## How rialto-gstreamer Works

**Rialto** is a client-server media playback framework for RDK. It decouples the application (client) from direct hardware decoder access by routing media through a privileged **RialtoServer** process that exclusively manages the platform's media decoders.

**rialto-gstreamer** ([GitHub](https://github.com/rdkcentral/rialto-gstreamer)) is a GStreamer plugin library that provides custom sink elements acting as the **client side** of this architecture. It registers four sink elements:

- **`rialtomsevideosink`** — receives video buffers
- **`rialtomseaudiosink`** — receives audio buffers
- **`rialtomsesubtitlesink`** — receives subtitle/CC data
- **`rialtowebaudiosink`** — for web audio PCM data

These sinks behave like normal GStreamer sink elements (they accept buffers, handle state changes, process events/queries), but instead of rendering media locally, they forward compressed media data over IPC (via `RIALTO_SOCKET_PATH`) to **RialtoServer**, which performs the actual decoding and rendering on the platform's hardware decoders. The key class `GStreamerMSEMediaPlayerClient` manages this IPC session and maps GStreamer concepts (segments, flushes, EOS, caps) to Rialto's `IMediaPipeline` API.

The plugin's rank is set to `INT_MAX` when `RIALTO_SOCKET_PATH` is defined, so it auto-plugs as the preferred sink.

### Data Flow: Push-to-Queue, Pull-from-Queue

rialto-gstreamer uses two internal delegate models: **PullMode** and **PushMode**. When used with AAMP (via `appsrc`), the **PullMode** delegates are active:

1. **AAMP pushes** buffers into `appsrc` via `gst_app_src_push_buffer()`.
2. The GStreamer chain function (`rialto_mse_base_sink_chain`) delivers these buffers to the `PullModePlaybackDelegate::handleBuffer()` method.
3. `handleBuffer()` wraps each buffer into a `GstSample` and enqueues it into an internal `std::queue<GstSample*>` (capped at 24 entries). If the queue is full, it blocks on a condition variable to apply backpressure to AAMP.
4. **RialtoServer pulls** data from this queue over IPC by calling `getFrontSample()` (peek) and `popSample()` (consume). `popSample()` signals the condition variable to unblock AAMP's push path.

```
AAMP (push) → appsrc → rialto sink chain → m_samples queue → RialtoServer (pull via IPC) → HW decoder
```

Each Rialto sink element (`rialtomsevideosink`, `rialtomseaudiosink`, `rialtomsesubtitlesink`) is a separate `GstElement` instance with its own `PullModePlaybackDelegate` (via type-specific subclasses: `PullModeVideoPlaybackDelegate`, `PullModeAudioPlaybackDelegate`, `PullModeSubtitlePlaybackDelegate`). This means there is **one queue per track**, each independently applying backpressure to its respective `appsrc`.

The **PushMode** delegates (used by other clients like WebKit) take a different approach where data is sent to RialtoServer immediately in the chain function rather than being queued.

### How Queued Buffers Reach RialtoServer

When RialtoServer needs data, it sends a `needMediaData` notification to the sink. This triggers a `PullBufferMessage` on a puller thread, which:

1. Calls `getFrontSample()` to peek at the next `GstSample` in the queue.
2. Maps the `GstBuffer` and passes it to `BufferParser::parseBuffer()`, which creates a Rialto `MediaSegment` containing:
   - The raw elementary stream data bytes
   - PTS/duration from the `GstBuffer`
   - `codec_data` extracted from the `GstCaps` structure (e.g. SPS/PPS for H.264)
   - DRM/protection metadata from buffer metadata
3. Calls `addSegment()` to write the `MediaSegment` into shared memory.
4. Calls `popSample()` to consume the sample and unblock AAMP's push path.
5. After processing up to `frameCount` buffers, sends a `haveData()` notification to RialtoServer.

### Init Segments Are Not Passed Through rialto-gstreamer

rialto-gstreamer does **not** forward init segments (e.g. moov/ftyp atoms) to RialtoServer as distinct entities. By the time data reaches the Rialto sinks, the GStreamer pipeline's demuxer (`qtdemux` inside playbin) has already parsed init segments and extracted codec configuration into `GstCaps`. The `BufferParser` then extracts `codec_data` from caps and attaches it to each `MediaSegment`. This is how RialtoServer receives codec initialization information — as per-sample metadata, not as separate init segment pushes.

## How AAMP Uses Rialto Sinks (`eAAMPConfig_useRialtoSink = true`)

When `eAAMPConfig_useRialtoSink` is enabled, AAMP's GStreamer pipeline in `middleware/InterfacePlayerRDK.cpp` makes several adaptations:

### 1. Flag Propagation
The config bool flows from `AampConfig` → `m_gstConfigParam->useRialtoSink` → `gstPrivateContext->usingRialtoSink` during `Configure()`.

### 2. Sink Element Creation (`SetupStream`)
- **Video**: Creates `rialtomsevideosink` via `gst_element_factory_make` and sets it as the `video-sink` on the playbin. For clear HLS/TS, it sets `has-drm=false`. The sink reference is stored in `gstPrivateContext->video_sink`.
- **Audio**: Creates `rialtomseaudiosink` and sets it as `audio-sink`. Stored in `gstPrivateContext->audio_sink`.
- **Subtitles**: Creates a `rialtomsesubtitlesink` inside a bin with `vipertransform`, sets it as `text-sink` on a playbin. Alternatively, for closed-caption-only streams, it uses `SetupClosedCaptionControlStream()`.

### 3. Stream Info Context
After configuring all streams, AAMP sends a `GstContext("streams-info")` to the pipeline with bitmasks indicating which streams (video/audio/text) are active. This tells the Rialto sinks when `allSourcesAttached()` should be called on the server side.

### 4. `single-path-stream` Property
AAMP sets the `single-path-stream` property on the video sink to `true` when there is no audio track (video-only), so Rialto knows not to wait for an audio source attachment.

### 5. Segment Delivery
For Rialto sinks, AAMP pushes segment info via `gst_app_src_push_sample()` with a `GstSample` containing a `GstSegment`, rather than sending a segment event directly. This is how Rialto expects to receive timing/rate information.

### 6. Trickplay
During trick modes (`Flush`), if the rate != 1x with Rialto, AAMP sends EOS to the audio stream (`GstPlayer_SignalEOS`) and stops audio injection, since trick-play is video-only.

### 7. Subtitle PTS Offset
When using Rialto, subtitle PTS offsets are sent via a custom downstream OOB event (`set-pts-offset`) rather than seeking the subtitle sink element.

### 8. Buffer Underflow Handling
AAMP connects to the `buffer-underflow-callback` signal on `rialtomsevideosink`/`rialtomseaudiosink` when they transition from NULL→READY, to be notified of decoder underruns.

### 9. Flush Bypasses
Some pipeline manipulations (like toggling async on audio sink during flush) are skipped when `usingRialtoSink` is true, since Rialto handles synchronization internally.

## Summary
AAMP demuxes and decrypts content as usual, pushes elementary stream buffers into `appsrc` elements, and the Rialto GStreamer sinks transparently forward those buffers to RialtoServer for hardware-accelerated decoding/rendering — abstracting away platform-specific decoder details behind the Rialto IPC boundary.
