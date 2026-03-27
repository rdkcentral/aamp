---
description: TDD and SOLID rules for the direct-rialto AampRialtoPlayer component
applyTo:
  - "direct-rialto/**"
  - "test/utests/tests/AampRialtoPlayerTests/**"
  - "test/utests/tests/AampRialtoMediaPipelineClientTests/**"
---

# Direct-Rialto Component Instructions

These rules apply **only** to `direct-rialto/` and its companion test directories.
They intentionally do **not** extend to the rest of AAMP to avoid merge conflicts
and the risk of breaking unrelated code.

---

## TDD Workflow — Mandatory for All Changes

Every change to `direct-rialto/` must follow the **Red → Green → Refactor** cycle:

1. **Red** — Write a failing test that specifies the new behavior before touching
   production code.  The test must compile and fail for the right reason.
2. **Green** — Write the minimum production code needed to make the test pass.
   Do not add anything not covered by a test.
3. **Refactor** — Clean up duplication, naming, and structure while keeping all
   tests green.

### Rules
- Never modify `direct-rialto/` production code without a corresponding test.
- Tests live in `test/utests/tests/AampRialtoPlayerTests/` or
  `test/utests/tests/AampRialtoMediaPipelineClientTests/`.
- Always read `.github/instructions/testing.instructions.md` before writing or
  modifying any test.
- Verify by building and running the affected test binary after every change:
  ```
  cd test/utests/build/tests/AampRialtoPlayerTests && make && ./AampRialtoPlayerTests
  ```
- The TDD phase table in `docs/rialto-integration/aamp-rialto-player-analysis.md`
  is the authoritative backlog.  Consult it before starting any new work.

---

## SOLID Principles — Applied to New Code in `direct-rialto/`

Apply all five SOLID principles when writing new classes or interfaces inside
`direct-rialto/`.  These principles **must not** be used as justification to
refactor existing AAMP code outside this directory.

| Principle | How to apply it here |
|-----------|----------------------|
| **SRP** | `AampRialtoPlayer` orchestrates; injection workers, the pipeline client, and the demuxers each own one concern. Extract a new class rather than growing an existing one. |
| **OCP** | Add behavior by introducing new types (e.g. a per-source `SourceWorker`) rather than adding `if/switch` branches to existing classes. |
| **LSP** | Any class that replaces `IMediaPipeline` or `IMediaPipelineClient` in tests must be a drop-in substitute with no behavioral surprises. |
| **ISP** | Keep callback interfaces (`NeedDataCallback`, `PlaybackStateCallback`) small and single-purpose.  Do not add unrelated methods to `AampRialtoMediaPipelineClient`. |
| **DIP** | Depend on `firebolt::rialto::IMediaPipeline` and `IMediaPipelineFactory`, never on concrete implementations.  New internal types must be injected via constructor or setter, not created with `new` inside business logic. |

---

## Scope Boundary — Do Not Refactor the Rest of AAMP

> **Hard rule:** Changes originating in `direct-rialto/` must not cascade into
> files outside `direct-rialto/`, `test/utests/tests/AampRialtoPlayerTests/`, or
> `test/utests/tests/AampRialtoMediaPipelineClientTests/`.

Specifically:
- Do **not** modify `StreamSink.h`, `priv_aamp.h`, `AampConfig.*`, or any other
  shared AAMP header to accommodate a `direct-rialto/` change.
- Do **not** refactor `aampgstplayer.*` or other `StreamSink` implementations.
- If a change requires touching shared infrastructure, raise it as a separate,
  explicit ask — it must be reviewed and agreed independently.

This boundary exists to keep the direct-Rialto work mergeable without risk to
the rest of the AAMP pipeline.

---

## Architecture Guidance

The target architecture is described in
`docs/rialto-integration/aamp-rialto-player-analysis.md` (Improvement Plan,
Steps 2–3 and TDD Phase 13).  Key design intentions:

- **Phase 13 (outstanding):** Replace the single `m_injectionThread` with
  per-source worker queues so video and audio injection are independent.
  Add a `PlayerState` enum state machine to replace scattered booleans.
- Model the injection architecture on `rialto-gstreamer`'s `BufferPuller +
  MessageQueue` pattern.
- Prefer `std::thread` + `std::condition_variable` over platform-specific
  primitives.  Keep all threading contained within `AampRialtoPlayer`.
