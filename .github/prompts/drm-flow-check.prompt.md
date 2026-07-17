---
description: "Verify DRM flow correctness against sequence diagrams"
---

# DRM Flow Check

## Rules
1. **Read before write** — Read the full DRM helper and session manager files first
2. **Find overrides not base class** — Check Widevine/PlayReady/ClearKey/CONSEC overrides
3. **Backward compatibility** — DRM session lifecycle must not change
4. **Input validation** — Validate PSSH, initData, systemId before processing
5. **Fallback with logging** — Log all DRM failures with error codes and retry status
6. **Unit tests** — Mock license server responses in tests

## Verification Steps
1. Trace the DRM flow from initData detection through license acquisition
2. Verify session creation matches diagram in docs/aamp-core-sequence-diagrams/06-drm-session-manager.md
3. Verify middleware DRM helper selection matches middleware/docs/sequence-diagrams/04-drm.md
4. Check key rotation handling
5. Verify session teardown on Stop/channel-change

## Reference Diagrams
- docs/aamp-core-sequence-diagrams/06-drm-session-manager.md
- middleware/docs/sequence-diagrams/04-drm.md
- middleware/docs/sequence-diagrams/05-externals.md (ContentSecurityManager)
