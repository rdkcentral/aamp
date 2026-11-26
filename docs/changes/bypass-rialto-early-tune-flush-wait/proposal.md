## Why

The early-tune flush can spend up to 100 ms waiting for a Rialto pipeline state even though the requested seek position can be retained and applied later by the per-stream source events. Avoiding this wait can reduce flush latency and improve tune time.

## What Changes

- Pass explicit early-new-tune intent through the stream-sink and middleware flush APIs.
- For Rialto early-tune flushes, retain the requested seek position for later per-stream seeks and bypass the pipeline state wait and immediate pipeline-wide seek.
- Preserve existing flush behavior for non-early calls and non-Rialto sinks.
- **BREAKING**: The virtual `Flush` API signature changes; implementations, mocks, and consumers must be updated and rebuilt.

## Impact

- Affected specs: `specs/early-tune-flush/spec.md`
- Affected code: AAMP tune orchestration, `StreamSink`/`AAMPGstPlayer` flush interfaces, `InterfacePlayerRDK::Flush`, and related tests/fakes.
