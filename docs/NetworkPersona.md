# Network Persona — Simulated Latency Injection for Real AAMP Playback

**Component:** `AampNetworkPersona` (`AampNetworkPersona.h` / `AampNetworkPersona.cpp`)  
**Config key:** `networkPersonaFile`

---

## Overview

Network persona injection lets you exercise AAMP's ABR heuristics under
controlled, repeatable network conditions **without requiring a real congested
network**.  The same JSON persona format used by standalone abrsim tool
simulator is accepted here, so you can validate that the adaptation behaviour
observed in the simulator matches what real AAMP does on the same persona.

AAMP wraps each `curl_easy_perform` (and the equivalent call in the legacy
`priv_aamp.cpp` download path) with two padding sleeps:

1. **TTFB sleep** — before the download starts, sleep for a sampled
   time-to-first-byte (base RTT + jitter + occasional spike + new-connection
   TCP penalty).
2. **Transfer-time sleep** — after curl finishes, sleep for any remaining time
   needed so that total wall-clock time (TTFB + curl + idle) matches the
   persona's predicted transfer duration for the downloaded body size.

Because all real timing is observed by AAMP's bandwidth estimator, the
ABR algorithm sees realistic throughput samples and adapts exactly as it
would on a genuinely slow network.

**This feature is test-only.**  The singleton's `IsLoaded()` check is a
single lock-free atomic load; when no persona is configured the overhead is
in the single-digit nanosecond range and the download path is otherwise
unmodified.

---

## Quick Start

### 1. Set the config key at runtime

Using the AAMP CLI or any config path:

```json
{
  "networkPersonaFile": "/path/to/persona.json"
}
```

Or via the UVE API from JavaScript:

```javascript
player.setAampCfg({ networkPersonaFile: "/path/to/persona.json" });
```

### 2. Point it at a persona file

If both `aamp` and `aamp_test_internal` are cloned side-by-side (the typical
developer layout), the canned personas from `aamp_test_internal` are
immediately usable:

```
  aamp/                          ← this repo
  aamp_test_internal/
    test/tools/abrsim/personas/
      wifi_good.json
      wifi_congested.json
      XiOne_CableWifi_SingleDevice.json
      XiOne_CableWifi_TwoDevices.json
      XiOne_CableWifi_3Devices_Congested.json
      XiOne_CableWifi_4Devices_Congested.json
```

Example — simulate a congested WiFi environment:

```json
{
  "networkPersonaFile": "../aamp_test_internal/test/tools/abrsim/personas/wifi_congested.json"
}
```

Or use an absolute path:

```json
{
  "networkPersonaFile": "/home/user/aamp_test_internal/test/tools/abrsim/personas/mobile_3g.json"
}
```
	
---

## Persona File Format

A persona is a JSON object describing a statistical network model.  All fields
are optional; defaults model a reasonable broadband connection.

```jsonc
{
  // Base round-trip time in milliseconds
  "base_rtt_ms": 85.0,

  // Standard deviation of RTT jitter (Gaussian)
  "rtt_jitter_ms": 20.0,

  // Probability that a given request suffers a TTFB spike (server hiccup)
  "ttfb_spike_p": 0.05,

  // Extra milliseconds added when a TTFB spike occurs
  "ttfb_spike_ms": 200.0,

  // Mean throughput in Mbps (lognormal distribution)
  "mean_thr_mbps": 20.0,

  // Log-standard-deviation of throughput (higher = more variable)
  "thr_sigma_ln": 0.40,

  // Number of TCP burst/flush events per segment download
  "bursts_per_segment": 8,

  // Std-dev of per-burst jitter in milliseconds
  "flush_jitter_ms": 6.0,

  // Probability of a packet-loss / retransmit stall event per segment
  "late_chunk_p": 0.01,

  // Extra milliseconds added when a stall occurs
  "late_chunk_extra_ms": 120.0,

  // Probability that curl reuses an existing TCP connection (vs new connection)
  "p_conn_reuse": 0.95,

  // Extra milliseconds for TCP handshake + DNS when a new connection is opened
  "new_conn_penalty_ms": 170.0
}
```

---

## Multi-Persona (Sequence) Format

To simulate a changing network over time — e.g. good → congested → good — use
a **sequence file**: a JSON array where each entry adds a `duration_s` field
specifying how long that persona is active.  The last entry runs until the
session ends.

```json
[
  {
    "description": "Good WiFi for first 60 s",
    "duration_s": 60,
    "base_rtt_ms": 30.0,
    "rtt_jitter_ms": 5.0,
    "mean_thr_mbps": 50.0,
    "thr_sigma_ln": 0.20,
    "bursts_per_segment": 8,
    "flush_jitter_ms": 3.0,
    "late_chunk_p": 0.002,
    "late_chunk_extra_ms": 80.0,
    "p_conn_reuse": 0.97,
    "new_conn_penalty_ms": 120.0
  },
  {
    "description": "Congested for next 120 s",
    "duration_s": 120,
    "base_rtt_ms": 180.0,
    "rtt_jitter_ms": 60.0,
    "mean_thr_mbps": 5.0,
    "thr_sigma_ln": 0.80,
    "bursts_per_segment": 10,
    "flush_jitter_ms": 25.0,
    "late_chunk_p": 0.10,
    "late_chunk_extra_ms": 400.0,
    "p_conn_reuse": 0.80,
    "new_conn_penalty_ms": 300.0
  },
  {
    "description": "Recovery — runs to end of session",
    "duration_s": 0,
    "base_rtt_ms": 40.0,
    "rtt_jitter_ms": 8.0,
    "mean_thr_mbps": 35.0,
    "thr_sigma_ln": 0.25,
    "bursts_per_segment": 8,
    "flush_jitter_ms": 4.0,
    "late_chunk_p": 0.005,
    "late_chunk_extra_ms": 100.0,
    "p_conn_reuse": 0.95,
    "new_conn_penalty_ms": 140.0
  }
]
```

The sequence clock starts on the **first download** after the persona is
loaded.  Setting `duration_s: 0` (or omitting it) on the last entry means
"run indefinitely".

> **Tip:** The abrsim tool's scenario builder in the Web UI generates
> multi-persona sequences in this exact format.  You can copy a scenario JSON
> from the abrsim tool and use it directly with `networkPersonaFile`.

---

## Comparing abrsim vs Real AAMP on the Same Persona

Run abrsim with a persona, note the average bitrate profile and rebuffer
count, then run real AAMP with the same persona file.  Differences between
the two reveal discrepancies between the simulator model and AAMP's actual
behaviour.

```bash
# abrsim — standalone simulation
cd aamp_test_internal/test/tools/abrsim
./abrsim --persona personas/wifi_congested.json --live --target-latency 6 \
         --duration 600 --out /tmp/sim_result.csv

# Real AAMP — set networkPersonaFile before tuning
#  e.g. via aamp.cfg:
echo '{"networkPersonaFile":"../aamp_test_internal/test/tools/abrsim/personas/wifi_congested.json"}' \
  > ~/.aamp.cfg
./AampUVEPlayerDemo
```

---

## PRNG Seeding and Reproducibility

The `AampNetworkPersona` singleton seeds its Mersenne-Twister PRNG from
`std::random_device` at construction time — i.e. with hardware entropy,
**not** a fixed seed.  This is intentional:

* Real AAMP playback involves many other sources of non-determinism
  (thread scheduling, OS timer granularity, CDN response times, GStreamer
  pipeline timing, etc.) that make bit-exact reproducibility impossible even
  if the persona PRNG were fixed.
* A fixed persona seed would give a false impression of reproducibility
  while the overall session outcome still varies between runs.
* The statistical properties of the persona (mean throughput, jitter
  distribution, spike probability) are what matter for ABR validation, not
  the exact sample sequence.

For controlled A/B comparisons, **use abrsim** (which does support `--seed`)
to verify algorithm behaviour deterministically, then use real AAMP with a
persona to confirm the production code follows the same adaptation pattern
under the same statistical conditions.

---

## Timeout and Bail-out Interaction

The persona sleep logic respects AAMP's existing download-timeout and
early-abort mechanisms:

| Mechanism | Behaviour with persona active |
|---|---|
| `iDownloadTimeout` (curl total timeout) | TTFB sleep counts against the budget. If the persona predicts a download longer than the timeout, `CURLE_OPERATION_TIMEDOUT` is returned — the same as on a real slow network. |
| `iStartTimeout` / `iStallTimeout` / `iLowBWTimeout` | Progress-callback timers start **after** the TTFB sleep, measuring actual curl transfer time only, so bail-out thresholds fire correctly. |
| `Release()` / track abort | The idle sleep is chunked into 50 ms slices. `mDownloadActive` is checked between each slice, so cancellation interrupts the sleep within 50 ms. |

---

## Implementation Notes

| File | Role |
|---|---|
| `AampNetworkPersona.h` / `.cpp` | Singleton; JSON loading; TTFB and transfer-time sampling |
| `downloader/AampCurlDownloader.cpp` | Wraps `curl_easy_perform` with persona sleeps in the `AampCurlDownloader` path |
| `priv_aamp.cpp` | Same wrapping for the legacy inline curl download path |
| `AampConfig.h` / `AampConfig.cpp` | Registers `eAAMPConfig_NetworkPersonaFile` / `networkPersonaFile` |
| `test/utests/fakes/FakeAampNetworkPersona.cpp` | Stub for unit tests — always returns `IsLoaded() == false` |

---

# Generating a Persona from a Real Session (Inline Network Persona)

The personas consumed above can also be **produced** directly from a real
playback session. AAMP builds a compact **network persona JSON** as a
statistical model **in memory as downloads occur**, and emits it as a single
log line at the end of the session.

## Why

Because the persona is fitted in memory while downloads happen, a tiny JSON can
be produced directly at session end:

- **No persistent storage** — nothing is written to disk.
- **Ready instantly** — the persona is available the moment playback stops.
- **Bounded memory** — fixed footprint regardless of session length (hours of
  playback cost the same as minutes).
- **Flexible sink** — the JSON can go to a console log, or be handed to JSPP and
  transmitted to Viper Player Analytics.

## How it is emitted

On `Stop()`, AAMP logs the persona as a single line (always on — this does
**not** require any config flag):

```cpp
// priv_aamp.cpp
auto& fitter = aamptrace::NetPersonaFitter::GetInstance();
std::string inlinePersona = fitter.BuildMinimalPersonaJson();
if (!inlinePersona.empty())
{
    AAMPLOG_MIL("NET_PERSONA %s", inlinePersona.c_str());
}
```

Example log line:

```
NET_PERSONA {"base_rtt_ms": 28.5, "rtt_jitter_ms": 6.7, "ttfb_spike_p": 0.1,
"ttfb_spike_ms": 42.0, "mean_thr_mbps": 185.3, "thr_sigma_ln": 0.42,
"bursts_per_segment": 4, "burst_bytes_cv": 0.33, "cadence_ms": 198.5,
"cadence_jitter_ms": 21.4, "flush_jitter_ms": 6, "late_chunk_p": 0.03,
"late_chunk_extra_ms": 310.0, "p_conn_reuse": 0.86, "new_conn_penalty_ms": 25.1}
```

(Shown wrapped for readability; the real line is single-line JSON.)

To replay a captured persona with the simnet tool, paste the JSON into
`simnet/simnet/persona.json`. To replay it with real AAMP, point
`networkPersonaFile` (see [Quick Start](#quick-start)) at a file containing it.

## How it works

Metrics are fed to `NetPersonaFitter` during every download; no raw samples are
kept:

```
Per download (NetTrace, one call per request):
  ├─► NetPersonaFitter::AddRequest(ttfb, connReused)          — O(1) streaming + TTFB histograms
  ├─► NetPersonaFitter::AddBurst(... gap, bytes ...)  × bursts — O(1) streaming + gap histogram
  └─► NetPersonaFitter::AddRequestBurstSummary(count, sum bytes, sum bytes²)
                                                              — one atomic per-request summary
On Stop():
  └─► NetPersonaFitter::BuildMinimalPersonaJson()             — O(bucketCount) one-shot query
```

Each parameter is produced by one of three O(1)-ingestion mechanisms:

- **Streaming scalar** — running sums/counts (mean, ratio, sample std).
- **Streaming histogram** ([`AampTimingHistogram.h`](../AampTimingHistogram.h)) — a
  fixed-bucket histogram giving approximate median / percentile / tail aggregates
  without retaining raw samples. Error on any quantile is ±½ bucket.
- **Constant** — a fixed value.

The histogram exposes the strict-tail helpers `CountAboveMs(t)` and
`ApproximateMeanAboveMs(t)` used by the spike / late-chunk fields; they exclude
the bucket containing `t`, mirroring the file persona's `v > t` semantics.

## Fitted Parameters

### Computed inline

| Field | Source | How it is computed |
|---|---|---|
| `base_rtt_ms` | histogram | median of reused-connection TTFB (falls back to all TTFB when < 5 reused) |
| `rtt_jitter_ms` | histogram | robust std = (P75 − P25) / 1.349, with sample-std fallback when IQR ≤ 0 |
| `ttfb_spike_p` | histogram | fraction of reused TTFB strictly above its P90 (only when ≥ 20 reused samples) |
| `ttfb_spike_ms` | histogram | mean of the reused-TTFB tail above P90, minus `base_rtt_ms` |
| `mean_thr_mbps` | streaming scalar | geometric mean of per-burst rate: `exp(mean(ln rate)) · 8 / 1e6` |
| `thr_sigma_ln` | streaming scalar | sample std of `ln(rate)` |
| `bursts_per_segment` | histogram | median of per-request burst counts (clamped ≥ 1) |
| `burst_bytes_cv` | histogram | median across requests of the per-request byte-size CV |
| `cadence_ms` | streaming scalar | mean of guard-band gaps (0.10–0.50 s), fallback to all gaps |
| `cadence_jitter_ms` | streaming scalar | sample std of the same gap set |
| `flush_jitter_ms` | constant | `6` (hardware constant) |
| `late_chunk_p` | histogram | fraction of inter-burst gaps at/above `cadence + 2·jitter` |
| `late_chunk_extra_ms` | histogram | mean of the late-gap tail, minus `cadence_ms` |
| `p_conn_reuse` | streaming scalar | reused-connection requests / total requests |
| `new_conn_penalty_ms` | histogram | median(fresh TTFB) − median(reused TTFB), with fallback |

Per-request aggregates (`bursts_per_segment`, `burst_bytes_cv`) are summarized
once per request by `NetTrace` via `AddRequestBurstSummary()`, so grouping stays
correct even when media tracks download concurrently.

### Not yet inline (4 of 19)

| Field | Reason | Path to add |
|---|---|---|
| `thr_rho` | AR(1) autocorrelation of log-throughput is **order-dependent**; a histogram discards ordering | a separate O(1) streaming AR(1) estimator (follow-up) |
| `capacity_drop_p` | static default (`0.0`); not derived from the trace | echo the constant |
| `capacity_drop_factor` | static default (`0.6`); not derived from the trace | echo the constant |
| `rtt_inflation_ms` | static default (`0.0`); not derived from the trace | echo the constant |

## Histogram configuration

| Histogram | Bucket width | Range | Buckets |
|---|---|---|---|
| TTFB (all / reused / fresh) | 1 ms | 0–3000 ms | 3001 each |
| Inter-burst gaps | 5 ms | 0–10000 ms | 2001 |
| Bursts per request | 1 | 0–1024 | 1025 |
| Burst byte-size CV | 0.01 | 0–10 | 1001 |

Values at or above a histogram's range land in its overflow bucket; watch
`OverflowCount()` if a bound is ever set too low. Total fixed footprint ≈ 85 KB.

## Complexity

| Operation | Cost |
|---|---|
| Ingestion (per request and per burst) | **O(1)** |
| End-of-session query | **O(B)** (B = fixed total bucket count) |
| Memory | **O(B)** fixed (~85 KB, any session length) |
| Storage | none |

Quantiles are approximate (±½ bucket) and use nearest-bucket, not interpolation
— the trade-off for constant memory and no sorting.

## Unit tests (persona fitting)

`NetPersonaFitterTests` (`test/utests/tests/NetPersonaFitterTests/`) covers the
persona with exact oracles:

| Test | What it validates |
|---|---|
| `StreamingEmptyReturnsEmptyJson` | Inline persona is empty when no data was collected |
| `StreamingComputesMinimalFields` | Inline fields (mean/ratio + RTT medians + burst shape + tails) against exact oracles |
| `StreamingBurstCountMedian` | `bursts_per_segment` = median of per-request burst counts |
| `StreamingBurstCvMedian` | `burst_bytes_cv` = median of per-request byte-size CVs |
| `StreamingTtfbSpikeTail` | `ttfb_spike_p` / `ttfb_spike_ms` from the reused-TTFB tail above P90 |
| `StreamingLateChunkTail` | `late_chunk_p` / `late_chunk_extra_ms` from the gap tail above cadence + 2·jitter |
| `StreamingKeepRecordFalseDoesNotGrowVectors` | `keepRecord=false` updates streaming only (bounded memory) |
| `ResetStreamingClears` | `ResetStreaming()` clears all streaming state |
| `StreamingSurvivesFilePersonaSwap` | Swapping out the retained request/burst records does not disturb streaming aggregates |

Run with:
```sh
ctest -R NetPersonaFitterTests -V
```
