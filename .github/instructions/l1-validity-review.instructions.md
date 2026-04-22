---
description: L1 test validity review checklist and verdict criteria
applyTo:
  - "test/utests/**"
---

# L1 Test Validity & Review

---

## What Makes a Valid L1 Test

A valid L1 test in this repository:

- Tests **one component** in isolation from its real dependencies.
- Uses **fakes or mocks** for all dependencies — never real implementations.
- Asserts on **component behavior** (state, return values, side effects).
- Does not assert on fake/mock internal behavior.
- Covers all reachable code paths: success, error, edge cases, boundaries.
- Follows the naming, structure, and build conventions in
  `l1-structure.instructions.md` and `l1-build-run.instructions.md`.
- Builds and passes via `cd test/utests && ./run.sh`.

---

## Common Invalid Patterns

| Pattern | Why it is invalid |
|---|---|
| Asserting on `config.IsConfigSet()` or `config.GetConfigValue()` directly | Tests the fake, not the component |
| Including real `.cpp` dependencies in CMake | Defeats isolation; should use fakes |
| Expecting real config persistence from `FakeAampConfig` | Fake `SetConfigValue(bool)` is a no-op |
| Testing only the happy path | Missing error and boundary coverage |
| Creating a new test suite when one already exists for the component | Duplicate coverage; extend the existing suite |
| Instantiating real dependency objects in test code | Defeats isolation even if CMake only links fakes |
| Inventing build commands not in `l1-build-run.instructions.md` | May not work in CI; causes confusion |
| Test name like `Test1`, `TestA` | Non-descriptive; use `Class_Method_Behavior` |
| `EXPECT_EQ` / `ASSERT_EQ` on `float` or `double` values | Exact equality is unreliable for floating-point; use `EXPECT_DOUBLE_EQ`, `EXPECT_FLOAT_EQ`, or `EXPECT_NEAR` |

---

## Review Checklist

### Mock/Fake Usage
- [ ] All `EXPECT_*` assertions target the component under test
- [ ] No assertions on fake method return values (unless via `EXPECT_CALL`)
- [ ] No `memcmp` on fake buffer data
- [ ] No verification of fake state changes without testing component response
- [ ] Mock return values set via `EXPECT_CALL().WillOnce(Return())` or `WillRepeatedly(Return())`
- [ ] No real dependency objects instantiated in test code (use fakes/mocks)

### Assertion Correctness
- [ ] No `EXPECT_EQ` / `ASSERT_EQ` on `float` or `double` values — use `EXPECT_DOUBLE_EQ`, `EXPECT_FLOAT_EQ`, or `EXPECT_NEAR`

### Component Behavior Focus
- [ ] Tests verify component member variables and state after operations
- [ ] Tests verify component method return values
- [ ] Tests verify component error handling paths
- [ ] `EXPECT_CALL` used to verify component→dependency interactions

### Structure & Build
- [ ] Test directory is `test/utests/tests/[ComponentName]Tests/`
- [ ] Files: runner `.cpp`, cases `.cpp`, `CMakeLists.txt`
- [ ] `CMakeLists.txt` links `fakes` first
- [ ] `CMakeLists.txt` does not include real dependency sources
- [ ] Copyright header present in all new files (with current year)
- [ ] Doxygen tags on all test methods
- [ ] Builds and passes via `cd test/utests && ./run.sh`

---

## Verdict Language

When reviewing L1 tests, use exactly one of these verdicts:

| Verdict | Meaning |
|---|---|
| **Valid L1 test** | Meets all criteria; ready to merge |
| **Mostly valid, but needs revision** | Core approach is correct; specific items need fixing (list them) |
| **Not a good L1 test** | Structurally correct but tests the wrong thing (e.g., tests fake behavior) |
| **Not an L1 test** | Does not belong in `test/utests/tests/` — may be an integration test, manual test, or exploratory code |

Always state the verdict clearly, then provide specific items to address.
