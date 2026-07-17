---
description: "Review GStreamer pipeline lifecycle for correctness"
---

# Pipeline Lifecycle Review

## Rules
1. **Read before write** — Read aampgstplayer.cpp and the GStreamer plugin files first
2. **Find overrides not base class** — Check platform-specific pipeline element overrides
3. **Backward compatibility** — Pipeline state transitions must follow GStreamer spec
4. **Input validation** — Validate caps, buffer sizes, and codec info before pipeline setup
5. **Fallback with logging** — Log pipeline errors with GST_ERROR and provide graceful degradation
6. **Unit tests** — Test pipeline state transitions and error recovery

## Verification Steps
1. Verify pipeline creation matches docs/aamp-core-sequence-diagrams/02-gstreamer-pipeline.md
2. Check state transitions: NULL->READY->PAUSED->PLAYING and reverse
3. Verify flush/seek operations reset pipeline correctly
4. Check EOS handling and pipeline teardown
5. Verify buffer injection rate matches ABR decisions

## Reference Diagrams
- docs/aamp-core-sequence-diagrams/02-gstreamer-pipeline.md
- docs/aamp-core-sequence-diagrams/13-stream-sink-manager.md
- middleware/docs/sequence-diagrams/06-gst-plugins.md
