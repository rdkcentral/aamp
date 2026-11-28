# Rialto reference pipeline (GStreamer)

A GStreamer implementation of the Rialto client API
`firebolt::rialto::IMediaPipeline`. It runs in-process, decoding and rendering
with a local GStreamer pipeline, and follows Rialto server behaviour. Any client
written against the Rialto API can then play real audio and video where no
Rialto server exists.

Uses:

- **Direct Rialto on the simulator.** AAMP's `AampRialtoPlayer` plays real A/V
  on macOS or Ubuntu without a Rialto server (see [Selecting the backend](#selecting-the-backend)).
- **A/B testing against real Rialto.** The same client binary runs against real
  Rialto or this simpler reference, which helps separate client bugs from Rialto
  bugs.
- **Other Rialto clients.** The library depends only on the public Rialto
  headers and GStreamer, so other players can link it as their pipeline backend.

## Layout

| Path | Content |
|------|---------|
| `include/rialto-reference/GstReferencePipeline.h` | Public API: `createGstMediaPipeline()`, `createGstMediaPipelineFactory()` |
| `source/GstReferencePipeline.cpp` | The pipeline |
| `source/MediaCaps.cpp` | Rialto MIME type / source / segment to GStreamer caps (one table) |
| `source/Executor.h` | Serial task thread (bus/timers, client callbacks) |
| `source/ReferenceLog.cpp` | `[RialtoRef]` stderr logging |

The library is static (`RialtoReferencePipeline`) and needs GStreamer 1.18 or
later with `gstreamer-app`. If GStreamer is missing, CMake defines no target.

## Pipeline

```
video:    appsrc ! decodebin ! queue ! videoconvert ! videobalance ! <video sink>
audio:    appsrc ! decodebin ! queue ! audioconvert ! audioresample ! volume ! <audio sink>
subtitle: appsrc ! fakesink        (paced and counted for EOS, not rendered)
```

Rialto server behaviour that is reproduced (`media/server/gstplayer` in the
Rialto repository):

- **Data requests.** appsrc `need-data`/`enough-data` drive `notifyNeedMediaData`
  (24 frames per request). Segments staged by `addSegment()` are pushed when
  `haveData()` completes the request. A request that ends with
  `NO_AVAILABLE_SAMPLES` is re-issued after 50 ms.
- **appsrc limits** are the server's: `max-time` 1 s for audio and video (falling
  back to `max-buffers`, then bytes), and `min-percent` 20.
- **Caps.** Caps come from the server's MIME mapping, segment alignment, stream
  format, codec data, video size, and audio rate and channels. Codec data, size,
  rate and channels carried on segments update the caps.
- **Seek.** `setPosition()` sends `SEEKING`, flushes every source, queues a new
  segment at the position, sends `SEEK_DONE`, then issues fresh data requests.
- **Per-source position and flush.** `setSourcePosition()` queues a segment that
  is pushed ahead of the source's next buffers. `flush()` flushes one source and
  sends `notifySourceFlushed`.
- **Timestamps.** Buffers keep the client's timestamps; segments map them to
  running time, so `getPosition()` is on the client's timeline.
- **States.** `IDLE` follows `allSourcesAttached()`. `PAUSED` is sent on
  reaching PAUSED, except for the re-preroll after a flush. `PLAYING` is sent on
  reaching PLAYING. `END_OF_STREAM` follows pipeline EOS. An error is reported as
  `FAILURE`, unless it is a stream error after every source ended, which is
  reported as EOS.
- **Rate.** `setPlaybackRate()` applies an instant rate change once PLAYING.

Where it intentionally differs from the server:

- A source attached after a pipeline-level `setPosition()` starts at that
  position.
- If no position was ever set, every source is anchored at the first PTS seen,
  unless `RIALTO_REF_STRICT_POSITION=1`.
- The flush on seek also applies while the pipeline is still prerolling.
- `audio/x-ac3` and `audio/x-ac4` are in the MIME table.

## Codecs

Decoding uses `decodebin`, so any codec in the MIME table plays if a decoder
plugin for it is installed. To add a codec, add a row to `mimeTable()` in
`source/MediaCaps.cpp`. Tested so far: H.264 (avc) and AAC (raw).

## Environment

| Variable | Effect |
|----------|--------|
| `RIALTO_REF_VIDEO_SINK` | Video sink as a gst-launch description, e.g. `fakesink sync=true` for headless runs. Default `autovideosink` |
| `RIALTO_REF_AUDIO_SINK` | Audio sink, likewise. Default `autoaudiosink` |
| `RIALTO_REF_VERBOSE=1` | Log every data request and `haveData()` |
| `RIALTO_REF_STRICT_POSITION=1` | Don't anchor at the first PTS: a source that gets data before any position starts its segment at 0, as on the server, so a missing `setSourcePosition()`/`setPosition()` shows up as a startup delay of the first PTS (logged per source) |

`GST_DEBUG` works as usual.

## Selecting the backend

In AAMP simulator builds, the Rialto simulator (`test/rialto/`) links this
library and picks a backend for each pipeline it creates:

| Setting | Backend |
|---------|---------|
| `RIALTO_SIM_BACKEND` unset or `sim` | Existing simulator: no decoding, modelled clock (default; L2 tests) |
| `RIALTO_SIM_BACKEND=gst` | This reference pipeline |

With `useDirectRialto=true`, the AAMP config `rialtoSimBackend=gst` sets the
environment variable for you. The real Rialto client ignores it.

The variable is process-wide: AAMP sets it when a player's sink is created,
and the simulator reads it when that player creates its pipeline. Players in
one process with different `rialtoSimBackend` values can therefore pick up
each other's choice. This is accepted for a simulator-only debug setting;
use the same backend for every player in a process.

The CMake option `RIALTO_SIM_GST_REFERENCE` (default `ON`) builds the library
into the simulator.

Example, headless:

```bash
mkdir -p /tmp/cfg && printf 'useDirectRialto=true\nrialtoSimBackend=gst\nprogress=true\n' > /tmp/cfg/aamp.cfg
AAMP_CFG_DIR=/tmp/cfg RIALTO_REF_VIDEO_SINK="fakesink sync=true" RIALTO_REF_AUDIO_SINK="fakesink sync=true" \
    ./build/Debug/aamp-cli <clear H.264/AAC DASH or HLS URL>
```

Drop the two sink variables to get a video window and audio output.

## Not yet supported

- Decryption. Encrypted samples are pushed as-is, with a warning.
- Rendering subtitles and closed captions.
- Negative rates, `switchSource()`, volume ramps, video window placement.
- `notifyBufferUnderflow`, `notifyPlaybackInfo`.
