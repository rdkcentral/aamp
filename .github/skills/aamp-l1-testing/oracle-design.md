# AAMP L1 Oracle Design

Before creating or reviewing an L1 test, define the assumed behavioral contract.

## Required First Step

State all of the following before proposing tests:
- Intended purpose of the component or method
- Success behavior
- Failure behavior
- Invariants
- Edge cases
- Observable outcomes

## Preferred Oracle Sources

Use these sources in order of priority:
1. Explicit comments or docs
2. Public API semantics
3. Caller expectations
4. Error handling behavior
5. Bug or regression intent
6. Nearby validated tests

## Never Use These as the Sole Oracle

- Current internal implementation details
- Fake or mock behavior
- Incidental private state with no behavioral meaning
- Assertions that merely restate configured mock return values

## Required Test Design Rule

If a proposed test can only be justified by
"the code currently behaves this way"
then reject or rewrite the test.

## Testability Assessment

Before writing tests for a method, assess whether it is testable at L1:

**Warning signs that a method is too complex for L1:**
- Requires more than ~5 mock setups to reach a single code path
- Has cyclomatic complexity > 15 (use `/cyclomatic-complexity` to measure)
- Mixes I/O, state mutation, and control flow in a single method
- Requires reaching into private state to verify outcomes
- Has deeply nested conditionals that make path isolation impractical

**When a method fails this assessment:**
1. State that the method is not practically testable at L1 in its current form.
2. Recommend refactoring into smaller, testable units (extract method, split
   responsibilities) before writing tests.
3. If refactoring is out of scope, defer to L2 integration testing and
   document the gap with a skipped-test comment explaining why.

Do not write brittle, implementation-coupled tests just to achieve coverage
on an overly complex method.

## Required Output During Review

For each test, state:
- Assumed contract
- Oracle for correctness
- Fake/mock assumptions
- Overfitting risk
