# 02 - Design Decisions

## Overview
Key design decisions in AAMP core and middleware, with rationale traced to source.

## DES-001: Plugin-Based Stream Format Selection
- **Decision:** Use factory pattern to select StreamAbstraction subclass based on URL/manifest format
- **Rationale:** Supports HLS, DASH, Progressive, HDMI-in, OTA, RMF without modifying core Tune logic
- **Source:** priv_aamp.cpp TuneHelper() — switch on mMediaFormat
- **Trade-off:** Each format requires full StreamAbstraction implementation; adding new formats requires new subclass

## DES-002: Dedicated Inject Thread Per Track
- **Decision:** Each MediaTrack runs its own inject thread (RunInjectLoop)
- **Rationale:** Decouples fragment download from GStreamer buffer injection; prevents pipeline stalls
- **Source:** streamabstraction.cpp RunInjectLoop(), StreamAbstractionAAMP.h MediaTrack
- **Trade-off:** Thread management complexity; requires synchronization for flush/stop

## DES-003: Curl Connection Pooling
- **Decision:** Reuse curl handles via AampCurlStore with per-host connection pools
- **Rationale:** Reduces TLS handshake overhead, improves segment download latency
- **Source:** AampCurlStore.h, downloader/AampCurlDownloader.cpp
- **Trade-off:** Memory usage for idle connections; requires LRU eviction

## DES-004: Centralized Event Bus
- **Decision:** Single AampEventManager dispatches all events; listeners register by event type
- **Rationale:** Decouples event producers from consumers; supports async and sync dispatch
- **Source:** AampEventManager.cpp, AampEventListener.cpp
- **Trade-off:** All events go through single queue; high-frequency events could bottleneck

## DES-005: Configuration Owner Priority
- **Decision:** Config values have owner priority (DEFAULT < AAMP_CFG < APP < STREAM < DEV)
- **Rationale:** Allows layered override — defaults ship with code, app/stream can override, dev can force
- **Source:** AampConfig.h ConfigPriority enum, AampConfig.cpp SetValue with owner check
- **Trade-off:** Debugging requires knowing which owner set a value

## DES-006: ABR Steady-State vs Buffer-Based
- **Decision:** Two ABR modes — steady state (bandwidth-based) and buffer-based (occupancy-based)
- **Rationale:** Steady state maximizes quality; buffer-based prevents underflow during congestion
- **Source:** br/ABRManager.cpp GetDesiredProfileOnSteadyState/GetDesiredProfileOnBuffer
- **Trade-off:** Mode switching logic adds complexity

## DES-007: TSB Segment-Based Storage
- **Decision:** TSB stores individual segments as separate entries with metadata index
- **Rationale:** Enables random access seek within buffer window; simplifies eviction (remove oldest)
- **Source:** AampTsbDataManager.cpp, AampTSBSessionManager.cpp
- **Trade-off:** Disk I/O per segment; requires periodic cleanup

## DES-008: DRM License Pre-Fetching
- **Decision:** Dedicated pre-fetcher thread acquires licenses ahead of playback position
- **Rationale:** Eliminates license acquisition latency during key rotation
- **Source:** AampDRMLicPreFetcher.cpp, AampDRMLicPreFetcherInterface.h
- **Trade-off:** Wasted licenses if user seeks past pre-fetched keys

## DES-009: Middleware DRM Bridge Pattern
- **Decision:** DrmInterface class bridges AAMP core DRM requests to platform-specific middleware
- **Rationale:** Isolates AAMP from platform DRM details; enables testing with mock DRM
- **Source:** drm/DrmInterface.h/cpp, middleware/drm/
- **Trade-off:** Extra indirection layer; must keep bridge API stable

## DES-010: GStreamer Pipeline Reuse
- **Decision:** Pipeline is flushed and reconfigured on channel change rather than destroyed/recreated
- **Rationale:** Faster tune time; avoids GStreamer element re-negotiation overhead
- **Source:** ampgstplayer.cpp Flush(), priv_aamp.cpp TeardownStream (newTune flag)
- **Trade-off:** Pipeline state must be carefully managed; stale state bugs possible
