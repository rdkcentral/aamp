---
description: TDD and SOLID rules for all code in the direct-rialto/ directory
applyTo:
  - "direct-rialto/**"
  - "test/utests/tests/AampRialto*/**"
---

# Direct-Rialto Component Instructions

## Response Checklist — Complete Before Sending Any Reply

Before sending a response that includes changes to `direct-rialto/` code, verify
every item below:

- [ ] TDD cycle followed: Red test written before production code was changed.
- [ ] All affected test binaries built and run with zero failures.
- [ ] A ready-to-use `git commit message` is included in the response
      (see **Commit Messages** section below for the required format).

---

These rules apply to **every file under `direct-rialto/`** — including existing
classes (`AampRialtoPlayer`, `AampRialtoMediaPipelineClient`, `PlayerStateMachine`,
`SourceWorker`) and any new class added to that directory in the future — and to
their companion test directories.
They intentionally do **not** extend to the rest of AAMP to avoid merge conflicts
and the risk of breaking unrelated code.

---

## TDD Workflow — Mandatory for All Changes

Every change to `direct-rialto/` must follow the **Red → Green → Refactor** cycle:

1. **Red** — Write a failing test that specifies the new behavior before touching
   production code.  The test must compile and fail for the right reason.
2. **Green** — Write the minimum production code needed to make the test pass.
   Do not add anything not covered by a test.
3. **Refactor** — Clean up duplication, naming, and structure while keeping all
   tests green.

### Rules
- Never modify `direct-rialto/` production code without a corresponding test.
- Tests live under `test/utests/tests/` in a directory named after the class
  under test (e.g. `AampRialtoPlayerTests/`, `AampRialtoMediaPipelineClientTests/`).
  New classes added to `direct-rialto/` must have a corresponding test directory.
- Always read `.github/instructions/testing.instructions.md` before writing or
  modifying any test.
- Verify by building and running the affected test binary after every change, 
  for example:
  ```
  cd test/utests/build/tests/AampRialtoPlayerTests && make && ./AampRialtoPlayerTests
  ```
- The TDD phase table in `docs/rialto-integration/aamp-rialto-player-analysis.md`
  is the authoritative backlog.  Consult it before starting any new work.

---

## SOLID Principles — Applied to New Code in `direct-rialto/`

Apply all five SOLID principles when writing new classes or interfaces inside
`direct-rialto/`.  These principles **must not** be used as justification to
refactor existing AAMP code outside this directory.

| Principle | How to apply it here |
|-----------|----------------------|
| **SRP** | `AampRialtoPlayer` orchestrates; injection workers, the pipeline client, and the demuxers each own one concern. Extract a new class rather than growing an existing one. |
| **OCP** | Add behavior by introducing new types (e.g. a per-source `SourceWorker`) rather than adding `if/switch` branches to existing classes. |
| **LSP** | Any class that replaces `IMediaPipeline` or `IMediaPipelineClient` in tests must be a drop-in substitute with no behavioral surprises. |
| **ISP** | Keep callback interfaces (`NeedDataCallback`, `PlaybackStateCallback`) small and single-purpose.  Do not add unrelated methods to `AampRialtoMediaPipelineClient`. |
| **DIP** | Depend on `firebolt::rialto::IMediaPipeline` and `IMediaPipelineFactory`, never on concrete implementations.  New internal types must be injected via constructor or setter, not created with `new` inside business logic. |

---

## Scope Boundary — Do Not Refactor the Rest of AAMP

> **Hard rule:** Changes originating in `direct-rialto/` must not cascade into
> files outside `direct-rialto/` or its companion test directories under
> `test/utests/tests/AampRialto*/`.

Specifically:
- Do **not** modify `StreamSink.h`, `priv_aamp.h`, `AampConfig.*`, or any other
  shared AAMP header to accommodate a `direct-rialto/` change.
- Do **not** refactor `aampgstplayer.*` or other `StreamSink` implementations.
- If a change requires touching shared infrastructure, raise it as a separate,
  explicit ask — it must be reviewed and agreed independently.

This boundary exists to keep the direct-Rialto work mergeable without risk to
the rest of the AAMP pipeline.

---

## Coding Style — Mandatory for All Code in `direct-rialto/`

These rules apply to every `.cpp` and `.h` file under `direct-rialto/`, regardless
of whether the code is new or modified.

### File naming
Every source file under `direct-rialto/` must:
- Be prefixed with `Aamp` (no underscore separator).
- Use **CamelCase** for the remainder of the name.
- Contain **no underscore characters** anywhere in the file name.

```
// BAD
player_state_machine.cpp
PlayerStateMachine.h

// GOOD
AampPlayerStateMachine.cpp
AampPlayerStateMachine.h
```

### Single exit point per function
Every function must have **at most one `return` statement**.  This makes control
flow linear, eliminates hidden exit paths, and prevents resource-management bugs
when locally acquired resources must be released before exiting.

- **Avoid:** early `return` / `return <value>` guards deep inside a function.
- **Prefer:** a single result variable declared at the top, modified via `if/else`
  branches, and returned at the very end.
- Lambdas follow the same rule.

```cpp
// BAD
bool AampRialtoPlayer::DoSomething()
{
    if (!m_pipeline)
        return false;       // early return
    bool ok = m_pipeline->doSomething();
    return ok;
}

// GOOD
bool AampRialtoPlayer::DoSomething()
{
    bool ok = false;
    if (!m_pipeline)
    {
        AAMPLOG_WARN("pipeline is null");
    }
    else
    {
        ok = m_pipeline->doSomething();
    }
    return ok;
}
```

### Braces required for all blocks
**Every** `if`, `else`, `for`, `while`, and `do` body must be enclosed in `{}`,
even when it contains only a single statement.  This prevents accidental scope
errors when lines are added later.

```cpp
// BAD
if (m_worker) m_worker->flush();
for (auto &s : samples) queue.push_back(s);

// GOOD
if (m_worker)
{
    m_worker->flush();
}
for (auto &s : samples)
{
    queue.push_back(s);
}
```

---

## Logging — No Silent Failures

Every code path that silently skips expected work **must** emit a log message at
the appropriate level.  The rule of thumb: if a future developer would be
surprised that nothing happened, it must be logged.

### Log level selection

| Level | When to use |
|-------|-------------|
| `AAMPLOG_ERR` | A required precondition is absent and the feature **will not work** as a result. Playback failure, data loss, or a hung stream is the expected outcome. Examples: `drmBridge` is null when protection params are present; `pipeline` is null when EOS must be signalled. |
| `AAMPLOG_WARN` | The path is skipped and functionality is **degraded** but the player may continue. Examples: an encrypted sample is injected without a DRM session; a Rialto error notification arrives but is not forwarded to the player. |
| `AAMPLOG_INFO` | The path is legitimately skipped and both branches are expected, but the skip is worth recording for log analysis. Examples: a `SendTransfer` call arrives for a track that has no source yet (e.g. subtitle before Configure). |
| `AAMPLOG_TRACE` | High-frequency paths where logging both outcomes would create log noise in normal operation. |

### Mandatory logging points

The following patterns **must always** have a log:

1. **Null pointer guard that abandons work** — any `if (!ptr) return;` or
   equivalent where `ptr` being null is not the routine case.
2. **Feature buffer / deferred storage** — when data (e.g. protection params,
   codec data) is accepted but cannot be applied immediately because a dependent
   object does not exist yet, log at `AAMPLOG_INFO` or `AAMPLOG_WARN` so it is
   clear the data was received and where it went.
3. **Rialto API call failures** — every `pipeline->foo()` that returns `bool` or
   a status enum must check the result and log at `AAMPLOG_ERR` or
   `AAMPLOG_WARN` on failure.
4. **Unimplemented / stub callbacks** — pipeline-client callbacks that are not
   yet forwarded to the player (e.g. `notifyPlaybackError`) must log at
   `AAMPLOG_WARN` so failures are not silently swallowed.

### What not to log

- Do **not** add `AAMPLOG_INFO` at every entry/exit of trivial accessors or
  pure-virtual stubs — this creates noise without analytical value.
- Do **not** duplicate information already present in an immediately preceding
  log line at a higher level.

---

## Commit Messages — Always Provide After a Summary of Changes

Whenever you provide a summary of changes made to `direct-rialto/` code, you
**must** also supply a ready-to-use `git commit message` in the following format:

```
<Imperative subject line, ≤ 72 characters>

<Body: one paragraph explaining *why* the change was made and what
approach was taken.  Omit obvious restatements of the diff.>

Files changed:
- <path> — <one-line reason>
- ...
```

Rules:
- Subject line must be imperative mood ("Add", "Fix", "Refactor", not "Added").
- Body must explain motivation, not just mechanics.
- List every file touched, with a short reason for each.
- The message must be usable verbatim with `git commit -m` or in a PR title/body.

---

## Architecture Guidance

The target architecture is described in
`docs/rialto-integration/aamp-rialto-player-analysis.md` (Improvement Plan,
Steps 2–3 and TDD Phase 13).  Key design intentions:

- **Phase 13 (complete):** Per-source `SourceWorker` threads replace the single
  `m_injectionThread`; a GoF State-pattern `PlayerStateMachine` replaces
  scattered booleans.  All 81 L1 tests pass.
- Model the injection architecture on `rialto-gstreamer`'s `BufferPuller +
  MessageQueue` pattern.
- Prefer `std::thread` + `std::condition_variable` over platform-specific
  primitives.
- Any new class added to `direct-rialto/` must follow the same rules: TDD cycle,
  SOLID principles, and a dedicated test directory.
