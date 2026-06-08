# CMCD v1 Spec Completion (AAMP)

## What This Is

A brownfield enhancement to AAMP's Common Media Client Data (CMCD) support, bringing the
existing partial implementation into **full compliance with the CTA-5004 (CMCD v1)
specification**. AAMP already emits a subset of CMCD keys over custom HTTP request headers;
this work completes the standard key set and corrects serialization so AAMP's CMCD output is
spec-conformant for CDNs and analytics platforms that consume it.

## Core Value

When AAMP requests a segment or manifest, the CMCD it attaches is a complete, correctly
serialized CTA-5004 v1 payload — so any spec-compliant CDN or analytics collector can parse
every standard key AAMP sources.

## Requirements

### Validated

<!-- Inferred from existing code on branch feat/cmcd — already shipped and relied upon. -->

- ✓ CMCD collector exists (`AampCMCDCollector` + `support/aampmetrics/*CMCDHeaders`) — existing
- ✓ CMCD wired into HLS, DASH/MPD fragment collectors and the MPD downloader — existing
- ✓ Per-media-type header classes (Video/Audio/Manifest/Subtitle) — existing
- ✓ Emits subset of v1 keys over custom headers: `sid, ot, br, tb, bs, bl, nor, nrr` — existing
- ✓ Header grouping into `CMCD-Session / CMCD-Object / CMCD-Request / CMCD-Status` — existing
- ✓ Comcast-custom keys `com.comcast-dns/fb/lb` — existing
- ✓ Session UUID / traceId generation — existing
- ✓ L1 unit-test coverage for the CMCD collector (`AampCMCDCollectorTests`) — existing

### Active

<!-- This milestone's scope. Hypotheses until shipped and validated. -->

- [ ] Emit all standard v1 keys the player can source, adding the currently-missing ones:
      `cid, d, dl, mtp, su, pr, sf, st, v, rtp`
- [ ] Apply full CTA-5004 §3 serialization rules: round `br`/`tb`/`mtp` to nearest 100 kbps,
      round `bl`/`dl` to nearest 100 ms, quote `sid`/`cid`, alpha-sort keys within each header,
      correct header names (no stray `:` in the header key)
- [ ] Correctly populate the four CMCD header groups per the spec's key→header mapping
      (Object / Request / Session / Status)
- [ ] Retain Comcast-custom keys (`com.comcast-*`) alongside the standard keys
- [ ] Unit-test coverage for every newly emitted key and for the serialization rules

### Out of Scope

- CMCD **v2** (CTA-5004-A) — Response mode, Event mode, new v2 keys — explicitly descoped this milestone
- Query-argument transport (`CMCD=...`) — headers-only per decision; query mode deferred
- JSON / batch / collector-endpoint reporting — request mode only, no reporting endpoint
- Removing or changing the Comcast-custom keys — kept for backward compatibility
- A v1/v2 version toggle — moot; this milestone targets v1 only (`v=1`)

## Context

- **Codebase:** AAMP (Advanced Adaptive Media Player), RDK media engine. C++17. See
  `.planning/codebase/` for the full map (ARCHITECTURE, STACK, STRUCTURE, TESTING, CONVENTIONS).
- **Branch:** `feat/cmcd` (off `dev_sprint_25_2`).
- **CMCD entry points today:** `AampCMCDCollector::CMCDGetHeaders` builds headers via
  `CMCDHeaders::BuildCMCDCustomHeaders`, overridden per media type in
  `support/aampmetrics/{Video,Audio,Manifest,Subtitle}CMCDHeaders.cpp`. Key constants live in
  `support/aampmetrics/CMCDHeaders.h`.
- **Known issues found during scoping:**
  - Header map keys carry a stray trailing `:` (e.g. `"CMCD-Session:"`) — likely produces
    malformed header names; verify against the downstream curl header path.
  - `br`/`tb` are emitted raw (`std::to_string(bitrate)`) rather than rounded to 100 kbps.
  - `sid` is emitted unquoted; spec requires quoted string tokens for `sid`/`cid`.
  - Keys are not alpha-sorted within a header group.
- **Config:** CMCD is gated by an `eAAMPConfig_*` flag in `AampConfig`; new keys must respect
  the existing enable/disable path.
- **Source of truth:** CTA-5004 "Web Application Video Ecosystem — Common Media Client Data" (v1).

## Constraints

- **Tech stack**: C++17, GStreamer-based AAMP engine — no new languages or heavy deps.
- **Transport**: Custom HTTP request headers only (no query-arg or POST transport).
- **Compatibility**: Must not break the existing CMCD enable/disable config path or the
  per-media-type collector wiring; deployed-device behavior changes only where spec compliance
  requires it (rounding/quoting/sorting/header-name fixes).
- **Testing**: New keys and serialization rules must be covered by L1 GoogleTest unit tests,
  matching the project's existing `test/utests` conventions.
- **Spec fidelity**: Output must conform to CTA-5004 §3 serialization rules.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Target CMCD **v1** only (descope v2) | User reduced scope to a complete, correct v1 before any v2 work | — Pending |
| **Headers-only** transport | Extends the existing custom-header path; smallest correct change | — Pending |
| Emit **all** standard v1 keys | "Full spec" — add `cid, d, dl, mtp, su, pr, sf, st, v, rtp` | — Pending |
| **Full** CTA-5004 §3 serialization compliance | Spec-conformant wire output for CDNs/analytics; accept format change | — Pending |
| **Keep** Comcast-custom keys | Valid per spec's reverse-DNS custom-key rule; avoid telemetry regression | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd:complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-06-09 after initialization*
