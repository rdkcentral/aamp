# Copilot Prompting Guidelines for AAMP Development

This checklist helps ensure Copilot produces accurate, safe, useful, and cost-conscious outputs when working on AAMP, including DASH, HLS, buffering, ABR, DRM, manifest handling, playback state, and log analysis.

These guidelines assume Copilot usage may be metered by token consumption, model choice, context size, and agent activity. The goal is not to avoid Copilot, but to use it deliberately and efficiently.

---

## Core Principle

Treat Copilot prompts as metered engineering work.

Before asking Copilot, reduce the input to the smallest useful context:

- define the goal
- define the scope
- include only relevant files, logs, manifests, or snippets
- ask for structured output
- avoid long exploratory prompt chains
- prefer a short, focused prompt over a broad open-ended request

---

# Must-Haves

## 1. Start with a clear goal

State the task in one sentence first.

Good:

```text
Analyze why playback stalls after a bitrate switch.
```

Poor:

```text
Something is wrong with playback.
```

## Prompt Feedback Mechanism

The repository instructions include an automatic prompt feedback mechanism.

Developers should expect prompts to be assessed for quality. When the prompt is already clear, complete, and low-assumption, no feedback may be shown. When feedback is shown, it should be treated as useful guidance rather than noise.

The feedback is intended to help developers:

- reduce ambiguity
- avoid excessive context usage
- reduce unnecessary Copilot cost
- improve evidence quality
- avoid broad or speculative requests
- get better answers with fewer follow-up prompts

Feedback may appear in this compact format:

```text
Scores: Completeness X/10, Assumptions X/10, Clarity X/10 | Critique: <brief> | Improve: <specific edit>
```

Scoring is interpreted as follows:

| Score | Meaning |
|---|---|
| Completeness | Higher is better. Measures whether the prompt includes the goal, context, scope, constraints, evidence, and success criteria. |
| Assumptions  | Lower is better. Measures how much Copilot must infer or guess. |
| Clarity      | Higher is better. Measures whether the request is specific, understandable, and actionable. |

Developers should pay particular attention to feedback when:

- Completeness is below 8
- Assumptions is above 2
- Clarity is below 8
- Copilot suggests a more specific prompt wording
- the critique says the prompt is too broad, underspecified, or likely to require unnecessary context

A prompt may be scored harshly if it omits:

- target
- scope
- constraints
- success criteria
- relevant evidence
- expected output format

Very vague prompts such as:

```text
Build something.
```

or:

```text
Fix playback.
```

should be expected to receive low Completeness and Clarity scores, and a high Assumptions score.

Do not ignore this feedback. It is there to prevent common failure modes:

- Copilot guessing the intent
- unnecessary repository traversal
- excessive log or file context being loaded
- expensive iterative prompt chains
- broad refactors before root cause is understood
- answers that are difficult to validate

When feedback is shown, revise the prompt before continuing where practical.

For example, instead of continuing with:

```text
Fix playback.
```

use the suggested improvement to produce something like:

```text
Analyze why DASH playback stalls after an ABR upswitch. Focus on buffer state and ABR decision logic only. Use the provided 80-line log excerpt and the named source files. Do not modify code yet. Return root cause, evidence, minimal fix, risks, and validation steps.
```

The feedback mechanism is not intended to slow development down. It is intended to make prompts:

- cheaper
- clearer
- safer
- more deterministic
- easier to validate
- less dependent on follow-up questions

If feedback is suppressed, that normally means the current prompt is already considered sufficiently complete, clear, and low-assumption.

---

## 2. Define scope explicitly

Mention only the relevant scope.

Include:

- files, classes, or components
- stream type: DASH or HLS
- functional area: ABR, buffering, DRM, manifest parsing, subtitle handling, playback state, etc.
- whether the request is analysis-only or should include code changes

Good:

```text
Focus only on DASH ABR switching in StreamAbstractionAAMP and related buffer management. Do not inspect unrelated DRM code unless the evidence points there.
```

---

## 3. Minimise context before prompting

Do not paste large inputs by default.

Before using Copilot:

- filter logs
- isolate timestamps
- identify the relevant playback event
- include only the smallest set of files needed
- summarise known findings instead of pasting full investigations

Avoid:

- entire logs
- whole manifests
- unrelated stack traces
- multiple large source files
- repository-wide questions without a clear reason

Recommended limits:

| Input Type | Suggested Limit |
|---|---:|
| Logs | Prefer under 100 lines; avoid exceeding 300 lines |
| Source files | Prefer 1-3 files |
| Manifest content | Relevant periods, variants, renditions, or tags only |
| Chat history | Start a new chat when changing topic |
| Agent tasks | Use only with bounded scope |

---

## 4. Describe observable behaviour

Describe what happens, when it happens, and under what conditions.

Good:

```text
Playback stalls about 10 seconds after an ABR upswitch on a DASH live stream. Audio continues for several seconds, then video freezes. The issue occurs only with Widevine-enabled streams.
```

Include:

- tune, seek, trickplay, ABR switch, license renewal, discontinuity, or stall point
- live or VOD
- DASH or HLS
- DRM type if relevant
- device/platform if relevant
- whether the issue is reproducible

---

## 5. State constraints

Examples:

- Do not change public APIs.
- Preserve existing playback behaviour.
- Avoid performance regressions.
- Maintain DRM flow.
- Avoid additional network requests.
- Do not change manifest parsing outside the affected stream type.
- Prefer changes local to the identified module.
- Do not perform a broad refactor unless explicitly requested.

---

## 6. Require evidence-based reasoning

Always ask Copilot to ground its answer in evidence.

Ask it to separate:

- facts
- hypotheses
- conclusions
- unknowns

Good:

```text
Base the analysis only on the provided code and logs. Separate facts, hypotheses, and conclusions. Point to the code paths or log lines that support each conclusion.
```

---

## 7. Ask for structured output

Request clear sections.

Recommended sections:

- Summary
- Root cause
- Evidence
- Minimal fix
- Risks
- Validation steps
- Unknowns
- Follow-up improvements

---

## 8. Prefer minimal safe changes

Explicitly say:

```text
Prefer the smallest safe fix before suggesting refactors.
```

This matters for both engineering risk and Copilot cost. Broad refactors often require more context, more iterations, and more agent activity.

---

## 9. Require uncertainty to be called out

Use:

```text
State assumptions and unknowns explicitly. Do not present guesses as facts.
```

---

## 10. Avoid unnecessary prompt chains

Under usage-based pricing, repeated small follow-up prompts can become expensive because context history may be reused.

Poor pattern:

```text
Explain this.
Now explain that.
Now compare it with another approach.
Now rewrite it.
Now make it cleaner.
```

Better pattern:

```text
Analyze the issue, identify the root cause, propose the smallest safe fix, list risks, and provide validation steps in one response.
```

---

# Cost-Aware Prompting Rules

## 1. Do not use Copilot as a log search tool

Use local tools first:

- grep
- rg
- klogg
- scripts
- existing test output
- targeted log filters

Then provide Copilot with the reduced evidence.

Poor:

```text
Analyze this 20,000-line AAMP log.
```

Better:

```text
The failure occurs after an ABR upswitch at 12:04:31. Here are the 60 relevant log lines from 10 seconds before to 20 seconds after the switch.
```

---

## 2. Do not use Copilot as a repository crawler by default

Poor:

```text
Explain how playback works in AAMP.
```

Better:

```text
Explain how this specific code path handles buffer updates after a DASH fragment download. Limit the analysis to these files.
```

---

## 3. Use agent or plan mode selectively

Agent mode can be expensive because it may inspect files, generate plans, make changes, run tools, and iterate.

Use agent or plan mode when:

- changes span multiple files
- sequencing matters
- there are multiple subsystems involved
- manual coordination would be error-prone

Avoid agent mode for:

- small local fixes
- simple explanations
- single-function review
- trivial refactoring
- broad exploratory investigation

Always bound the task.

Good:

```text
Create a plan first. Do not implement yet. Limit the plan to DASH ABR handling in these files only.
```

---

## 4. Use cheaper interaction patterns first

Prefer this order:

1. inline completion
2. targeted chat question
3. focused patch request
4. plan mode
5. agent mode

Do not start with the most expensive interaction mode unless the task justifies it.

---

## 5. Start new chats for new topics

Long chats accumulate context.

Start a new chat when:

- moving to a different defect
- switching from DASH to HLS
- switching from buffering to DRM
- moving from investigation to implementation
- changing files or subsystems significantly

---

# Task-Specific Guidance

## Code Creation

Must include:

- where the code should live
- what must not change
- expected behaviour
- tests or validation steps
- performance constraints
- whether implementation is requested now or only a plan

Good structure:

```text
Design first.
List impacted files.
Propose the patch.
Describe validation.
List risks.
```

Cost-aware addition:

```text
Do not inspect unrelated files unless required. Ask before expanding scope.
```

---

## Bug Analysis and Fixing

Must include:

- symptom
- expected behaviour
- reproduction conditions
- relevant logs or code
- recent changes if known
- scope boundaries

Always ask for:

- root cause, not just a fix
- minimal fix first
- risk assessment
- validation steps

Good prompt:

```text
Analyze this playback stall. Focus on DASH ABR switching and buffer state only. Use the provided logs and code snippets. Identify the first abnormal event, root cause, smallest safe fix, risks, and validation steps. Do not suggest broad refactoring unless needed for correctness.
```

---

## Architecture or New Features

Must include:

- problem statement
- current behaviour
- desired behaviour
- performance expectations
- reliability expectations
- integration points
- constraints
- whether the request is for options only or implementation

Always ask for:

- multiple design options
- tradeoffs
- integration points
- migration risks
- validation strategy

Cost-aware addition:

```text
Provide a design proposal only. Do not inspect or rewrite code unless explicitly asked.
```

---

## Plan Agent for Complex Work

Use plan mode or agent mode when:

- changes span multiple files
- multiple subsystems are involved
- sequencing is non-trivial
- validation requires multiple steps
- rollback planning is needed

Prompt:

```text
Create a plan first. Do not implement yet.
```

The plan should include:

- impacted files
- steps
- assumptions
- risks
- validation
- rollback
- points where human confirmation is needed

Cost-aware addition:

```text
Keep the plan bounded to the named files and subsystems. Do not expand the search unless the current evidence is insufficient.
```

---

## Log Debugging for AAMP Runs

Must include:

- time window
- anchor event: tune, seek, ABR switch, license renewal, discontinuity, stall, error
- stream type: DASH or HLS
- DRM type if relevant
- environment or device
- expected behaviour
- observed behaviour

Always ask for:

- timeline reconstruction
- first abnormal event
- ranked root causes
- evidence for each root cause
- unknowns
- next log lines or instrumentation needed

Cost-aware log prompt:

```text
Analyze only the following log excerpt. Do not ask for or infer from the full log unless necessary. Reconstruct the timeline, identify the first abnormal event, rank possible root causes, and cite the evidence for each.
```

---

# Nice-to-Haves

When useful, ask for:

- multiple hypotheses
- timeline of events
- current versus proposed behaviour
- instrumentation suggestions
- two solution levels:
  - minimal fix
  - cleaner follow-up
- test cases
- rollback considerations
- performance impact
- risk to DASH, HLS, DRM, subtitles, downloads, or buffering

---

# Common Mistakes

Avoid:

- asking "fix this" with no context
- pasting excessive logs
- pasting unrelated files
- asking broad architecture questions when a focused question would work
- jumping to refactors before root cause
- ignoring constraints such as performance, DRM, timing, and public APIs
- accepting answers without evidence
- keeping one long-running chat for unrelated issues
- using agent mode for small tasks
- repeatedly refining prompts when a single structured prompt would work
- asking Copilot to rediscover information already known

---

# Before and After Prompting Patterns

## Before: Expensive and vague

```text
This playback is broken. Look through the code and fix it.
```

Problems:

- no scope
- no stream type
- no evidence
- likely to trigger broad code search
- likely to produce speculative answers
- high cost risk

## After: Focused and cost-aware

```text
Goal:
Analyze why playback stalls after a DASH ABR upswitch.

Context:
The stall occurs on a live DASH stream about 10 seconds after an upswitch. The issue is reproducible with DRM enabled.

Scope:
Focus on DASH ABR selection and buffer state handling. Use only the provided files and log excerpt unless there is clear evidence that another subsystem is involved.

Constraints:
Do not change public APIs. Preserve existing playback behaviour. Prefer the smallest safe fix.

Evidence:
Relevant log excerpt and code snippets are below.

What I want back:
- root cause
- evidence
- minimal fix
- risks
- validation steps
- assumptions and unknowns
```

---

# Default Prompt Structure

Use this template:

```text
Goal:

Context:

Scope:

Constraints:

Evidence:

Cost-control instruction:
Use only the provided context unless there is a clear reason to expand scope. Prefer a concise, evidence-based answer.

What you want back:
- Summary
- Root cause
- Evidence
- Minimal fix
- Risks
- Validation
- Assumptions and unknowns
```

---

# Default Plan-Only Prompt Structure

Use this when you want a report or plan without changes:

```text
Goal:

Context:

Scope:

Constraints:

Evidence:

Instruction:
Create a plan only. Do not modify files. Do not generate a patch yet.

What you want back:
- Findings
- Recommended approach
- Impacted files
- Risks
- Validation plan
- Open questions
```

---

# Default Log Analysis Prompt Structure

```text
Goal:
Analyze this AAMP playback issue.

Context:
Stream type:
Playback mode:
DRM:
Device/environment:
Anchor event:
Time window:

Observed behaviour:

Expected behaviour:

Evidence:
Relevant log excerpt only.

Instructions:
Reconstruct the timeline.
Identify the first abnormal event.
Rank possible root causes.
Cite evidence for each conclusion.
State assumptions and unknowns.
Do not request or analyze the full log unless this excerpt is insufficient.

What you want back:
- Timeline
- First abnormal event
- Ranked root causes
- Evidence
- Next checks
- Validation steps
```

---

# AAMP-Specific Reminder

Most AAMP issues are multi-layer problems involving some combination of:

- manifest handling: DASH or HLS
- network timing
- fragment download behaviour
- buffer state
- ABR selection
- DRM/license flow
- player state transitions
- platform integration
- eventing and telemetry

Prompt as if the issue may span layers, but do not load every layer into context by default.

Start narrow, follow the evidence, and expand scope only when justified.