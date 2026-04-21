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
~/repos/
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
