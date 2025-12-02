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
  **Critical:** L1 Test structure, build flow, Fake framework, Google Test rules.

### Language-Specific
- `cpp.instructions.md`  
- `legacy-cpp-patterns.instructions.md`   
- `js.instructions.md`  

Copilot must reference these files when generating language-specific code.

---