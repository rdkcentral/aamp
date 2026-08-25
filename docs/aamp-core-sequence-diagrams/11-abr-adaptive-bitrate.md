# 11. ABR (Adaptive Bitrate) — Sequence Diagrams

**Source Files Read**:
- `abr/ABRManager.h` (complete)
- `abr/ABRManager.cpp` (complete — 900+ lines)
- `abr/AampAbrBandwidthEstimators.h` (complete)
- `priv_aamp.cpp` LoadAampAbrConfig (complete)
- `streamabstraction.cpp` GetDesiredProfileOnBuffer/OnSteadyState (complete)

**Confidence: 100%**

---

## 1. ABR Decision Flow (Main Algorithm)

`mermaid
sequenceDiagram
    participant MT as MediaTrack
    participant SA as StreamAbstractionAAMP
    participant ABR as ABRManager
    participant Config as AampConfig

    MT->>SA: UpdateProfileBasedOnFragmentDownloaded()
    SA->>ABR: GetDesiredBitrateProfile(currentBW)
    ABR->>ABR: Check ABR enabled (config)
    ABR->>ABR: GetNetworkBandwidth() from EWMA estimator
    ABR->>ABR: Apply ABR cache (rolling window of BW samples)
    ABR->>ABR: Remove outliers (configurable threshold)
    ABR->>ABR: Calculate effective bandwidth
    alt Ramp Up (buffer > maxBuffer threshold)
        ABR->>ABR: Find next higher profile <= effectiveBW
        ABR->>ABR: Check iframe-only constraints
        ABR-->>SA: Higher profile index
    else Ramp Down (buffer < minBuffer threshold)
        ABR->>ABR: Find next lower profile
        ABR->>ABR: Increment NetworkDropCount or ErrorDropCount
        ABR-->>SA: Lower profile index
    else Steady State
        ABR-->>SA: Current profile (no change)
    end
    SA->>SA: Apply profile (update fragment URL to new bitrate)
    SA->>SA: NotifyBitRateChangeEvent()
`

## 2. Bandwidth Estimation (EWMA)

`mermaid
sequenceDiagram
    participant Curl as CurlDownloader
    participant SA as StreamAbstractionAAMP
    participant ABR as ABRManager
    participant Est as BandwidthEstimator

    Curl-->>SA: Fragment downloaded (size, duration, downloadTime)
    SA->>ABR: UpdateABRBandwidth(downloadedBitsPerSec)
    ABR->>Est: AddSample(bandwidth)
    Est->>Est: EWMA calculation: new = alpha * sample + (1-alpha) * previous
    Est->>Est: Store in rolling cache (configurable length)
    ABR->>ABR: Update mNetworkBandwidth
    Note over ABR: Cache params: Life, Length, Outlier threshold, Consistency
`

## 3. Buffer-Based ABR (Low Latency)

`mermaid
sequenceDiagram
    participant SA as StreamAbstractionAAMP
    participant ABR as ABRManager
    participant Priv as PrivateInstanceAAMP

    SA->>SA: GetDesiredProfileOnBuffer()
    SA->>SA: Get current buffer duration
    SA->>ABR: GetMaxBufferThreshold() / GetMinBufferThreshold()
    alt Low Latency Mode
        SA->>SA: Use tighter thresholds (from LLDashServiceData)
        alt Buffer critically low
            SA->>SA: Immediate ramp down to lowest profile
        else Buffer recovering
            SA->>SA: Gradual ramp up
        end
    else Normal Mode
        alt buffer > abrMaxBuffer
            SA->>ABR: GetDesiredBitrateProfile(rampUp)
        else buffer < abrMinBuffer
            SA->>ABR: GetDesiredBitrateProfile(rampDown)
        end
    end
    SA->>Priv: NotifyBitRateChangeEvent(newBitrate, reason)
`

## 4. ABR Configuration Loading

`mermaid
sequenceDiagram
    participant Priv as PrivateInstanceAAMP
    participant ABR as ABRManager
    participant Config as AampConfig

    Priv->>Priv: LoadAampAbrConfig()
    Priv->>Config: Get ABRCacheLife, CacheLength, SkipDuration
    Priv->>Config: Get ABRNWConsistency, ThresholdSize
    Priv->>Config: Get MaxABRNWBufferRampUp, MinABRNWBufferRampDown
    Priv->>Config: Get ABRCacheOutlier, BufferCounter
    Priv->>Config: Get ABRBandwidthEstimator type
    Priv->>ABR: ReadPlayerConfig(abrConfig)
    ABR->>ABR: Store all config values
    ABR->>ABR: Initialize bandwidth estimator based on type
`

---

## Key Implementation Details

| Aspect | Implementation |
|--------|---------------|
| **Algorithm** | Hybrid: bandwidth-based + buffer-based |
| **Estimator** | EWMA with configurable alpha (ABRBandwidthEstimator) |
| **Cache** | Rolling window (ABRCacheLength samples, ABRCacheLife ms) |
| **Outlier removal** | Samples > ABRCacheOutlier * average are discarded |
| **Ramp up** | Only when buffer > MaxABRNWBufferRampUp for N consecutive checks |
| **Ramp down** | Immediate when buffer < MinABRNWBufferRampDown |
| **Low latency** | Tighter thresholds, faster ramp-down, buffer-first decisions |
| **Profile constraints** | Min/Max bitrate, 4K bitrate, iframe-only profiles |
| **Metrics** | NetworkDropCount, ErrorDropCount, RateCorrectionCount → VideoEnd |
