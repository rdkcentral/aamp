# Archived Patches

## middleware-fixes-4c1d90a.patch

**Status**: Historical — no longer applied during builds.

**Background**: This patch was applied to middleware-player-interface commit `bd2b3b1` (June 1, 2025)
to fix build failures on macOS and add the `setPtsOffset()` method missing from that commit.

**What it fixed**:
1. `gst-plugins/gst_subtec/CMakeLists.txt`: Added proper `gstreamer-base-1.0` pkg-config dependency
2. `CMakeLists.txt`: Removed Linux-only OpenSSL include conditional (fixes `openssl/evp.h` not found on macOS)
3. `CMakeLists.txt`: Added GStreamer `link_directories()` for macOS
4. `gst-plugins/CMakeLists.txt`: Added UUID pkg-config check and link dirs for macOS
5. `subtitle/subtitleParser.h`, `subtec/subtecparser/WebVttSubtecParser.{cpp,hpp}`: Added `setPtsOffset()` virtual method

**Why archived**: As of VPAAMP-881, middleware-player-interface HEAD (commit `a55c02d`, July 2025) includes
all these fixes natively. The pinned commit `bd2b3b1` and this patch are no longer used.

**Related ticket**: VPAAMP-881 — Migrate to HEAD of middleware-player-interface
