---
description: 'Check the Direct Rialto documentation in docs/direct-rialto against the current implementation, and correct it where it is wrong, stale or incomplete.'
agent: agent
---

# Task: Verify and update the Direct Rialto documentation

## Objective

The Direct Rialto documentation in `docs/direct-rialto/` is delivered with the
code. Check that it is still **factually correct** against the current
implementation, and update it where necessary.

This is a maintenance task. Do not rewrite the documentation from scratch, and
do not change sections that are already correct.

Documents in scope:

- `docs/direct-rialto/README.md` (overview and document map)
- `architecture.md`, `comparison.md`, `lifecycle.md`, `playback-flow.md`,
  `threading.md`, `error-handling.md`, `integration.md`, `limitations.md`
- `direct-rialto/README.md` (short pointer to the documents)
- The `useDirectRialto` entries in the top-level `README.md` and
  `CONFIGURATION.md`

Sources to check against:

- `direct-rialto/` (implementation)
- The code Direct Rialto shares or replaces: `AAMPGstPlayer`, `StreamSink`,
  `AampStreamSinkManager`, `PrivateInstanceAAMP`, `StreamAbstractionAAMP`
- `.libs/rialto` (Rialto client and server)
- `.libs/rialto-gstreamer` (the Rialto GStreamer plugin)
- `.libs/middleware-player-interface`
- L1 tests under `test/utests/tests/` and the simulator under `test/rialto/`

## 1. Scope the work first

Optional input: a git ref, a commit range, or a list of files or topics that
have changed. If none is given, check everything.

To focus the effort, list what changed since the documentation was last
updated:

```bash
git log --format='%h %s' -- docs/direct-rialto/
git log --format='%h %s' <last-doc-commit>..HEAD -- direct-rialto/ test/rialto/ \
    test/utests/tests/ scripts/
git diff --stat <last-doc-commit>..HEAD -- direct-rialto/
```

Changed source drives the first pass. Then do a lighter sweep of the
unchanged areas, because errors can also come from external code
(`.libs/...`) changing, or from claims that were never correct.

## 2. What to verify

Work through the documents and test each *claim*, not each sentence. Read the
code that the claim is about. Do not rely on class names, comments or the
existing documentation as evidence.

### Structure and names

- Every class, function, file and member named in the documents still exists
  and has the described role.
- Every source link resolves. Check the relative path and that the symbol is
  still in that file.
- New or removed files in `direct-rialto/` are reflected in the source map and
  the class tables.

### Behaviour

- **Class relationships and ownership** (composition, who creates and
  destroys what), against the class diagrams.
- **State machines and gates**: states, transitions, triggers and actions,
  including the player state machine, `GateMode` and the `needData` request
  lifecycle.
- **Call sequences** in the sequence diagrams, including the order of the
  Rialto calls and which of them happen at startup.
- **Threading**: which thread runs each callback, which locks and atomics
  protect which state, and what is guaranteed on shutdown. Check Rialto's own
  source for the callback context, not only the AAMP side.
- **Error propagation**: what is detected, what is mapped, what reaches AAMP,
  and what is only logged.
- **Cleanup and teardown** paths.
- **Configuration and build**: option names, defaults, how Direct Rialto is
  selected, and the build scripts (`scripts/install_options.sh`,
  `install_rialto.sh`, `install_aampcli.sh`).
- **Tests**: the L1 suites, the L2 tests that exercise Direct Rialto, and the
  simulator. Counts are approximate and say so. Re-check them if tests moved.
- **The comparison with the existing path.** For the GStreamer route, read
  the `rialto-gstreamer` plugin source to confirm who makes each Rialto call
  and when. State which part of the GStreamer-side behaviour is inferred.

### Limitations and ideas

- Every item in `limitations.md` still holds. If a source change has fixed a
  limitation, update or remove the entry and say which change fixed it. Do not
  leave a fixed issue listed.
- Check for new limitations or risks introduced by changes since the last
  update. Add them only with evidence and the correct classification
  (confirmed limitation, potential risk, improvement opportunity).

### Diagrams

A diagram that renders is not necessarily right. Check that every arrow,
participant, state and transition matches the code. Look for diagrams that
imply a thread, ordering or ownership the code does not have.

## 3. Claims that need particular care

These were wrong or overstated in earlier drafts. Keep them accurate.

- **Server-side decryption is shared.** With the Rialto GStreamer sinks the
  key information is also passed to the server, which decrypts. It is not a
  Direct Rialto advantage. What differs is how AAMP reaches the key system:
  `DrmSessionManager` is shared, but Direct Rialto replaces the OCDM session
  with one built on Rialto `IMediaKeys`.
- **Source-set and codec change limits come from the Rialto server.** They
  apply to both paths. Do not present them as a Direct Rialto disadvantage.
- **Performance claims.** Memory savings have been demonstrated in project
  testing. Faster tune times were seen only in early development and must be
  described as needing confirmation, measured to video on screen and not to a
  first-frame notification. Do not add timings or figures unless they come
  from a source the reader can check.
- **Concurrency.** Describe the move of responsibilities into AAMP as a
  trade-off, not as better or worse.
- **Evidence convention.** Keep the *confirmed*, *inferred* and *unverified*
  labels. Do not upgrade an *inferred* or *unverified* claim without finding
  the evidence. Do not delete a caveat unless the evidence now supports the
  claim. Do not invent design rationale.
- **Attach timing.** Attaching on the init segment is DASH behaviour. HLS-TS
  attaches every source on the first `SendCopy`.

## 4. How to make changes

- Make the smallest edit that makes the text correct. Preserve the existing
  structure, headings, anchors and wording where they are right.
- Keep the document map order in `docs/direct-rialto/README.md` as the reading
  order. If a document is added, removed or moved, update the document map and
  the previous and next links at the bottom of every page so that the chain
  still matches the table.
- Keep terminology consistent across documents. Link to the document that owns
  a topic rather than repeating it.
- Use Mermaid for diagrams, in fenced `mermaid` blocks, with a title and a
  short explanation. Use PlantUML only where Mermaid cannot express the diagram.
- Use repository-relative links that work on GitHub.
- New files need the RDK copyright header. Use the current year.
- Do not modify production source code, and do not install or change Git hooks
  or Git configuration.
- If you are unsure whether something is wrong, do not guess. Record it as an
  open question in your report, and in the document as *unverified* if that is
  what it is.
- Do not commit unless asked.

## 5. Validation

Run what is available and say plainly what was not run.

1. **Links and anchors.** Every relative link and `#anchor` in the documents
   resolves.
2. **Mermaid syntax.** Every diagram renders, using a local Mermaid tool if one
   can be set up without changing the repository. Then check the meaning, as
   in section 2.
3. **GodSpell.** The pre-commit hook runs GodSpell on staged files.
   - The hook is `aamp_test_internal/test/github_hooks/pre-commit`. The tool and
     dictionaries are under `aamp_test_internal/test/tools`.
   - Run it against `docs/direct-rialto` and any other changed Markdown, using
     the `develop` dictionaries.
   - Fix a flagged word by correcting or rewording it. Add a word to a
     dictionary only if it is a legitimate term. Genuine English words go in
     `words_alpha.txt`, and tool or code tokens in `extra.txt`. Never add Jira
     keys or ticket prefixes. Remove the reference instead.
4. **Forbidden patterns.** The hook also rejects words listed in
   `aamp_test_internal/test/github_hooks/forbidden_patterns.txt`, including
   some vendor and device names. If one is hit, reword it. This can also be
   triggered by Mermaid node identifiers and file names.
5. **Production source unchanged.** `git status` shows only documentation files
   changed.

## 6. Report

Finish with a concise report:

1. **Verified against**: the commit hash of the repository, and the state of
   `.libs/rialto`, `.libs/rialto-gstreamer` and
   `.libs/middleware-player-interface` if known.
2. **Findings** that needed a correction. For each: the document and section,
   the source location that shows the truth, the severity (factual error,
   misleading or overstated, missing behaviour, stale), and what you changed.
3. **Open questions** you could not resolve, and why.
4. **Parts checked and found correct**, in a line or two, so the reader knows
   the coverage.
5. **Validation results**: links, Mermaid, GodSpell, forbidden patterns, and
   anything that could not be run.
6. **Files changed.**

Do not list findings you could not support with source evidence.

### Optional: independent review

For a large change, a focused review by a separate agent is worth doing after
the edits. Give it the changed documents and the corresponding source, and ask
it to report only evidence-backed errors. Weigh each finding against the source
yourself before changing anything.
