---
description: "Workflow for fixing middleware issues with spec-driven approach"
---

# Middleware Issue Fix

## Rules
1. **Read before write** — Read the full file, its caller chain, and sequence diagram first
2. **Find overrides not base class** — Check all derived classes that override the method
3. **Backward compatibility** — Fix must not alter existing behavior for other callers
4. **Input validation** — Verify the fix handles all edge cases (null, empty, overflow)
5. **Fallback with logging** — Add AAMPLOG_WARN/ERR at failure points with context
6. **Unit tests** — Add regression test proving the fix works

## Workflow
1. Identify the failing component using middleware/docs/sequence-diagrams/
2. Read the relevant source file completely
3. Trace the call chain from InterfacePlayerRDK.cpp through to the failure point
4. Identify root cause vs symptom
5. Propose fix with minimal blast radius
6. Verify no sequence diagram flow is broken

## Reference Diagrams
- middleware/docs/sequence-diagrams/ (all 9 files)
- docs/aamp-core-sequence-diagrams/ (if issue crosses into core)
