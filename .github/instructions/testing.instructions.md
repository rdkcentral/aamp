---
description: General testing philosophy and non-L1 test patterns
---

# Testing Copilot Instructions

## Scope

This file covers **general testing principles** that apply across all test
types in the AAMP repository (L1 unit tests, integration tests, JS tests, etc.).

For C++ L1 unit test specifics, see the dedicated instruction files:
- `l1-build-run.instructions.md` — mandatory build/run workflow
- `l1-structure.instructions.md` — directory layout, naming, CMake
- `l1-fakes-mocks.instructions.md` — AAMP fake/mock patterns
- `l1-validity-review.instructions.md` — review checklist and verdicts

---

## General Testing Principles

- New or modified public behaviour should have tests **proportionate
  to risk and complexity** — not by a blanket rule that every public function
  must have a dedicated unit test.
- Prioritise tests for code paths that are:
  - on the playback / buffering / ABR / DRM hot path,
  - historically a source of regressions,
  - difficult to validate at integration level,
  - or implementing a non-obvious contract.
- Trivial getters, thin pass-throughs, and pure forwarding wrappers do not
  require dedicated unit tests.
- Do not chase a numeric coverage target. Coverage is a *diagnostic*, not a
  goal. Tests written only to raise a coverage percentage tend to be
  brittle, implementation-coupled, and actively harmful.
- Test the **observable behaviour and contract** of the code under test,
  including success, error, edge, and boundary conditions that are
  reachable and meaningful.
- For L1 tests specifically, the oracle-design and anti-brittleness rules
  in `l1-oracle-design.instructions.md` and
  `l1-validity-review.instructions.md` take precedence over any generic
  coverage guidance.
- All tests must run via the CI pipeline.
