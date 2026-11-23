# Native Media over QUIC Engine (POC)

A native C++20 MOQT client that plays the MediaKind reference publication (MOQT draft-22, an MSF
catalog describing CMAF-packaged H.264/AAC) through GStreamer on macOS. The same architecture
targets RDK next. Project documents:

- [`README.rtf`](README.rtf): summary and scope
- [`docs/EXECUTION_PLAN.md`](docs/EXECUTION_PLAN.md): phases, gates and questions
- [`ARCHITECTURE.md`](ARCHITECTURE.md) and [`FAQ.md`](FAQ.md)
- [`docs/integration-contract.md`](docs/integration-contract.md): what the relay and publication actually do (with evidence)
- [`docs/ADR-001-transport.md`](docs/ADR-001-transport.md): moq-dev/moq selection and pin
- [`docs/telemetry.md`](docs/telemetry.md): JSONL events and measurement definitions
- [`docs/reference/`](docs/reference): the Shaka page, catalog snapshot and backend notes this started from

## Layout

```text
src/app/          CLI (nmqe-player) and platform startup
src/player/       lifecycle, join policy, bounded queues, live-edge/stall/tune telemetry
src/transport/    ObjectSource seam + live MOQT adapter; moq_shim/ = Rust C ABI over moq-dev/moq
src/catalog/      MSF catalog model
src/mediaformat/  CMAF parsing via AAMP's mp4demux (../mp4demux), compiled standalone
src/media/api/    Rialto-shaped Media Pipeline API
src/media/gst/    GStreamer backend (the only place GStreamer headers appear)
src/media/null/   Null pipeline (timing/ordering validation)
src/common/       clock, event loop, telemetry, config
src/replay/       capture recording decorator and replay source
config/platform/  macOS and RDK profiles
tests/            unit tests, replay -> null integration, fixtures
tools/            offline run aggregation
```

The boundaries are checked at configure time by `cmake/BoundaryCheck.cmake`.

## Prerequisites (macOS)

- Homebrew `cmake ninja pkg-config gstreamer cjson googletest`
- The Rust toolchain via rustup (`rust-toolchain` ≥ 1.98.1)
- Checkouts of this AAMP repo (for `mp4demux/`) and, next to it,
  `middleware-player-interface` (for `DemuxDataTypes.h`). Override them with the CMake cache
  variables `NMQE_AAMP_DIR` and `NMQE_MIDDLEWARE_DIR`.
- Access to the internal network (VPN) and the relay URL (`--relay` or `NMQE_RELAY_URL`) for live playback.

## Build and test

```bash
export PKG_CONFIG_PATH=$(brew --prefix gstreamer)/lib/pkgconfig:/opt/homebrew/lib/pkgconfig
cmake -S . -B build -G Ninja
ninja -C build
ctest --test-dir build --output-on-failure
```

## Run

```bash
# Live, with GStreamer (opens a video window); Ctrl-C to stop
./build/nmqe-player --platform config/platform/macos.json --telemetry runs/live.jsonl

# Raw QUIC instead of WebTransport
./build/nmqe-player --relay moqt://<relay-host>:4443

# Null pipeline for 15 s, recording a capture for deterministic replay
./build/nmqe-player --pipeline null --duration 15 --capture runs/ref.capture

# Replay a capture
./build/nmqe-player --replay runs/ref.capture

# Aggregate repeated runs
tools/summarize_runs.py runs/*.jsonl
```

`--help` lists every option (queue caps, transport delay budget, trace, log levels, track overrides).

## Test runner GUI

```bash
cp config/endpoints.example.json config/endpoints.json   # once; add the relay (not committed)
tools/test_runner/run_test_runner.sh
```

The runner launches either `nmqe-player` or the Shaka reference page (`docs/reference/Shaka-MoQ.html`,
served locally and opened in a throwaway Chrome profile) against a saved or ad-hoc endpoint. It exposes
the common player options (`--transport-log`, pipeline, platform, log level, extra args), supports
repeated runs, and prints a summary per run naming the furthest tune stage reached. Run files go to
`runs/`: `<stamp>.jsonl` / `<stamp>.log` for native runs, `<stamp>-shaka.jsonl` for the page's reported
console and playback events.
