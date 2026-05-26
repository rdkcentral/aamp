---
description: Derive correctness oracles for AAMP L1 tests when no dedicated spec exists
applyTo:
  - "test/utests/**"
---

# AAMP L1 Oracle Design

Before creating or reviewing an L1 test, define the assumed behavioral contract.

## Required first step

State all of the following before proposing tests:
- intended purpose of the component or method
- success behavior
- failure behavior
- invariants
- edge cases
- observable outcomes

## Preferred oracle sources

Use these sources in order:
1. explicit comments or docs
2. public API semantics
3. caller expectations
4. error handling behavior
5. bug or regression intent
6. nearby validated tests

## Never use these as the sole oracle

- current internal implementation details
- fake or mock behavior
- incidental private state with no behavioral meaning
- assertions that merely restate configured mock return values

## Required test design rule

If a proposed test can only be justified by
"the code currently behaves this way"
then reject or rewrite the test.

## Testability assessment

Before writing tests for a method, assess whether it is testable at L1.
The thresholds below are **advisory heuristics**, not hard gates — use
judgement and consider the component context.

**Warning signs that a method is likely too complex for L1:**
- Requires roughly 5 or more mock setups to reach a single code path
- Cyclomatic complexity in the high teens or above (use
  `/cyclomatic-complexity` to measure)
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

## Required output during review

For each test, state:
- assumed contract
- oracle for correctness
- fake/mock assumptions
- overfitting risk
