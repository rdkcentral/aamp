# Native Video over MoQ Player Architecture

**Updated:** 2026-11-19  
**Scope:** Native-player component boundaries. Project sequencing and measurement gates belong in the execution plan; decision rationale belongs in the FAQ and ADRs.

## 1. Architectural Intent

The POC is a native, live-first MOQT client for macOS and RDK. The first playback target is the actual H.264/AAC reference publication. H.265/HEVC is the next codec iteration when required by the demonstration publication.

The architecture separates transport, publication/media interpretation, player policy, measurement, and platform playback so that no specific MOQT library, codec, GStreamer pipeline, RDK decoder, or unverified publication assumption becomes the architecture of the application.

MoQ is expected to improve live-edge position and reduce post-tune catch-up relative to the selected vanilla DASH baseline. The architecture makes those quantities, along with tune time, latency, stability, stalls, and recovery, directly observable.

## 2. System Context

```text
                       Live source
                            |
                    encoder / packager
                            |
              +-------------+-------------+
              |                           |
      CMAF-to-MoQ path              DASH baseline path
              |                           |
          MOQT relay                 DASH delivery
              |                           |
      Native MoQ player          comparison player
       macOS and RDK            agreed device/config
              |                           |
       decoder / sinks             decoder / sinks
              |                           |
           display                     display

       Shared source and measurement clocks are preferred.
```

The native project owns only the MoQ-client path and its measurement output. The DASH path is a comparison baseline whose source, player, device, configuration, and observability must be agreed with the backend and demo teams.

## 3. Development Sequence and Codec Model

The component model is codec-neutral within the bounds required by the POC, but development is sequenced deliberately:

1. consume the actual MSF catalog and CMAF-packaged H.264/AAC reference publication;
2. prove native playback and measurement on macOS;
3. port the same architecture to RDK;
4. add H.265/HEVC codec configuration and decoder validation when a representative publication is available.

H.265/HEVC must not require a transport or player redesign. If the actual HEVC publication exposes a conflicting media contract, the minimum required change is documented and implemented at the catalog/media or backend layer.

## 4. Canonical Component View

```text
+------------------------------------------------------------------+
| Application / CLI                                                |
| tune | status | stop | configuration | run orchestration         |
+------------------------------+-----------------------------------+
                               |
+------------------------------v-----------------------------------+
| Player Core                                                      |
| lifecycle | track selection | join policy | bounded queues        |
| live-edge policy | catch-up state | discontinuity recovery       |
+----------------------+----------------------+---------------------+
                       |                      |
           +-----------v---------+  +---------v----------------+
           | Catalog / Media     |  | Transport              |
           | MSF/CMSF model      |  | session/setup          |
           | CMAF init/fragments |  | subscriptions          |
           | H.264/AAC; HEVC     |  | Groups/Objects/stats   |
           | timestamps/SAP      |  | endpoint/auth adapter  |
           +-----------+---------+  +------------+------------+
                       ^                         |
                       |                 live MOQT adapter
                       |                         |
                       |        +----------------v---------------+
                       +--------| Received Object boundary       |
                                | track/group/object/payload/time |
                                +----------------+---------------+
                                                 ^
                                                 |
                                      capture / replay source

+------------------------------------------------------------------+
| Rialto-shaped Media Pipeline API                                 |
| sources | need data | add segment | flush | lifecycle | stats    |
+------------------------------+-----------------------------------+
                               |
+------------------------------v-----------------------------------+
| Lean GStreamer Backend                                           |
| appsrc | parser | hardware/software decoder | audio/video sinks  |
+------------------------------+-----------------------------------+
                               |
                    Platform decoder and display

Telemetry and clock services observe every boundary. They are sidecars,
not a layer through which media flows.
```

## 5. Ownership Boundaries

### Application

Owns command parsing, configuration, platform startup, dependency construction, run orchestration, and demo entry points. macOS main-loop shims and RDK deployment glue remain outside the player core.

### Player core

Owns:

- tune, status, and stop;
- track selection;
- start and join strategy;
- bounded queue limits and drop policy;
- live-edge state and catch-up state;
- discontinuity and recovery policy;
- stable-playback determination;
- future adaptation hooks.

The player does not parse QUIC packets and does not construct GStreamer pipelines.

### Transport

Owns:

- native endpoint connection and MOQT setup;
- certificate, authentication, and authorization adaptation as required;
- namespace and publication discovery required by the reference contract;
- subscribe and unsubscribe;
- Group/Object arrival and subscription state;
- transport/session statistics;
- adaptation of the selected library's threading, buffer, and ownership model.

Only `src/transport/` may include selected-library headers or expose its concrete types.

### Received Object boundary

A small internal model separates transport from media and creates a deterministic test seam.

```cpp
struct ObjectId {
    std::string track;
    uint64_t group;
    uint64_t object;
    std::optional<uint64_t> subgroup;
};

struct ReceivedObject {
    ObjectId id;
    uint8_t priority;
    std::shared_ptr<const ByteBuffer> payload;
    int64_t receiveMonoNs;
    int64_t receiveWallMs;
};
```

Both the live adapter and capture/replay source emit this model. It is an internal ownership and test boundary, not a public plug-in system or a commitment to multiple live transports.

### Catalog and media

Owns:

- the deployed MSF catalog and required CMSF fields;
- declared catalog version and unknown-field reporting;
- track declarations and relationships;
- CMAF initialization and fragment parsing;
- H.264, AAC, and subsequent HEVC codec configuration required by the POC;
- sample PTS, DTS, duration, random-access flags, and relevant encryption metadata;
- optional `prft` mapping;
- discontinuity detection.

The parser implements the actual reference contract first. CMSF behavior is feature-detected or confirmed by the integration contract; CMAF packaging alone does not establish CMSF conformance.

### Media Pipeline API

A deliberately small subset of the Rialto Media Pipeline API shape is owned in the project namespace. It defines source attachment, pull/need-data callbacks, access-unit submission, flush/discontinuity, playback lifecycle, and basic statistics.

The API contains no MOQT, MSF/CMSF, CMAF-container, or GStreamer concrete types.

### GStreamer backend

Owns:

- `appsrc` configuration and backpressure signaling;
- codec parser, decoder, and sink selection;
- platform caps and element properties;
- pipeline clock and A/V presentation;
- bus, error, and QoS handling;
- append, decode, and presentation observations available on the platform.

No GStreamer header or concrete type appears above `src/media/gst/`.

### Telemetry and clocks

Owns:

- schema version and run identity;
- monotonic and wallclock timestamps;
- event correlation keys;
- source-clock mapping where available;
- run configuration and summary output;
- MoQ-versus-DASH comparison inputs and derived measures outside the real-time path.

Telemetry observes component boundaries without taking ownership of transport, player, or backend state.

## 6. Data Flow

```text
relay
  |
  | MOQT Group/Object callback
  v
ReceivedObject
  | stamp receive time
  v
BoundedTrackQueue
  | order and validate; apply declared overflow policy
  v
Catalog/CMAF adapter
  | init -> MediaSource configuration
  | fragment -> access-unit MediaSegment(s)
  | sample flags -> random-access evidence
  v
Rialto-shaped Media Pipeline API
  | addSegment / haveData
  v
GStreamer appsrc
  v
parser -> decoder -> sink -> display
```

Each traced sample can carry a correlation key derived from track and media timestamp so offline analysis can join receive, demux, submit, append, decode, and presentation observations.

## 7. Threading Model

```text
MOQT library thread(s)
        |
        | copy metadata; share immutable payload; never block
        v
BoundedTrackQueue ------------------------------+
                                                 |
GStreamer streaming threads                     |
        | need-data / bus / QoS                  |
        +-------------------+                    |
                            v                    v
                    Player event loop (single owner)
                            |
                            | policy, demux orchestration,
                            | Media Pipeline API calls
                            v
                     GStreamer backend
```

Rules:

1. The player event loop owns mutable player state.
2. Transport callbacks enqueue immutable Objects and post state changes; they do not call the media backend directly.
3. GStreamer callbacks post notifications to the player loop; they do not re-enter player policy synchronously.
4. Payload lifetime is explicit. The backend copies into a `GstBuffer` during submission or wraps shared immutable storage below the API.
5. Stop and close are idempotent and drain or cancel outstanding callbacks before destruction.
6. Telemetry emission does not block transport or presentation paths.

## 8. Join, Random Access, and Discontinuity

The POC may begin with a single live-join policy derived from the reference publication. It does not assume that every Group starts with an IDR unless the backend contract and media evidence establish that behavior.

Random-access evidence is drawn from:

- confirmed catalog/publication declarations;
- CMAF sample flags and SAP behavior;
- the documented publisher contract;
- captured-reference tests.

Possible implementations include waiting for the next verified random-access Group or consuming the current Group while discarding samples until a usable random-access point. Audio may join independently when its timeline relationship is known.

A Group gap, timestamp jump, initialization change, codec change, or explicit publication discontinuity produces a player-level event. The player decides whether to flush, reacquire a random-access point, resubscribe, or stop with a diagnostic.

## 9. Buffering, Live Edge, and Backpressure

Buffering is bounded and live-oriented.

```text
transport -> bounded Object queue -> bounded backend -> sink
```

- Object queues have byte and media-duration caps per track.
- Overflow never blocks a transport callback indefinitely.
- Video is dropped only up to a verified random-access boundary, so what remains can still be decoded.
- Backend queue limits and sink latency are platform-profile values logged at startup.
- Initial values are engineering defaults to measure and tune.
- Newest-known media time, queued media time, submitted media time, and rendered media time are tracked separately.
- Catch-up state records acceleration, skip, discard, abandonment, reacquisition, and return to stable playback where applicable.
- Live-edge distance is measured separately from total end-to-end (capture-to-display) latency.

## 10. Performance Observation Model

### Tune milestones

```text
tune requested
  -> transport connected
  -> MOQT setup complete
  -> subscriptions active
  -> first Object
  -> initialization complete
  -> first audio submitted/rendered
  -> first video submitted/rendered
  -> stable playback
  -> stable live-edge operation
```

### Primary measures

- steady-state rendered distance from newest-known media;
- maximum and time-weighted live-edge distance after tune;
- post-tune catch-up duration and actions;
- tune-to-first-frame;
- tune-to-stable-playback;
- tune-to-stable-live-edge;
- end-to-end (capture-to-display) latency;
- player receive-to-present latency;
- stall count and duration;
- recovery time after a defined disruption or discontinuity;
- A/V offset, dropped media, CPU, RSS, and queue depth.

### Media timeline

- DTS derives from `tfdt` plus sample durations.
- PTS derives from DTS plus composition offsets.
- Conversion uses each track's integer timescale to avoid floating-point drift.

### Measurement layers

1. **Camera and burned source timestamp:** required end-to-end latency ground truth.
2. **CMAF `prft`:** automated mapping when present and backed by a valid producer-clock relationship.
3. **Decoded timestamp probe:** controlled diagnostic where decoded frames are accessible.

### Comparative analysis

The native player emits sufficient raw events and run configuration for repeated MoQ-versus-DASH comparisons. Offline analysis reports absolute values, deltas, distributions, confidence intervals, sample counts, effect sizes, and material confounders.

The architecture does not require the DASH player to share code with the native client. It requires a documented mapping between comparable milestones and quantities.

## 11. Platform Model

```text
                   Media Pipeline API
                           |
                 GstMediaPipeline backend
                           |
             +-------------+--------------+
             |                            |
          macOS                         RDK
   VideoToolbox/software        SoC decoder / display path
   macOS audio/video sinks      platform audio/video sinks
```

A platform profile selects candidate elements and low-latency properties. Startup validation logs installed and selected elements, caps, queue values, sink properties, clocks, and decoder mode. Platform-specific main-loop, decoder, sink, and deployment details remain below the application/media boundaries.

## 12. Assumptions and Deferred Generalization

Initial simplifying assumptions:

- one selected video track and one selected audio track;
- one codec/profile combination at a time;
- configured namespace, catalog location, and track names;
- one reference-publication join policy;
- fixed initial queue and buffering limits;
- no DRM requirement for the first reference publication;
- one relay and one selected transport implementation.

Each assumption is validated against the actual reference publication. A conflict is resolved in favor of the reference contract and recorded.

Strategically deferred generalization includes ABR, multi-track selection, DRM integration, alternate audio, text, trickplay, LOC, multi-relay policy, generalized discovery, production UI, and browser-client work.

## 13. Repository Boundaries

```text
src/app/            CLI, configuration, and platform startup
src/player/         lifecycle and live policy
src/transport/      selected MOQT library adapter
src/catalog/        MSF/CMSF catalog model
src/mediaformat/    CMAF parser and sample model
src/media/api/      Rialto-shaped interfaces
src/media/gst/      GStreamer implementation
src/common/         clock, telemetry, event loop, and result types
src/replay/         captured Object source
config/platform/    macOS and RDK profiles
tests/fixtures/     catalogs, init data, Objects, and fragments
docs/               contract, ADRs, telemetry, run reports, and demo procedure
```

Enforce with build dependencies and CI checks:

- no GStreamer includes outside `src/media/gst/`;
- no selected-MOQT-library includes outside `src/transport/` and isolated spikes;
- no platform decoder or sink decisions in player policy;
- no CMSF assertion based only on CMAF packaging;
- no production code dependency on capture/replay;
- no unbounded media queue without an explicit failure.

## 14. Deliberate Non-Architecture

The following are project decisions or sequencing rather than permanent architecture:

- libmoq versus libquicr comparison;
- the current draft-22 relay pin;
- H.264-first development order;
- the demo codec;
- the initial queue sizes, repetition counts, and latency thresholds;
- the selected DASH comparison player;
- whether alternative packaging is explored later.

Record transport selection and other durable decisions in ADRs. Keep changing integration facts in the integration contract and experimental results in run reports.
