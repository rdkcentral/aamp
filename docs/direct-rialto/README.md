# Direct Rialto documentation

This folder documents `direct-rialto/`, the AAMP `StreamSink` that plays media by calling the Rialto client API directly instead of through rialto-gstreamer sinks. It is written so that an engineer can understand the component without reading the code. Diagrams are Mermaid blocks embedded in the pages, so GitHub renders them in place.

Start with the [Overview](00-direct-rialto-overview.md).

## Pages

| # | Page | What it covers |
|---|---|---|
| 00 | [Overview](00-direct-rialto-overview.md) | What direct-rialto is, how it differs from the GStreamer Rialto path, when it is selected, and a glossary seed. |
| 01 | [Architecture and class model](01-architecture-and-class-model.md) | Layers, every class and interface, ownership and lifetime, and the seams used by tests. |
| 02 | [Lifecycle and state machine](02-player-lifecycle-and-state-machines.md) | Construction to `Stop()`, the `PlayerStateMachine`, play holds, error handling and teardown. |
| 03 | [Media data path](03-media-data-path.md) | How samples reach Rialto: the needData/haveData handshake, gating, back-pressure, codecs, encryption and EOS. |
| 04 | [Flush and seek](04-flush-seek-and-preroll.md) | The flush cycle, rapid seeks, deferred position resolution, pre-roll and trick-play. |
| 05 | [Threading model and locking](05-threading-and-locking-model.md) | Every thread, lock and atomic, the lock order, cross-thread protocols and rules for contributors. |
| 06 | [DRM](06-drm.md) | How AAMP's DRM framework creates Rialto key sessions, licence flow, key status and callback safety. |
| 07 | [Subtitles and TTML](07-subtitles-cc-and-ttml-offset.md) | Sidecar subtitles, TTML in MP4 and inband closed captions, and the TTML display-offset logic. |
| 08 | [Monitoring and diagnostics](08-av-monitoring-timers-and-position.md) | Timers, progress reporting, position calculation, the AV health monitor and key log messages. |
| 09 | [Integration, configuration and build](09-integration-configuration-and-build.md) | The `StreamSink` surface, notifications into AAMP core, configuration, CMake and runtime requirements. |
| 10 | [Testing](10-testing.md) | L1 suites, fakes and mocks, the Rialto simulator, L2 tests and coverage gaps. |
| 11 | [Workarounds and improvement register](11-workarounds-and-improvement-register.md) | Every known workaround, limitation and possible bug, with a prioritised shortlist. |
| 12 | [Troubleshooting and glossary](12-troubleshooting-and-glossary.md) | Building a timeline from logs, a symptom guide, debugging tips and the full glossary. |

## Recommended reading order

1. [Overview](00-direct-rialto-overview.md), then [Architecture and class model](01-architecture-and-class-model.md).
2. [Lifecycle and state machine](02-player-lifecycle-and-state-machines.md) and [Media data path](03-media-data-path.md) for normal playback.
3. [Flush and seek](04-flush-seek-and-preroll.md) and [Threading model and locking](05-threading-and-locking-model.md) before changing any flush, lock or timer code.
4. The remaining pages when working in those areas.

## Keeping the documentation current

- Update the page that describes a behaviour, and any affected diagram, in the same pull request as the code change.
- Reference code as `File.cpp:Symbol`, not by line number, so references survive edits.
- Edit diagrams in place. Preview them on GitHub or with the [Mermaid live editor](https://mermaid.live) before committing.
- Record new workarounds and known issues in the [register](11-workarounds-and-improvement-register.md) with the next free ID for their area. Never reuse an ID.
