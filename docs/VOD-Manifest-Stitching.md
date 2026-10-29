# VOD Ad Insertion via Manifest Stitching

This document describes AAMP's client-side VOD manifest stitcher (`AampVodStitcher`):
what it does, how it splices ads into DASH VOD content, the assumptions it makes
about audio, video and text tracks, its known limitations, and how applications
(including Player Platform JS) integrate with it.

The application-facing API is specified in
[AAMP-UVE-API.md, "CDAI Mechanism #3 – VOD CDAI (Manifest-Stitching)"](../AAMP-UVE-API.md).
This document focuses on behaviour and internals.

---

## 1. Purpose

Without stitching, VOD ad insertion is done by *player switching*: the application
pauses a main-content player at the ad position, plays the ad on another player and
switches back. Transitions depend on progress-position polling, so they are
non-deterministic (a little main content can play past the insertion point), and
trick play across a break means swapping player instances.

With manifest stitching, AAMP builds a single static multi-period DASH MPD before
playback starts:

```
[ad pre#0][ad pre#1][content 0 .. t1][ad mid#0][ad mid#1][content t1 .. t2] ... [ad post#0]
```

Content and ad periods are presented by one player instance. Ad transitions are
period boundaries, so they are deterministic and seamless, and seeking or trick
play across breaks works like any multi-period VOD.

---

## 2. End-to-end flow

```
App                               AAMP
 | registerVodAdBreak(...)  ----->  PrivateInstanceAAMP::RegisterVodAdBreak
 |                                    (queued until the CDAI object exists, replayed at tune)
 | setAlternateContent(...)  ---->  PrivateCDAIObjectMPD::SetAlternateContents
 | notifyReservationCompletion ->     (ad URL stored on the VodAdBreakInfo)
 | load(mainMpdUrl)  ------------>  StreamAbstractionAAMP_MPD::Init
 |                                    download main MPD
 |                                    BuildStitchedVodManifest()        <- AampVodStitcher.cpp
 |                                      wait (<= 10 s) for all breaks to resolve
 |                                      fetch all ad MPDs in parallel (<= 6 at a time)
 |                                      split content periods, splice ad periods
 |                                      record per-ad event windows
 |                                    replace MPD text with the stitched MPD, parse, play
 | <---- reservationStart / placementStart / placementProgress /
 |       placementEnd / reservationEnd   (CheckVodStitchedAdEvents on each progress tick)
```

Key code locations:

| Area | Location |
| ---- | -------- |
| JS API | `jsbindings/jsmediaplayer.cpp`, `jsbindings/jsbindings.cpp` (`registerVodAdBreak`, `cancelVodAdBreak`) |
| Registration and pre-tune queue | `PrivateInstanceAAMP::RegisterVodAdBreak` / `CancelVodAdBreak` (`priv_aamp.cpp`) |
| Break registry | `PrivateCDAIObjectMPD::mVodAdBreaks`, `mVodAdBreakOrder` (`admanager_mpd.h`) |
| Stitch invocation | `StreamAbstractionAAMP_MPD::Init` manifest path (`fragmentcollector_mpd.cpp`), guarded by `client-dai` and VOD, once per session (`mVodManifestStitched`) |
| Stitcher | `BuildStitchedVodManifest`, `TrimPeriod`, `SnapVodSpliceTime`, `ResolveVodSplicePoint` (`AampVodStitcher.cpp/.h`) |
| Ad events | `PrivateCDAIObjectMPD::CheckVodStitchedAdEvents`, `ResetVodAdEventTracker` (`admanager_mpd.cpp`), delivered via `PrivateInstanceAAMP::DeliverAdEvents` |

When `mVodManifestStitched` is set, the dynamic (non-stitched) VOD CDAI paths are
bypassed: break lookahead, EOS/postroll interception and the CDAI period state
machine.

---

## 3. Stitching algorithm

### 3.1 Inputs

- **Main MPD**: the downloaded VOD manifest, one or more `<Period>` elements.
- **Breaks**: each registered `breakId` carries one ad MPD URL and an
  `insertionPointSec` on the **source content timeline**. Breaks sharing an
  insertion point are chained in registration order.
- **Ad MPDs**: fetched in parallel. A failed fetch, or an ad MPD that fails
  validation (section 6.1), drops that ad ("play through"). If no ad remains, the
  original MPD is used unchanged.

### 3.2 Period walk

Main periods are walked in order. The cumulative period durations give each period's
position on the source timeline, and an insertion point is assigned to the period
containing it. A point exactly at the end of the last period counts as a postroll.
For each insertion point in a period:

1. Resolve the splice point (section 3.3).
2. Emit the content slice `[cursor, splice)` as a period, unless it is empty (section 3.5).
3. Emit every chained ad's periods. Ad periods are copied as-is; their
   `SegmentTimeline`s are re-based but not trimmed.
4. Record an event-tracker entry for each ad (section 5).
5. Set `cursor = splice`.

Finally the remainder `[cursor, periodEnd)` is emitted, unless it is empty.

Every emitted period gets an explicit `id` (`mcN` for content, `adN` for ads),
`start` and `duration`, plus an absolute `<BaseURL>`. Content periods use the main
MPD's directory and ad periods use their own MPD's directory. The MPD header is
copied from the main MPD with `type="static"` and the new
`mediaPresentationDuration`.

### 3.3 Splice point resolution

DASH segments cannot be split, so a splice must fall on a segment boundary.
`ResolveVodSplicePoint()` turns a requested insertion point into the actual split:

1. **Reference track.** The period's reference timeline is the first
   non-trick-play **video** `SegmentTimeline`. The video type is detected from
   `contentType`/`mimeType` anywhere in the containing `AdaptationSet`, including
   on a `Representation`. Trick-play sets
   (`http://dashif.org/guidelines/trickmode`) are skipped. If there is no video,
   the first track with a timeline is used.
2. **Time mapping.** The period-relative time maps onto the timeline as
   `presentationTimeOffset + sec × timescale`, so timelines that do not start at
   zero are handled.
3. **Snap.** If a reference boundary lies within `AAMP_VOD_SPLICE_SNAP_TOLERANCE_SEC`
   (0.1 s, in `AampDefine.h`) of the requested time, the nearest such boundary is
   used. Otherwise the splice rounds **up** to the next boundary, so the segment
   straddling the insertion point plays before the ad.
4. **Monotonic.** The splice is never moved before the content already emitted.
5. **Capped at the Period duration.** If the last segment overruns the Period's
   presentation end, the splice stays at the Period duration. This keeps the ad from
   being delayed and the presentation from being extended.

### 3.4 Per-track trimming

`TrimPeriod(body, start, end)` re-snaps both edges with the same reference rule,
so every call for a period agrees on where the split is. It then trims each
`SegmentTemplate`/`SegmentTimeline` in the period:

- Each track uses **its own** boundary nearest the common splice time (within the
  tolerance), otherwise its next boundary. Audio and video therefore split at the
  same content position, give or take sub-segment differences in their boundaries
  (e.g. 16 ms in the field asset). They are never a whole segment apart.
- Each slice's `presentationTimeOffset` is set to its first segment's `t`, and its
  `startNumber` is advanced by the number of segments dropped from the front.
- The content before and after an ad meet exactly. Every original segment is emitted
  exactly once across a period's slices: no content plays twice, and none is skipped.

### 3.5 Empty slices

If both edges of a window snap to the same boundary, the window is empty. This
happens for a splice just after the period start, a splice at or near the end of
the timeline, or two insertion points that snap to the same boundary.
`TrimPeriod()` then returns an empty body and the stitcher emits no content period
for it. Without this, a window would be padded to one segment and that segment
would appear twice.

### 3.6 Example (field asset)

Video segments are 1.92 s with an irregular pair around the break. The audio
boundary is 16 ms after the video boundary:

```
video  ... |1186.56 ---- 2.64 ---- |1189.200 - 1.2 - |1190.40 ...
audio  ... |1186.56 ---- 2.656 ----  |1189.216 -1.184-|1190.40 ...
```

A midroll registered at 1189.2 s splits video at 1189.200 and audio at 1189.216.
Content resumes after the ad at those same boundaries.

---

## 4. Timeline and positions

- The stitched MPD has a continuous 0-based timeline that includes ad time.
  `getCurrentPosition()`, progress events and ad event times are all on the
  **stitched** timeline. With a 140 s preroll, a midroll at source time 1189.2 s
  starts at stitched time 1329.275 s; the 75 ms is the difference between the ad
  MPDs' actual durations and their nominal ones.
- Ad period durations come from the ad MPDs (`Period@duration`, falling back to
  `mediaPresentationDuration` or the timeline span). They can differ slightly from
  the ad server's nominal durations, so applications should take ad start times
  from AAMP events rather than computing them.
- Content period durations use the snapped splice points, so the declared timeline
  matches the segments.

---

## 5. Ad events

While stitching, AAMP records one tracker entry per ad:

- the **reservation window**: start of the splice point and the total duration of
  all ads chained there;
- the **ad window**: this ad's start and duration.

On each progress tick, `CheckVodStitchedAdEvents(positionMs)` fires:

| Event | When | `time` field delivered to JS |
| ----- | ---- | ---------------------------- |
| `reservationStart` | position ≥ reservation start (once per `breakId`) | ms |
| `placementStart` | position ≥ ad start | seconds, truncated (see section 7.4) |
| `placementProgress` | periodically during the ad | |
| `placementEnd` | position ≥ ad end | seconds, truncated |
| `reservationEnd` | all ads of the `breakId` have ended | ms (end of the whole reservation window) |

Events are queued and delivered in order once the playhead reaches each event's
target position. After a seek or retune, `ResetVodAdEventTracker()` re-arms every
entry so events fire again as breaks are crossed.

---

## 6. Assumptions and limitations

> **Content requirements at a glance**
>
> | | Requirement | If not met |
> | --- | --- | --- |
> | Main content | Every track uses `SegmentTemplate` + `SegmentTimeline` | Tracks with other addressing are not split; after each ad they restart from the start of the period (section 6.1) |
> | Ad MPDs | Any addressing, but each ad Period's duration must be known: `Period@duration`, else `MPD@mediaPresentationDuration`, else a `SegmentTimeline` span | An ad Period of unknown duration is **silently skipped** |
> | Ad MPDs | Segment URLs resolve relative to the ad MPD's own location | An ad relying on an MPD-level `<BaseURL>` fails to load (section 6.1) |
> | All MPDs | Numeric attributes are well formed | The ad is skipped, or for the main MPD stitching is abandoned (section 6.1) |
>
> Production main content and ads meet these today. Third-party ad MPDs should be
> checked against them before they are enabled.

### 6.1 Manifest and addressing

- **DASH VOD only**, with `client-dai` enabled. Live and HLS are not stitched.
- **Main content: `SegmentTemplate` + `SegmentTimeline` only.** Main-content tracks
  using `SegmentTemplate@duration` (`$Number$` without a timeline), `SegmentBase`,
  `SegmentList` or single-file `BaseURL` addressing are copied **untrimmed** into
  every content slice. After an ad they would restart from the beginning of the
  period, so such content is not suitable for stitching.
- **Ads: any addressing, but the duration must be known.** Ad Periods are copied
  without trimming, so `SegmentTemplate@duration`, `SegmentBase` and `SegmentList`
  ads play as they are. Each ad Period's duration comes from `Period@duration`,
  else `MPD@mediaPresentationDuration` (single-Period ads), else the span of its
  `SegmentTimeline`s. An ad Period whose duration cannot be determined (e.g. a
  `SegmentBase` ad with no duration attributes) is skipped without an error.
- **Malformed MPDs.** Before stitching, the main MPD and every ad MPD are checked
  by `CanStitchVodMpd()`. It verifies that every number the stitcher parses is
  well formed: Period/MPD durations, `timescale` (non-zero),
  `presentationTimeOffset`, `startNumber`, and `SegmentTimeline` `t`/`d`/`r`.
  - A malformed **ad** MPD is skipped (played through, like a failed download).
  - A malformed **main** MPD abandons stitching, and the original MPD is played
    without stitched ads.
  - Any other exception during stitching is also caught and leads to the original
    MPD.
- **`SegmentTimeline` `r="-1"`** (repeat until the next `S` or the Period end) is
  not supported. Such an entry expands to no segments.
- **Stitching happens once, before playback**, with every ad resolved before
  `load()`. The stitcher waits up to 10 s for outstanding resolutions, which delays
  tune. Breaks cannot be added or removed after tune; `cancelVodAdBreak` only works
  before `load()`.
- **Period durations.** A period without `@duration` falls back to the MPD's
  `mediaPresentationDuration`, which is correct only for single-period content.
  `Period@start` in the main MPD is ignored; periods are assumed to be contiguous.
- **BaseURL handling.** Each emitted period gets an absolute `<BaseURL>` derived
  from its manifest URL's directory. Consequences:
  - An MPD-level `<BaseURL>` in the main MPD (e.g. pointing at another CDN path) is
    overridden for content periods.
  - An ad MPD's MPD-level `<BaseURL>` is lost, because only ad period bodies are
    copied.
  - A period that already has its own Period-level `<BaseURL>` ends up with two,
    which DASH treats as alternatives.
  - Segment URLs become absolute, so query parameters on the manifest URL (e.g.
    CDN auth tokens) are no longer propagated to segment requests when
    `propagateUriParameters` is enabled.
- **Header.** `type` and `mediaPresentationDuration` are rewritten in place. If the
  main MPD lacks either attribute, it is inserted textually and could land in the
  `<?xml?>` declaration. VOD MPDs normally carry both.
- **Period-level elements** (`EventStream`, `ContentProtection`, `SupplementalProperty`)
  are copied into every slice of their period, so period-level events may be
  repeated per slice.

### 6.2 Video

- Video drives the splice. With the 0.1 s window, an insertion point within 100 ms
  of a video boundary is split exactly at that boundary. Otherwise the split moves
  up to the next boundary, so up to one segment of extra content plays before the
  ad. For frame-accurate breaks, content should be conditioned with segment
  boundaries (IDR) at the ad positions.
- If the last video segment of a period overruns the Period's end, the overrun is
  part of the source. The stitcher caps the splice at the Period duration, but
  whether the player presents the overrun depends on AAMP's period-end handling
  (see `TrimPeriodTailSegments` in `AampMPDTailTrim.cpp`, enabled with PTS
  restamping).
- Trick-play (I-frame) tracks are trimmed like any other timeline but never drive
  the splice.

### 6.3 Audio

- Audio splits at its own boundary nearest the video splice. Audio and video
  segment boundaries rarely coincide exactly, so a few ms (up to the audio segment
  granularity) of audio may straddle the transition. This is inherent, and is
  expected to be handled by AAMP's discontinuity processing at period changes.
- If an audio track has **no** boundary within 0.1 s of the video splice, it rounds
  up to its next boundary. This is bounded by one audio segment.
- Every audio `AdaptationSet` (languages, codecs) is trimmed independently by the
  same rule.

### 6.4 Text / subtitles

- **Segmented text** (e.g. fragmented TTML/WebVTT in MP4 with a `SegmentTimeline`)
  is trimmed like audio. Text segments are often long (several seconds), and in
  that case the straddling text segment is kept before the ad. Cues timed after
  the splice may then be present in the pre-ad period (normally clipped at the
  period end), and the same cues may appear again after the ad if the post-ad slice
  starts at an earlier text boundary.
- **Side-loaded or single-file text** (one VTT/TTML file per period via `BaseURL`)
  is not trimmed. Every content slice references the whole file with the original
  period-relative timing, so subtitles after the first break would be mistimed.
  Such content needs out-of-band subtitle handling.
- **Ad MPDs** usually have no text tracks. Text selection across ad and content
  periods relies on AAMP's normal multi-period track matching.

### 6.5 Ads

- One ad MPD per `breakId`. A pod is several `breakId`s at the same insertion point.
- Ad MPDs may be multi-period; all their periods are inserted in order.
- Ad Periods are not trimmed, so their segment addressing is unrestricted, but
  their duration must be determinable (see the table at the start of section 6).
- Ad and content periods may differ in codecs, resolution or DRM. AAMP handles these
  as ordinary period transitions, including discontinuity processing and new init
  segments.
- Ad segments are fetched from the ad MPD's directory (see BaseURL handling in
  section 6.1).

---

## 7. Integration guide

### 7.1 Generic application

1. Create the player and enable client DAI: `initConfig({ "client-dai": true })`.
2. For every ad, before `load()`:
   - `registerVodAdBreak({ breakId, insertionPointSec, breakDurationSec, breakType })`,
     where `insertionPointSec` is on the **source** timeline. For a postroll, use the
     content duration.
   - `setAlternateContent({ reservationId: breakId, placementRequest: { id: adId, url } })`
   - `notifyReservationCompletion(breakId, 0)`
3. Subscribe to `reservationStart`, `placementStart`, `placementProgress`,
   `placementEnd` and `reservationEnd`, and drive ad UI and beaconing from them.
4. Call `load(mainMpdUrl)`.

Recommendations:

- Prefer insertion points on (or within 0.1 s of) video segment boundaries. Ad
  positions from an ad decision server should come from the same cue points used
  to condition the content.
- Do not compute ad start positions yourself: actual ad durations come from the ad
  MPDs. Use the times reported in AAMP events.
- Keep a fallback (player switching or no ads) for AAMP builds without
  `registerVodAdBreak`, and for content that does not meet the assumptions in
  section 6.

### 7.2 Player Platform JS (UVE VOD)

| Concern | PPJS implementation |
| ------- | ------------------- |
| Enable | Feature flag `uveVodManifestStitching` (`UveVODAdManager`); `UveAAMPPlayer` then sets `client-dai: true` |
| Capability check | `UveAAMPPlayer.supportsVodManifestStitching()`: `registerVodAdBreak` must exist on the AAMP object |
| Registration | `UveVODStitchedAdController.registerBreaks()`: for each FreeWheel temporal slot, each ad is registered as its own break `"<slotCustomId>#<index>"` at the slot's insertion point (postroll at content length), then resolved with `setAlternateContent` + `notifyReservationCompletion` |
| Ad identity | AAMP `adId` = `"<breakId>:<FreeWheel adId>"` |
| Timing | Estimated `startMs` = insertion point + cumulative prior ad duration. The AAMP-reported placement start is adopted when it differs materially (e.g. 1329.275 s vs FreeWheel 1329.2 s) |
| Rendering and beacons | `UveStitchedAdRenderer`, driven by placement events, with a position-based fallback when `placementEnd` is late or missing |

Because PPJS registers each ad of a pod as a separate `breakId` at the same point:

- `reservationStart` fires once per ad, all at the start of the pod.
- Each `reservationEnd` targets the end of the whole pod (see section 8, finding 1).

---

## 8. Known issues (outside the splice logic)

1. **Chained-ad events delivered late.** For chained `breakId`s, each
   `reservationEnd` is queued with the end of the whole reservation window as its
   delivery target. `DeliverAdEvents` delivers the queue strictly in order, so this
   event blocks the next ad's `placementStart` until an unconditional flush. In the
   field logs, placement starts for ads 2–4 of a 4-ad pod reached the application
   about 20–60 s late, which triggered PPJS's position-based fallback. Options: set each
   ad's reservation window to that ad (or fire the reservation end once per
   splice), or deliver queued events whose target has passed without blocking on
   the head of the queue.
2. **Placement event `time` unit.** Placement events carry their `position` in whole
   seconds (truncated), while reservation events and the API documentation use
   milliseconds.
3. **`getVirtualPosition()`.** It is documented in `AAMP-UVE-API.md` as a JS API
   mapping stitched positions to the source timeline, but no JS binding exists. The
   internal `GetVirtualPosition()` belongs to the non-stitched VOD path and maps
   source to virtual time.

---

## 9. Configuration

| Setting | Effect |
| ------- | ------ |
| `client-dai` (`eAAMPConfig_EnableClientDai`) | Required. Enables CDAI, and with it VOD stitching when breaks are registered |
| `AAMP_VOD_SPLICE_SNAP_TOLERANCE_SEC` (`AampDefine.h`, compile time, 0.1 s) | Snap window for splice points |
| `kMaxParallelAdFetches` (`AampVodStitcher.cpp`, 6) | Concurrent ad MPD downloads |
| Resolution wait (`BuildStitchedVodManifest`, 10 s) | Maximum tune-time wait for outstanding ad resolutions |

---

## 10. Testing and diagnostics

L1 suites (`test/utests/tests/`):

- `AampVodStitcherTests`: splice resolution and trimming. Covers boundary-aligned
  and mid-segment splices, the snap window, nearest-boundary choice,
  `presentationTimeOffset` mapping, A/V coordination, reference-track selection,
  empty slices, the Period-duration cap, every-segment-exactly-once invariants,
  and malformed-number handling.
- `VodStitchedAdEventsTests`: `CheckVodStitchedAdEvents` / `ResetVodAdEventTracker`.
- `AdManagerMPDTests` (`VodCdai_*`): registration, resolution and cancellation.

Build and run: `cd test/utests && ./run.sh`, then
`build/tests/AampVodStitcherTests/AampVodStitcherTests`.

Useful log lines:

| Log | Meaning |
| --- | ------- |
| `[VodStitcher] Inserted ad break=<id> dur=<s> at timeline=<s>` | Stitched-timeline start of each ad |
| `[VodStitcher] Stitched MPD: <n> periods, total duration=<s>` | Stitch summary |
| `[VodStitcher] Timeout waiting for all VOD ads` / `Ad fetch failed` | Ads missing from the stitch |
| `CacheFragment ... position(before restamp) <s> ... mPTSOffsetSec <s>` | Per-segment injection; the last content segment before an ad should end at the splice |
| `[CDAI-VOD] Sending AD_<event>` and `[CDAI] Delivered AdEvent[...]` | When an event was generated and when it reached the application |
