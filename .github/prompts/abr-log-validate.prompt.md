---
description: Validate observed ABR runtime behavior against the normative spec using logs or traces
agent: agent
---

You are validating runtime behavior against the ABR normative spec.
Infer behavior only from explicit log/trace evidence provided.
Cite log lines for every claim. Say "Not determinable" where logs are insufficient.

**Normative spec:** See `.github/instructions/abr.instructions.md` (authoritative).

**Runtime logs / traces:** (paste here)
Include timestamps where available. Relevant events: buffer level, selected profile, predicted download time, bail events, targetLatency changes, stableBufferTime, playbackSpeed changes, state transitions.

**Config used:** (paste values, or note if unknown)

## Tasks

**1. Timeline reconstruction**
Reconstruct the sequence of key state changes:
- buffer, playbackSpeed, latency, targetLatency, stableBufferTime, selected profile, bail events

**2. Compliance checks** — for each, PASS/FAIL with quoted log evidence:
- Danger episode one-shot rule (§11.1)
- targetLatency clamping floor and ceiling (§1.6, §11)
- Speed change hysteresis — must pass through 1× (§10.3)
- "All profiles predict underflow" fallback behavior (§6.2)
- Bail condition correctness — continuing predicts underflow AND restart avoids it (§8.2)

**3. Output**
- PASS/FAIL findings with log evidence (quote lines)
- Missing instrumentation — what must be logged to make compliance determinable
