# Native Media over QUIC Engine: agent notes

## Build / verify

```bash
export PKG_CONFIG_PATH=$(brew --prefix gstreamer)/lib/pkgconfig:/opt/homebrew/lib/pkgconfig
cmake -S . -B build -G Ninja && ninja -C build && ctest --test-dir build --output-on-failure
```

- cargo is at `~/.cargo/bin` (rustup). The Rust shim builds through CMake (`cmake/MoqShim.cmake`) with `--locked`.
- A live smoke test needs the VPN and the relay URL in `NMQE_RELAY_URL` (not committed):
  `./build/nmqe-player --pipeline null --duration 10 --telemetry /dev/null`.
- Two tests skip unless `tests/fixtures/captures/reference.nmqecap` exists. Record it with
  `--capture`. Never commit it (it contains broadcast media).

## Rules

- Boundaries (enforced by `cmake/BoundaryCheck.cmake`): GStreamer only in `src/media/gst/`;
  `moq_shim.h` only in `src/transport/`; capture/replay only from `src/replay/` and `src/app/`.
- `src/mediaformat/aamp_compat/` lets AAMP's `mp4demux/MP4Demux.cpp` build without libaamp or GStreamer.
  Don't edit AAMP sources for this project.
- moq-dev/moq is pinned in `src/transport/moq_shim/Cargo.toml`. A pin change needs an update to
  `docs/ADR-001-transport.md` and a relay interop run.
- No Jira ticket numbers in source, comments or tests. Commit messages start with the ticket.
- Use the system GStreamer from Homebrew (1.28). `/Library/Frameworks/GStreamer.framework` (1.24) also
  exists on this machine, and mixing the two causes duplicate-class crashes.
