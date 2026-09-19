# AampRialtoPlayer — Analysis and Improvement Plan

## Context

`AampRialtoPlayer` and `AampRialtoMediaPipelineClient` are a **Proof-of-Concept** that allows
AAMP to talk directly to the Rialto client library (`firebolt::rialto::IMediaPipeline`) without
going through a GStreamer pipeline. They serve the same role as **rialto-gstreamer** (see
[rialto-gstreamer-overview.md](rialto-gstreamer-overview.md)), but skip the GStreamer layer
entirely:

```
rialto-gstreamer path:
  AAMP → appsrc → qtdemux → rialto sink (GStreamer plugin) → IPC → RialtoServer → HW decoder

AampRialtoPlayer path (PoC):
  AAMP → Mp4Demux → AampRialtoPlayer → IPC → RialtoServer → HW decoder
```

Only the minimum functionality required to play a simple clear stream has been implemented.

---

## Files Involved

| File | Location (current) | Purpose |
|------|---------------------|---------|
| `AampRialtoPlayer.h` | root | `StreamSink` implementation; owns the pipeline and injection thread |
| `AampRialtoPlayer.cpp` | root | Implementation |
| `AampRialtoMediaPipelineClient.h` | root | `IMediaPipelineClient` implementation; forwards callbacks |
| `AampRialtoMediaPipelineClient.cpp` | root | Implementation |

The `Mp4Demux` class (`mp4demux/MP4Demux.{h,cpp}`) is used to demux raw MP4 fragments and is
shared with other parts of AAMP — it should remain where it is.

---

## Comparison with rialto-gstreamer

### 1. `notifyNeedMediaData` called on Rialto IPC thread

**rialto-gstreamer** posts a `NeedDataMessage` onto a dedicated per-source `MessageQueue` and
returns immediately. All `addSegment`/`haveData` calls happen on the message-queue thread.

**AampRialtoPlayer** acquires `m_injectorMutex` from the IPC callback thread, pushes a
`PendingNeedData`, and signals the injection thread. Holding a mutex inside a Rialto IPC callback
is fragile and could cause priority inversion.

### 2. Single injection thread for all tracks

**rialto-gstreamer** gives each source its own `BufferPuller` + message-queue thread. Video and
audio injection are independent and parallel.

**AampRialtoPlayer** uses a single `m_injectionThread` that processes video and audio
sequentially. If Rialto sends simultaneous `needData` requests for both tracks, one track stalls
until the other finishes — causing head-of-line blocking.

### 3. No `NO_SPACE` retry on `addSegment`

**rialto-gstreamer** stops sending segments for the current request when `addSegment()` returns
`NO_SPACE` and calls `haveData(OK)` for the frames that were accepted.

**AampRialtoPlayer** logs a warning on `!= OK` but still calls `haveData(OK)`, silently dropping
the segment and lying to RialtoServer about delivery.

### 4. `notifyCancelNeedMediaData` has a race

Clearing the pending-request deque in `OnCancelNeedMediaData` is not effective if `InjectSamples`
is already executing for that request (the lock is released during injection).

### 5. No `setSourcePosition` before first injection

**rialto-gstreamer** calls `client->setSourcePosition()` on every `GST_EVENT_SEGMENT`. This
anchors RialtoServer's timeline and gates injection behind `m_segmentSet`.

**AampRialtoPlayer** never calls `setSourcePosition()`. RialtoServer does not know the playback
position before data arrives, which will break non-zero start times, seeks, and live streams.

### 6. `Flush` does not inform RialtoServer

**rialto-gstreamer** calls `client->flush(sourceId, resetTime)` on `GST_EVENT_FLUSH_STOP`.

**AampRialtoPlayer** only clears local queues; `m_pipeline->flush()` is never called. RialtoServer
continues decoding stale data across seek boundaries.

### 7. `Pause()` / `SetPlayBackRate()` / `Discontinuity()` are stubs

These are no-ops, so pause on start, trickplay, and seamless seek with rate changes are all
broken.

### 8. `notifyPlaybackState` is a no-op

No feedback path tells AAMP when RialtoServer actually reaches PLAYING or PAUSED. `Stream()` calls
`play()` fire-and-forget with no confirmation.

### 9. `MediaSegmentVideo` does not carry codec data

**rialto-gstreamer** attaches `codec_data` (SPS/PPS for H.264) from GStreamer caps to every
`MediaSegment` via `BufferParser::addCodecDataToSegment()`.

**AampRialtoPlayer** never calls `segment->setCodecData()`. Decoders that require in-band codec
data will fail.

### 10. `CheckAllSourcesAttached` breaks for single-track streams

The check `if (m_videoDemuxer && m_videoSourceId < 0) return;` prevents `allSourcesAttached()`
from being called when video-only (no audio demuxer) or audio-only (no video demuxer) playback is
used — the condition is never satisfied.

### 11. `attachSource` does not handle ABR/format changes

`attachSource` is called once per init segment. Mid-stream format changes (e.g. ABR switch) that
bring a new init segment will attempt to attach a source that is already attached, with no
re-attach or `switchSource` path.

---

## Improvement Plan

### Step 1 — Move files to their own directory (foundational)

Create `direct-rialto/` at the repository root (mirroring `downloader/`, `mp4demux/`, etc.) and
move the four files there. The name distinguishes this direct-Rialto path from the
rialto-gstreamer path. Update `CMakeLists.txt` to reflect the new paths.

**Files to move:**

```
AampRialtoPlayer.h                → direct-rialto/AampRialtoPlayer.h
AampRialtoPlayer.cpp              → direct-rialto/AampRialtoPlayer.cpp
AampRialtoMediaPipelineClient.h   → direct-rialto/AampRialtoMediaPipelineClient.h
AampRialtoMediaPipelineClient.cpp → direct-rialto/AampRialtoMediaPipelineClient.cpp
```

**`CMakeLists.txt` change** (line ~445):
```cmake
# Before:
AampRialtoMediaPipelineClient.cpp AampRialtoPlayer.cpp AampRialtoPlayer.h

# After:
direct-rialto/AampRialtoMediaPipelineClient.cpp
direct-rialto/AampRialtoPlayer.cpp
direct-rialto/AampRialtoPlayer.h
```

No changes to include paths are needed because the headers are included as `"AampRialtoPlayer.h"`
and `"AampRialtoMediaPipelineClient.h"` — `CMakeLists.txt` already adds the root as an include
directory. If a subdirectory-scoped include is preferred, add `direct-rialto/` to
`include_directories`.

---

### Step 2 — Add a per-source message queue (parallel injection)

Replace the single `m_injectionThread` with a per-source architecture modelled on rialto-gstreamer's
`BufferPuller + MessageQueue`:

```
┌─────────────────────┐     ┌─────────────────────┐
│ VideoSourceQueue    │     │ AudioSourceQueue     │
│  thread             │     │  thread              │
│  NeedDataMessage →  │     │  NeedDataMessage →   │
│  addSegment loop    │     │  addSegment loop     │
│  haveData()         │     │  haveData()          │
└─────────────────────┘     └─────────────────────┘
```

`OnNeedMediaData` posts a `NeedDataMessage` to the appropriate per-source queue and returns
immediately, eliminating the mutex acquisition on the IPC thread.

---

### Step 3 — Add a state machine

Replace the scattered booleans with an explicit enum:

```cpp
enum class PlayerState
{
    IDLE,               // constructed, no pipeline
    PIPELINE_CREATED,   // load() succeeded
    SOURCES_ATTACHING,  // waiting for all attachSource() calls
    SOURCES_ATTACHED,   // allSourcesAttached() sent
    PLAYING,
    PAUSED,
    FLUSHING,
    STOPPED,
    ERROR
};
```

Transitions are driven by:
- `Configure()` → `PIPELINE_CREATED`
- Each `attachSource()` success, reaching expected count → `SOURCES_ATTACHED`
- `Stream()` → `PLAYING` (after `notifyPlaybackState(PLAYING)` received)
- `Pause()` → `PAUSED`
- `Flush()` → `FLUSHING` → back to `SOURCES_ATTACHED`
- `Stop()` → `STOPPED`

---

### Step 4 — Implement `setSourcePosition` before first injection

Before calling `haveData()` for the first time on a source, call:

```cpp
m_pipeline->setSourcePosition(sourceId, firstSamplePts, /*resetTime=*/true);
```

Gate injection on a per-source `m_segmentSet` flag (set by `setSourcePosition`), mirroring
`PullModePlaybackDelegate::isReadyToSendData()`.

---

### Step 5 — Implement `Flush` properly

```cpp
void AampRialtoPlayer::Flush(double position, int rate, bool shouldTearDown)
{
    // 1. Clear local queues (already done)
    // 2. Signal RialtoServer
    if (m_pipeline)
    {
        if (m_videoSourceId >= 0)
            m_pipeline->flush(m_videoSourceId, /*resetTime=*/true);
        if (m_audioSourceId >= 0)
            m_pipeline->flush(m_audioSourceId, /*resetTime=*/true);
    }
    // 3. Reset segment position gates so injection waits for setSourcePosition
    m_videoSegmentSet = false;
    m_audioSegmentSet = false;
}
```

---

### Step 6 — Implement `Pause()` and `SetPlayBackRate()`

```cpp
bool AampRialtoPlayer::Pause(bool pause, bool /*forceStop*/)
{
    if (!m_pipeline) return false;
    return pause ? m_pipeline->pause() : m_pipeline->play();
}

bool AampRialtoPlayer::SetPlayBackRate(double rate)
{
    if (!m_pipeline) return false;
    return m_pipeline->setPlaybackRate(rate);
}
```

---

### Step 7 — React to `notifyPlaybackState`

Forward state changes to AAMP and use them to confirm async transitions:

```cpp
void AampRialtoMediaPipelineClient::notifyPlaybackState(PlaybackState state)
{
    if (m_playbackStateCallback)
        m_playbackStateCallback(state);
}
```

In `AampRialtoPlayer`, on `PlaybackState::PLAYING`/`PAUSED`, advance the player state machine and
notify AAMP via the appropriate `PrivateInstanceAAMP` callback.

---

### Step 8 — Set codec data on every `MediaSegment`

Extract codec data from `Mp4Demux::GetCodecInfo()` at `attachSource` time and cache it. On every
`InjectSamples` call, set it:

```cpp
if (!m_videoCodecData.empty())
{
    auto cd = std::make_shared<firebolt::rialto::CodecData>();
    cd->data = m_videoCodecData;
    cd->type = firebolt::rialto::CodecDataType::BUFFER;
    segment->setCodecData(cd);
}
```

---

### Step 9 — Fix `CheckAllSourcesAttached` for single-track streams

Count expected sources at `Configure()` time:

```cpp
int m_expectedSourceCount = 0; // set in Configure()
int m_attachedSourceCount = 0; // incremented in AttachVideoSource / AttachAudioSource

void AampRialtoPlayer::CheckAllSourcesAttached()
{
    std::lock_guard lock(m_sourceAttachMutex);
    if (m_allSourcesAttached || m_attachedSourceCount < m_expectedSourceCount)
        return;
    m_allSourcesAttached = true;
    m_pipeline->allSourcesAttached();
}
```

---

### Step 10 — Handle `addSegment(NO_SPACE)`

```cpp
for (auto &sample : samples)
{
    auto status = m_pipeline->addSegment(requestId, segment);
    if (status == firebolt::rialto::AddSegmentStatus::NO_SPACE)
    {
        // re-queue this sample and remaining ones at the front of the deque
        // then break — they will be sent on the next needData request
        break;
    }
    ++addedSegments;
}
```

---

### Step 10 — Handle `addSegment(NO_SPACE)` properly

*(Content already detailed in the code sketch above.)*

---

## TDD Implementation Plan

Each implementation step follows the **Red → Green → Refactor** cycle: write the failing test(s)
first, make them pass with the minimal implementation change, then clean up. Tests are the
specification; the implementation satisfies them.

Follow the project's L1 test conventions (`.github/instructions/testing.instructions.md`).
Tests live under `test/utests/tests/` and are built/run via `test/utests/run.sh`.

> **Current status (as of 2026-04-06):** Phases 0–13 are complete.
> Both test suites build and all 81 tests pass
> (`AampRialtoPlayerTests`: 75 tests; `AampRialtoMediaPipelineClientTests`: 6 tests).
> Phase 13 (per-source injection queues + GoF state machine) is now complete.

---

### Phase 0 — Test scaffold ✅ DONE

Everything that follows depends on being able to inject a mock `IMediaPipeline` into
`AampRialtoPlayer`. Set up the full test harness first so subsequent phases can immediately write
failing tests.

#### 0a — Mock `IMediaPipeline`

Create `test/utests/mocks/MockIMediaPipeline.h`:

```cpp
#include "IMediaPipeline.h"
#include <gmock/gmock.h>

class MockIMediaPipeline : public firebolt::rialto::IMediaPipeline
{
public:
    MOCK_METHOD(bool, load,
        (firebolt::rialto::MediaType, const std::string &, const std::string &), (override));
    MOCK_METHOD(bool, attachSource,
        (std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSource> &), (override));
    MOCK_METHOD(bool, allSourcesAttached, (), (override));
    MOCK_METHOD(bool, play, (), (override));
    MOCK_METHOD(bool, pause, (), (override));
    MOCK_METHOD(bool, stop, (), (override));
    MOCK_METHOD(bool, setPlaybackRate, (double), (override));
    MOCK_METHOD(bool, flush, (int32_t, bool), (override));
    MOCK_METHOD(bool, setSourcePosition, (int32_t, int64_t, bool, double, uint64_t), (override));
    MOCK_METHOD(firebolt::rialto::AddSegmentStatus, addSegment,
        (uint32_t, const std::unique_ptr<firebolt::rialto::IMediaPipeline::MediaSegment> &), (override));
    MOCK_METHOD(bool, haveData,
        (firebolt::rialto::MediaSourceStatus, uint32_t), (override));
    // add remaining pure virtuals as needed
};
```

#### 0b — Mock `IMediaPipelineFactory`

Create `test/utests/mocks/MockIMediaPipelineFactory.h`:

```cpp
class MockIMediaPipelineFactory : public firebolt::rialto::IMediaPipelineFactory
{
public:
    MOCK_METHOD(std::unique_ptr<firebolt::rialto::IMediaPipeline>, createMediaPipeline,
        (std::weak_ptr<firebolt::rialto::IMediaPipelineClient>,
         const firebolt::rialto::VideoRequirements &), (const, override));
};
```

#### 0c — Expose a factory injection seam in `AampRialtoPlayer`

Refactor `Configure()` to accept an optional factory parameter (defaults to the real singleton),
so tests can inject `MockIMediaPipelineFactory` without touching production code paths:

```cpp
void Configure(...,
    std::shared_ptr<firebolt::rialto::IMediaPipelineFactory> factory = nullptr);
```

#### 0d — Create the test directories and `CMakeLists.txt` files

**`test/utests/tests/AampRialtoPlayerTests/`**

```
AampRialtoPlayerTests.cpp       # Test runner
AampRialtoPlayerTestCases.cpp   # Test cases (starts empty — filled per phase)
CMakeLists.txt
```

```cmake
set(EXEC_NAME AampRialtoPlayerTests)
set(AAMP_ROOT "../../../../")
set(UTESTS_ROOT "../../")
include(${CMAKE_CURRENT_LIST_DIR}/../CommonTestIncludes.cmake)

set(TEST_SOURCES
    AampRialtoPlayerTests.cpp
    AampRialtoPlayerTestCases.cpp
)
set(AAMP_SOURCES
    ${AAMP_ROOT}/direct-rialto/AampRialtoPlayer.cpp
    ${AAMP_ROOT}/direct-rialto/AampRialtoMediaPipelineClient.cpp
    ${AAMP_ROOT}/mp4demux/MP4Demux.cpp
    ${AAMP_ROOT}/mp4demux/AampMp4Demuxer.cpp
    ${AAMP_ROOT}/AampGrowableBuffer.cpp
)
add_executable(${EXEC_NAME} ${TEST_SOURCES} ${AAMP_SOURCES})
target_link_libraries(${EXEC_NAME} fakes -pthread ${GMOCK_LINK_LIBRARIES} ${GTEST_LINK_LIBRARIES})
aamp_utest_run_add(${EXEC_NAME})
```

**`test/utests/tests/AampRialtoMediaPipelineClientTests/`**

```
AampRialtoMediaPipelineClientTests.cpp
AampRialtoMediaPipelineClientTestCases.cpp
CMakeLists.txt
```

CMakeLists.txt: same pattern; `AAMP_SOURCES` contains only
`direct-rialto/AampRialtoMediaPipelineClient.cpp`.

Run `test/utests/run.sh` once to integrate the new directories into the build. All subsequent
phase iterations can use `make && ./AampRialtoPlayerTests` directly.

---

### Phase 1 — `AampRialtoMediaPipelineClient` callbacks ✅ DONE

These tests cover the existing class behaviour and act as a regression baseline.

**Write first (Red):**

| Test | Assertion |
|------|-----------|
| `notifyNeedMediaData_WithCallback_InvokesCallback` | Installed callback receives correct `sourceId`, `frameCount`, `requestId` |
| `notifyNeedMediaData_WithoutCallback_DoesNotCrash` | No crash when no callback is installed |
| `notifyCancelNeedMediaData_WithCallback_InvokesCallback` | Installed cancel callback receives correct `sourceId` |
| `notifyCancelNeedMediaData_WithoutCallback_DoesNotCrash` | No crash |
| `SetNeedDataCallback_ReplacesExisting` | Only the most recently installed callback fires |
| `AllOtherNotifications_DoNotCrash` | `notifyPlaybackState`, `notifyPosition`, `notifyDuration`, etc. do not crash |

**Make pass (Green):** The existing implementation should satisfy these; fix any that don't.

---

### Phase 2 — `Configure` / pipeline creation ✅ DONE

**Write first (Red):**

| Test | Assertion |
|------|-----------|
| `Configure_ValidFormats_CreatesPipeline` | `load()` is called on the mock; pipeline handle is stored |
| `Configure_NullFactory_DoesNotCrash` | Graceful handling when factory returns `nullptr` |
| `Configure_VideoOnly_CreatesVideoDemuxerOnly` | `m_videoDemuxer` set, `m_audioDemuxer` null |
| `Configure_AudioOnly_CreatesAudioDemuxerOnly` | `m_audioDemuxer` set, `m_videoDemuxer` null |

**Implement (Green):** Factory injection seam from Phase 0c enables these tests.

---

### Phase 3 — Fix `CheckAllSourcesAttached` ✅ DONE  *(fixes issue #10)*

**Write first (Red):**

| Test | Assertion |
|------|-----------|
| `SendTransfer_BothSources_CallsAllSourcesAttachedOnce` | `allSourcesAttached()` called exactly once after both init fragments |
| `SendTransfer_VideoOnlyStream_CallsAllSourcesAttachedAfterVideo` | `allSourcesAttached()` fires without waiting for audio |
| `SendTransfer_AudioOnlyStream_CallsAllSourcesAttachedAfterAudio` | `allSourcesAttached()` fires without waiting for video |

**Implement (Green):** Replace the `m_videoDemuxer && m_videoSourceId < 0` guard with the
`m_expectedSourceCount` / `m_attachedSourceCount` counter approach.

---

### Phase 4 — `attachSource` and codec data ✅ DONE  *(fixes issues #6 and #9)*

**Write first (Red):**

| Test | Assertion |
|------|-----------|
| `SendTransfer_InitFragment_AttachesVideoSourceWithMimeType` | `attachSource()` called with `video/h264` for H.264 content |
| `SendTransfer_InitFragment_AttachesAudioSourceWithConfig` | `attachSource()` called with correct channel/rate |
| `InjectSamples_SetsCodecDataOnVideoSegment` | `segment->getCodecData()` is non-null for H.264 stream |
| `InjectSamples_SetsCodecDataOnHevcSegment` | `segment->getCodecData()` is non-null for H.265 stream |

**Implement (Green):** Cache codec data in `AttachVideoSource` / `AttachAudioSource`; call
`segment->setCodecData()` inside `InjectSamples`.

---

### Phase 5 — Segment injection baseline ✅ DONE  *(covers existing injection path)*

**Write first (Red):**

| Test | Assertion |
|------|-----------|
| `SendTransfer_MediaFragment_EnqueuesSamples` | Non-init fragments enqueued in correct deque |
| `EndOfStreamReached_SetsEosFlag` | `m_videoEos` / `m_audioEos` set; injection thread wakes |
| `OnNeedMediaData_EnqueuesRequest` | Request appears in the correct pending-req deque |
| `OnCancelNeedMediaData_ClearsRequests` | Pending-req deque for that source is emptied |
| `InjectSamples_CallsAddSegmentAndHaveData` | `addSegment()` called per sample; `haveData(OK)` called once |
| `InjectSamples_EosOnly_CallsHaveDataWithEos` | `haveData(EOS)` when no samples and `eos=true` |
| `InjectSamples_NullPipeline_DoesNotCrash` | No crash when pipeline is null |
| `Stream_CallsPlay` | `m_pipeline->play()` is invoked |
| `Stop_CallsPipelineStop` | `m_pipeline->stop()` is called; injection thread joins cleanly |

**Implement (Green):** These mostly cover existing behaviour; fix any gaps.

---

### Phase 6 — `setSourcePosition` before first injection ✅ DONE  *(fixes issue #5)*

**Write first (Red):**

| Test | Assertion |
|------|-----------|
| `InjectSamples_FirstInjection_CallsSetSourcePosition` | `setSourcePosition()` is called with the PTS of the first sample |
| `InjectSamples_SubsequentInjections_DoNotCallSetSourcePosition` | `setSourcePosition()` called only once per source |

**Implement (Green):** Add per-source `m_segmentSet` flag; call `setSourcePosition` before first
`addSegment` if flag is unset.

---

### Phase 7 — `Flush` ✅ DONE  *(fixes issue #6)*

**Write first (Red):**

| Test | Assertion |
|------|-----------|
| `Flush_CallsPipelineFlushForEachAttachedSource` | `m_pipeline->flush(videoSourceId, true)` and `flush(audioSourceId, true)` called |
| `Flush_ClearsLocalQueues` | `m_videoSampleQueue`, `m_audioSampleQueue`, pending-req deques all empty after `Flush()` |
| `Flush_ResetsSegmentSetFlag` | After `Flush()`, the next injection triggers `setSourcePosition` again |
| `Flush_NoPipeline_DoesNotCrash` | No crash when `m_pipeline` is null |

**Implement (Green):** Call `m_pipeline->flush()` per source; reset `m_segmentSet` flags.

---

### Phase 8 — `Pause` / `SetPlayBackRate` ✅ DONE  *(fixes issue #7)*

**Write first (Red):**

| Test | Assertion |
|------|-----------|
| `Pause_True_CallsPipelinePause` | `m_pipeline->pause()` invoked |
| `Pause_False_CallsPipelinePlay` | `m_pipeline->play()` invoked |
| `SetPlayBackRate_CallsPipelineSetPlaybackRate` | `m_pipeline->setPlaybackRate(rate)` invoked with correct value |

**Implement (Green):** One-line delegating implementations.

---

### Phase 9 — `notifyPlaybackState` forwarding ✅ DONE  *(fixes issue #8)*

**Write first (Red):**

| Test | Assertion |
|------|-----------|
| `notifyPlaybackState_Playing_AdvancesPlayerState` | Player state becomes `PLAYING` |
| `notifyPlaybackState_Paused_AdvancesPlayerState` | Player state becomes `PAUSED` |
| `notifyPlaybackState_Error_SetsErrorState` | Player state becomes `ERROR` |

**Implement (Green):** Add `PlaybackStateCallback` to `AampRialtoMediaPipelineClient`; handle in
`AampRialtoPlayer`.

---

### Phase 10 — `addSegment(NO_SPACE)` ✅ DONE  *(fixes issue #3)*

**Write first (Red):**

| Test | Assertion |
|------|-----------|
| `InjectSamples_AddSegmentNoSpace_StopsAndRequeues` | When `NO_SPACE` is returned, remaining samples go back on the front of the deque; `haveData(OK)` reports only accepted count |
| `InjectSamples_AddSegmentNoSpaceOnFirst_RequeuesAll` | All samples requeued when first `addSegment` returns `NO_SPACE` |

**Implement (Green):** Add the re-queue loop on `NO_SPACE`.

---

### Phase 11 — Parallel injection behavioral tests ✅ DONE  *(integration tests for Phase 13 architecture)*

Integration-level tests that define the expected external behavior of the per-source
architecture.  These pass today because the single injection thread meets the behavioral
contracts; they will continue to pass — and provide the regression net — when the
architecture is replaced in Phase 13.

| Test | Status |
|------|--------|
| `SimultaneousVideoAudio_BothInjectedIndependently` | ✅ passes |
| `HighFrequencyNeedData_NoDeadlock` | ✅ passes |

---

### Phase 12 — Codec data forwarded on every injected segment ✅ DONE

The Rialto server uses per-segment codec data (equivalent to GStreamer's `caps codec_data`
field) to update the downstream decoder whenever codec parameters change mid-stream.

**Tests added:**

| Test | Assertion |
|------|-----------|
| `InjectSamples_VideoSegment_CarriesCodecDataFromInitFragment` | Every injected video segment carries the codec data cached from the init fragment |
| `InjectSamples_AudioSegment_CarriesCodecDataFromInitFragment` | Every injected audio segment carries the codec data cached from the init fragment |
| `InitFragment_SecondVideoInit_UpdatesCodecDataWithoutReattaching` | A second video init fragment (mid-stream codec change) refreshes the cache without re-attaching |

**Implementation:** `AttachVideoSource` / `AttachAudioSource` update the codec-data cache
before the early-return guard so re-init fragments are handled.  `InjectSamples` calls
`segment->setCodecData()` on every `MediaSegment`.

---

### Phase 13 — Per-source queues + `PlayerState` state machine ✅ DONE

This phase replaced the single `m_injectionThread` with per-source `SourceWorker` objects
and introduced a GoF State-pattern `PlayerStateMachine`.

**Files introduced:**

| File | Purpose |
|------|---------|
| `direct-rialto/AampPlayerStateMachine.h` | GoF State interface + `PlayerStateId` enum + `PlayerStateMachine` context |
| `direct-rialto/AampPlayerStateMachine.cpp` | 9 concrete state classes + all transition implementations |
| `direct-rialto/AampSourceWorker.h` | Per-source injection worker declaration; also owns `QueuedSample` + `PendingNeedData` types |
| `direct-rialto/AampSourceWorker.cpp` | Worker thread: drains needData requests + sample queues, calls InjectFn |
| `docs/rialto-integration/draw_state_machine.py` | Python script (requires `pip install graphviz`) to render a directed graph of the state machine |

**Key changes to `AampRialtoPlayer`:**

1. **Per-source workers** — `m_videoWorker` and `m_audioWorker` (`std::unique_ptr<SourceWorker>`) replace
   `m_injectionThread`, `m_injectorMutex`, `m_injectorCv`, `m_stopInjection`, `m_videoSampleQueue`,
   `m_audioSampleQueue`, `m_videoPendingReqs`, `m_audioPendingReqs`, `m_videoEos`, `m_audioEos`.
   `OnNeedMediaData` now posts to the worker queue **without acquiring any global mutex**,
   eliminating the priority-inversion risk on the Rialto IPC callback thread (issues #1 and #2).

2. **GoF State Machine** — `m_stateMachine` (`PlayerStateMachine`) tracks the player lifecycle.
   Every state transition is logged at `AAMPLOG_MIL` level.
   Concrete states: `IdleState`, `PipelineCreatedState`, `SourcesAttachingState`,
   `SourcesAttachedState`, `PlayingState`, `PausedState`, `FlushingState`, `StoppedState`, `ErrorState` —
   each implementing only the transitions valid from that state.

3. **`InjectSamples` signature** — changed to return rejected samples (`std::vector<QueuedSample>`)
   rather than take a `requeueDest` deque by reference.  The `SourceWorker` re-queues them at the
   front of its internal queue.

**State transitions (rendered by `draw_state_machine.py`):**

```
IDLE ──onPipelineLoaded──► PIPELINE_CREATED
PIPELINE_CREATED ──onSourceAttaching──► SOURCES_ATTACHING
SOURCES_ATTACHING ──onAllSourcesAttached──► SOURCES_ATTACHED
SOURCES_ATTACHED ──onPlaybackStarted──► PLAYING
PLAYING ──onPlaybackPaused──► PAUSED
PAUSED ──onPlaybackStarted──► PLAYING
{SOURCES_ATTACHED,PLAYING,PAUSED} ──onFlush──► FLUSHING
FLUSHING ──onSourceAttaching──► SOURCES_ATTACHING   (re-tune path)
any ──onStop──► STOPPED
any ──onError──► ERROR
any ──onReconfigure──► IDLE   (re-tune resets the machine)
```

**New tests added (Phase 13):**

| Test | Assertion |
|------|-----------|
| `StateMachine_InitialState_IsIdle` | Before Configure(), state is IDLE |
| `StateMachine_AfterSuccessfulConfigure_IsPipelineCreated` | load() OK → PIPELINE_CREATED |
| `StateMachine_AfterFailedLoad_RemainsIdle` | load() fails → stays IDLE |
| `StateMachine_AfterFirstVideoInit_IsSourcesAttaching_ForDualTrack` | Video only (dual-track) → SOURCES_ATTACHING |
| `StateMachine_AfterBothInitFragments_IsSourcesAttached` | Both init frags → SOURCES_ATTACHED |
| `StateMachine_VideoOnlyStream_IsSourcesAttachedAfterVideoInit` | Video-only → SOURCES_ATTACHED |
| `StateMachine_AudioOnlyStream_IsSourcesAttachedAfterAudioInit` | Audio-only → SOURCES_ATTACHED |
| `StateMachine_AfterPlaybackStartedNotification_IsPlaying` | PLAYING notification → PLAYING |
| `StateMachine_AfterPausedNotification_IsPaused` | PAUSED notification → PAUSED |
| `StateMachine_AfterResumeFromPaused_IsPlaying` | PAUSED + PLAYING notification → PLAYING |
| `StateMachine_AfterFlushFromSourcesAttached_IsFlushing` | Flush from SOURCES_ATTACHED → FLUSHING |
| `StateMachine_AfterFlushFromPlaying_IsFlushing` | Flush from PLAYING → FLUSHING |
| `StateMachine_AfterFlushFromPaused_IsFlushing` | Flush from PAUSED → FLUSHING |
| `StateMachine_AfterFlushThenReconfigure_ResetsToIdle` | FLUSHING + reconfigure → IDLE |
| `StateMachine_AfterStop_IsStopped` | Stop() → STOPPED |
| `StateMachine_StopFromPlaying_IsStopped` | Stop from PLAYING → STOPPED |
| `StateMachine_AfterFailureNotification_IsError` | FAILURE notification → ERROR |
| `StateMachine_Reconfigure_ResetsFromStoppedToIdle` | STOPPED + reconfigure → IDLE |
| `StateMachine_Reconfigure_ResetsFromErrorToIdle` | ERROR + reconfigure → IDLE |
| `SourceWorker_OnNeedMediaData_DoesNotBlockCallerThread` | Rapid-fire needData does not deadlock |

---

## Summary: TDD Sequence

| Phase | What is tested | Implementation work | Status |
|-------|---------------|---------------------|--------|
| 0 | *(scaffold only)* | Mocks, factory seam, test directories, CMakeLists.txt | ✅ Done |
| 1 | `AampRialtoMediaPipelineClient` callbacks | Baseline / no changes needed | ✅ Done |
| 2 | `Configure` / pipeline creation | Factory injection seam | ✅ Done |
| 3 | `CheckAllSourcesAttached` correctness | Source-count counter | ✅ Done |
| 4 | `attachSource` + codec data on segments | Cache and attach codec data | ✅ Done |
| 5 | Injection baseline (enqueue, EOS, play, stop) | Fix any gaps in existing code | ✅ Done |
| 6 | `setSourcePosition` before first injection | Per-source `m_segmentSet` flag | ✅ Done |
| 7 | `Flush` calls pipeline + resets state | `m_pipeline->flush()` per source | ✅ Done |
| 8 | `Pause` / `SetPlayBackRate` | One-line delegating implementations | ✅ Done |
| 9 | `notifyPlaybackState` forwarding | Playback state callback | ✅ Done |
| 10 | `addSegment(NO_SPACE)` re-queue | Re-queue loop | ✅ Done |
| 11 | Parallel injection behavioral tests | *(tests only — regression harness for Phase 13)* | ✅ Done |
| 12 | Codec data forwarded on every injected segment | Cache update on re-init; `setCodecData()` on every segment | ✅ Done |
| 13 | Per-source queues + GoF state machine tests (26 new tests) | Per-source queues + `PlayerState` state machine | ✅ Done |
