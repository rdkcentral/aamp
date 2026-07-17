---
description: "Specification for middleware externals subsystem (CSM, watermark)"
---

# Middleware Externals Spec

## Rules
1. **Read before write** — Read all files in middleware/externals/ before proposing changes
2. **Find overrides not base class** — Check ContentSecurityManagerSession overrides
3. **Backward compatibility** — External service interfaces must remain stable
4. **Input validation** — Validate session tokens, URLs, and certificate data
5. **Fallback with logging** — On external service failure: log, retry with backoff, then emit error event
6. **Unit tests** — Mock external services (CSM, watermark server) in all tests

## Reference Diagrams
- middleware/docs/sequence-diagrams/05-externals.md
- middleware/docs/sequence-diagrams/04-drm.md
- docs/aamp-core-sequence-diagrams/06-drm-session-manager.md
