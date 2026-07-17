---
description: "Review vendor/SoC integration code for platform compliance"
---

# Vendor SoC Review

## Rules
1. **Read before write** — Read the vendor-specific file AND the base class it overrides
2. **Find overrides not base class** — Vendor code MUST only override designated virtual methods
3. **Backward compatibility** — Vendor changes must not affect other SoC platforms
4. **Input validation** — Validate all hardware-specific parameters (buffer sizes, codec caps)
5. **Fallback with logging** — If hardware feature unavailable, fallback to software path with log
6. **Unit tests** — Mock hardware interfaces in tests

## Verification Steps
1. Verify vendor code only touches files under vendor/<platform>/
2. Check that no generic middleware code has platform #ifdefs added
3. Verify GStreamer element names match platform capabilities
4. Check buffer allocation aligns with SoC memory constraints

## Reference Diagrams
- middleware/docs/sequence-diagrams/09-vendor-soc.md
- middleware/docs/sequence-diagrams/06-gst-plugins.md
- docs/aamp-core-sequence-diagrams/02-gstreamer-pipeline.md
