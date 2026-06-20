---
description: Single-function or single-module ABR spec compliance check
agent: agent
---

You are reviewing one function or module for compliance with the ABR normative spec.
Cite evidence for every claim. Say "Not determinable" where code is insufficient.
Do not propose redesigns; provide the smallest compliant patch in pseudocode.

**Normative spec:** See `.github/instructions/abr.instructions.md` (authoritative).

**Function/module under review:**
- File: (name)
- Symbol(s): (function or class)
- Code: (paste here)

## Tasks

1. Identify which spec rules this code is responsible for — list section numbers.
2. For each relevant rule:
   - PASS / FAIL / PARTIAL / NOT FOUND / NOT DETERMINABLE
   - Evidence from code (quotes)
   - Explanation
3. For each FAIL/PARTIAL:
   - Smallest compliant fix in pseudocode (no refactor unless necessary)
4. Provide 3 to 5 unit tests that directly assert the spec rules applicable to this function.

Severity tags: S0 Safety (underflow/freeze) | S1 Latency | S2 Stability (thrash) | S3 Quality only
