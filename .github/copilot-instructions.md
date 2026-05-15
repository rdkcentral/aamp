# Custom Instructions for GitHub Copilot
These instructions define how GitHub Copilot should behave for this repository.  
They apply to all code suggestions, documentation, tests, diagrams, and refactoring work.

---

# ==============================
#  BASIC CODING GUIDELINES
# ==============================

## 1. Code Formatting
- Indentation uses hard tabs (4-space width).
- Maximum line length target is 80 characters.
- Add spaces around operators and after commas.

## 2. Testing
- All public functions require unit tests.
- Use Google Test/Google Mock.
- Review **`instructions/testing.instructions.md`** before creating any tests.
- All tests must run via the CI pipeline.

### L1 Test Workflow (Mandatory)
For L1 test creation, review, validity checks, or failure diagnosis, prefer
the **L1 Test Engineer** agent (`@l1-test-engineer`). It enforces the full
workflow below automatically. When the agent is not explicitly selected,
Copilot must still follow these steps:

1. **Identify the component under test** before writing any code.
2. **Check for existing tests** under `test/utests/tests/` — do not create
   duplicate suites. See `instructions/l1-structure.instructions.md`.
3. **Derive the behavioral contract and correctness oracle** before
   proposing or reviewing tests.
   See `instructions/l1-oracle-design.instructions.md`.
4. **Follow the approved build/run workflow exactly.** For new tests, the
   mandatory first step is `cd test/utests && ./run.sh`. Do not invent
   alternate build commands. See `instructions/l1-build-run.instructions.md`.
5. **Use AAMP fakes and mocks**, not generic GoogleTest patterns. Fake
   behavior intentionally differs from production — adapt expectations.
   See `instructions/l1-fakes-mocks.instructions.md`.
6. **Test component behavior, not fake/mock behavior.** Every assertion
   must verify how the component under test responds, not how a
   dependency stub works.
7. **Do not invent file names, paths, or CMake structure.** Follow the
   exact conventions in `instructions/l1-structure.instructions.md`.
8. **When reviewing tests**, use the verdict language and checklist from
   `instructions/l1-validity-review.instructions.md`.

## 3. Version Control
- Use meaningful, imperative commit messages (“Add X”, “Fix Y”).
- Break large changes into small, focused commits.

## 4. Code Reviews
- All changes must go through pull requests.
- Feedback must be constructive and based on these guidelines.

## 5. Performance Considerations
- Optimize only when profiling indicates a need.
- Favor clarity before micro-optimization.
- Consider memory usage and embedded constraints at all times.

---

# ==============================
#  PROMPT FEEDBACK GUIDELINES
# ==============================

## Prompt Feedback (Compact)
- Always assess prompt quality for every prompt and emit scores unless the scoring thresholds for suppression are met.
- When feedback is not suppressed, use the following format:
- Format: `Scores: Completeness X/10, Assumptions X/10, Clarity X/10 | Critique: <brief> | Improve: <specific edit>`.
- Scoring: Completeness and Clarity are higher-is-better; Assumptions is lower-is-better.
- Strict rubric for underspecified prompts:
  - If the prompt is extremely vague (for example: "build something"), score it harshly.
  - For these prompts, use: Completeness 0-3/10, Clarity 0-3/10, Assumptions 7-10/10.
  - If target, scope, constraints, or success criteria are omitted,
    cap Completeness and Clarity at 7/10 and set Assumptions to at least 3/10.
- Keep it short and specific; avoid generic advice.
- Never suppress scored feedback for underspecified prompts (including
  prompts that fall under the strict rubric above).
- Suppress displayed feedback when Completeness >= 8, Assumptions <= 2,
  and Clarity >= 8,
  unless the user explicitly asks to apply feedback to the current prompt.
- Determine suppression from the current user prompt only; retrospective analysis of earlier prompts should be provided only when explicitly requested.
- Prefer high-compliance guidance: suggest exact wording that reduces
  ambiguity and improves instruction precision.

---

# ==============================
#  COPILOT MASTER INSTRUCTIONS
# ==============================

This section tells Copilot how to reason about architecture, style, safety, and file structure.  
Language-specific patterns live in `.github/instructions/`.

## Core Philosophy
1. **Embedded Systems Focus**  
   Code must be efficient, predictable, minimize memory, and avoid unnecessary dynamic allocations.

2. **Video Streaming Domain**  
   This is a streaming video player. Low latency, correct buffering, and real-time behavior are critical.

3. **Modernization Goal**  
   New code should be modern C++ (RAII, smart pointers, interfaces).  
   Legacy code should be gently refactored toward modern patterns.

4. **Testing First**  
   Always reference `instructions/testing.instructions.md` before writing tests.

---

# ==============================
#  GENERAL GUIDELINES
# ==============================
- Prefer self-documenting code over excessive comments.
- Apply RAII for all resources.
- Follow DRY (Don't Repeat Yourself).
- Keep cyclomatic complexity low.
- Prefer composition over inheritance.
- Never introduce new dependencies without justification.

---

# ==============================
#  ARCHITECTURAL PRINCIPLES (SOLID)
# ==============================
- **SRP:** Every class must have a single responsibility.
- **OCP:** Extend behavior via interfaces, not by editing core logic.
- **LSP:** Subtypes must be drop-in replacements.
- **ISP:** Interfaces must be small and specific.
- **DIP:** Depend on abstractions, not concrete classes.

---

# ==============================
#  SECURITY GUIDELINES
# ==============================
- Validate all inputs.
- Sanitize any external or untrusted data.
- Avoid leaking internal state in error messages.
- Follow least privilege.
- Never hardcode credentials or keys.

---

# ==============================
#  DOCUMENTATION & DIAGRAMS
# ==============================
- Use Doxygen-style comments for all APIs.
- Generate diagrams with PlantUML.
- See `instructions/diagrams.instructions.md` for details.

---

# ==============================
#  COPYRIGHT HEADER
# ==============================
All new files must include the RDK copyright header:

If not stated otherwise in this file or this component's license file the
following copyright and licenses apply:

Copyright <Current Year> RDK Management

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

---

# ==============================
#  DIRECTORY STRUCTURE RULES
# ==============================
Copilot must follow the expected directory layout:

- **Unit tests:** `test/`
- **Mocks/Fakes:** `test/fakes/`
- **Diagrams:** `docs/diagrams/`
- **Special instruction files:** `.github/instructions/`

---

# ==============================
#  PROHIBITED PATTERNS
# ==============================
Copilot must **not** generate:

- Raw `new`/`delete` unless explicitly required.
- God classes or monolithic files.
- Hidden global state.
- Unnecessary singletons.
- Large refactors without being asked.
- Excessive template metaprogramming.
- Wide-reaching architectural changes without context.

---

# ==============================
#  EXPECTED PULL REQUEST BEHAVIOR
# ==============================
When generating or updating PRs:

- Follow `.github/pull_request_template.md`.
- PR titles must be imperative (“Add X”, “Update Y”).
- PR descriptions must include:
  - **Summary**
  - **Changes**
  - **Testing**
  - **Risk**
  - **Related Issues**
- Link to architecture files when appropriate (`aamp.instructions.md`).

---

# ==============================
#  WHEN COPILOT SHOULD ASK QUESTIONS
# ==============================
Copilot must request clarification when:

- Ownership semantics are ambiguous (`shared_ptr` vs `unique_ptr`).
- Behavior depends on legacy AAMP behavior that isn’t visible in the snippet.
- A change might affect performance-critical streaming paths.
- New interfaces or abstractions might conflict with existing architecture.
- Code generation requires choosing between multiple complex design patterns.

---

# ==============================
#  GUIDE TO SPECIALIZED INSTRUCTIONS
# ==============================
The `.github/instructions/` directory contains deeper rules:

### Architecture / Testing
- `aamp.instructions.md`  
  Describes AAMP’s architecture, modules, and legacy constraints.

- `testing.instructions.md`  
  General testing philosophy, Python/JS patterns, and integration testing.

### L1 Unit Testing (read all five for any L1 work)
- `l1-build-run.instructions.md`  
  Mandatory build/run workflow. Do not skip or improvise.

- `l1-structure.instructions.md`  
  Directory layout, file naming, CMake patterns, existing-test checks.

- `l1-fakes-mocks.instructions.md`  
  AAMP-specific fake/mock guidance. Golden rule: test the component, not the fake.

- `l1-oracle-design.instructions.md`  
  Behavioral contract and correctness oracle derivation. Required before writing or reviewing tests.

- `l1-validity-review.instructions.md`  
  Review checklist and verdict language for L1 test quality.

### L1 Test Engineer Agent
- `agents/l1-test-engineer.agent.md`  
  Preferred specialist for L1 test creation, review, diagnosis, and explanation.
  Reads and enforces all five L1 instruction files. Select via `@l1-test-engineer`.

### Language-Specific
- `cpp.instructions.md`  
- `legacy-cpp-patterns.instructions.md`   
- `js.instructions.md`  

Copilot must reference these files when generating language-specific code.

## AAMP log debugging

- For AAMP run-log analysis, use the reusable prompt file `/aamp-log-debug`.
- When debugging logs, build a timeline first, identify the first abnormal event, and separate facts from hypotheses.
- Correlate manifest, network, buffering, ABR, DRM, and player-state evidence before concluding root cause.
- Prefer minimal safe fixes and minimal additional instrumentation.

## Code analysis prompts

- For accurate measurement of method or function cyclomatic complexity, use the reusable prompt file `/cyclomatic-complexity`.
- When reporting complexity, show the counted decision points step by step and call out any ambiguity from macros or compile-time conditionals.

---