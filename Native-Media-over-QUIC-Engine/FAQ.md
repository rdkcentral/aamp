# Native Video over MoQ POC - FAQ

**Updated:** 2026-11-19  
This FAQ records the scope and implementation rationale for the native video-over-MoQ POC. The execution plan defines sequencing and gates; `ARCHITECTURE.md` defines component boundaries.

## What are we building?

A native C++20 video-over-MoQ client for macOS and RDK. The first working path will consume the actual H.264/AAC reference publication. H.265/HEVC is the likely next codec iteration and should use the same transport, publication, player, media-pipeline, and telemetry architecture.

The client will use a selected MOQT library, interpret the catalog and CMAF media, apply bounded live-player policy, submit access units through a small Rialto-shaped Media Pipeline API, and decode/render through GStreamer.

## What is the project expected to demonstrate?

The project starts with the assumption that MoQ wins over a selected vanilla DASH path for:

- maintaining playback closer to the live edge; and
- reducing the need for post-tune live catch-up.

The question is how much improvement is realized, how consistent it is, and whether it is practically and statistically significant. The POC will also measure tune-to-first-frame, tune-to-stable-playback, end-to-end (capture-to-display) latency, player-added latency, stability, stalls, A/V behavior, and recovery.

The native POC is being developed to demonstrate MoQ playback on native platforms.

## What is the comparison baseline?

The backend and demo teams must identify the vanilla DASH publication path, player, device, and configuration. The same source should feed the MoQ and DASH paths wherever possible.

Each comparison must record the source, encoder settings, publication path, network conditions, device, decoder, player configuration, clock method, and test order. Repeated runs will report distributions, deltas, variability, confidence intervals, and material confounders.

## What reference publication exists today?

The current reference publication is described as:

> MOQT draft-22 using an MSF version 1 catalog that describes CMAF-packaged H.264/AAC media.

Known catalog values include:

- namespace `mediakind-test`;
- H.264/AVC `avc1.4d4020`, 1280x720, track `video0.m4s`;
- AAC-LC `mp4a.40.2`, 48 kHz, track `audio0.m4s`;
- video and audio in `renderGroup: 1`.

CMSF conformance, native relay connectivity, authentication, Group/Object mapping, initialization-data delivery, and producer-reference timestamps still require confirmation.

## Is the current publication CMSF?

We currently have a working MSF catalog carrying CMAF-packaged media. I provided the actual catalog produced by the publisher. If you specifically need a catalog conforming to the CMSF draft rather than the MSF catalog we’re using with Shaka, we should distinguish those before calling it CMSF.

## Why start with H.264 instead of H.265/HEVC?

The actual reference publication is H.264/AAC. Starting there removes an avoidable dependency and gives the native client a real publisher, relay, catalog, and media path from the first playback milestone.

H.265/HEVC is expected to be a bounded follow-on: codec configuration, CMAF sample handling where different, caps negotiation, and decoder validation. It should not require a transport or player redesign.

The demo codec remains an open backend question. If the planned sports publication uses H.265/HEVC, that iteration becomes part of demo readiness.

## What sports content will be used for the demo?

The current news source is expected to be replaced by sports content. The source or feed, availability date, encoder path, codec, profile, level, resolution, frame rate, bitrate, GOP structure, fragment/chunk duration, and audio format must be confirmed.

The preferred setup sends the same source through both MoQ and DASH paths so that live-edge, catch-up, tune, latency, stall, and recovery results are comparable.

## What is still unknown about the backend contract?

The main open items are:

- native raw-QUIC endpoint versus WebTransport-only availability;
- hostname, port, certificate behavior, and ALPN;
- target MOQT version for development and the demo;
- authentication and authorization requirements;
- CMSF conformance;
- Group/Object-to-CMAF mapping;
- GOP, SAP, fragment, chunk, Group, and Object behavior;
- initialization-data delivery;
- `prft` presence, clock source, and synchronization;
- demo sports source and codec;
- the vanilla DASH comparison path;
- repository ownership and CI/dependency requirements.

These items belong in a written integration contract with an owner, evidence, and confirmation date.

## Which simplifying assumptions are allowed?

Simplifying assumptions are encouraged when they accelerate the native POC and do not conflict with the actual reference publication. Initial examples are:

- one video and one audio track;
- one codec/profile at a time;
- configured namespace, catalog, and track names;
- one confirmed live-join policy;
- fixed initial queue and buffering limits;
- no DRM for the first publication;
- no ABR, alternate audio, text, trickplay, or generalized discovery.

Every assumption must be stated. If the reference publication conflicts with it, the reference contract takes precedence and the client implements the minimum compatible behavior.

## What work is strategically deferred?

Deferred work includes:

- multi-profile ABR and MoQ-aware adaptation;
- generalized multi-track support;
- DRM integration beyond preserving relevant metadata;
- alternate audio and text;
- trickplay and production controls;
- LOC or alternative packaging;
- generalized discovery and multi-relay policy;
- production UI, operations integration, and hardening;
- browser-client development and browser-platform testing.

Deferral means the initial architecture should not block the work, not that the first POC must implement it.

## Why C++ instead of Rust?

The player composition and platform layer uses C++20 because the target playback environment is RDK, GStreamer, and a Rialto-shaped media boundary. Rust may remain inside a selected MOQT implementation behind a C ABI.

Transport selection is based on actual relay compatibility, object-level API fit, RDK build viability, observability, and maintainability rather than language alone.

## Why compare libmoq and libquicr?

They provide two credible native integration paths. The comparison is intentionally bounded. Each spike must connect to the actual relay, subscribe to the H.264/AAC publication, expose the required Object data and statistics, reconnect cleanly, and cross-build for the intended RDK environment.

One implementation is selected and pinned. The losing spike is archived; permanent support for two transport engines is not a goal.

## What if neither library speaks the relay's MOQT version?

Stop at the transport gate and document the exact mismatch. The remediation must be explicit: pin or patch a client, update the relay, or expose a compatible native endpoint.

Testing only against a different local protocol version would not prove the required native path.

## Why use CMAF?

The actual reference publication is CMAF-packaged, so the native POC must consume it. The player parses CMAF above the media-backend boundary to retain visibility into initialization data, timestamps, random-access evidence, discontinuities, optional encryption metadata, and telemetry.

Alternative packaging such as LOC is deferred. It is not required to establish the native reference path or measure the expected MoQ improvements.

## Why GStreamer?

GStreamer supplies codec, clock, decoder, audio, sink, and platform integration so the POC can focus on MOQT, publication semantics, player policy, and measurement.

It remains below a small Media Pipeline API. GStreamer headers and concrete types do not appear above `src/media/gst/`.

## Why use a Rialto-shaped Media Pipeline API without the Rialto client/server runtime?

The API shape provides a familiar media-submission, lifecycle, callback, and statistics contract relevant to RDK. The POC implements only the required subset and avoids adding distributed runtime infrastructure to the critical path.

The boundary also keeps player and publication logic independent of GStreamer details.

## Why demux CMAF before the GStreamer boundary?

The boundary is access-unit oriented. Parsing CMAF above it lets the player own media timing, random-access evidence, discontinuity handling, codec configuration, optional encryption metadata, and correlation telemetry.

GStreamer owns decode and presentation mechanics, not the MOQT catalog or publication model.

## Why add a capture/replay Object source?

Capture/replay separates transport compatibility from catalog, CMAF, and decode work. Authorized Objects from the real reference publication can be replayed through the same `ReceivedObject` boundary used by the live adapter.

This allows deterministic tests, parallel work, regression fixtures, and faster diagnosis without creating a permanent plug-in system.

## Can the POC assume each MOQT Group begins with an IDR?

Only if the backend contract and reference media establish that behavior.

A useful simplifying assumption may be that Group boundaries align with random-access points, but it must be validated against catalog/publication declarations and CMAF sample/SAP flags. If the assumption is false, the player must join or drop using verified random-access evidence.

## Is `prft` required?

No for playback. It is useful for automated latency correlation when present and when its producer clock can be mapped correctly.

Camera measurement with a burned source timestamp is the required end-to-end latency ground truth. Missing `prft` removes one measurement layer but does not block playback.

## How will live-edge position and post-tune catch-up be measured?

The client records the newest known media time, the rendered media time, and the state changes between tune and stable playback.

Primary measures include:

- steady-state distance from newest known media;
- maximum and time-weighted live-edge distance after tune;
- time from first frame to stable live-edge operation;
- time spent in accelerated, skip, discard, or reacquisition behavior;
- media dropped or abandoned during acquisition;
- resulting stalls and A/V effects.

The DASH baseline must expose equivalent observations or the closest defensible measurement. Any mismatch in observability is reported.

## How will tune time, latency, stalls, and recovery be measured?

Tune telemetry includes request, connection/setup, subscription, first Object, initialization complete, first audio, first video, first rendered frame, and stable playback.

Latency uses:

1. camera plus burned source timestamp for end-to-end ground truth;
2. `prft` correlation when a valid clock mapping exists;
3. decoded timestamp probing in controlled environments where frames are accessible.

Stall and recovery telemetry includes stall start/end, cause evidence, queue state, discontinuity, resubscription or reacquisition action, first usable random-access media, and return to stable live-edge playback.

## How will significance be reported?

The report will not rely on a single run. It will use repeated comparable MoQ and DASH runs, record test order and configuration, and report:

- absolute values for each path;
- absolute and percentage deltas;
- median and relevant percentiles;
- variability and confidence intervals;
- sample count;
- practical effect size;
- material confounders or non-equivalent conditions.

The backend and demo teams should agree on any formal significance threshold before the comparison is presented externally.

## Are initial thresholds product requirements?

No. Queue caps, A/V tolerances, tune/stop repetition counts, soak duration, and buffering limits begin as engineering defaults. Each run records the actual configuration.

A default becomes a requirement only through an explicit decision based on measured behavior or demo needs.

## Why is ABR not part of initial success?

The first goal is a stable, measurable, single-profile native path against the actual reference publication. ABR adds multiple profiles, switching policy, publication relationships, and additional recovery states before the base path is proven.

ABR and MoQ-aware priority, deadline, fallback, and abandonment policy are strategically deferred until the native path and measurement baseline are stable.

## Where should the code and documents live?

Repository ownership is an open project question. Once implementation begins, the recommended engineering source of truth is a Git repository containing the code and the execution plan, architecture, FAQ, integration contract, ADRs, fixtures, and run reports.

Shared document stores may be used as a review and presentation surface, but independently editable authoritative copies should be avoided. The broader team must confirm the repository, owner, CI, security, licensing, and dependency-review requirements.

## What is the documentation hierarchy?

- **README:** project summary, scope, expected performance outcome, and current starting point.
- **Execution plan:** phases, gates, measurements, questions, risks, and actions.
- **Architecture:** components, ownership, data flow, threading, and boundaries.
- **Integration contract:** confirmed backend and publication details.
- **ADRs:** evidence behind durable technical selections.
- **Run reports:** configurations, measurements, comparisons, failures, and conclusions.
