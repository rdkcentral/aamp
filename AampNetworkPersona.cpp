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
// ParsePersonaParams  (static helper)
// ─────────────────────────────────────────────────────────────────────────────

/*static*/
void AampNetworkPersona::ParsePersonaParams(const std::string& json, PersonaParams& p)
{
	FindNumber(json, "base_rtt_ms",         p.baseRttMs);
	FindNumber(json, "rtt_jitter_ms",       p.rttJitterMs);
	FindNumber(json, "ttfb_spike_p",        p.ttfbSpikeP);
	FindNumber(json, "ttfb_spike_ms",       p.ttfbSpikeMs);
	FindNumber(json, "thr_sigma_ln",        p.thrSigmaLn);
	FindNumber(json, "flush_jitter_ms",     p.flushJitterMs);
	FindNumber(json, "late_chunk_p",        p.lateChunkP);
	FindNumber(json, "late_chunk_extra_ms", p.lateChunkExtraMs);
	FindNumber(json, "p_conn_reuse",        p.pConnReuse);
	FindNumber(json, "new_conn_penalty_ms", p.newConnPenaltyMs);
	FindInt   (json, "bursts_per_segment",  p.burstsPerSegment);

	double meanMbps = 0.0;
	if (FindNumber(json, "mean_thr_mbps", meanMbps) && meanMbps > 0.0)
		p.meanThrLn = std::log(meanMbps * 1e6 / 8.0);

	// Clamp probability fields to [0,1] and std-dev/count fields to >= 0 so
	// that downstream distribution constructors never receive out-of-range values.
	p.pConnReuse       = std::max(0.0, std::min(1.0, p.pConnReuse));
	p.ttfbSpikeP       = std::max(0.0, std::min(1.0, p.ttfbSpikeP));
	p.lateChunkP       = std::max(0.0, std::min(1.0, p.lateChunkP));
	p.rttJitterMs      = std::max(0.0, p.rttJitterMs);
	p.thrSigmaLn       = std::max(0.0, p.thrSigmaLn);
	p.flushJitterMs    = std::max(0.0, p.flushJitterMs);
	p.burstsPerSegment = std::max(0, p.burstsPerSegment);
}

// ─────────────────────────────────────────────────────────────────────────────
// IsLoaded / LoadFromFile
// ─────────────────────────────────────────────────────────────────────────────

bool AampNetworkPersona::IsLoaded() const
{
	// Atomic load — no mutex needed on the hot download path.
	return mLoaded.load(std::memory_order_acquire);
}

bool AampNetworkPersona::LoadFromFile(const std::string& path)
{
	// Fast path: already loaded — avoid taking the mutex on every playback call.
	if (mLoaded.load(std::memory_order_acquire)) return true;
	std::lock_guard<std::mutex> lk(mMutex);
	if (mLoaded.load(std::memory_order_relaxed)) return true;  // double-check under lock

	std::ifstream ifs(path);
	if (!ifs)
	{
		AAMPLOG_WARN("AampNetworkPersona: cannot open '%s'", path.c_str());
		return false;
	}

	std::ostringstream buf;
	buf << ifs.rdbuf();
	const std::string json = buf.str();

	// ── Detect sequence (array) vs single persona (object) ──────────────────
	const std::size_t first = json.find_first_not_of(" \t\n\r");
	if (first != std::string::npos && json[first] == '[')
	{
		// Array of persona entries.  Extract each top-level {...} block.
		int depth = 0;
		std::size_t objStart = std::string::npos;
		for (std::size_t i = 0; i < json.size(); ++i)
		{
			if (json[i] == '{') {
				if (depth == 0) objStart = i;
				++depth;
			} else if (json[i] == '}') {
				--depth;
				if (depth == 0 && objStart != std::string::npos) {
					const std::string entryJson = json.substr(objStart, i - objStart + 1);
					PersonaEntry entry;
					FindNumber(entryJson, "duration_s", entry.durationS);
					ParsePersonaParams(entryJson, entry.params);
					mSequence.push_back(std::move(entry));
					objStart = std::string::npos;
				}
			}
		}
		AAMPLOG_INFO("AampNetworkPersona: loaded sequence of %zu persona(s) from '%s'",
					 mSequence.size(), path.c_str());
	}
	else
	{
		// Single persona object — backward-compatible.
		PersonaEntry entry;
		entry.durationS = 0.0; // run forever
		ParsePersonaParams(json, entry.params);
		mSequence.push_back(std::move(entry));
		AAMPLOG_INFO("AampNetworkPersona: loaded single persona from '%s'", path.c_str());
	}

	if (mSequence.empty())
	{
		AAMPLOG_WARN("AampNetworkPersona: no valid entries found in '%s'", path.c_str());
		return false;
	}

	mLoaded.store(true, std::memory_order_release);
	const PersonaParams& p0 = mSequence[0].params;
	AAMPLOG_WARN("AampNetworkPersona: active — base_rtt=%.0fms mean_thr=%.2fMbps (ln=%.3f sigma=%.3f) ttfb_spike_p=%.2f new_conn=%.0fms entries=%zu",
				 p0.baseRttMs,
				 std::exp(p0.meanThrLn) * 8.0 / 1e6,
				 p0.meanThrLn, p0.thrSigmaLn,
				 p0.ttfbSpikeP, p0.newConnPenaltyMs,
				 mSequence.size());
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CurrentParamsLocked  (must be called under mMutex)
// ─────────────────────────────────────────────────────────────────────────────

const AampNetworkPersona::PersonaParams& AampNetworkPersona::CurrentParamsLocked() const
{
	// For single-entry sequences there is nothing to track.
	if (mSequence.size() == 1) return mSequence[0].params;

	// Lazily start the sequence clock on the first sampling call.
	if (!mSequenceStarted) {
		mSequenceStarted = true;
		mSequenceStart   = std::chrono::steady_clock::now();
	}

	const double elapsedS = std::chrono::duration<double>(
		std::chrono::steady_clock::now() - mSequenceStart).count();

	double cumulative = 0.0;
	for (const auto& entry : mSequence) {
		if (entry.durationS <= 0.0) return entry.params; // last/forever entry
		cumulative += entry.durationS;
		if (elapsedS < cumulative) return entry.params;
	}
	return mSequence.back().params; // past end of all explicit durations
}

// ─────────────────────────────────────────────────────────────────────────────
// SampleTtfbMs
// ─────────────────────────────────────────────────────────────────────────────

double AampNetworkPersona::SampleTtfbMs(bool assumeNewConnection)
{
	std::lock_guard<std::mutex> lk(mMutex);
	if (!mLoaded) return 0.0;
	const PersonaParams& p = CurrentParamsLocked();

	double ttfbMs = 0.0;

	// New-connection TCP handshake / DNS penalty
	if (assumeNewConnection)
	{
		ttfbMs += p.newConnPenaltyMs;
	}
	else
	{
		std::bernoulli_distribution connReuseDist(p.pConnReuse);
		if (!connReuseDist(mRng))
			ttfbMs += p.newConnPenaltyMs;
	}

	// Base RTT with Gaussian jitter
	std::normal_distribution<double> rttDist(p.baseRttMs, p.rttJitterMs);
	ttfbMs += std::max(1.0, rttDist(mRng));

	// Occasional TTFB spike (server hiccup / head-of-line blocking)
	std::bernoulli_distribution spikeDist(p.ttfbSpikeP);
	if (spikeDist(mRng))
		ttfbMs += p.ttfbSpikeMs;

	return ttfbMs;
}

// ─────────────────────────────────────────────────────────────────────────────
// SampleTransferMs
// ─────────────────────────────────────────────────────────────────────────────

double AampNetworkPersona::SampleTransferMs(std::size_t bytes)
{
	std::lock_guard<std::mutex> lk(mMutex);
	if (!mLoaded || bytes == 0) return 0.0;
	const PersonaParams& p = CurrentParamsLocked();

	// Per-download independent throughput sample from the marginal lognormal
	// distribution of the current persona (no AR(1) carry-over between requests).
	std::normal_distribution<double> thrDist(p.meanThrLn, p.thrSigmaLn);
	const double rawBytesPerSec = std::exp(thrDist(mRng));
	// Guard against underflow (→ 0), overflow (→ inf) or NaN from extreme
	// sigma values; clamp to [1 KB/s, 10 GB/s].
	constexpr double kMinBytesPerSec = 1.0e3;
	constexpr double kMaxBytesPerSec = 1.0e10;
	const double effectiveBytesPerSec = std::isfinite(rawBytesPerSec)
		? std::max(kMinBytesPerSec, std::min(kMaxBytesPerSec, rawBytesPerSec))
		: kMinBytesPerSec;
	double transferMs = (static_cast<double>(bytes) / effectiveBytesPerSec) * 1000.0;

	// Per-burst TCP flush jitter (models irregular delivery within a segment)
	std::normal_distribution<double> flushDist(0.0, p.flushJitterMs);
	for (int b = 0; b < p.burstsPerSegment; ++b)
		transferMs += std::abs(flushDist(mRng));

	// Occasional stall — packet loss / retransmit event
	std::bernoulli_distribution lateDist(p.lateChunkP);
	if (lateDist(mRng))
		transferMs += p.lateChunkExtraMs;

	AAMPLOG_WARN("AampNetworkPersona::SampleTransferMs bytes=%zu predicted=%.0fms (effectiveBytesPerSec=%.0f rawBytesPerSec=%.0f)",
				 bytes, transferMs, effectiveBytesPerSec, rawBytesPerSec);
	return transferMs;
}
