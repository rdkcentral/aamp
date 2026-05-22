# Rialto Simulator

This directory contains a **Rialto server simulator** (`libRialtoClient.so`)
that replaces the real Rialto client library when building AAMP without the
`rialto` option.

## Purpose

The `direct-rialto/` code in AAMP depends on Rialto client symbols at link
time. On platforms where the real Rialto server is not available, this
simulator provides a functional in-process implementation that drives AAMP's
DirectRialto code path through its entire state machine without requiring a
real Rialto server or media decoder.

The simulator implements:
- **`IMediaPipelineFactory`** — creates simulator pipelines
- **`IMediaPipeline`** — accepts load/attachSource/play/stop, fires
  `notifyPlaybackState`, `notifyNeedMediaData`, and `notifyPosition`
  callbacks on the `IMediaPipelineClient`
- **`IControlFactory` / `IControl`** — immediately reports
  `ApplicationState::RUNNING` so `waitForRunning()` succeeds
- **`IClientLogControlFactory`** — no-op log control
- **`MediaSegment::copy()`** variants — non-inline member implementations

## When is it used?

- **`install-aamp.sh`** (without `rialto` option): The simulator is built and
  linked automatically. CMake detects that `libRialtoClient.so` is not
  present in `.libs/lib/` and falls back to building the simulator.

- **`install-aamp.sh rialto`**: The real Rialto libraries are built and
  installed to `.libs/lib/`. CMake finds the real library and uses it
  directly — the simulator is never built.

## Runtime behaviour

When linked against the simulator:

1. `IControl::registerClient()` immediately sets state to `RUNNING`
2. `IMediaPipeline::load()` / `attachSource()` / `allSourcesAttached()`
   succeed and fire `notifyNeedMediaData` for each attached source
3. `play()` fires `notifyPlaybackState(PLAYING)` and starts a position
   reporting thread that advances at wall-clock speed from the first
   known PTS
4. `addSegment()` captures the first segment PTS as the playback base
5. `haveData()` schedules further `notifyNeedMediaData` calls
6. `stop()` fires `notifyPlaybackState(STOPPED)` and stops threads

All actions are logged to stderr with a `[RialtoSim]` prefix so that L2
integration tests can match simulator events.

## L2 test usage

The L2 test `AAMP-RIALTO-10000_Tune` exercises the full DirectRialto code
path using this simulator, verifying state transitions from IDLE through
PLAYING and position reporting.
