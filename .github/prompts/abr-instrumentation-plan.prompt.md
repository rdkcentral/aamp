---
description: Design assertions and log events to make ABR spec compliance auditable at runtime
agent: agent
---

You are designing assertions and instrumentation to make ABR spec compliance auditable.
Every MUST/MUST NOT rule should be detectable from logs or assertions.
Be precise about required event fields. Do not propose redesigns.

**Normative spec:** See `.github/instructions/abr.instructions.md` (authoritative).

**Existing logging/events:** (paste examples, or note none)

## Tasks

1. For each MUST/MUST NOT rule in the spec:
   - Propose an invariant check (assertion) or structured log event
   - Specify required fields (buffer, predicted times, chosen profile, alternatives, etc.)

2. Identify which rules cannot be audited with current data and what must be added.

3. Provide a minimal event schema (JSON-like) for:
   - Profile decision
   - Prediction inputs and outputs
   - Bail decision and alternatives considered
   - targetLatency adjustments (with reason and episode ID)
   - stableBufferTime resets and advances
   - playbackSpeed transitions

4. Provide example log lines for one normal scenario and one edge-case scenario.
