# 03 - Implementation Mapping

## Overview
Maps each requirement to its implementing source files and key functions.

## REQ-001: Stream Playback Initialization
| File | Function/Class | Role |
|------|---------------|------|
| priv_aamp.cpp | Tune() | Entry point — URL parsing, config load, format detection |
| priv_aamp.cpp | TuneHelper() | Creates StreamAbstraction, calls Init/Start |
| priv_aamp.cpp | TeardownStream() | Stops previous stream before new tune |
| main_aamp.cpp | PlayerInstanceAAMP::Tune() | Public API wrapper |

## REQ-002: Adaptive Bitrate Switching
| File | Function/Class | Role |
|------|---------------|------|
| br/ABRManager.cpp | GetDesiredProfileOnSteadyState() | Bandwidth-based profile selection |
| br/ABRManager.cpp | GetDesiredProfileOnBuffer() | Buffer-occupancy-based selection |
| br/ABRManager.cpp | CheckRampDown() / CheckRampUp() | Ramp logic with hysteresis |
| streamabstraction.cpp | UpdateProfileBasedOnFragmentCache() | Triggers ABR check |

## REQ-003: DRM License Acquisition
| File | Function/Class | Role |
|------|---------------|------|
| drm/AampDRMLicManager.cpp | AampDRMLicenseManager | Orchestrates license requests |
| drm/DrmInterface.cpp | DrmInterface | Bridge to middleware DRM |
| AampDRMLicPreFetcher.cpp | AampLicensePreFetcher | Pre-fetch thread and queue |
| middleware/drm/ | Various | Platform DRM session management |

## REQ-004: GStreamer Pipeline Management
| File | Function/Class | Role |
|------|---------------|------|
| ampgstplayer.cpp | AAMPGstPlayer::Configure() | Pipeline element creation |
| ampgstplayer.cpp | AAMPGstPlayer::Send() | Buffer injection to appsrc |
| ampgstplayer.cpp | AAMPGstPlayer::Flush() | Seek/flush pipeline |
| ampgstplayer.cpp | AAMPGstPlayer::Stop() | Pipeline teardown |

## REQ-005: Time-Shift Buffer
| File | Function/Class | Role |
|------|---------------|------|
| AampTsbDataManager.cpp | AampTsbDataManager | Segment storage and retrieval |
| AampTSBSessionManager.cpp | AampTSBSessionManager | Session lifecycle, seek |
| AampTsbReader.cpp | AampTsbReader | Read segments from TSB |

## REQ-006: Event Notification
| File | Function/Class | Role |
|------|---------------|------|
| AampEventManager.cpp | SendEvent() | Dispatch event to listeners |
| AampEventManager.cpp | AddEventListener() | Register listener by type |
| AampEvent.h | AAMPEventType enum | All event type definitions |
| AampEventListener.cpp | AAMPEventObjectListener | Listener interface |

## REQ-007: Configuration Management
| File | Function/Class | Role |
|------|---------------|------|
| AampConfig.h | AampConfig class | Config storage, enums for all settings |
| AampConfig.cpp | ReadAampCfgTxtFile() | File-based config loading |
| AampConfig.cpp | ProcessConfigJson() | JSON config processing |
| AampConfig.cpp | SetValue() | Owner-priority-based setting |

## REQ-008: Scheduled Task Execution
| File | Function/Class | Role |
|------|---------------|------|
| AampScheduler.cpp | ScheduleTask() | Queue async task |
| AampScheduler.cpp | ExecuteAsyncTask() | Worker thread execution |
| AampScheduler.cpp | RemoveAllTasks() | Cleanup on stop |

## REQ-009: Network Download Management
| File | Function/Class | Role |
|------|---------------|------|
| AampCurlStore.h | CurlStore | Connection pool management |
| downloader/AampCurlDownloader.cpp | AampCurlDownloader | HTTP download with retry |
| AampCMCDCollector.cpp | AampCMCDCollector | CMCD header generation |

## REQ-010: HLS Fragment Collection
| File | Function/Class | Role |
|------|---------------|------|
| ragmentcollector_hls.cpp | StreamAbstractionAAMP_HLS | HLS stream abstraction |
| ragmentcollector_hls.cpp | TrackState::IndexPlaylist() | M3U8 parsing |
| ragmentcollector_hls.cpp | TrackState::FetchFragment() | Segment download |
| ragmentcollector_hls.cpp | TrackState::RunFetchLoop() | Continuous fetch thread |

## REQ-011: DASH/MPD Fragment Collection
| File | Function/Class | Role |
|------|---------------|------|
| ragmentcollector_mpd.cpp | StreamAbstractionAAMP_MPD | DASH stream abstraction |
| ragmentcollector_mpd.cpp | Init() | Manifest parse, period enumeration |
| ragmentcollector_mpd.cpp | FetchFragment() | Segment download by template/timeline |

## REQ-012: Stream Sink Management
| File | Function/Class | Role |
|------|---------------|------|
| AampStreamSinkManager.cpp | AampStreamSinkManager | Active/inactive sink routing |
| AampStreamSinkManager.h | Interface | Sink lifecycle management |

## REQ-013: Input Shim Abstraction
| File | Function/Class | Role |
|------|---------------|------|
| hdmiin_shim.cpp | StreamAbstractionAAMP_HDMIIN | HDMI input capture |
| ota_shim.cpp | StreamAbstractionAAMP_OTA | Over-the-air tuner |
| 
mf_shim.cpp | StreamAbstractionAAMP_RMF | RDK Media Framework |
| compositein_shim.cpp | StreamAbstractionAAMP_COMPOSITEIN | Composite video input |

## REQ-014: Middleware DRM Integration
| File | Function/Class | Role |
|------|---------------|------|
| middleware/drm/DrmSessionManager.cpp | DrmSessionManager | Manages DRM sessions, connects AAMP to platform DRM via OCDM |
| middleware/externals/contentsecuritymanager/ | CSM | Content security sessions |

## REQ-015: Middleware GStreamer Plugins
| File | Function/Class | Role |
|------|---------------|------|
| middleware/gst-plugins/cdmidecryptor/ | GstCdmiDecryptor | Decryption element |
| middleware/gst-plugins/ | Various | Custom GStreamer elements |
