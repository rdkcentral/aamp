---
agent: 'agent'
description: 'Derive the intended behavioral contract before writing or reviewing L1 tests'
---
Derive the intended behavioral contract before writing or reviewing L1 tests.

Do this in order:

1. Infer the intended contract from names, comments, callers, error handling, existing validated tests, and bug context.
2. Write:
   - Assumed Behavioral Contract
   - Invariants
   - Observable Outcomes
   - Failure Semantics
   - Oracle Sources
3. State:
   - what must not be used as the oracle
   - fake/mock assumptions
   - overfitting risks
4. Only then create or review tests.

Reject tests whose only oracle is the current implementation.
Do not test fake/mock behavior.
State why each test checks correctness rather than merely documenting current behavior.

