# Inline Network Persona

In-memory generation of a compact **network persona JSON**, emitted as a single log
line at the end of a playback session.

---

## Why

The persona is built as a statistical model **in memory as downloads occur**, so a tiny
JSON can be produced directly at session end:

- **No persistent storage** — nothing is written to disk.
- **Ready instantly** — the persona is available the moment playback stops.
- **Bounded memory** — fixed footprint regardless of session length (hours of playback
  cost the same as minutes).
- **Flexible sink** — the JSON can go to a console log, or be handed to JSPP and
  transmitted to Viper Player Analytics.

---

## How it is emitted

On `Stop()`, AAMP logs the persona as a single line (always on — this does **not**
require any config flag):

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

To use the output with simnet, paste the JSON into `simnet/simnet/persona.json`.

---

## How it works

Metrics are fed to `NetPersonaFitter` during every download; no raw samples are kept:

```
Per download (NetTrace, one call per request):
  ├─► NetPersonaFitter::AddRequest(ttfb, connReused)          — O(1) streaming + TTFB histograms
  ├─► NetPersonaFitter::AddBurst(... gap, bytes ...)  × bursts — O(1) streaming + gap histogram
  └─► NetPersonaFitter::AddRequestBurstSummary(count, Σbytes, Σbytes²)
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

The histogram exposes the tail helpers `CountAtOrAboveMs(t)` and
`ApproximateMeanAtOrAboveMs(t)` used by the spike / late-chunk fields.

---

## Parameters

### Computed inline

| Field | Source | How it is computed |
|---|---|---|
| `base_rtt_ms` | histogram | median of reused-connection TTFB (falls back to all TTFB when < 5 reused) |
| `rtt_jitter_ms` | histogram | robust std = (P75 − P25) / 1.349, with sample-std fallback when IQR ≤ 0 |
| `ttfb_spike_p` | histogram | fraction of reused TTFB at/above its P90 (only when ≥ 20 reused samples) |
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

Per-request aggregates (`bursts_per_segment`, `burst_bytes_cv`) are summarized once per
request by `NetTrace` via `AddRequestBurstSummary()`, so grouping stays correct even
when media tracks download concurrently.

### Not yet inline (4 of 19)

| Field | Reason | Path to add |
|---|---|---|
| `thr_rho` | AR(1) autocorrelation of log-throughput is **order-dependent**; a histogram discards ordering | a separate O(1) streaming AR(1) estimator (follow-up) |
| `capacity_drop_p` | static default (`0.0`); not derived from the trace | echo the constant |
| `capacity_drop_factor` | static default (`0.6`); not derived from the trace | echo the constant |
| `rtt_inflation_ms` | static default (`0.0`); not derived from the trace | echo the constant |

---

## Histogram configuration

| Histogram | Bucket width | Range | Buckets |
|---|---|---|---|
| TTFB (all / reused / fresh) | 1 ms | 0–3000 ms | 3001 each |
| Inter-burst gaps | 5 ms | 0–10000 ms | 2001 |
| Bursts per request | 1 | 0–1024 | 1025 |
| Burst byte-size CV | 0.01 | 0–10 | 1001 |

Values at or above a histogram's range land in its overflow bucket; watch
`OverflowCount()` if a bound is ever set too low. Total fixed footprint ≈ 85 KB.

---

## Complexity

| Operation | Cost |
|---|---|
| Ingestion (per request and per burst) | **O(1)** |
| End-of-session query | **O(B)** (B = fixed total bucket count) |
| Memory | **O(B)** fixed (~85 KB, any session length) |
| Storage | none |

Quantiles are approximate (±½ bucket) and use nearest-bucket, not interpolation — the
trade-off for constant memory and no sorting.

---

## Unit tests

`NetPersonaFitterTests` (`test/utests/tests/NetPersonaFitterTests/`) covers the persona
with exact oracles:

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
