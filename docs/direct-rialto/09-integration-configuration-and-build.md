# Direct Rialto - Integration, configuration and build

## Purpose

This page describes the seams between direct-rialto and the rest of AAMP: how `AampRialtoPlayer` is selected and created, which `StreamSink` methods it implements (and which it ignores), how it notifies `PrivateInstanceAAMP`, which configuration options influence it, how it is built and linked, what must be present at run time, and where AAMP code outside `direct-rialto/` contains special cases for it. Use it when you need to enable the path, debug a build or link problem, or understand why core code behaves differently when direct-rialto is on.

## Source files

| File | Role |
| --- | --- |
| `AampStreamSinkManager.cpp:CreateSinkInstance` | Factory that picks `AampRialtoPlayer` or `AAMPGstPlayer` from `eAAMPConfig_useDirectRialto` |
| `StreamSink.h` | The sink contract implemented by `AampRialtoPlayer` |
| `direct-rialto/AampRialtoPlayer.cpp` | StreamSink implementation |
| `direct-rialto/IStreamSinkNotifiable.h`, `PrivateInstanceAAMPNotifiable.h/.cpp` | Notification interface and adapter back into `PrivateInstanceAAMP` |
| `AampConfig.cpp`, `AampConfig.h` | Config entries and the `DoCustomSetting` rules |
| `priv_aamp.cpp` (constructor) | Installs the DirectRialto CC manager type and the Rialto DRM session creator |
| `CMakeLists.txt` | Compiles `direct-rialto/*.cpp` into libaamp; links `RIALTO_LIBRARIES` |
| `cmake/FindRialto.cmake` | Locates the real Rialto client library or selects the simulator |
| `test/rialto/CMakeLists.txt`, `RialtoSimulator.cpp` | Builds the in-process simulator used when real Rialto is not linked |
| `scripts/install_rialto.sh`, `scripts/install_options.sh`, `scripts/install_aampcli.sh`, `install-aamp.sh` | Clone/build Rialto, expose the `rialto` and `--rialto-force-simulator` options, build aamp-cli twice |
| `streamabstraction.cpp`, `fragmentcollector_mpd.cpp`, `fragmentcollector_hls.cpp`, `priv_aamp.cpp`, `mp4demux/AampMp4Demuxer.h`, `main_aamp.cpp` | Core code with direct-rialto or Rialto special cases (see the last section) |

## How the player is selected and created

[[diagram:direct-rialto-build-selection-activity]]

*Build-time choice of Rialto library (real or simulator) and run-time choice of sink.*

1. The application (or aamp-cli) constructs `PlayerInstanceAAMP` (`main_aamp.cpp`). That constructs `PrivateInstanceAAMP`, whose constructor reads `eAAMPConfig_useDirectRialto` once:
   - `PlayerCCManager::SetRialto(useRialtoSink, useDirectRialto)` selects `PlayerDirectRialtoCCManager` (from middleware-player-interface) when `useDirectRialto` is set.
   - If `useDirectRialto` is set, `makeRialtoSessionCreator()` is passed to `AampDRMLicenseManager`, so DRM sessions are Rialto `IMediaKeys` sessions instead of OCDM.
2. `PlayerInstanceAAMP` then asks `AampStreamSinkManager::CreateStreamSink`, which calls `CreateSinkInstance`: `useDirectRialto` creates `new AampRialtoPlayer(aamp, id3Handler, exportFrames)`; otherwise `new AAMPGstPlayer(...)`. If the sink is missing at tune time (`PrivateInstanceAAMP` tune path) the same call is made again.
3. If a client hands in its own `StreamSink` (`SetStreamSink`), the factory is bypassed and direct-rialto is not used.

Rules that follow from this:

- `useDirectRialto` must be set before the player instance exists. `AampConfig::DoCustomSetting` reverts any attempt to change it through app, stream or tune settings and logs "cannot be changed dynamically after player creation". Device config, environment and operator settings read before creation do work.
- `useMp4Demux` is forced on whenever `useDirectRialto` is set (`DoCustomSetting` logs "useDirectRialto requires useMp4Demux; forcing it on"): the player is fed demuxed ISO BMFF samples and codec data, not a container for a GStreamer demuxer.
- `UsingRialto()` (`AampConfig::IsUsingRialto`) is true if either `useRialtoSink` or `useDirectRialto` is set. It does not say which. Code that must distinguish the two checks `useDirectRialto` directly.
- In single-pipeline mode there is one sink for several `PrivateInstanceAAMP` instances; `AampStreamSinkManager::SetActive` calls `AampRialtoPlayer::ChangeAamp`, `Flush(position, rate, true, true)` and `ResetFirstFrame`. See the architecture page for the ownership consequences.

## StreamSink API surface

The table lists every method `AampRialtoPlayer` overrides. "No-op" means the method logs and returns without effect.

| Method | Behaviour in `AampRialtoPlayer` |
| --- | --- |
| `Configure(codecInfo, bESChange, setReady)` | Waits for any flush, then reuses the pipeline if the source set is unchanged or tears down and rebuilds: creates the pipeline, sources (video, audio, subtitle or inband CC) and arms the position-pending hold |
| `SendCopy` | Injects a data fragment that AAMP did not demux (muxed HLS-TS path); on first data attaches all sources using the codec format alone |
| `SendTransfer` | Injects an ISO BMFF fragment: init fragments go to `processInitFragment` then `AttachSource`; data fragments are demuxed by the source (`discontinuity` argument is only logged) |
| `SendSample` | Injects one already-demuxed sample (`injectSingleSample`) |
| `PipelineConfiguredForMedia` | True if the track's source exists and is attached |
| `EndOfStreamReached` | Latches EOS on the source; auto-signals subtitle EOS when video reaches EOS |
| `Stream` | Records a play request with the playback controller; `play()` is issued when no `PlayHold` remains |
| `Stop(keepLastFrame)` | Aborts injection, stops and releases the pipeline and control backend, resets controller and positions, state machine to IDLE |
| `Flush(position, rate, shouldTearDown, positionIsAuthoritative)` | Flushing seek: stages position and rate, gates sources, calls `setPosition()`; completes on SEEK_DONE. Falls back to teardown or a no-cycle path when not flushable |
| `FlushTrack` | No-op |
| `SeekStreamSink(position, rate)` | `Flush(position, rate, false)` followed by `Stream()` |
| `SetPlayBackRate` | `pipeline->setPlaybackRate(rate)` |
| `Pause(pause, forceStopPreBuffering)` | Pause cancels a pending play request and calls `pause()` (optionally stops pre-roll); unpause ungates sources and calls `play()` |
| `GetDurationMilliseconds` | Last duration reported by Rialto (`notifyDuration`) |
| `GetPositionMilliseconds` | `getPosition()` minus the segment-start baseline, clamped for forward play; 0 while FLUSHING/FLUSHED or position pending |
| `GetVideoPTS` | `getPosition()` converted to a 90 kHz PTS; 0 when unavailable |
| `SetVideoRectangle`, `GetVideoRectangle`, `GetVideoSize` | Store "x,y,w,h" and call `setVideoWindow()`; size is parsed back from the stored rectangle (Rialto has no size query) |
| `SetVideoZoom` | No-op (logs not supported) |
| `SetVideoMute`, `SetSubtitleMute` | Cache the flag and call `setMute()` on the attached source; re-applied when the source first attaches |
| `SetAudioVolume` | Caches 0-100; volume 0 mutes the audio source, otherwise `setVolume()` with 0.0-1.0; re-applied on pipeline creation and audio attach |
| `SetSubtitlePtsOffset` | No-op |
| `Discontinuity(mediaType)` | Ignored before the first sample; PTS-restamp case completes immediately, otherwise signals EOS on the track (and subtitle for video) and resumes buffering; may arm the deferred position flush |
| `CheckForPTSChangeWithTimeout(timeout)` | Compares `GetVideoPTS()` with the last value; false (stalled) only if unchanged for longer than the timeout |
| `IsCacheEmpty` | Always false |
| `ResetEOSSignalledFlag` | No-op |
| `NotifyFragmentCachingOngoing` / `Complete` | Adds/releases the `FragmentCaching` hold; ongoing also requests play and pauses the pipeline |
| `QueueProtectionEvent`, `ClearProtectionEvent` | Store or clear the per-track PSSH data used when sources attach |
| `SignalTrickModeDiscontinuity` | No-op |
| `StopBuffering(forceStop)` | Ungates all sources and calls `play()` (both modes resume unconditionally) |
| `SetTextStyle` | No-op (logs not supported) but returns true |
| `GetVideoPlaybackQuality` | `getStats()` of the video source (rendered/dropped) while PLAYING or PAUSED |
| `SignalSubtitleClock` | Sets the subtitle source position from the pipeline position |
| `SetPauseOnStartPlayback` | Not supported; logs a warning if enabled |
| `NotifyInjectorToResume` / `NotifyInjectorToPause` | Ungate every source / abort in-flight injection and drop new samples |
| `UnblockTrackInjection(type)` | Releases the injector for one track; ungating needs `Stream()` |
| `SetStreamCaps(type, codecInfo)` | Attaches the track's source from caps supplied by core (the already-demuxed path) |
| `IsAssociatedAamp`, `ChangeAamp`, `SetEncryptedAamp`, `ResetFirstFrame` | Single-pipeline support: compare/switch the owning `PrivateInstanceAAMP`, install a DRM bridge for an encrypted player, re-arm first-frame notification |

Not overridden (base defaults apply): `AdjustPlayBackRate` (returns true), `getCCDecoderHandle` (0; the handle is instead passed through `NotifyFirstFrameReceived`), `GetTextStyle` (false), `GetEncryptedAampId` (-1).

`AampRialtoPlayer` also implements `IDirectRialtoCC` (`setTextTrackIdentifier`, `setCCMute`), whose pointer is given to the CC manager as the decoder handle when the subtitle source is inband closed caption.

## Notifications back into PrivateInstanceAAMP

[[diagram:direct-rialto-notification-sequence]]

*Rialto events arrive on Rialto's thread; void notifications are queued onto the AAMP scheduler thread; two discontinuity calls are synchronous.*

`PrivateInstanceAAMPNotifiable` forwards each call. Void notifications are queued with `PrivateInstanceAAMP::ScheduleAsyncTask` rather than called inline. The reason, from the source comment: a Rialto callback thread may hold a Rialto lock and then need an AAMP lock (`mLock`, `mStreamLock`) while an AAMP thread holds that lock and waits on a Rialto call (an AB/BA deadlock). Targets are re-read on each call (`ChangeAamp()` can switch them) and a call with no target is dropped with a warning.

| Notification | Fired by (player) when | What core does |
| --- | --- | --- |
| `NotifyFirstFrameReceived(ccHandle)` | Every PLAYING state (`HandlePlaybackStarted`) | Skips if IDLE; sets state PLAYING unless first-video-frame-displayed is required; signals `waitforplaystart`; sends the tuned event if configured for GStreamer-playing; initialises CC with the handle; handles pause-on-start |
| `NotifyFirstBufferProcessed(videoRect)` | Every PLAYING state | Skips if IDLE; SEEKING becomes PLAYING (unless first-frame-displayed is required); resets trick start time; with SecManager or Firebolt informs the DRM manager of mute, speed and window size |
| `LogFirstFrame()`, `LogTuneComplete()` | First PLAYING of a session only | Profiler first-frame bucket; tune-end metrics and tune-time event |
| `NotifyFirstVideoFrameDisplayed()` | First PLAYING, and PLAYING while core state is SEEKING | Does nothing unless `mFirstVideoFrameDisplayedEnabled`; then sets state or pauses for seek-while-paused |
| `NotifyEOSReached()` | Rialto `END_OF_STREAM` | If a discontinuity is pending, continues discontinuity processing; for VOD forward play sets COMPLETE and sends `AAMP_EVENT_EOS`; rewind reaching the start reports progress at position 0; other cases (live, TSB) are handled in `PrivateInstanceAAMP::NotifyEOSReached` |
| `MonitorProgress(false, false)` | Progress timer tick (GLib loop) | Emits the progress event unless IDLE/SEEKING/COMPLETE/RELEASED or downloads disabled |
| `GetProgressReportIntervalSeconds()` | `StartProgressTimer` (query) | Reads `eAAMPConfig_ReportProgressInterval` |
| `GetState()` | `HandlePlaybackStarted` (query) | Returns the core player state |
| `NotifyBufferUnderflow(track)` | Rialto buffer underflow for a known source | With `enableAampUnderflowMonitor` only logs; otherwise calls `ScheduleRetune(eGST_ERROR_UNDERFLOW)`, which itself ignores underflow retunes, so effectively a log either way |
| `NotifyPlaybackError(failure, text, retry)` | FAILURE state (`AAMP_TUNE_GST_PIPELINE_ERROR`, retry on, once per session); Rialto error DECRYPTION (`AAMP_TUNE_DRM_DECRYPT_FAILED`, retry on) or UNKNOWN (pipeline error, retry on); attach failure (pipeline error, retry off) | `SendErrorEvent(failure, text, retry)` |
| `NotifyOutputProtectionRecovered()` | Rialto error OUTPUT_PROTECTION (HDCP recovered) | Mutes the sink video directly (transient) and calls `ScheduleRetune(eGST_ERROR_OUTPUT_PROTECTION_ERROR, VIDEO)` |
| `CompleteDiscontinuityDataDeliverForPTSRestamp(type)` | `Discontinuity()` with PTS restamp active and no ES change; synchronous | Unblocks the thread waiting for discontinuity processing |
| `NotifyPipelinePausedToUnderflowMonitor()` | `Discontinuity()` EOS branch with the underflow monitor enabled; synchronous | `mpStreamAbstractionAAMP->NotifyPipelinePausedToUnderflowMonitor()` so the monitor disarms its deadline |
| `SendMonitorAvEvent(...)` | `AampRialtoMonitorAV` report tick | Emits a `MonitorAVStatusEvent` (synchronous event mode) if play is enabled |
| `NotifySpeedChanged(rate, changeState)` | Never called by direct-rialto | Core would set PAUSED/PLAYING/trick state; the method exists in the interface and adapter only |

### Calls that bypass the notifiable

`AampRialtoPlayer` also calls `PrivateInstanceAAMP` directly: `mConfig` reads (`MonitorAV` settings, `GStreamerBufferingBeforePlay`, `EnablePTSReStamp`, `EnableAampUnderflowMonitor`, `ReportProgressInterval`), `ResumeTrackDownloads(track)`, `ReconfigureForElementaryStreamUpdate()` and `WillFlushOnDiscontinuity()`. `AampDrmBridge` reads `mDRMLicenseManager`, `mMediaFormat` and `GetSessionId()`.

## Configuration options that affect direct-rialto

Defaults are the first field of each entry in the `AampConfig.cpp` table.

| Option (`eAAMPConfig_...`) | Default | Effect |
| --- | --- | --- |
| `useDirectRialto` | false | Selects `AampRialtoPlayer`, the DirectRialto CC manager and the Rialto DRM session creator. Read once at player creation; cannot be changed afterwards |
| `useRialtoSink` | false | Selects Rialto GStreamer sinks inside `AAMPGstPlayer`. With `useDirectRialto` it is not needed: `IsUsingRialto()` is true for either (see ARCH-40) |
| `useMp4Demux` | false | Forced on by `useDirectRialto` |
| `GStreamerBufferingBeforePlay` (`gstBufferAndPlay`) | true | Enables pre-roll before `play()` at normal rate. Switched off when `initialBuffer` is greater than 0 |
| `ReportProgressInterval` (`progressReportingInterval`, seconds) | 1 | Interval of the progress timer and the AV monitor sample timer |
| `MonitorAV` (`monitorAV`) | false | Creates `AampRialtoMonitorAV` per pipeline |
| `MonitorAVReportingInterval` | 1000 ms | AV monitor report cadence |
| `MonitorAVSyncThresholdPositive` / `Negative` | 100 / -100 ms | A/V sync delta thresholds |
| `MonitorAVJumpThreshold` | 100 ms | Unexpected position-jump threshold |
| `EnableAampUnderflowMonitor` (`enableAampUnderflowMonitor`) | true | Decides who handles underflow; `Discontinuity()` also notifies the monitor when set |
| `EnablePTSReStamp` (`enablePTSReStamp`) | false | Chooses between the restamp discontinuity path and the EOS path in `Discontinuity()`; also changes the subtitle demux/restamp split in core |
| `DisableTTML` (`disableTTML`) | false | When false and Rialto is in use, core saves the subtitle init header of VOD content (see special cases) |

Where config is set: `aamp.cfg` (read from the config path), the `AAMP_CFG_TEXT` / `AAMP_CFG_BASE64` environment variables (`AampConfig.cpp`), and operator/device settings. The L2 framework passes a config text such as `useMp4Demux=true`, `useDirectRialto=true`, `useRialtoSink=true`.

## Build wiring

| Item | Detail |
| --- | --- |
| Sources | `LIBAAMP_SOURCES` in `CMakeLists.txt` lists every `direct-rialto/*.cpp`; there is no option to leave them out |
| Includes | `${CMAKE_CURRENT_SOURCE_DIR}/direct-rialto`, `${RIALTO_INCLUDE_DIRS}`, `.libs/include` (middleware headers such as `IDirectRialtoCC.h`); Rialto headers are included unprefixed (`IControl.h`) |
| Find module | `cmake/FindRialto.cmake`: `find_library(libRialtoClient.so)` and `find_path(IMediaPipeline.h)`; cache options `RIALTO_FORCE_SIMULATOR` and `RIALTO_VERBOSE_BUILD`; `APPLE` always uses the simulator. `RIALTO_INCLUDE_DIRS` is set even if the library is missing so L1 builds can compile with Rialto mocked |
| Fallback | If `RIALTO_FOUND` is false, `CMakeLists.txt` runs `add_subdirectory(test/rialto)`, sets `RIALTO_LIBRARIES` to the `RialtoSimulator` target (output name `RialtoClient`, SOVERSION 0) and defaults the include dir to `.libs/include/rialto` |
| Link | `${RIALTO_LIBRARIES}` is part of `LIBAAMP_DEPENDS` |
| Imported target | `Rialto::RialtoClient` exists but is not used for linking (not GLOBAL; the original commit replaced it with `${RIALTO_LIBRARIES}`) |

Build/option matrix:

| Command | Rialto client linked | Result |
| --- | --- | --- |
| `bash install-aamp.sh` (Linux) | Simulator (`-DRIALTO_FORCE_SIMULATOR=ON` is passed by `aampcli_install_build_linux_fn`) | `build/aamp-cli`, runs direct-rialto against `libRialtoClient.so.0` from `build/test/rialto` |
| `bash install-aamp.sh rialto` | Simulator build as above, plus a second build against real Rialto built from source (`OPTION_RIALTO_REFERENCE`, default `v0.25.0`; rialto-gstreamer `v0.23.0`) | Also `build-rialto/aamp-cli`, linked to `libRialtoClient.so.1` found through explicit `-DRIALTO_LIBRARY` / `-DRIALTO_INCLUDE_DIR`; installed under `.libs-rialto` |
| `bash install-aamp.sh rialto --rialto-force-simulator` | Simulator only | The `rialto` option is cleared with a warning |
| macOS | Simulator only | `rialto` option rejected with a warning; `FindRialto` forces the simulator |
| CI image (`.github/Dockerfile.ci`) | Headers only | `rialto_install_build_fn false` clones and copies the public headers so L1 tests compile; `CPLUS_INCLUDE_PATH` includes `include/rialto` |

`scripts/install_rialto.sh` always clones the Rialto repository and copies its public headers to `.libs/include/rialto`, because L1 tests need them even without building Rialto. Building real Rialto (`rialto_build_fn`) additionally builds protobuf (`-p` sets the branch), rialto and rialto-gstreamer into `.libs`.

## Runtime requirements

- **A Rialto server and client socket.** The Rialto client library connects to the server using `RIALTO_SOCKET_PATH` (absolute path of the socket) or `RIALTO_SOCKET_FD` (file descriptor). direct-rialto never reads these itself; this was verified in the pinned Rialto v0.25.0 client (`media/client/ipc/source/IpcClient.cpp`), which logs "No rialto socket specified" if neither is set. The L2 tests set `RIALTO_SOCKET_PATH=/tmp/rialto-0`.
- **The server must be RUNNING.** `AampRialtoPipelineSession::CreateAndLoad` waits up to 2000 ms for `ApplicationState::RUNNING`. After the timeout it logs a warning and proceeds. If `IMediaPipelineFactory::createFactory()` returns null it logs "is the Rialto server running?" and the tune fails to create a pipeline.
- **A GLib main loop on the default context.** The progress timer, the pre-roll monitor and the AV monitor use GLib timers on the default main context. The core does not run a loop; the host must (aamp-cli runs one in `Aampcli::aampGstPlayerStreamThread`). Without it there are no progress reports and pre-roll never completes (see ARCH-22).
- **Logs.** Rialto client log lines are bridged into the AAMP log with the tag `RialtoClient`. Server logs go to the system journal (the Rialto test README shows `journalctl -f` and `RIALTO_DEBUG=5` / `RIALTO_CONSOLE_LOG=1`).
- **Simulator runs.** Nothing external is needed: the simulator reports `RUNNING` immediately and logs with a `[RialtoSim]` prefix.

## Using aamp-cli

1. Build with `bash install-aamp.sh` (simulator) or `bash install-aamp.sh rialto` (real).
2. Enable the path in `aamp.cfg`: `useDirectRialto=true` (and `useMp4Demux=true`, which is forced anyway). `useRialtoSink=true` is also set in the L2 tests.
3. Run `build/aamp-cli <url>`. The binary's run path includes `build/test/rialto` and `.libs/lib`, so it loads `libRialtoClient.so.0` (the simulator) without `LD_LIBRARY_PATH`. For the real build run `build-rialto/aamp-cli` with `LD_LIBRARY_PATH` pointing at `.libs-rialto/lib` and `.libs/lib`, plus `RIALTO_SOCKET_PATH`, with the Rialto server manager running (`aamp_test_internal/test/Rialto/startmanager.sh`, then `curl -X POST -d "" localhost:9008/SetState/YouTube/Active`).
4. Look for `AampRialtoMediaPipelineClient: constructed` and `PlayerState: IDLE -> PIPELINE_CREATED` in the log; the L2 tests use these lines as markers.

The existing `aamp_test_internal/test/Rialto/README.md` describes the rialto-gstreamer path (`useRialtoSink=true`), not direct-rialto.

## Places outside direct-rialto with direct-rialto-specific code

| File:Symbol | Condition | What it does and why |
| --- | --- | --- |
| `AampStreamSinkManager.cpp:CreateSinkInstance` | `useDirectRialto` | Creates `AampRialtoPlayer` instead of `AAMPGstPlayer` |
| `priv_aamp.cpp` (constructor) | `useDirectRialto` | `PlayerCCManager::SetRialto(...)`; `makeRialtoSessionCreator()` into `AampDRMLicenseManager` |
| `AampConfig.cpp:DoCustomSetting` | `useDirectRialto` | Reverts dynamic changes; forces `useMp4Demux` on |
| `AampConfig.cpp:IsUsingRialto` | either Rialto flag | Shared helper behind `PrivateInstanceAAMP::UsingRialto()` |
| `streamabstraction.cpp` (Mp4Demux setup) | `useDirectRialto` and `FORMAT_SUBTITLE_MP4` | Subtitle MP4 gets an `AampMp4Demuxer` in core; raw TTML/WebVTT do not. Comment explains that `AampRialtoPlayer::Configure()` queues the source attachment |
| `streamabstraction.cpp` (subtitle restamp guard) | `!playContext` | Avoids double restamping for direct-rialto plus subtitle MP4 (uint64 underflow otherwise) |
| `streamabstraction.cpp` (`UpdateSubtitleClockTask`) | `UsingRialto()` | Subtitle clock thread is not started |
| `fragmentcollector_mpd.cpp` (subtitle period change) | `useDirectRialto` | Does not call `StopTrackDownloads(eMEDIATYPE_SUBTITLE)` because the subtitle playContext persists and the next init segment must reach the sink |
| `fragmentcollector_mpd.cpp` (init) | `UsingRialto()` | Saves the subtitle init header from VOD content for reuse after pre-roll ads; injects a saved subtitle init when subtitles are off so the pipeline can complete |
| `fragmentcollector_hls.cpp` (text track selection) | `UsingRialto()` | Always selects a default text track; CC uses `FORMAT_INVALID` so a subtitle source exists for control |
| `priv_aamp.cpp:StopTrackInjection` | discard | Calls `sink->UnblockTrackInjection(type)`; "Direct Rialto blocks the injector thread(s) whilst waiting for NeedData" |
| `mp4demux/AampMp4Demuxer.h` (`mAborted`) | n/a | Abort flag so an in-flight fragment stops sending samples when the sink unblocks injection |
| `priv_aamp.cpp:GetStreamFormat` | `UsingRialto()`, local TSB, rate not normal | Audio and subtitle formats forced to `FORMAT_INVALID` in trickplay |
| `priv_aamp.cpp:isDecryptClearSamplesRequired` | `UsingRialto()` | No decrypt call for clear samples |
| `main_aamp.cpp` | `IsUsingRialto()` | Routes logging to the Ethan logger (no `sd_journal` in the container) |
| `AampStreamSinkManager` single-pipeline code | any sink | `ChangeAamp`, `Flush(..., true, true)`, `ResetFirstFrame`, `SetEncryptedAamp` are part of the `StreamSink` contract that `AampRialtoPlayer` implements. `GetEncryptedAampId` is not overridden (ARCH-44) |

## Related pages

- Direct Rialto - Overview (top page)
- Direct Rialto - Architecture and class model
- Direct Rialto - Lifecycle and state machine
- Direct Rialto - Media data path
- Direct Rialto - Flush and seek
- Direct Rialto - Testing

## Workarounds and improvement candidates

See `_register/ARCH.md`: ARCH-08 to ARCH-10 (StreamSink methods that are no-ops or misleading), ARCH-15 and ARCH-16 (notifications that do nothing), ARCH-22 (GLib loop dependency), ARCH-23 to ARCH-35 (special cases and workarounds outside the folder), ARCH-30 and ARCH-31 (config rules and missing README entry), ARCH-33 and ARCH-34 (build wiring), ARCH-44 (`GetEncryptedAampId`) and ARCH-45 (stale simulator README).

## Open questions

- Is `useRialtoSink=true` required alongside `useDirectRialto=true` for any behaviour? `IsUsingRialto()` is true for either, and `PlayerCCManager::SetRialto` prefers the direct flag, but the L2 tests set both and this was not verified by running them without it.
- The `real Rialto` aamp-cli build installs under `.libs-rialto` (`install_aampcli.sh`, `framework.py`), but no `.libs-rialto` directory exists in this working tree; the repository snapshot examined here has only a `build-rialto` build directory whose `aamp-cli` could not resolve `libRialtoClient.so.1` without extra library paths.
- Which embedding applications run the default GLib main context, and who does so in the product stack (aamp-cli does)?
