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

- All public functions require unit tests.
- Aim for minimum 90% code coverage.
- Test all code paths: success, error, edge cases, boundary conditions.
- All tests must run via the CI pipeline.
