---
description: "Specification for middleware SoC abstraction layer"
---

# Middleware SoC Spec

## Rules
1. **Read before write** — Read the base abstraction AND vendor-specific override files
2. **Find overrides not base class** — Each vendor (Amlogic, Broadcom, Realtek) overrides the base SoC interface
3. **Backward compatibility** — Base interface must not change; only vendor overrides may be modified
4. **Input validation** — Validate hardware capability queries and buffer allocation params
5. **Fallback with logging** — If platform feature unsupported: log, use software fallback
6. **Unit tests** — Mock SoC capabilities per platform in tests

## Reference Diagrams
- middleware/docs/sequence-diagrams/09-vendor-soc.md
- middleware/docs/sequence-diagrams/06-gst-plugins.md
- docs/aamp-core-sequence-diagrams/15-shims.md
