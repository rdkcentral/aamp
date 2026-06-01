# Adaptive Bitrate & Latency Control  
## Normative Functional Specification (v1)

This specification defines the required behavior of an adaptive bitrate (ABR) controller and associated latency-control logic for an IP video player.  
It is intended to validate correctness of an existing implementation.

---

## 1. State Variables

### 1.1 buffer
Amount of fully decodable media queued for playout.

- Measured in media time at nominal playback rate (1×)
- Changes only due to:
  - consumption during playback, or
  - injection of new decodable media
- **SHALL NOT** fluctuate due to estimation, measurement, or playback speed changes

---

### 1.2 latency
Difference between the current playout position and the live edge.

- Measured in seconds of media time
- Measured continuously, regardless of playback state

---

### 1.3 playbackSpeed
Current presentation speed.

- One of `{ 0, 0.97, 1.0, 1.03 }`
- `0` indicates frozen playback (no consumption)

---

### 1.4 networkHistory
Historical measurements used to estimate:

- network throughput
- request time-to-first-byte

Predictions **SHALL** be based on the most likely (point-estimate) outcome.

---

### 1.5 videoProfiles
A totally ordered set of video representations.

- Ordered by increasing segment download cost (e.g., bitrate, size)
- Lowest profile = fastest to download

---

### 1.6 targetLatency
Mutable control target for live latency.

- Bounded:  
  `cfgDefaultTargetLatency ≤ targetLatency ≤ cfgMaxTargetLatency`

---

### 1.7 stableBufferTime
Time duration for which `buffer` has remained continuously greater than `cfgDangerBuffer`.

- Resets to `0` immediately when `buffer ≤ cfgDangerBuffer`
- Advances only while playback is active (`buffer > 0`)

---

## 2. Playback State

### 2.1 BUFFERING
Defined by `buffer == 0`.

- `playbackSpeed` **SHALL** be `0`
- No media consumption occurs
- No semantic distinction between:
  - initial tune
  - post-underflow recovery

---

### 2.2 PLAYING
Defined by `buffer > 0`.

- Media is consumed at `playbackSpeed`

---

## 3. Underflow Semantics

### 3.1 Underflow (Hard Failure)
Underflow occurs when `buffer` reaches `0` and no decodable media remains.

- User-visible frozen video

---

### 3.2 Danger Buffer (Early Warning)
`cfgDangerBuffer` defines a dangerously low buffer threshold.

- Crossing it is **not** underflow
- Used as an early-warning signal to trigger corrective action

---

## 4. Predicted Underflow
A predicted underflow occurs if:

- given current `buffer`, and
- predicted real-world download time of required next media,

the buffer is expected to reach `0` before new decodable media is injected.

Predictions **SHALL**:

- assume `1×` playback speed
- use the most likely estimated download time

---

## 5. Absolute Safety Invariant
The ABR algorithm **MUST NOT** deliberately select or continue any action that predicts underflow **if an alternative exists that avoids predicted underflow**.

This invariant dominates all quality, latency, and stability objectives.

---

## 6. Profile Selection Rules

### 6.1 Normal Case
When selecting a next segment, the player **SHALL** choose the highest `videoProfile` that does **not** predict underflow.

---

### 6.2 All Profiles Predict Underflow

#### BUFFERING (`buffer == 0`)
- Select `cfgDefaultVideoProfile`

#### PLAYING (`buffer > 0`)
- Select the **lowest** `videoProfile`

---

## 7. “Build Buffer” Rule
The player **SHOULD** avoid profiles whose predicted download time exceeds their media duration.

- This rule **SHALL NOT** override the Absolute Safety Invariant

---

## 8. Bail Behavior

### 8.1 Eligibility
Bail decisions **SHALL** be evaluated continuously during an active download as network bursts arrive.

---

### 8.2 Bail Condition
The player **SHALL** bail from the current download if:

1. Continuing predicts underflow, **and**
2. Abandoning and restarting with another profile avoids predicted underflow

---

### 8.3 Properties
- No minimum progress threshold exists
- Bail decisions are purely prediction-based

---

## 9. ABR vs. Latency Control
ABR and latency control are intentionally decoupled.

- ABR predictions **SHALL** assume `1×` playback
- Latency control **SHALL NOT** influence ABR decisions

---

## 10. Latency Control (Playback Speed)

### 10.1 Applicability
Latency correction **SHALL** be evaluated only in the PLAYING state (`buffer > 0`).

---

### 10.2 Speed Selection
Let:

```
Δ = latency − targetLatency
```

- If `Δ > cfgLatencyVariance` → `playbackSpeed = 1.03`
- If `Δ < −cfgLatencyVariance` → `playbackSpeed = 0.97`
- Otherwise → `playbackSpeed = 1.0`

---

### 10.3 Hysteresis
Speed transitions **MUST** pass through `1.0`.

- Direct `0.97 ↔ 1.03` transitions are prohibited

---

## 11. Dynamic Target Latency Adjustment

### 11.1 Relaxation (Poor Network)
On transition from:

```
buffer > cfgDangerBuffer  →  buffer ≤ cfgDangerBuffer
```

- Increase `targetLatency` by `cfgLatencyChange`
- Occurs **once per danger episode**
- Clamp to `cfgMaxTargetLatency`

---

### 11.2 Tightening (Sustained Health)
If:

- `stableBufferTime > cfgLatencyStable`, and
- `targetLatency > cfgDefaultTargetLatency`

Then:

- Decrease `targetLatency` by `cfgLatencyChange`
- Clamp to `cfgDefaultTargetLatency`

---

## 12. Non-Goals (Explicit)

The following are explicitly out of scope:

- Using playback speed to avoid underflow
- Entangling ABR and latency control
- Worst-case or pessimistic prediction models
