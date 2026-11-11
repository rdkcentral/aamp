# Prioritised shortlist

Start here. The full register below has every item; this list picks the ones most worth acting on. IDs link to rows in the full tables.

## Fix first: likely user-visible bugs with small fixes

| # | ID | Summary | Why first | Effort |
|---|---|---|---|---|
| 1 | DRM-01 | Licence renewal sends the **original** challenge, because the renewal message is discarded and the "challenge received" flag is never reset. | Breaks key rotation and licence expiry on long or live sessions. | S |
| 2 | DATA-16 | After a failed deferred attach, the waiting injector is not released (`WaitForAttach` ignores `attachPending`). | The AAMP track thread appears hung until the next flush or stop. | S |
| 3 | SUB-01 | The TTML passthrough/linear decision is never reset on a reused pipeline. | Subtitles at the wrong time after period or ad transitions. | S |
| 4 | ARCH-16 | With AAMP's underflow monitor off, an underflow never triggers a retune. | Playback can stall indefinitely after an underflow. | S |
| 5 | LIFE-03, LIFE-04 | Pipeline creation, `load()` and `allSourcesAttached()` failures are not reported; after `load()` fails the tune hangs silently. | Hard-to-diagnose field hangs. | S |
| 6 | FLUSH-04 | An abandoned flush (no `SEEK_DONE` in 5 s) keeps the old position baseline. | Wrong position reported after a failed seek. | S |
| 7 | THR-03 | A late `PLAYING` after `Stop()` restarts the progress timer. | Progress events from a stopped player. | S |
| 8 | ARCH-44 | `GetEncryptedAampId` is not overridden, which can wrongly clear encrypted headers in single-pipeline mode. | Encrypted playback after ads in single-pipeline mode. | S |
| 9 | SUB-03 | WebVTT-in-MP4 may be attached as `text/ttml`, depending on attach order. | Subtitles missing or mistimed. | S |
| 10 | FLUSH-02 | Slow-motion rate is probably never applied. | Feature gap compared with the GStreamer path. | S |

## Structural improvements

| Theme | IDs | Summary |
|---|---|---|
| Licence fetch under `m_attachMutex` | DRM-05, THR-04, ARCH-04 | A blocking DRM licence fetch (up to about 20 s) holds `m_attachMutex`, stalling mute/CC calls and `SEEK_DONE` handling. Acquire the session before taking the lock. |
| Untimed waits | DATA-01, LIFE-05, DRM-03 | Injector waits, the flush timeout and DRM deregistration have no independent timer, so stalls are silent. Add diagnostic timeouts and a flush watchdog. |
| `AampRialtoPlayer` size | ARCH-02, ARCH-12 | About 4,200 lines in one class with test hooks on the public API. Split by responsibility (attach, flush, notifications, CC). |
| Polling instead of Rialto events | MON-03, ARCH-14, DATA-20 | Position is an IPC call per query; `notifyPosition`, `notifyQos`, `notifyFirstFrameReceived` and `notifyPlaybackInfo` are ignored. |
| AV monitor capability | MON-01, MON-02 | With one shared position only stall and trick-play can be detected, and reports continue while paused. |
| GLib main-loop dependency | ARCH-22, THR-05 | Timers need the host's default main loop; without it pre-roll never completes. `play()` IPC can run on the host's loop. |
| Concurrency tests | THR-08, ARCH-43 | No L1 test drives two threads into the player, and there is no base-class suite for `AampRialtoMediaSource`. |
| Sink-specific code in AAMP core | ARCH-23 to ARCH-35, DATA-10, DATA-11 | Many `UsingRialto()`/`useDirectRialto` branches in collectors and core. Replace them with `StreamSink` capability queries. |

## Needs the Rialto team

| IDs | Question |
|---|---|
| DATA-04 | Why does `hasDrm=false` crash the Rialto server for clear content? |
| DATA-03, ARCH-13 | Why does attaching audio before video fail with `not-linked`? |
| FLUSH-01 | Why is `setSourcePosition` needed on the subtitle source after a pipeline `setPosition`? |
| ARCH-19, ARCH-20 | Hard-coded 4K video requirements and `isLive=false` on `load()`. |
| ARCH-11 | Ubuntu lock-up without a server text-track sink. |
| DATA-17 | Must segment buffers stay valid until `haveData()` completes? |
