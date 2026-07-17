# 01 - Functional Requirements

## Overview
Functional requirements for AAMP player and middleware, traced to source code.

## REQ-001: Stream Playback Initialization
- **Description:** Player must support initialization of HLS, DASH, and Progressive streams
- **Source:** priv_aamp.cpp (Tune/TuneHelper), ragmentcollector_hls.cpp, ragmentcollector_mpd.cpp
- **Acceptance:** Player detects stream format from URL/manifest and creates appropriate StreamAbstraction

## REQ-002: Adaptive Bitrate Switching
- **Description:** Player must dynamically switch bitrates based on network conditions
- **Source:** br/ABRManager.cpp, StreamAbstractionAAMP.h (GetDesiredProfileOnBuffer/GetDesiredProfileOnSteadyState)
- **Acceptance:** ABR ramps up when bandwidth is stable, ramps down on buffer underrun

## REQ-003: DRM License Acquisition
- **Description:** Player must acquire DRM licenses before playing protected content
- **Source:** drm/AampDRMLicManager.cpp, drm/DrmInterface.cpp, AampDRMLicPreFetcher.cpp
- **Acceptance:** License acquired before first frame; supports Widevine, PlayReady, ClearKey

## REQ-004: GStreamer Pipeline Management
- **Description:** Player must create, configure, and manage GStreamer pipeline for A/V rendering
- **Source:** ampgstplayer.cpp, ampgstplayer.h
- **Acceptance:** Pipeline created on Tune, flushed on Seek, destroyed on Stop

## REQ-005: Time-Shift Buffer (TSB)
- **Description:** Player must support time-shift buffering for live streams
- **Source:** AampTsbDataManager.cpp, AampTSBSessionManager.cpp, AampTsbReader.cpp
- **Acceptance:** TSB stores segments to disk, supports seek-back within buffer window

## REQ-006: Event Notification
- **Description:** Player must notify application of state changes via events
- **Source:** AampEvent.h, AampEventManager.cpp, AampEventListener.cpp
- **Acceptance:** Events dispatched for tuned, playing, paused, error, EOS, bitrate change

## REQ-007: Configuration Management
- **Description:** Player must support runtime and file-based configuration
- **Source:** AampConfig.h, AampConfig.cpp
- **Acceptance:** Config loaded from file, overridden by JS/app, validated with owner priority

## REQ-008: Scheduled Task Execution
- **Description:** Player must support deferred and periodic task scheduling
- **Source:** AampScheduler.h, AampScheduler.cpp
- **Acceptance:** Tasks queued by ID, executed on dedicated thread, removable by ID

## REQ-009: Network Download Management
- **Description:** Player must manage HTTP downloads with retry, timeout, and CMCD
- **Source:** AampCurlStore.h, downloader/AampCurlDownloader.cpp, AampCMCDCollector.cpp
- **Acceptance:** Downloads retried on failure, timeouts enforced, CMCD headers attached

## REQ-010: HLS Fragment Collection
- **Description:** Player must parse HLS playlists and fetch fragments in sequence
- **Source:** ragmentcollector_hls.cpp, ragmentcollector_hls.h
- **Acceptance:** Playlists parsed, fragments fetched in order, DRM init data extracted

## REQ-011: DASH/MPD Fragment Collection
- **Description:** Player must parse MPD manifests and fetch segments per adaptation set
- **Source:** ragmentcollector_mpd.cpp, ragmentcollector_mpd.h
- **Acceptance:** Periods enumerated, segments fetched by template/timeline, dynamic refresh

## REQ-012: Stream Sink Management
- **Description:** Player must route decoded frames to appropriate output sink
- **Source:** AampStreamSinkManager.cpp, AampStreamSinkManager.h
- **Acceptance:** Active sink receives buffers; inactive sink created for seamless switch

## REQ-013: Input Shim Abstraction
- **Description:** Player must support non-HTTP inputs (HDMI-in, OTA, RMF, composite)
- **Source:** hdmiin_shim.cpp, ota_shim.cpp, mf_shim.cpp, compositein_shim.cpp
- **Acceptance:** Each shim implements StreamAbstractionAAMP interface for its input type

## REQ-014: Middleware DRM Integration
- **Description:** Middleware must bridge AAMP DRM requests to platform DRM subsystem
- **Source:** middleware/drm/, middleware/externals/contentsecuritymanager/
- **Acceptance:** DRM sessions created/destroyed, keys provided to decryptor

## REQ-015: Middleware GStreamer Plugins
- **Description:** Middleware must provide GStreamer elements for decryption and playback
- **Source:** middleware/gst-plugins/, middleware/gst-plugins/cdmidecryptor/
- **Acceptance:** Decryptor plugin registered, receives encrypted buffers, outputs clear
