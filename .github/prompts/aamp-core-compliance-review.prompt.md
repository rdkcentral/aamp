---
description: "Review AAMP core code changes for compliance with architecture patterns"
---

# AAMP Core Compliance Review

## Rules
1. **Read before write** — Read the full source file and its sequence diagram before suggesting changes
2. **Find overrides not base class** — StreamAbstractionAAMP has HLS/MPD/Progressive subclasses; check overrides
3. **Backward compatibility** — Public API in main_aamp.h must not change signatures
4. **Input validation** — All Tune/Seek/SetRate calls must validate URL, position, rate
5. **Fallback with logging** — Use AAMPLOG macros at every failure point with error context
6. **Unit tests** — Every core change must include or update unit tests

## Reference Diagrams
- docs/aamp-core-sequence-diagrams/01-tune-playback-lifecycle.md
- docs/aamp-core-sequence-diagrams/02-gstreamer-pipeline.md
- docs/aamp-core-sequence-diagrams/03-stream-abstraction.md
- docs/aamp-core-sequence-diagrams/07-event-manager.md
- docs/aamp-core-sequence-diagrams/08-config-scheduler.md

## Checklist
- [ ] Does the change align with the Tune/TuneHelper/TeardownStream lifecycle?
- [ ] Are MediaTrack fetch/inject loops preserved?
- [ ] Is the event dispatch contract maintained?
- [ ] Are scheduler task IDs properly managed (no leaks)?
- [ ] Is AampConfig ownership respected (operator > channel > default)?
