---
description: "Review middleware code changes for compliance with architecture patterns"
---

# Middleware Compliance Review

## Rules
1. **Read before write** — Read the full file and its sequence diagram before suggesting changes
2. **Find overrides not base class** — Always check for derived class overrides before modifying base
3. **Backward compatibility** — Never break existing API contracts
4. **Input validation** — All public methods must validate parameters
5. **Fallback with logging** — Every failure path must log and provide a fallback
6. **Unit tests** — Every change must include or update unit tests

## Reference Diagrams
- middleware/docs/sequence-diagrams/01-root-level-middleware.md
- middleware/docs/sequence-diagrams/04-drm.md
- middleware/docs/sequence-diagrams/06-gst-plugins.md
- middleware/docs/sequence-diagrams/09-vendor-soc.md

## Checklist
- [ ] Does the change align with the sequence diagrams above?
- [ ] Are all DRM session lifecycles preserved?
- [ ] Is GStreamer pipeline state machine respected?
- [ ] Are vendor SoC abstractions maintained (no platform-specific code in generic layers)?
- [ ] Is InterfacePlayerRDK.cpp entry point contract unchanged?
- [ ] Are error codes propagated correctly through the middleware stack?
