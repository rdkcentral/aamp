---
description: 'Investigate and document the Direct Rialto architecture, implementation, threading, lifecycle and integration, with engineering diagrams and validation.'
agent: agent
---

# Task: Comprehensive engineering documentation for Direct Rialto

## Objective

Thoroughly investigate and document the Direct Rialto feature implemented in the `direct-rialto` source folder.

The audience is experienced C++ software engineers who need to understand the feature's architecture, design, implementation, integration, threading, lifecycle and technical decisions.

The documentation should enable an engineer to understand, maintain, troubleshoot and extend the feature **without initially needing to read the source code**.

However, it should not become an excessively verbose reference manual.

**Prioritise technical accuracy, architectural understanding, clear diagrams and concise explanations over large amounts of prose.**

Produce Markdown documentation suitable for browsing directly in GitHub.

## 1. Working approach and token efficiency

You are operating in GitHub Copilot Agent mode in VS Code.

First inspect the repository structure and identify the implementation, relevant existing functionality and external dependencies.

Use a structured investigation followed by documentation generation and validation.

### Subagent delegation

You may delegate independent investigations to subagents where doing so improves accuracy, context management or efficiency.

If the environment supports selecting subagent models, prefer:

| Investigation | Preferred model |
|---|---|
| Basic code exploration and dependency discovery | A lightweight model such as Claude Haiku |
| Architecture and comparison with existing implementation | Claude Sonnet 5.5 |
| Complex C++ concurrency, callbacks and lifetime analysis | GPT-6.1 Sol |
| Independent documentation verification | Claude Sonnet 5.5 or GPT-6.1 Sol |

These are preferences rather than mandatory assignments. Use only models available and supported by the current environment.

Do not create subagents unnecessarily.

Delegate only substantial, bounded investigations that can be performed independently.

Subagents should:

- Prefer read-only investigation.
- Return concise, evidence-based findings.
- Provide relevant source files, classes and functions.
- Identify important interactions and call paths.
- Distinguish confirmed behaviour from inferred behaviour.
- Identify relevant risks and unresolved questions.
- Avoid large code dumps and unnecessary detail.

The main agent remains responsible for consolidating findings, maintaining consistent terminology, generating documentation and validating the final result.

### Efficiency requirements

- Avoid repeatedly analysing the same files without good reason.
- Follow relevant call paths rather than reading unrelated code.
- Reuse investigation findings between stages.
- Avoid overlapping subagent investigations unless independent verification is valuable.
- Keep intermediate reports concise.
- Avoid unnecessary tooling and dependencies.
- Do not create diagrams or documentation files that add little value.
- Reserve deeper reasoning for genuinely complex behaviour.

Token efficiency must not compromise accuracy or omit important architectural details.

## 2. Initial code investigation

Before writing documentation, investigate:

1. All relevant source files, classes and interfaces in `direct-rialto`.
2. The existing player architecture and playback functionality.
3. How Direct Rialto is instantiated, configured and selected.
4. Existing functionality that Direct Rialto replaces, bypasses, supplements or shares.
5. Relevant AAMP, Rialto and Middleware Player Interface interactions.
6. Dependencies, factories, interfaces and callback registrations.
7. Construction, initialisation and destruction.
8. State management and lifecycle transitions.
9. Threading, asynchronous execution and synchronisation.
10. Resource ownership and lifetime.
11. Error handling, recovery and cleanup.
12. Configuration and build-time options.
13. Tests and significant missing coverage.
14. Existing relevant documentation and comments.

Follow actual call paths and relationships.

Do not assume behaviour solely from class names, comments or intended architecture.

**Document the actual implementation rather than an idealised design.**

### External library source code

The repository contains source code for external libraries relevant to Direct Rialto.

Locations:

- **Rialto Client/Server:** `.libs/rialto`
- **Middleware Player Interface:** `.libs/middleware-player-interface`

Investigate these source trees where needed to understand Direct Rialto.

In particular:

- Trace relevant calls into external libraries.
- Understand callback registration and invocation.
- Identify callback execution contexts and threading guarantees.
- Investigate asynchronous operations and thread-to-thread communication.
- Establish API ownership and object lifetime requirements.
- Trace relevant media data, playback control, events and errors.
- Identify state and lifecycle dependencies across component boundaries.
- Verify assumptions about external APIs against their implementations.

Pay particular attention to whether callbacks crossing the Rialto Client/Server boundary execute synchronously or asynchronously, which threads execute them, and how their lifetimes are managed.

Use the external library implementation as evidence rather than assuming its behaviour.

Include relevant external components in diagrams where useful.

Link to external source files when appropriate.

**Do not comprehensively document the external libraries.** Restrict investigation to the functionality and implementation paths relevant to Direct Rialto.

If an external implementation cannot be verified, clearly identify the uncertainty.

## 3. Documentation structure and locations

Create all Markdown documentation under:

`docs/direct-rialto/`

The primary entry point must be:

`docs/direct-rialto/README.md`

Suggested structure:

```text
docs/
└── direct-rialto/
    ├── README.md
    ├── architecture.md
    ├── integration.md
    ├── lifecycle.md
    ├── playback-flow.md
    ├── threading.md
    ├── error-handling.md
    ├── limitations.md
    ├── diagrams/
    │   └── [optional PlantUML sources]
    └── tools/
        └── [optional generation/validation tools]

images/
└── direct-rialto/
    └── [generated diagram images if required]
```

This is a suggested structure, not a mandatory list.

Adapt it to the implementation. Combine related topics where appropriate to avoid unnecessary fragmentation.

The README should provide a useful architectural overview that an experienced engineer can read in approximately 5–10 minutes.

More detailed documents should support progressive investigation.

Use relative links compatible with GitHub.

Do not modify production source code.

## 4. Documentation content

### A. High-level overview

Explain:

- What Direct Rialto does.
- Why the feature exists.
- The problem it solves.
- Its main capabilities.
- How it fits into the existing player architecture.
- How it differs from the existing playback implementation.
- Significant dependencies.
- Important design decisions and trade-offs.

Include a high-level architecture diagram showing the surrounding player architecture and relevant Direct Rialto components.

This section should establish a clear mental model before introducing implementation details.

### B. Comparison with existing functionality

This is particularly important.

Investigate the existing implementation outside `direct-rialto`.

Explain:

- How the existing playback approach works.
- What Direct Rialto changes or replaces.
- Which components and behaviours remain shared.
- Differences in player selection and creation.
- Differences in data and control flow.
- Differences in event handling and callback behaviour.
- Differences in threading and resource ownership.
- Differences in lifecycle and teardown.
- Behavioural and compatibility differences.
- Important architectural advantages and trade-offs.

Include a comparison table.

Where helpful, include equivalent sequence diagrams for operations through the old and new implementations, particularly playback startup.

Distinguish documented design rationale from conclusions inferred from code.

### C. Architecture, classes and ownership

Identify the significant classes and interfaces.

Document:

- Responsibilities.
- Inheritance and interface implementation.
- Composition and dependencies.
- Object creation and ownership.
- Lifetime and destruction.
- Significant public APIs.
- Callback and event relationships.
- Interactions with external libraries.

Include class and component diagrams.

Avoid including every method or trivial implementation detail.

Prefer simplified architectural diagrams with additional detailed diagrams where genuinely necessary.

Include a concise class responsibility table.

### D. Lifecycle and state management

Explain the feature's lifecycle, including where applicable:

- Construction and initialisation.
- Configuration and resource acquisition.
- Playback preparation and startup.
- Playback control.
- Pausing, resuming and seeking.
- Stopping.
- Error handling and recovery.
- Resource release and destruction.

Identify actual state variables and state machines.

Document significant states, transitions, triggering events, preconditions and transition actions.

Include state diagrams based on the implementation.

Where multiple independent state machines exist, document them separately and explain how they interact.

Identify invalid transitions or important state assumptions.

### E. Playback, control and data flow

Document the significant operational paths using appropriate sequence diagrams and flowcharts.

Potential scenarios include:

- Player selection and creation.
- Initialisation.
- Playback startup.
- Media data processing and delivery.
- Playback control operations.
- Event and callback propagation.
- Error handling.
- Shutdown and cleanup.

Select scenarios based on the actual implementation.

Show important decisions, component interactions and asynchronous boundaries.

Explain non-obvious behaviour and significant variations between success and failure paths.

Avoid merely describing every arrow in a diagram.

### F. Threading and concurrency

**Investigate this area in particular depth.**

The documentation should explain the threading model clearly enough for an engineer to safely modify the implementation.

Investigate:

- Threads and execution contexts.
- Thread ownership and creation.
- Synchronous and asynchronous operations.
- Callback execution contexts.
- Thread affinity.
- Task dispatch and event queues.
- Cross-thread communication.
- Shared mutable state.
- Locks, mutexes, atomics and condition variables.
- Synchronisation and ordering guarantees.
- Reentrancy.
- Potential races or deadlocks.
- Object lifetime during asynchronous callbacks.
- Cancellation and shutdown.
- Destruction while callbacks or work remain outstanding.

Include thread interaction diagrams or sequence diagrams showing asynchronous boundaries.

Where useful, provide a table of significant callbacks and their execution contexts.

Clearly distinguish:

- Guarantees established by Direct Rialto.
- Guarantees established by external library source or documentation.
- Behaviour inferred from implementation.
- Behaviour that cannot be verified.

Pay particular attention to threading and callback interactions across the Direct Rialto, Rialto Client/Server and Middleware Player Interface boundaries.

Do not assert thread safety without evidence.

### G. Error handling, recovery and diagnostics

Explain:

- Error detection.
- Error propagation between components.
- Error mapping and translation.
- Retry, recovery or fallback behaviour.
- Partial initialisation failure handling.
- Cleanup after failures.
- Asynchronous failure scenarios.
- Significant logging and diagnostic information.
- Unexpected or insufficiently handled failures.

Include an error scenario table.

Where useful, provide an error-handling flowchart or sequence diagram.

### H. Configuration and integration

Document:

- Relevant build options.
- Feature flags.
- Runtime configuration.
- How Direct Rialto is selected.
- External dependencies.
- Platform-specific assumptions.
- Integration with other player components.
- Behaviour when the feature is unavailable.

Only document configuration and integration behaviour that exists in the repository.

### I. Workarounds, limitations and technical debt

Create a dedicated section covering:

- Workarounds.
- Compatibility accommodations.
- Known limitations.
- Fragile assumptions.
- Potential bugs.
- Concurrency and lifetime concerns.
- Incomplete error handling.
- Unnecessary complexity.
- Opportunities to improve maintainability, reliability or performance.
- Missing or inadequate testing.

For each finding, describe:

1. The existing behaviour or implementation.
2. The relevant source location.
3. Why it may be problematic.
4. The apparent reason for the approach, if known.
5. A possible improvement or investigation.

Classify findings as:

- **Confirmed limitation** — established from available evidence.
- **Potential risk** — a credible concern requiring verification.
- **Improvement opportunity** — a possible enhancement rather than a confirmed defect.

Do not label speculative concerns as confirmed bugs.

Prioritise actionable, evidence-based findings.

## 5. Diagram requirements

Use **Mermaid as the preferred diagram format**.

GitHub supports Mermaid diagrams directly in Markdown.

Embed Mermaid diagrams within fenced `mermaid` blocks.

Suitable diagram types include:

- Architecture and component diagrams.
- Class diagrams.
- State diagrams.
- Sequence diagrams.
- Flowcharts.
- Threading and asynchronous event-flow diagrams.

Use PlantUML only where it provides a meaningful improvement over Mermaid.

For PlantUML:

- Store editable `.puml` sources in `docs/direct-rialto/diagrams/`.
- Generate SVG files in `images/direct-rialto/`.
- Reference SVG images using relative Markdown links.
- Add reproducible generation tools under `docs/direct-rialto/tools/`.
- Document the regeneration procedure.
- Prefer local rendering without external hosted services.

Avoid unnecessary rendering dependencies.

### Diagram quality

Every diagram must:

- Be based on actual implementation details.
- Use correct class and component relationships.
- Show accurate states and transitions.
- Accurately represent execution order and callback behaviour.
- Be clearly titled.
- Have a short explanation.
- Be readable on GitHub.
- Use consistent terminology.
- Avoid excessive complexity.

Prefer several focused diagrams over one enormous diagram.

**Validate both diagram syntax and semantic accuracy.**

A diagram is not correct merely because it renders successfully.

Verify important relationships, transitions and execution paths against the code, including relevant external library implementations.

## 6. Source references and evidence

Where useful, include repository-relative links to source files.

Reference important classes, interfaces and functions.

Prefer source links over copying large sections of code.

Ensure that significant technical claims can be traced to the implementation, tests or existing documentation.

In particular, verify:

- Class relationships.
- Object ownership.
- Call sequences.
- State transitions.
- Threading and callback execution.
- Error propagation.
- Cleanup and destruction.

Do not invent design rationale, guarantees or implementation details.

Explicitly identify unresolved assumptions.

## 7. Documentation style and readability

Write for experienced software engineers.

Aim for a high-quality internal engineering design document rather than automatically generated API documentation.

Follow these principles:

- Start with the big picture.
- Progressively introduce technical detail.
- Use clear headings and concise explanations.
- Prefer diagrams for complex relationships.
- Use tables for responsibilities, comparisons and states.
- Avoid unnecessary repetition.
- Avoid documenting trivial methods.
- Avoid lengthy introductions to standard C++ concepts.
- Explain why important implementation details matter.
- Highlight non-obvious behaviour and constraints.
- Link between documents rather than duplicating content.

Do not target an arbitrary page count.

The documentation should be as concise as practical while remaining sufficiently complete for engineering use.

The README should establish a useful overview in approximately 5–10 minutes.

The detailed documents should provide the depth necessary for maintenance and debugging.

## 8. GodSpell and pre-commit validation

The repository uses a pre-commit Git hook that runs GodSpell to validate spelling and terminology.

Relevant locations:

- **Git hook implementation:** `aamp_test_internal/test/github_hooks`
- **GodSpell tooling and dictionaries:** `test/tools/GodSpell/`

Investigate the hook and GodSpell configuration.

Establish:

- Which file types are checked.
- Whether Markdown documentation is included.
- Whether Mermaid or PlantUML sources are checked.
- Which dictionaries and whitelists are used.
- How terminology is validated.
- How the hook is normally executed.

Ensure newly generated documentation follows the repository's spelling and terminology requirements.

When GodSpell reports an unrecognised word, decide whether to:

1. Correct a genuine spelling error.
2. Reword the documentation using accepted terminology.
3. Add a legitimate technical term to the appropriate dictionary or whitelist.

Prefer correcting spelling or improving wording rather than unnecessarily expanding the whitelist.

However, do not change meaningful technical terminology, library names, class names or API names simply to satisfy the spell checker.

Only add entries that are legitimate and necessary.

Follow existing dictionary format and conventions.

Avoid broad whitelist entries that could conceal future spelling mistakes.

Where practical:

- Execute the appropriate GodSpell validation.
- Resolve relevant issues.
- Re-run validation after changes.
- Avoid modifying unrelated dictionary entries.

Minimal changes to the GodSpell dictionaries are permitted.

Do not modify production source code.

Do not install or modify Git hooks or developer Git configuration.

If validation cannot be executed, explain the limitation.

Include GodSpell validation results and any dictionary changes in the final report.

## 9. Investigation and execution stages

Complete the task in four stages.

### Stage 1 — Investigation and plan

Inspect the Direct Rialto implementation, existing player architecture and relevant external libraries.

Produce a concise plan describing:

- Your understanding of the feature.
- Important classes and integration points.
- Existing functionality requiring comparison.
- Proposed Markdown documents.
- Proposed diagrams and their purposes.
- Areas requiring deeper investigation.
- Any useful subagent assignments.

Keep this plan concise.

**Proceed with the task after planning rather than stopping for approval, unless a genuine blocker prevents progress.**

### Stage 2 — Documentation generation

Create the documentation and diagrams.

Investigate additional call paths where necessary.

Use Mermaid by default.

Generate PlantUML source files, SVGs and scripts only where appropriate.

Ensure consistent terminology and a coherent explanation across documents.

### Stage 3 — Validation and refinement

Review the documentation against the actual implementation.

Where appropriate, delegate a focused independent review to a subagent.

Verify:

- Architectural descriptions.
- Comparisons with existing functionality.
- Class and component relationships.
- Object ownership and lifetime.
- State transitions.
- Important call sequences.
- Threading and callback assumptions.
- External library interactions.
- Error handling and cleanup.
- Diagram correctness.
- Mermaid syntax where tooling is available.
- PlantUML generation if used.
- Markdown links and image references.
- GodSpell compliance.
- Missing important technical topics.
- Unnecessary verbosity and duplication.

Run applicable validation tools where available.

If a tool is unavailable, report that rather than claiming validation succeeded.

Refine the documentation based on findings.

Confirm production source code has not been modified.

### Stage 4 — Final report

Provide a concise summary containing:

1. Documentation files created.
2. Diagrams created.
3. Main architectural findings.
4. Important differences from existing functionality.
5. Significant workarounds, limitations and potential risks.
6. Any GodSpell dictionary changes.
7. Validation results.
8. Unresolved questions or external dependencies requiring further verification.

Do not repeat the generated documentation in the final report.

## 10. Completion criteria

The documentation is complete when an experienced engineer can understand:

- Why Direct Rialto exists.
- How it integrates into the player architecture.
- How it differs from existing playback functionality.
- The roles of its important classes.
- How components interact.
- How objects are created, owned and destroyed.
- How major playback and control operations work.
- How lifecycle and state transitions operate.
- How threading, callbacks and synchronisation work.
- How external Rialto and Middleware Player Interface components behave where relevant.
- How errors, recovery and cleanup are handled.
- How the feature is configured and selected.
- Which workarounds and limitations exist.
- Which technical risks and improvement opportunities remain.
- Where to find the relevant implementation code.

The output should be suitable for committing to the repository and maintaining alongside future implementation changes.

**Final priority: correctness, architectural clarity, useful diagrams, readability, maintainability and efficient token consumption.**

Begin by investigating the repository and proceed through the complete task.