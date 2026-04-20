---
applyTo: "test/utests/**,**/*Tests.cpp,**/*TestCases.cpp"
description: Derive correctness oracles for AAMP L1 tests when no dedicated spec exists
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

## Required output during review

For each test, state:
- assumed contract
- oracle for correctness
- fake/mock assumptions
- overfitting risk
