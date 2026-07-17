---
description: "Specification for middleware GStreamer plugin changes"
---

# Middleware GStreamer Plugin Spec

## Rules
1. **Read before write** — Read all files in middleware/gst-plugins/ before proposing changes
2. **Find overrides not base class** — GStreamer elements inherit from GstBaseTransform/GstElement
3. **Backward compatibility** — Plugin caps and pad templates must not change
4. **Input validation** — Validate buffer metadata, caps negotiation, and DRM context
5. **Fallback with logging** — On decryption failure: log GST_ERROR, push GAP event, continue
6. **Unit tests** — Test with mock encrypted buffers and DRM sessions

## Reference Diagrams
- middleware/docs/sequence-diagrams/06-gst-plugins.md
- docs/aamp-core-sequence-diagrams/02-gstreamer-pipeline.md
- middleware/docs/sequence-diagrams/04-drm.md
