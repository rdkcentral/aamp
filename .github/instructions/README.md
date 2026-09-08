# Copilot Instructions

This directory contains specialized GitHub Copilot instruction files,
organized by topic and language. Each file activates automatically via
its `applyTo` glob; load others manually when relevant.

## Files

### Architecture & language
- **`abr.instructions.md`** — Normative functional spec for ABR and latency-control logic.
  Activates on `abr/**`. Use with the `/abr-*` prompt files for structured reviews.
- **`aamp.instructions.md`** — AAMP architecture and current ("AS-IS")
  conventions. Activates on AAMP C/C++ sources.
- **`cpp.instructions.md`** — C++17 coding standards, documentation,
  memory and error-handling patterns. Canonical home for general C++
  rules.
- **`legacy-cpp-patterns.instructions.md`** — Modernization guidance for
  legacy C++ code that is being touched as part of the current task.
- **`js.instructions.md`** — JavaScript / TypeScript guidance for
  AAMP-adjacent tooling and UI code.

### Testing
- **`testing.instructions.md`** — General, language-agnostic testing
  philosophy (risk-based, not coverage-driven).
- **`l1-build-run.instructions.md`** — Mandatory build/run workflow for
  AAMP L1 unit tests under `test/utests/`.
- **`l1-structure.instructions.md`** — L1 directory layout, naming, and
  CMake patterns.
- **`l1-fakes-mocks.instructions.md`** — AAMP fake/mock architecture and
  anti-patterns. Canonical home for fake/mock rules.
- **`l1-oracle-design.instructions.md`** — How to derive a behavioural
  oracle for an L1 test when no formal spec exists.
- **`l1-validity-review.instructions.md`** — Review checklist and
  verdict language for L1 tests.

## Usage

### C++ development
The C++ files activate on `**/*.{cpp,h,hpp,cxx,hxx}`. For most edits
this is all you need. When refactoring legacy code, also keep
`legacy-cpp-patterns.instructions.md` in mind.

### L1 unit test work
All five `l1-*.instructions.md` files activate on `test/utests/**`. For
non-trivial L1 work, the `@l1-test-engineer` agent enforces the full
workflow automatically.

## Conventions
- Canonical-home rule: each policy lives in one file. Other files refer
  to it by name rather than duplicating wording. If you find the same
  rule restated in two files with different wording, prefer keeping the
  one in the more specialized file and reduce the other to a brief
  cross-reference.
- Do not introduce C++20 language or library features in active code
  (see `cpp.instructions.md`).
- Do not broaden `applyTo` globs without justification.
