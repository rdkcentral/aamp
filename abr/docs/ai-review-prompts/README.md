# AI Review Prompt Templates for ABR Compliance

This directory contains **AI prompt templates** for reviewing, validating, and auditing Adaptive Bitrate (ABR) and low-latency playback implementations against a **normative functional specification**.

The templates are designed to be:

- Spec-driven (normative, not heuristic)
- Evidence-based (claims must cite code or logs)
- Deterministic (no speculative answers)
- Composable (usable independently or together)

---

## Recommended File Layout

```text
ai-review-prompts/
├─ README.md
├─ SpecFirst_ABR_Compliance_Review.txt
├─ PR_Diff_Review.txt
├─ FunctionLevel_Compliance_Check.txt
├─ Runtime_Log_Trace_Validator.txt
├─ SpecToAssertions_Instrumentation_Plan.txt
├─ AI_Self_Check_Prompt.txt
└─ Output_Scoring_Rubric.txt
```

---

## Template Descriptions

### SpecFirst_ABR_Compliance_Review.txt
**Primary, comprehensive review template.**

Use when:
- Auditing ABR behavior end-to-end
- Validating correctness against the full spec
- Performing hardening or pre-release reviews

Outputs:
- Spec → code traceability matrix
- PASS / FAIL / PARTIAL classification
- Minimal fix suggestions
- Targeted test plan

---

### PR_Diff_Review.txt
**Focused review for incremental changes.**

Use when:
- Reviewing a pull request
- Checking whether a diff introduces spec violations

Outputs:
- Impacted spec rules
- New violations or ambiguity
- Missing or required tests

---

### FunctionLevel_Compliance_Check.txt
**Single-function or single-module validation.**

Use when:
- Reviewing profile selection, bail logic, latency control
- Refactoring localized logic

Outputs:
- Rule-level compliance verdict
- Minimal pseudocode fixes
- Unit test suggestions

---

### Runtime_Log_Trace_Validator.txt
**Observed-behavior validation using logs/traces.**

Use when:
- Investigating freezes or latency drift
- Validating live or simulated playback

Outputs:
- Timeline reconstruction
- Rule violations with log evidence
- Missing instrumentation calls

---

### SpecToAssertions_Instrumentation_Plan.txt
**Make the implementation self-auditing.**

Use when:
- Adding logs or assertions
- Improving long-term maintainability

Outputs:
- Mapping of spec rules to assertions/logs
- Required event fields
- Example event schemas

---

### AI_Self_Check_Prompt.txt
**Guardrail prompt to prepend to others.**

Enforces:
- Explicit rule listing
- Evidence-cited claims
- Honest “not determinable” responses
- No speculative redesigns

Highly recommended for all reviews.

---

### Output_Scoring_Rubric.txt
**Standardized evaluation rubric.**

Defines:
- PASS / PARTIAL / FAIL / NOT FOUND / NOT DETERMINABLE
- Severity levels:
  - S0: Safety (underflow risk)
  - S1: Latency correctness
  - S2: Stability (thrash/oscillation)
  - S3: Quality only

---

## Typical Usage Patterns

### Full Audit
- SpecFirst_ABR_Compliance_Review.txt
- Output_Scoring_Rubric.txt
- (Optional) SpecToAssertions_Instrumentation_Plan.txt

### PR Review
- PR_Diff_Review.txt
- AI_Self_Check_Prompt.txt

### Debugging a Freeze
- Runtime_Log_Trace_Validator.txt
- FunctionLevel_Compliance_Check.txt

---

## Final Notes

- The **normative spec** is the source of truth
- The conceptual overview is explanatory only
- These prompts are intentionally strict and repetitive

That discipline is what makes AI-based compliance reviews reliable.
