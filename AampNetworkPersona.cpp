/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file AampNetworkPersona.cpp
 * @brief Simulated network latency injection — implementation.
 */

#include "AampNetworkPersona.h"
#include "AampLogManager.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// Internal JSON key→number helpers (minimal, no external dependency)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

bool FindNumber(const std::string& s, const std::string& key, double& out)
{
    const std::string kq = "\"" + key + "\"";
    std::size_t kp = s.find(kq);
    if (kp == std::string::npos) return false;
    std::size_t colon = s.find(':', kp + kq.size());
    if (colon == std::string::npos) return false;
    std::size_t start = s.find_first_of("-0123456789", colon + 1);
    if (start == std::string::npos) return false;
    std::size_t end = s.find_first_not_of("0123456789.eE+-", start);
    try {
        out = std::stod(s.substr(start, end - start));
    } catch (...) { return false; }
    return true;
}

bool FindInt(const std::string& s, const std::string& key, int& out)
{
    double d = 0.0;
    if (!FindNumber(s, key, d)) return false;
    out = static_cast<int>(d);
    return true;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────

AampNetworkPersona::AampNetworkPersona()
    : mRng(std::random_device{}())
{
}

AampNetworkPersona& AampNetworkPersona::Instance()
{
    static AampNetworkPersona sInstance;
    return sInstance;
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadFromFile
// ─────────────────────────────────────────────────────────────────────────────

bool AampNetworkPersona::IsLoaded() const
{
    std::lock_guard<std::mutex> lk(mMutex);
    return mLoaded;
}

bool AampNetworkPersona::LoadFromFile(const std::string& path)
{
    std::lock_guard<std::mutex> lk(mMutex);
    if (mLoaded) return true;   // idempotent

    std::ifstream ifs(path);
    if (!ifs)
    {
        AAMPLOG_WARN("AampNetworkPersona: cannot open '%s'", path.c_str());
        return false;
    }

    std::ostringstream buf;
    buf << ifs.rdbuf();
    const std::string json = buf.str();

    // Parse known fields; unrecognised fields are silently ignored.
    FindNumber(json, "base_rtt_ms",         mBaseRttMs);
    FindNumber(json, "rtt_jitter_ms",        mRttJitterMs);
    FindNumber(json, "ttfb_spike_p",         mTtfbSpikeP);
    FindNumber(json, "ttfb_spike_ms",        mTtfbSpikeMs);
    FindNumber(json, "thr_sigma_ln",         mThrSigmaLn);
    FindNumber(json, "flush_jitter_ms",      mFlushJitterMs);
    FindNumber(json, "late_chunk_p",         mLateChunkP);
    FindNumber(json, "late_chunk_extra_ms",  mLateChunkExtraMs);
    FindNumber(json, "p_conn_reuse",         mPConnReuse);
    FindNumber(json, "new_conn_penalty_ms",  mNewConnPenaltyMs);
    FindInt   (json, "bursts_per_segment",   mBurstsPerSegment);

    double meanMbps = 0.0;
    if (FindNumber(json, "mean_thr_mbps", meanMbps) && meanMbps > 0.0)
    {
        const double meanBytesPerSec = meanMbps * 1e6 / 8.0;
        mMeanThrLn = std::log(meanBytesPerSec);
    }

    mLoaded = true;
    AAMPLOG_INFO("AampNetworkPersona: loaded '%s' "
                 "(mean=%.1f Mbps, rtt=%.0f±%.0f ms, spike_p=%.3f)",
                 path.c_str(), meanMbps, mBaseRttMs, mRttJitterMs, mTtfbSpikeP);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SampleTtfbMs
// ─────────────────────────────────────────────────────────────────────────────

double AampNetworkPersona::SampleTtfbMs(bool assumeNewConnection)
{
    std::lock_guard<std::mutex> lk(mMutex);
    if (!mLoaded) return 0.0;

    double ttfbMs = 0.0;

    // New-connection TCP handshake / DNS penalty
    if (assumeNewConnection)
    {
        ttfbMs += mNewConnPenaltyMs;
    }
    else
    {
        std::bernoulli_distribution connReuseDist(mPConnReuse);
        if (!connReuseDist(mRng))
            ttfbMs += mNewConnPenaltyMs;
    }

    // Base RTT with Gaussian jitter
    std::normal_distribution<double> rttDist(mBaseRttMs, mRttJitterMs);
    ttfbMs += std::max(1.0, rttDist(mRng));

    // Occasional TTFB spike (server hiccup / head-of-line blocking)
    std::bernoulli_distribution spikeDist(mTtfbSpikeP);
    if (spikeDist(mRng))
        ttfbMs += mTtfbSpikeMs;

    return ttfbMs;
}

// ─────────────────────────────────────────────────────────────────────────────
// SampleTransferMs
// ─────────────────────────────────────────────────────────────────────────────

double AampNetworkPersona::SampleTransferMs(std::size_t bytes)
{
    std::lock_guard<std::mutex> lk(mMutex);
    if (!mLoaded || bytes == 0) return 0.0;

    // Per-download independent throughput sample from the marginal lognormal
    // distribution (no AR(1) carry-over between requests).
    std::normal_distribution<double> thrDist(mMeanThrLn, mThrSigmaLn);
    const double effectiveBytesPerSec = std::exp(thrDist(mRng));
    double transferMs = (static_cast<double>(bytes) / effectiveBytesPerSec) * 1000.0;

    // Per-burst TCP flush jitter (models irregular delivery within a segment)
    std::normal_distribution<double> flushDist(0.0, mFlushJitterMs);
    for (int b = 0; b < mBurstsPerSegment; ++b)
        transferMs += std::abs(flushDist(mRng));

    // Occasional stall — packet loss / retransmit event
    std::bernoulli_distribution lateDist(mLateChunkP);
    if (lateDist(mRng))
        transferMs += mLateChunkExtraMs;

    return transferMs;
}
