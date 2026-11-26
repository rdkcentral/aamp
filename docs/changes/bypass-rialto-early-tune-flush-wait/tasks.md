## 1. Propagate Early-Tune Intent
- [x] 1.1 Add an explicit early-new-tune parameter to the flush interfaces and update implementations, mocks, fakes, and call sites; set it only for the early flush of a new tune.
- [x] 1.2 Add focused coverage that verifies early-tune intent is forwarded and ordinary flush callers remain unmarked.

## 2. Defer Rialto Early-Tune Seeks
- [x] 2.1 In the Rialto early-tune path, retain the requested position as a pending per-stream seek and return before the pipeline state wait and pipeline-wide seek.
- [x] 2.2 Verify the later source-event path applies the retained position with the existing flushing seek behavior.
- [x] 2.3 Verify ordinary Rialto flushes and all non-Rialto flushes retain their current wait and seek behavior.

## 3. Validate Tune Behavior
- [ ] 3.1 Run the focused AAMP and middleware flush tests, including non-early tune, app seek, trickplay, teardown, and non-Rialto cases.
- [ ] 3.2 On a Rialto target, compare flush latency and `configure_to_first_frame`; verify the requested playback position, first rendered media, A/V sync, and rapid-retune behavior.
