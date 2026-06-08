---
name: aamp-l1-testing
description: "**WORKFLOW SKILL** — Create, review, diagnose, or explain AAMP C++ L1 unit tests under test/utests/. USE FOR: writing new L1 test suites; reviewing test validity; diagnosing build/link failures; understanding AAMP fake/mock architecture; deriving correctness oracles. Covers GoogleTest, GoogleMock, AAMP fakes, build workflow, CMake patterns, and test validity verdicts."
---

# AAMP L1 Testing Skill

## When to Use

Use this skill when:
- Creating a new L1 unit test suite for an AAMP component
- Reviewing existing L1 tests for validity
- Diagnosing L1 test build or link failures
- Deriving a correctness oracle for a component method
- Understanding the AAMP fake/mock test architecture

## Mandatory Workflow

For every L1 task, follow this sequence in order:

### 1. Identify the Component Under Test

State the component name explicitly before writing any code. The component
corresponds to a single `.cpp` source file (e.g., `AampLatencyMonitor.cpp`).

### 2. Check for Existing Tests

Search `test/utests/tests/` for the component name and variants:
- `[ComponentName]Tests/` (current convention)
- `[ComponentName]Test/` (legacy — singular)
- `[ComponentName]/` (legacy — no suffix)

**Do not create duplicate suites.** If one exists, extend it.

### 3. Derive the Behavioral Contract

Before writing tests, state:
- Intended purpose of the component/method
- Success behavior
- Failure behavior
- Invariants and edge cases
- Observable outcomes

See [oracle-design.md](oracle-design.md) for the full oracle derivation process.

### 4. Assess Testability

If the method requires > 5 mock setups for one path, has cyclomatic
complexity > 15, or mixes I/O + state + control flow, flag it as needing
refactoring before L1 testing.

### 5. Write the Test

Follow:
- [structure.md](structure.md) — directory layout, file naming, CMake
- [fakes-mocks.md](fakes-mocks.md) — fake/mock patterns and anti-patterns
- [build-run.md](build-run.md) — approved build commands

### 6. Build and Verify

```bash
# First time (new suite):
cd test/utests && ./run.sh

# Iteration (existing suite):
cd test/utests/build/tests/[ComponentName]Tests && make && ./[ComponentName]Tests
```

### 7. Review (if applicable)

Apply the checklist and verdict language from [validity-review.md](validity-review.md).

## Constraints

- DO NOT generate generic GoogleTest advice that ignores AAMP conventions.
- DO NOT invent file names, paths, CMake variables, or build commands.
- DO NOT include real dependency `.cpp` files when a fake exists.
- DO NOT write assertions that test fake/mock behavior instead of component behavior.
- DO NOT skip the existing-test check.
- DO NOT create tests outside `test/utests/tests/`.
- DO NOT use `sleep()` / `usleep()` / `sleep_for()` for synchronization.

## Diagnosing Failures

1. Read the error output carefully.
2. Check the common failures table in [build-run.md](build-run.md).
3. Check whether the test asserts on fake behavior (most common L1 bug).
4. Check `CMakeLists.txt` for missing or extra sources/libraries.
5. Verify the test builds and runs via the approved workflow.

## Bundled References

| File | Content |
|------|---------|
| [build-run.md](build-run.md) | Approved build/run workflow |
| [structure.md](structure.md) | Directory layout, naming, CMake patterns |
| [fakes-mocks.md](fakes-mocks.md) | Fake/mock conventions and anti-patterns |
| [oracle-design.md](oracle-design.md) | Behavioral contract and oracle derivation |
| [validity-review.md](validity-review.md) | Review checklist and verdict language |
| [examples.md](examples.md) | Complete working examples from the codebase |
