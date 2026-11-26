## ADDED Requirements

### Requirement: Defer Rialto early-tune flush seeks
The system SHALL identify an early flush for a new tune explicitly. When that flush uses a Rialto sink, the system SHALL retain the requested seek position for the later per-stream source seek without waiting for pipeline readiness or issuing an immediate pipeline-wide seek. The deferred per-stream seek SHALL preserve the existing flushing behavior.

#### Scenario: Rialto early flush for a new tune
- **WHEN** AAMP performs an early stream-sink flush for a new tune and the active sink is Rialto
- **THEN** the middleware retains the requested seek position as pending and returns without the pipeline state wait or immediate pipeline-wide seek
- **AND** later per-stream source events apply the retained position using the existing flushing seek behavior

### Requirement: Preserve other flush behavior
The system SHALL preserve existing flush behavior for calls that are not explicitly marked as an early new-tune flush and for sinks other than Rialto.

#### Scenario: Ordinary flush on Rialto
- **WHEN** a flush uses a Rialto sink but is not marked as an early flush for a new tune
- **THEN** the middleware performs the existing pipeline readiness wait and pipeline-level seek behavior

#### Scenario: Early-tune flush on a non-Rialto sink
- **WHEN** AAMP performs an early flush for a new tune but the active sink is not Rialto
- **THEN** the middleware performs the existing non-Rialto flush behavior

