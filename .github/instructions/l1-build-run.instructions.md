---
description: L1 test build and run workflow — mandatory steps
applyTo:
  - "test/utests/**"
---

# L1 Test Build & Run Workflow

**This file defines the only approved build/run workflow for L1 tests.
Do not invent alternate commands, paths, or shortcuts.**

---

## New L1 Tests — Mandatory First Build

When L1 test files are **newly created** (new directory under `test/utests/tests/`),
the master script must run first to integrate them with cmake:

```bash
cd test/utests && ./run.sh
```

**Why:** `run.sh` discovers new test directories, generates makefiles, creates
the corresponding `test/utests/build/tests/[ComponentName]Tests/` directory,
and builds all tests. Without this step, cmake has no knowledge of the new
test target.

**Do not skip this step. Do not substitute it with a manual cmake invocation.**

---

## Iterative Development — After Initial Integration

Once `run.sh` has succeeded at least once for a given test suite, iterate
directly in the build directory:

```bash
cd test/utests/build/tests/[ComponentName]Tests
make
./[ComponentName]Tests
```

This is faster than a full `run.sh` and is the approved inner-loop workflow.

---

## Common Build Failures

| Error | Cause | Fix |
|---|---|---|
| `No rule to make target '[ComponentName]Tests'` | `run.sh` was never run after creating the test directory | Run `cd test/utests && ./run.sh` |
| `Makefile not found` | Working in the source directory instead of the build directory | Navigate to `test/utests/build/tests/[ComponentName]Tests/` |
| `Target not found` | Directory or file naming does not match convention | Verify the directory is `[ComponentName]Tests/` and files match exactly |
| `Undefined reference` errors | Fakes not linked, or real dependency accidentally included | Ensure `target_link_libraries` lists `fakes` **first**; remove real `.cpp` sources from `AAMP_SOURCES` if a fake provides them |
| `Multiple definition` errors | Both real and fake implementations compiled in | Remove the real source file from `CMakeLists.txt`; the `fakes` library already provides it |
| Unexplained build errors after `git rebase` or `git pull` | Stale cmake cache or generated files reference old paths/targets | Re-run `cd test/utests && ./run.sh` to regenerate the build tree (see recovery section below) |

---

## Recovery After Rebase

After a `git rebase`, `git merge`, or large `git pull`, the cmake cache
and generated build files may reference stale paths, renamed sources, or
removed targets. Symptoms include unexpected compile errors, missing
targets, or link failures that did not exist before the rebase.

**Recovery step:**

```bash
cd test/utests && ./run.sh
```

This regenerates the cmake build tree and rebuilds all tests from a clean
state. After `run.sh` succeeds, resume the normal iterative workflow.

**When to suspect stale artifacts:**
- The build was working before the rebase and now fails.
- The errors do not match any source-level change you made.
- The iterative `make` command fails but the error is not in your test code.

---

## Rules

- Do not invent cmake commands, build paths, or wrapper scripts that are not documented here.
- Do not modify `test/utests/CMakeLists.txt` or `test/utests/run.sh` without justification.
- Do not delete `test/utests/build/` — it is the official build tree.
- Clean up temporary debug files (`debug_*.cpp`, `temp_*.h`) before committing.
  Keep only the official deliverables: test runner, test cases, `CMakeLists.txt`.
