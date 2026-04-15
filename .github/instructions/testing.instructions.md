---
description: General testing philosophy and non-L1 test patterns
---

# Testing Copilot Instructions

## Scope

This file covers **general testing philosophy** for the AAMP repository.

**All C++ L1 unit test guidance is in dedicated instruction files:**
- `l1-build-run.instructions.md` — mandatory build/run workflow
- `l1-structure.instructions.md` — directory layout, naming, CMake
- `l1-fakes-mocks.instructions.md` — AAMP fake/mock patterns
- `l1-validity-review.instructions.md` — review checklist and verdicts

---

## General Testing Principles

- All public functions require unit tests.
- Use Google Test / Google Mock for C++.
- Aim for minimum 90% code coverage.
- Test all code paths: success, error, edge cases, boundary conditions.
- Test names must be descriptive: `ClassName_MethodName_ExpectedBehavior`.
- All tests must run via the CI pipeline.
- Prefer self-documenting test code; use Doxygen tags for documentation.
