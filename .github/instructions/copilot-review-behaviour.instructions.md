---
applyTo: "**"
---

# Copilot Review Behaviour

Apply this file together with all relevant instruction files under
`.github/instructions/`.

The purpose of review comments is to identify:
- real defects;
- real test risks;
- real maintainability problems;
- concrete violations of repository instructions.

Do not invent speculative issues.

## Review Constraints

DO NOT:
- speculate;
- guess about missing context;
- recommend broad refactors;
- recommend modernization for its own sake;
- suggest changes larger than the reviewed diff;
- generate large replacement patches;
- repeat the same concern more than once;
- re-raise previously dismissed concerns unless the code materially changed.

Do not comment on:
- theoretical race conditions without a plausible failing path;
- hypothetical null dereferences without a concrete path;
- stylistic preferences not required by repository instructions;
- pre-existing issues not worsened by the PR.

## Proportionality

Review comments must be proportional to the size and purpose of the PR.

For small PRs:
- prefer small local fixes;
- avoid architectural recommendations;
- avoid multi-file redesign suggestions.

Do not suggest changes that would require substantial additional L1
validation unless the current code is clearly incorrect.

Prefer the smallest reasonable fix.

## Evidence Requirement

Every review comment must identify:
- the exact code location;
- the specific violated rule, instruction, or concrete risk;
- why the issue matters in this repository;
- the smallest reasonable correction.

If this cannot be done clearly, do not leave the comment.

## Existing Repository Conventions

Repository conventions may intentionally differ from modern C++ guidance.

Do not request:
- unnecessary smart-pointer conversions;
- large-scale const/refactoring churn;
- broad STL modernization;
- architectural cleanup unrelated to the PR;
- replacement of intentional legacy-compatible patterns.

Respect the repository's established implementation style unless the PR
itself is explicitly modernizing that area.

## Testing Guidance

Do not request:
- extensive new test infrastructure for small fixes;
- broad L1 rewrites;
- unnecessary mock/fake redesigns.

Only request additional testing when:
- behaviour changed materially;
- an execution path is newly introduced;
- an existing test oracle is invalidated;
- coverage for the changed behaviour is genuinely missing.

## Review Quality

High-quality review comments are:
- specific;
- actionable;
- minimal;
- technically justified;
- non-repetitive.

Low-confidence comments should not be emitted.