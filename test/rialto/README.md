# Rialto Client Stub

This directory contains a stub implementation of the Rialto client library
(`libRialtoClient.so`) for use when building AAMP without the real Rialto
libraries.

## Purpose

The `direct-rialto/` code in AAMP depends on Rialto client symbols at link
time. On platforms where Rialto cannot be built (e.g. macOS, where protobuf
fails to compile), this stub provides the minimal set of symbols needed for
AAMP to link.

The stub provides no-op / nullptr-returning implementations of:
- `IMediaPipelineFactory::createFactory()`
- `IClientLogControlFactory::createFactory()`
- `IControlFactory::createFactory()`
- `MediaSegment::copy()` / `MediaSegmentAudio::copy()` / `MediaSegmentVideo::copy()`

## When is it used?

- **`install-aamp.sh`** (without `rialto` option): The stub is built and
  linked automatically. CMake detects that `libRialtoClient.so` is not
  present in `.libs/lib/` and falls back to building the stub.

- **`install-aamp.sh rialto`**: The real Rialto libraries are built and
  installed to `.libs/lib/`. CMake finds the real library and uses it
  directly — the stub is never built.

## Runtime behavior

When linked against the stub, all Rialto factory calls return `nullptr`.
The `direct-rialto/` code handles this gracefully (pipeline creation fails,
and AAMP falls back to GStreamer-based playback).
