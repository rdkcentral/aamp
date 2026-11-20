# ADR-001: MOQT Transport Selection

**Status:** Accepted for the POC (provisional on the bake-off follow-up below)
**Date:** 2026-11-20

## Context

The plan calls for a bake-off between `moq-dev/moq` (libmoq) and `Quicr/libquicr`. Interoperability
with the actual relay at MOQT draft-22 is a hard gate.

Facts at decision time:

| Candidate | MOQT drafts at HEAD (2026-11-20) | WebTransport | Language |
|---|---|---|---|
| moq-dev/moq `b3de94d` | moq-lite plus IETF draft-18, -20, **-22** (`moqt-22`) | yes (moq-quic) | Rust, C++ bindings via moq-ffi |
| Quicr/libquicr `main` | IETF **draft-18** (README badge) | yes | C++ (picoquic) |

The reference relay identifies as a moq-dev relay: its SETUP carries moq-dev's `HopId` / `Solicit` /
`Hidden` options.

## Decision

Use **moq-dev/moq pinned at `b3de94d65ba542aa52fd64aba34136b04d078175`** (`moq-net` 0.3.10 and
`moq-tokio` 0.19.23). It sits behind our own narrow C ABI (`src/transport/moq_shim`, Rust
staticlib), and the only consumer is `src/transport/MoqObjectSource.cpp`.

- Endpoint mode: WebTransport (`https://`) by default, which matches the browser path; raw QUIC
  (`moqt://`) works too and is selected with `--relay`.
- Version: `moq-transport-22` only (no version fallback).
- Object model: one moq-net frame = one MOQT Object. The Group ID is the moq-net group `start`,
  used verbatim, and the Object ID is the frame index.
- Subscription: `Range::live()` (current Group from Object 0), `Delay::max` = `--max-delay-ms`
  (default 1000), priority audio > video.

We did not use the generated `moq-ffi` C++ bindings. They need a forked `uniffi-bindgen-cpp`, and
they pull in moq-mux, hang and codec crates that we don't need. Our shim is about 400 lines and
exposes only the ReceivedObject inputs.

## Evidence (hard gate)

- 2026-11-20: connected, negotiated `moq-transport-22`, resolved namespace `mediakind-test`, and
  subscribed to `catalog`, `video0.m4s` and `audio0.m4s`. Received every Group with no gaps.
  Played H.264/AAC through GStreamer on macOS for 20 s with 0 stalls and 0 dropped frames.
- The same over raw QUIC.
- Lifecycle: the session opens and closes on each run, and the shim joins its runtime before
  `moqshim_session_close` returns.

## Consequences

- The build needs a Rust toolchain (≥ 1.98.1, per moq-dev's `rust-toolchain.toml`).
  `cargo build --locked` uses the committed `Cargo.lock`.
- RDK: the shim must be cross-compiled with `NMQE_RUST_TARGET` for the SDK ABI (rustls; no OpenSSL
  dependency). This is still untested and is the main risk for Phase 5.
- moq-dev treats IETF drafts as a "low-effort compatibility layer". A relay or draft change
  can break us, so re-run the relay interop check when you change the pin.

## Follow-up (bake-off remainder)

- libquicr would need draft-22 work (FILL_PARAMETERS replacing Joining FETCH, and the
  LOCATION_FILTER encoding). Record a libquicr spike result here, or close it as not pursued.
- Cross-compile the shim for the selected RDK SDK, and record the result here.
