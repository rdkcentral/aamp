# GStreamer Integration

## Overview

AAMP uses GStreamer for media pipeline management, decoding, and rendering.

## Architecture

**Files**: `aampgstplayer.h/cpp`, `gstaamptaskpool.h/cpp`, `middleware/InterfacePlayerRDK.cpp`

**Key Classes**:
- `AAMPGstPlayer`: GStreamer player wrapper
- `InterfacePlayerRDK`: Platform-specific GStreamer interface

## Pipeline Creation

### Pipeline Elements

Typical pipeline structure:
```
appsrc → parser → decoder → sink
```

For different formats:
- **ISO BMFF**: `qtdemux` parser
- **MPEG-TS**: `tsdemux` parser
- **HLS TS**: `tsdemux` with demuxing

### Pipeline Configuration

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

## Fragment Injection

### Injection Methods

1. **appsrc**: Push-based injection
2. **Stream Sink**: Custom sink interface

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

## Stream Sink

Custom sink interface for platform integration:
- `StreamSink`: Base interface
- Platform-specific implementations
- Frame export support

## Summary

GStreamer integration provides:
- Flexible pipeline configuration
- Multiple format support
- Platform-specific rendering
- Efficient fragment injection


