---
description: "Specification for middleware DRM subsystem changes"
---

# Middleware DRM Spec

## Rules
1. **Read before write** — Read all files in middleware/drm/ before proposing changes
2. **Find overrides not base class** — DRM helpers (Widevine, PlayReady, ClearKey, CONSEC) override DrmHelperBase
3. **Backward compatibility** — DRM session interface must remain stable for all helper implementations
4. **Input validation** — Validate PSSH box, system ID, init data format
5. **Fallback with logging** — On license failure: log error code, retry count, then fallback to clear content or error event
6. **Unit tests** — Mock CDM and license server in all DRM tests

## Reference Diagrams
- middleware/docs/sequence-diagrams/04-drm.md
- middleware/docs/sequence-diagrams/05-externals.md
- docs/aamp-core-sequence-diagrams/06-drm-session-manager.md
