# Network Persona Fitter

Automatic generation of a simnet **network persona JSON** directly from a live AAMP
playback session — no external Python tool or post-processing step required.

---

## Background

The LL-DASH network simulator (`simnet`) uses a 19-field JSON "persona" file to model
real-world network conditions (RTT, throughput, burst cadence, connection-reuse
behaviour, etc.).  Previously, generating that file required:

1. Enabling the NetTrace CSV dumps during a real playback session.
2. Copying the CSV files to a workstation.
3. Running `simnet/simnet/persona_fit.py` offline.

`NetPersonaFitter` (`net_persona_fitter.h` / `net_persona_fitter.cpp`) is a C++ port of
that Python script.  It accumulates request/burst data in memory as playback proceeds
and writes the persona JSON automatically when the player stops.

---

## How it works

```
curl write callback (in-band, per TCP segment)
  └─► NetTrace::OnWrite()           — timestamps each data burst

curl header callback
  └─► NetTrace::MarkChunked()       — notes chunked transfer encoding

After each curl_easy_perform() (inside GetFile retry loop):
  ├─► NetTrace::OnCompleteBytes()   — closes the final burst
  └─► NetTrace::SetCurlTimings()    — stores CURLINFO_* metrics (TTFB, DNS, etc.)

After retry loop exits (one call per GetFile()):
  └─► NetTrace::FlushCsv()
        ├─► aamp_net_requests.csv.<PID>   (one row per request)
        ├─► aamp_net_bursts.csv.<PID>     (one row per burst)
        └─► NetPersonaFitter::AddRequest() / AddBurst()   ← in-memory accumulation

On player Stop():
  └─► NetPersonaFitter::GeneratePersonaJson("/tmp/aamp_net_persona.json")
        └─► /tmp/aamp_net_persona.json.<PID>

On process exit (safety net — fires even if Stop() is never called):
  └─► atexit handler calls GeneratePersonaJson() again (idempotent)
```

The fitting logic mirrors `persona_fit.py` exactly: median/robust-std for RTT and TTFB
fields, AR(1) on log-rates for throughput, and per-request grouping for burst
statistics.

---

## Enabling

Add to `aamp.cfg` (or set at runtime via UVE `setVideoRect`-style config):

```
netTraceCsvDump=true
```

That single flag gates both the CSV output and the in-memory persona accumulation.

### Optional: override CSV output paths

Set environment variables **before** the first download.  The PID suffix is always
appended automatically.

```sh
export AAMP_REQ_CSV=/mnt/usb/aamp_net_requests.csv
export AAMP_BUR_CSV=/mnt/usb/aamp_net_bursts.csv
```

If neither variable is set and `netTraceCsvDump=true`, the default paths are:

| File | Default path |
|---|---|
| Requests CSV | `/tmp/aamp_net_requests.csv.<PID>` |
| Bursts CSV | `/tmp/aamp_net_bursts.csv.<PID>` |
| Persona JSON | `/tmp/aamp_net_persona.json.<PID>` |

---

## Output: persona JSON

The persona JSON is written to `/tmp/aamp_net_persona.json.<PID>` and contains 19
fields consumed directly by simnet:

| Field | Description |
|---|---|
| `base_rtt_ms` | Median TTFB of reused-connection requests (proxy for base RTT) |
| `rtt_jitter_ms` | Robust std-dev of reused-connection TTFB |
| `ttfb_spike_p` | Fraction of requests with TTFB above the 90th percentile |
| `ttfb_spike_ms` | Mean excess TTFB for spike requests (above baseline) |
| `mean_thr_mbps` | Geometric mean burst throughput in Mbps |
| `thr_sigma_ln` | Log-normal sigma of burst throughput |
| `thr_rho` | AR(1) autocorrelation of log-throughput across bursts |
| `bursts_per_segment` | Median burst count per request |
| `burst_bytes_cv` | Median coefficient of variation of burst sizes per request |
| `cadence_ms` | Median inter-burst gap (ms) |
| `cadence_jitter_ms` | Std-dev of inter-burst gap |
| `flush_jitter_ms` | Fixed flush-pipeline jitter (6 ms, hardware constant) |
| `late_chunk_p` | Fraction of gaps exceeding cadence + 2×jitter |
| `late_chunk_extra_ms` | Mean excess gap for late chunks |
| `p_conn_reuse` | Fraction of requests using a reused TCP connection |
| `new_conn_penalty_ms` | Median TTFB penalty for fresh vs. reused connections |
| `capacity_drop_p` | Probability of a sudden capacity drop (static default: 0.0) |
| `capacity_drop_factor` | Severity of capacity drop (static default: 0.6) |
| `rtt_inflation_ms` | RTT inflation under load (static default: 0.0) |

To use the output with simnet, copy the file to `simnet/simnet/persona.json` (without
the PID suffix).

---

## Manual QA test procedure

### Prerequisites
- Device or Linux host running AAMP with `netTraceCsvDump=true` in `aamp.cfg`.
- Read access to `/tmp/`.

### Steps

1. **Enable the feature** — confirm `aamp.cfg` contains `netTraceCsvDump=true`.

2. **Start a playback session** — tune to any HLS or DASH stream.  Allow at least
   30–60 seconds of uninterrupted playback to accumulate enough burst data for stable
   statistics (100+ bursts is sufficient; 300+ is better).

3. **Stop playback** — call `Stop()` via the UVE API or by closing the player.

4. **Verify the persona JSON was written:**
   ```sh
   ls -lh /tmp/aamp_net_persona.json.*
   cat /tmp/aamp_net_persona.json.<PID>
   ```

5. **Validate the JSON fields:**
   - All 19 fields listed in the table above are present.
   - `base_rtt_ms` is a positive finite number (typically 10–100 ms on a healthy
     connection; > 200 ms suggests high-latency or all-fresh connections).
   - `mean_thr_mbps` is positive and finite.
   - `p_conn_reuse` is in [0.0, 1.0].
   - `bursts_per_segment` is a positive integer ≥ 1.
   - No field should be `nan` or `inf` for a normal session.

6. **Cross-check against IP_AAMP_TUNETIME log** — `base_rtt_ms` should be in the
   same ballpark as the TTFB values visible in the curl timing fields of the
   `IP_AAMP_TUNETIME` log line.

7. **Verify CSV files were also written** (if needed for offline analysis):
   ```sh
   wc -l /tmp/aamp_net_requests.csv.*
   wc -l /tmp/aamp_net_bursts.csv.*
   head -2 /tmp/aamp_net_requests.csv.*
   ```
   Expect ≥ 1 header row + N data rows matching the number of completed downloads.

8. **Verify feature is off by default** — repeat with `netTraceCsvDump` absent from
   `aamp.cfg`.  Confirm `/tmp/aamp_net_persona.json.*` is **not** created and there is
   no measurable CPU or memory overhead during playback.

9. **Verify atexit safety net** — kill the player process with `SIGTERM` (without
   calling `Stop()`) and confirm the persona JSON is still written.  Note: SIGKILL
   will not trigger the atexit handler by design.

### Pass criteria
- Persona JSON file present, valid JSON, all 19 fields populated with finite values.
- CSV files present with correct headers and ≥ 1 data row.
- No persona file created when feature is disabled.
- No crashes, hangs, or log errors related to `NetPersonaFitter` or `NetTrace`.

---

## Scope / known limitations

- **`AampCurlDownloader` downloads are not instrumented.** Manifest downloads that go
  through `AampCurlDownloader` (rather than `GetFile`) do not contribute burst data to
  the fitter.  This is intentional — manifests are small and infrequent and would skew
  the segment-level persona.  The resulting persona reflects media-segment download
  behaviour only.

- **Retry attempts share a single `NetTrace` instance.** If `GetFile` retries a
  download, burst data from the failed attempt is included.  This reflects real network
  behaviour and is unlikely to skew the persona meaningfully.

- **Singleton accumulates across tunes within a process lifetime.** If the same process
  tunes multiple times before `Stop()` writes the JSON, all sessions contribute to the
  same persona.  This is generally desirable (more data = better fit), but should be
  noted when using the output to model a specific session.

---

## Unit tests

The `NetPersonaFitterTests` suite (`test/utests/tests/NetPersonaFitterTests/`) covers:

| Test | What it validates |
|---|---|
| `EmptyDataReturnsFalse` | `GeneratePersonaJson()` returns false with no data |
| `RealisticDataProducesValidJson` | All 19 fields present and within expected ranges for synthetic data |
| `CountsAccumulate` | `AddRequest` / `AddBurst` increment counters correctly |
| `OutputPathIncludesPid` | Output file is created at the PID-suffixed path |

Run with:
```sh
ctest -R NetPersonaFitterTests -V
```
