# Direct Rialto - Troubleshooting and glossary

## Purpose

A practical guide for investigating direct-rialto problems from logs: how to build a timeline, which log lines matter, a symptom-to-cause table that points to the right page, and the full glossary of terms used across this documentation set. For the full list of log messages see [Monitoring and diagnostics](08-av-monitoring-timers-and-position.md), section 8.

## Source files

| File | Role |
|---|---|
| `AampPlayerStateMachine.cpp` | `PlayerState: <A> -> <B>` milestone lines (the backbone of any timeline). |
| `AampRialtoPlaybackController.cpp` | `hold ... applied/released`, `play requested ...` lines: why `play()` did or did not happen. |
| `AampRialtoMediaSource.cpp`, `AampRialtoNeedDataHandshake.cpp` | Gate, EOS, attach, injector and `addSegment` lines for each track. |
| `AampRialtoPlayer.cpp` | Tune, flush, error, position and notification lines. |
| `AampRialtoPipelineSession.cpp` | Pipeline creation and the forwarded Rialto client log. |
| `test/rialto/RialtoSimulator.cpp` | In-process Rialto stand-in for reproducing problems on a PC (see [Testing](10-testing.md)). |

## 1. Getting useful logs

- **AAMP logging.** Normal logs include MIL (milestone), WARN and ERR. Many direct-rialto decisions (holds, gates, attach, needData) log at INFO, so enable info logging when investigating. Per-sample and per-tick detail is at TRACE and is very verbose.
- **Rialto client log** is forwarded into the AAMP log by `AampRialtoPipelineSession::LogHandler` as `<component> [<file>:<line> <function>] <message>`, at the matching level.
- **Rialto server log** is separate (the server process writes to syslog or the journal). Server-side failures such as `createMediaPipeline returned nullptr` or decryption errors usually need it.
- **Thread identity.** AAMP log lines carry the thread ID. Use it to tell the control thread, the injector threads, the Rialto event thread and the GLib main loop apart (see [Threading model and locking](05-threading-and-locking-model.md), section 2).

## 2. Building a timeline

Work through the log in this order and note the timestamp of each step. The first step that is missing or out of order is usually the problem.

1. `PlayerState: IDLE -> PIPELINE_CREATED`: the pipeline was created and loaded. Before it, look for `Proceeding to createMediaPipeline despite Rialto state not RUNNING` or pipeline-creation ERR lines.
2. `Attached source id=<n> mime=<m> mediaType=<t>` for each track, then `PlayerState: ... -> SOURCES_ATTACHED`. Missing audio or subtitle attaches mean they are waiting for video, or the codec is unknown.
3. `hold <name> released by <reason>, remaining=0x<mask>`: watch the mask fall to 0 (or to `0x10`, pre-roll only).
4. `pre-roll started ...` and `pre-roll complete (<reason>) after <n> ms` (if pre-roll is enabled).
5. `issuing play() (<reason>)`, then `PlayerState: ... -> PLAYING`.
6. For a seek: `PlayerState: PLAYING -> FLUSHING`, then `FLUSHING -> FLUSHED` (`SEEK_DONE`), then `issuing play()`, then `FLUSHED -> PLAYING`.

Hold bits in the `remaining=0x...` mask: `0x1` SourcesNotAttached, `0x2` Flushing, `0x4` PositionPending, `0x8` FragmentCaching, `0x10` PreRollIncomplete.

## 3. Symptom guide

| Symptom | Likely causes (check in this order) | Log evidence | Read |
|---|---|---|---|
| Tune never reaches `PIPELINE_CREATED` | Rialto server not running or not reachable; server refused the pipeline; `load()` failed (LIFE-04) | `Failed to create IMediaPipelineFactory`, `createMediaPipeline returned nullptr`, `load() failed` | [Lifecycle and state machine](02-player-lifecycle-and-state-machines.md) §4, [Integration](09-integration-configuration-and-build.md) |
| Stuck in `SOURCES_ATTACHING` | Video codec not identified yet (muxed TS before PMT); attach failed (unmapped codec); audio/subtitle waiting for video (ARCH-13); a failed deferred attach left an injector waiting (DATA-16) | `attach of mediaType=... skipped`, `attach failed for mediaType=`, no `Attached source` line for video | [Media data path](03-media-data-path.md) §10–11 |
| `SOURCES_ATTACHED` but `play()` never issued | A hold is still set: check the last `remaining=` mask. `0x4` means waiting for a first sample to resolve the position; `0x10` means pre-roll never completed (often no GLib main loop, ARCH-22); `0x1` with `allSourcesAttached() failed` | `hold ... released ... remaining=0x...`, `play requested by ... deferred, holds=0x...` | [Lifecycle and state machine](02-player-lifecycle-and-state-machines.md) §5, [Flush and seek](04-flush-seek-and-preroll.md) §7–8 |
| First frame late on every tune | Pre-roll running to its 1 s timeout; audio-only content taking a deferred flush (DATA-14); Rialto `RUNNING` wait of up to 2 s | `pre-roll complete (timeout)`, `pending position resolved by ...` | [Flush and seek](04-flush-seek-and-preroll.md) §8 |
| Seek hangs for about 5 s, then continues | `SEEK_DONE` lost or delayed; event thread blocked behind a DRM licence fetch (THR-04) | `SEEK_DONE not received within 5000 ms - abandoning flush` | [Flush and seek](04-flush-seek-and-preroll.md) §6, [Threading](05-threading-and-locking-model.md) §8 |
| Wrong position after a seek | Abandoned flush kept the old baseline (FLUSH-04); position pending reported as 0 | `abandoning flush`, `position not yet valid, reporting 0` | [Monitoring](08-av-monitoring-timers-and-position.md) §4, [Flush and seek](04-flush-seek-and-preroll.md) §4 |
| Playback stops after an underflow and never recovers | Underflow retune is ignored when AAMP's underflow monitor is off (ARCH-16) | `OnBufferUnderflow` INFO lines, then nothing | [Monitoring](08-av-monitoring-timers-and-position.md) §6 |
| Stop or channel change hangs | An injector thread blocked in the handshake was not released: `UnblockTrackInjection` not called before the join (DATA-11), or a deferred-attach waiter (DATA-16) | Last lines from the track's injector thread are a wait; no `injector bailing out` line | [Threading](05-threading-and-locking-model.md) §6.3, [Media data path](03-media-data-path.md) §9 |
| No progress events | No GLib main loop in the host (ARCH-22); `progressReportingInterval` not positive | `Starting progress timer` with no ticks; `Invalid progress interval` | [Monitoring](08-av-monitoring-timers-and-position.md) §1–5 |
| Encrypted content: no picture | No DRM session, so samples are dropped; licence timeout | `Dropping encrypted sample for sourceId=`, `createSession failed for mediaType=`, `timed out waiting for challenge/key status` | [DRM](06-drm.md) §3, §7 |
| Encrypted content fails after a while | Licence renewal sends a stale challenge (DRM-01); key expired (DRM-04) | `onLicenseRenewal`, then licence errors; Rialto `error=1` (DECRYPTION) | [DRM](06-drm.md) §4, §9 |
| Subtitles mistimed or missing | TTML mode stuck at passthrough (SUB-01); WebVTT-in-MP4 attached as TTML (SUB-03); subtitle muted (default) | `refineDisplayOffset: detected ...`, `subtitle setDisplayOffset ...`, `Attached source ... mime=text/ttml` | [Subtitles and TTML](07-subtitles-cc-and-ttml-offset.md) §3, §5 |
| Closed captions not shown | No inband-CC source (sidecar subtitles present or no video); CC manager not initialised (no first `PLAYING`); track identifier wrong | `Created inband CC subtitle source`, `setTextTrackIdentifier="..."` | [Subtitles and TTML](07-subtitles-cc-and-ttml-offset.md) §2 |
| No audio during fast-forward or rewind | Expected: audio and subtitle are forced EOS for rate != 1 | `eos (...) ... reason=` lines at the trick-play flush | [Flush and seek](04-flush-seek-and-preroll.md) §9 |
| Log flooded with needData lines | `OnNeedMediaData` logs every request at INFO (DATA-02) | `sourceId=<n> frameCount=<n> requestId=<n>` | [Media data path](03-media-data-path.md) §16 |

## 4. Debugging tips

- **Reproduce on a PC with the simulator.** `test/rialto/RialtoSimulator.cpp` builds a stand-in `libRialtoClient.so` that aamp-cli loads from `.libs/lib`. It has no decoding and no DRM, but it exercises the whole state, hold and handshake logic (see [Testing](10-testing.md)).
- **Check the L1 suites first.** Most components have a suite under `test/utests/tests/AampRialto*`. A focused test for a suspected interleaving is often quicker than a device repro.
- **Count holds, not states.** "Why didn't it play?" is almost always answered by the last `remaining=` mask.
- **Follow one track at a time.** Gate, EOS and attach lines include `sourceId` and `mediaType`. Filter on one track to see its handshake history.
- **Look at the Rialto server log for anything after `addSegment`.** The client cannot see decode, render or decrypt failures until the server reports them.

## 5. Glossary

| Term | Meaning |
|---|---|
| Rialto | RDK's client/server media framework. The client library (`libRialtoClient.so`) runs in the application; the Rialto server owns the hardware decoders and CDMs. |
| direct-rialto | The AAMP `StreamSink` (`AampRialtoPlayer`) that calls the Rialto client API directly, without GStreamer. Selected by `useDirectRialto`. |
| Rialto GStreamer sink path | The alternative (`useRialtoSink`): AAMP's GStreamer pipeline with rialto-gstreamer sink elements. |
| `IMediaPipeline` | Rialto client interface for one playback pipeline (`load`, `attachSource`, `play`, `setPosition`, `addSegment`, `haveData`...). |
| `IMediaKeys` | Rialto client interface for one key system's DRM sessions. |
| `IControl` / `ApplicationState::RUNNING` | Rialto client registration. The player waits for `RUNNING` (up to 2 s) before creating a pipeline. |
| Event thread | The Rialto client thread that delivers pipeline callbacks (needData, playback state, position, errors). |
| Injector thread | AAMP's per-track thread (one shared thread for muxed HLS-TS) that pushes samples into the sink. |
| Control thread | The AAMP thread that calls `Configure`, `Stream`, `Flush`, `Stop`, `Pause` and similar. |
| GLib main loop | The host application's default `GMainContext`, which runs direct-rialto's timers. |
| Track vs slot | AAMP track types versus the player's three source slots; they differ only for audio-only content on the video track. |
| Source | `AampRialtoMediaSource` (video, audio or subtitle): one Rialto source plus its handshake state. |
| SourceId | The integer Rialto assigns on `attachSource`; -1 while unattached. |
| `attachSource` / `allSourcesAttached` | Register each source, then declare the set complete. Rialto cannot add or remove sources afterwards. |
| needData | Rialto's request for up to `frameCount` frames for one source, identified by a request ID. |
| haveData | The answer to a needData, with status `OK`, `EOS` or `NO_AVAILABLE_SAMPLES`. |
| `addSegment` / `NO_SPACE` | Copy one sample into Rialto's shared memory. `NO_SPACE` means the buffer is full; the batch is closed and the sample retried on the next needData. |
| `MediaSegment` | One sample as Rialto sees it: PTS, duration, data, and optionally encryption metadata or a display offset. |
| Handshake | `AampRialtoNeedDataHandshake`: per-source state that reconciles Rialto's pull with AAMP's push. |
| Claim | A pending request handed to the caller, which must answer it with `haveData` after releasing the handshake lock. |
| Gate mode | Per-source injection mode: NONE (flow), BLOCKED (hold new samples), DROPPED (discard new samples). |
| Generation | Abort counter bumped by flush, stop or reset so that blocked injectors give up. |
| `PlayerStateMachine` | Records the lifecycle state: IDLE, PIPELINE_CREATED, SOURCES_ATTACHING, SOURCES_ATTACHED, PLAYING, PAUSED, FLUSHING, FLUSHED, ERROR. |
| Play request | AAMP's `Stream()`: "playback is wanted". The controller issues `play()` only when no hold remains. |
| PlayHold | A reason `play()` must wait: SourcesNotAttached, Flushing, PositionPending, FragmentCaching, PreRollIncomplete. |
| Flush cycle | `Flush()` stages a position and rate, enters FLUSHING, gates sources and calls `setPosition()`; `SEEK_DONE` commits and moves to FLUSHED. |
| `SEEK_DONE` | The Rialto playback state that ends a flush. The player abandons the flush if it has not arrived within 5 s. |
| Staged position, baseline | The flush target awaiting commit, and the segment-start position subtracted when reporting position. |
| Position pending / deferred flush | The real start position of a new period is unknown; the first sample from an elected track triggers the flush. |
| Pre-roll | Pausing with data flowing until 4 frames, buffer full, EOS or 1 s, then `play()`. Controlled by `gstBufferAndPlay`. |
| Pre-roll episode | A counter that invalidates pre-roll steps once a flush, configure or stop has ended that pre-roll. |
| Applied rate / `isVideoMaster` | The trick-play rate set on the video source when Rialto reports that video is not the master clock. |
| mksId | Media key session ID: the Rialto key session that decrypts a sample, stamped on every encrypted segment; -1 means none. |
| `IDrmBridge` | The player-side interface that turns protection data into an mksId (production: `AampDrmBridge`). |
| Session creator | `makeRialtoSessionCreator()`: makes AAMP's `DrmSessionManager` build Rialto-backed `DrmSession`s. |
| `CallbackScope` / running map | Tracks which threads are inside a key-session callback so a session can be torn down safely. |
| Display offset | Per-segment subtitle timing correction (ms) that the server applies to cue times. |
| PASSTHROUGH / LINEAR_OFFSET | TTML timing modes: cue times match the container PTS, or need an offset. |
| Inband CC | Closed captions carried in the video stream; a subtitle source with mime `text/cc`, controlled through `IDirectRialtoCC`. |
| Notifiable | `IStreamSinkNotifiable`: the narrow interface for notifications back to AAMP core, posted to AAMP's scheduler thread. |
| MonitorAV / AvHealth | The periodic A/V health classifier and its states (ok, stall, trickplay...), reported as `MonitorAV_<state>` events. |
| PTS restamping | AAMP rewriting timestamps so playback starts near 0 (`EnablePTSReStamp`); the source of `fragmentPTSoffset` and `mDisplayOffsetMs`. |
| `AampMp4Demuxer` | AAMP core's fMP4 demuxer (forced on by `useDirectRialto`); calls `SetStreamCaps` and `SendSample` on the sink. |
| playContext | AAMP's per-track media processor (here `AampMp4Demuxer`); tracks without one use `SendTransfer`. |
| Single-pipeline mode | One sink shared by several `PrivateInstanceAAMP` instances (for example ads), re-targeted with `ChangeAamp()`. |
| Simulator | `test/rialto/RialtoSimulator.cpp`, the in-process stand-in for `libRialtoClient.so`. |

## Open questions

None specific to this page. See the Open questions on each topic page.

## Related pages

- [Overview](00-direct-rialto-overview.md)
- [Monitoring and diagnostics](08-av-monitoring-timers-and-position.md)
- [Lifecycle and state machine](02-player-lifecycle-and-state-machines.md)
- [Flush and seek](04-flush-seek-and-preroll.md)
- [Threading model and locking](05-threading-and-locking-model.md)
- [Testing](10-testing.md)
- [Workarounds and improvement register](11-workarounds-and-improvement-register.md)

## Workarounds and improvement candidates

None owned by this page. The symptom guide references register IDs; see [Workarounds and improvement register](11-workarounds-and-improvement-register.md).
