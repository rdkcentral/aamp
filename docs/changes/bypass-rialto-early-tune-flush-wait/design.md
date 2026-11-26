## Context

The AAMP tune path invokes an early stream-sink flush before stream start. `InterfacePlayerRDK::Flush` stores the requested position as pending, then waits up to 100 ms for the pipeline to reach a usable state before attempting a pipeline-wide flushing seek. The observed optimization target is Rialto; other sink behavior is outside this change.

## Goals / Non-Goals

- Goals: Avoid the readiness wait for explicitly identified early new-tune flushes on Rialto; preserve the requested position for the existing deferred source-seek path.
- Non-Goals: Change ordinary flush, app-seek, trickplay, teardown/recovery, or non-Rialto behavior; alter general seek retry/error handling.

## Decisions

- Add explicit early-new-tune intent to the flush API. Do not infer this intent from `shouldTearDown`, pipeline state, or an app-seek flag.
- Enable the fast path only when both the explicit intent and Rialto sink selection are true.
- In the fast path, keep the position pending for later source events and do not perform the state wait or immediate pipeline-wide seek.
- Preserve the existing flushing semantics when the deferred per-stream seek is applied.
- Update all virtual overrides, fakes, mocks, and callers as part of the API signature change. This requires rebuilding dependent code and may break binary compatibility.
## Risks / Trade-offs

- A pending seek is not the same operation as a completed pipeline-wide seek. Verify the `Flush` return value and `AAMPGstPlayer` success side effects when a seek is deferred.
- Skipping the pipeline-level seek relies on later source events applying the saved position; test exact position, first rendered media, and A/V synchronization.
- The observed latency is based on Rialto diagnostics. Measure on the target platform and do not generalize the expected improvement to other backends.

## Migration Plan

Update all in-repository flush implementations and call sites to provide the new intent explicitly, using false for ordinary calls. Rebuild dependent targets, run focused tests, then validate the early-tune path on Rialto. No persisted data or runtime configuration migration is required.

## Open Questions

- None. Confirm the deferred-seek return-value behavior in implementation tests before changing the existing boolean contract.
