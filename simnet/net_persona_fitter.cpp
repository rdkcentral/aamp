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
 * @file net_persona_fitter.cpp
 * @brief Implementation of NetPersonaFitter — C++ port of persona_fit.py
 */

#include "net_persona_fitter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <unistd.h>

#include "../AampLogManager.h"

namespace aamptrace {

// ======================== Statistical helpers ========================

/// Compute the median of a sorted copy of vals. Returns 0 if empty.
static double Median(std::vector<double> vals)
{
	if (vals.empty()) return 0.0;
	std::sort(vals.begin(), vals.end());
	auto n = vals.size();
	if (n % 2 == 1)
	{
		return vals[n / 2];
	}
	return (vals[n / 2 - 1] + vals[n / 2]) / 2.0;
}

/// Compute percentile p (0-100) via linear interpolation on a sorted copy.
static double Percentile(std::vector<double> vals, double p)
{
	if (vals.empty()) return 0.0;
	std::sort(vals.begin(), vals.end());
	double idx = (p / 100.0) * static_cast<double>(vals.size() - 1);
	auto lo = static_cast<std::size_t>(std::floor(idx));
	auto hi = static_cast<std::size_t>(std::ceil(idx));
	if (lo == hi || hi >= vals.size()) return vals[lo];
	double frac = idx - static_cast<double>(lo);
	return vals[lo] * (1.0 - frac) + vals[hi] * frac;
}

/// Robust standard deviation via IQR/1.349 (matches persona_fit.py robust_std).
static double RobustStd(const std::vector<double>& vals)
{
	if (vals.size() < 2) return 0.0;
	double q1 = Percentile(vals, 25.0);
	double q3 = Percentile(vals, 75.0);
	double iqr = q3 - q1;
	if (iqr > 0.0)
	{
		return iqr / 1.349;
	}
	// Fallback: sample standard deviation
	double mean = std::accumulate(vals.begin(), vals.end(), 0.0) / static_cast<double>(vals.size());
	double sumSq = 0.0;
	for (double v : vals)
	{
		double d = v - mean;
		sumSq += d * d;
	}
	return std::sqrt(sumSq / static_cast<double>(vals.size() - 1));
}

/// Sample standard deviation (ddof=1).
static double SampleStd(const std::vector<double>& vals)
{
	if (vals.size() < 2) return 0.0;
	double mean = std::accumulate(vals.begin(), vals.end(), 0.0) / static_cast<double>(vals.size());
	double sumSq = 0.0;
	for (double v : vals)
	{
		double d = v - mean;
		sumSq += d * d;
	}
	return std::sqrt(sumSq / static_cast<double>(vals.size() - 1));
}

/// Simple OLS for AR(1): x_t = c + rho * x_{t-1} + eps.
/// Returns {c, rho, sigma_eps}.
struct Ar1Result {
	double c{0.0};
	double rho{0.0};
	double sigmaEps{0.0};
};

static Ar1Result FitAr1(const std::vector<double>& x)
{
	Ar1Result result;
	auto n = x.size();
	if (n < 3) return result;

	// OLS: y = c + rho * xPrev  where y = x[1..n-1], xPrev = x[0..n-2]
	double sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0;
	auto m = n - 1;
	for (std::size_t i = 0; i < m; ++i)
	{
		double xi = x[i];
		double yi = x[i + 1];
		sumX += xi;
		sumY += yi;
		sumXX += xi * xi;
		sumXY += xi * yi;
	}
	double dm = static_cast<double>(m);
	double denom = dm * sumXX - sumX * sumX;
	if (std::abs(denom) < 1e-15)
	{
		return result;
	}
	result.rho = (dm * sumXY - sumX * sumY) / denom;
	result.c = (sumY - result.rho * sumX) / dm;

	// Residual standard deviation (ddof=1)
	double sumResidSq = 0.0;
	for (std::size_t i = 0; i < m; ++i)
	{
		double resid = x[i + 1] - (result.c + result.rho * x[i]);
		sumResidSq += resid * resid;
	}
	result.sigmaEps = (m > 1) ? std::sqrt(sumResidSq / static_cast<double>(m - 1)) : 0.0;
	return result;
}

// ======================== Persona fitting ========================

/// Persona fields — mirrors the 19-field JSON output of persona_fit.py.
struct Persona {
	double baseRttMs{0.0};
	double rttJitterMs{0.0};
	double ttfbSpikeP{0.0};
	double ttfbSpikeMs{0.0};
	double meanThrMbps{200.0};
	double thrSigmaLn{0.8};
	double thrRho{0.15};
	int burstsPerSegment{4};
	double burstBytesCv{0.35};
	double cadenceMs{0.0};
	double cadenceJitterMs{0.0};
	double flushJitterMs{6.0};
	double lateChunkP{0.0};
	double lateChunkExtraMs{0.0};
	double pConnReuse{0.0};
	double newConnPenaltyMs{0.0};
	double capacityDropP{0.0};
	double capacityDropFactor{0.6};
	double rttInflationMs{0.0};
};

/// Fit request-side persona fields from accumulated RequestRecords.
static void FitRequests(const std::vector<RequestRecord>& requests, Persona& persona)
{
	if (requests.empty()) return;

	std::vector<double> reusedTtfbMs;
	std::vector<double> freshTtfbMs;
	std::vector<double> allTtfbMs;
	int reusedCount = 0;

	for (const auto& r : requests)
	{
		double ms = r.ttfbS * 1000.0;
		allTtfbMs.push_back(ms);
		if (r.connReused == 1)
		{
			reusedTtfbMs.push_back(ms);
			++reusedCount;
		}
		else
		{
			freshTtfbMs.push_back(ms);
		}
	}

	if (reusedTtfbMs.size() < 5)
	{
		persona.baseRttMs = Median(allTtfbMs);
		persona.rttJitterMs = RobustStd(allTtfbMs);
	}
	else
	{
		persona.baseRttMs = Median(reusedTtfbMs);
		persona.rttJitterMs = RobustStd(reusedTtfbMs);
	}

	persona.pConnReuse = static_cast<double>(reusedCount) / static_cast<double>(requests.size());

	if (reusedTtfbMs.size() >= 5 && freshTtfbMs.size() >= 3)
	{
		persona.newConnPenaltyMs = std::max(0.0, Median(freshTtfbMs) - Median(reusedTtfbMs));
	}
	else
	{
		persona.newConnPenaltyMs = std::max(0.0, persona.baseRttMs * 0.95);
	}

	// Tail spikes relative to reused baseline
	if (reusedTtfbMs.size() >= 20)
	{
		double p90 = Percentile(reusedTtfbMs, 90.0);
		std::vector<double> spikes;
		for (double v : reusedTtfbMs)
		{
			if (v > p90) spikes.push_back(v);
		}
		persona.ttfbSpikeP = static_cast<double>(spikes.size()) / static_cast<double>(reusedTtfbMs.size());
		if (!spikes.empty())
		{
			double sum = std::accumulate(spikes.begin(), spikes.end(), 0.0);
			persona.ttfbSpikeMs = (sum / static_cast<double>(spikes.size())) - persona.baseRttMs;
		}
	}
}

/// Fit burst-side persona fields from accumulated BurstRecords.
static void FitBursts(const std::vector<BurstRecord>& bursts, Persona& persona)
{
	if (bursts.empty()) return;

	constexpr double kGuardLow = 0.10;
	constexpr double kGuardHigh = 0.50;
	constexpr double kMinDuration = 1e-4;

	// Collect gaps and rates
	std::vector<double> allGaps;
	std::vector<double> guardedGaps;

	// Per-request grouping for bursts_per_segment and burst_bytes_cv
	std::map<uint64_t, int> maxBurstIdxPerReq;
	std::map<uint64_t, std::vector<double>> bytesPerReq;

	// Rates for AR(1) fitting
	struct RateEntry {
		uint64_t reqId;
		int burstIdx;
		double lnRate;
	};
	std::vector<RateEntry> rateEntries;

	for (const auto& b : bursts)
	{
		double dur = std::max(kMinDuration, b.durationS);
		double rateBps = static_cast<double>(b.bytes) / dur;

		allGaps.push_back(b.gapBeforeS);
		if (b.gapBeforeS >= kGuardLow && b.gapBeforeS <= kGuardHigh)
		{
			guardedGaps.push_back(b.gapBeforeS);
		}

		// Track max burst index per request
		auto it = maxBurstIdxPerReq.find(b.reqId);
		if (it == maxBurstIdxPerReq.end() || b.burstIdx > it->second)
		{
			maxBurstIdxPerReq[b.reqId] = b.burstIdx;
		}
		bytesPerReq[b.reqId].push_back(static_cast<double>(b.bytes));

		if (rateBps > 0.0)
		{
			rateEntries.push_back({b.reqId, b.burstIdx, std::log(rateBps)});
		}
	}

	// Cadence from guarded gaps
	const auto& cadenceSource = guardedGaps.empty() ? allGaps : guardedGaps;
	if (!cadenceSource.empty())
	{
		double sum = std::accumulate(cadenceSource.begin(), cadenceSource.end(), 0.0);
		persona.cadenceMs = (sum / static_cast<double>(cadenceSource.size())) * 1000.0;
		persona.cadenceJitterMs = SampleStd(cadenceSource) * 1000.0;
	}

	// Late chunk classification
	double lateThr = (persona.cadenceMs / 1000.0) + 2.0 * (persona.cadenceJitterMs / 1000.0);
	int lateCount = 0;
	std::vector<double> lateGaps;
	for (double g : allGaps)
	{
		if (g > lateThr)
		{
			++lateCount;
			lateGaps.push_back(g);
		}
	}
	persona.lateChunkP = static_cast<double>(lateCount) / static_cast<double>(allGaps.size());
	if (!lateGaps.empty())
	{
		double sum = std::accumulate(lateGaps.begin(), lateGaps.end(), 0.0);
		persona.lateChunkExtraMs = ((sum / static_cast<double>(lateGaps.size())) - (persona.cadenceMs / 1000.0)) * 1000.0;
	}

	// Bursts per segment: median of (max_burst_idx + 1) per request
	{
		std::vector<double> counts;
		for (const auto& kv : maxBurstIdxPerReq)
		{
			counts.push_back(static_cast<double>(kv.second + 1));
		}
		if (!counts.empty())
		{
			persona.burstsPerSegment = static_cast<int>(std::round(Median(counts)));
			if (persona.burstsPerSegment < 1) persona.burstsPerSegment = 1;
		}
	}

	// Burst bytes CV per request, then median across requests
	{
		std::vector<double> cvSeries;
		for (const auto& kv : bytesPerReq)
		{
			const auto& arr = kv.second;
			if (arr.size() < 2) continue;
			double mean = std::accumulate(arr.begin(), arr.end(), 0.0) / static_cast<double>(arr.size());
			if (mean <= 0.0) continue;
			double sumSq = 0.0;
			for (double v : arr)
			{
				double d = v - mean;
				sumSq += d * d;
			}
			double sd = std::sqrt(sumSq / static_cast<double>(arr.size() - 1));
			double cv = sd / mean;
			if (std::isfinite(cv))
			{
				cvSeries.push_back(cv);
			}
		}
		if (!cvSeries.empty())
		{
			persona.burstBytesCv = Median(cvSeries);
		}
	}

	// AR(1) on ln(rate) — sort by (reqId, burstIdx) then fit
	if (rateEntries.size() >= 3)
	{
		std::sort(rateEntries.begin(), rateEntries.end(),
			[](const RateEntry& a, const RateEntry& b) {
				if (a.reqId != b.reqId) return a.reqId < b.reqId;
				return a.burstIdx < b.burstIdx;
			});

		std::vector<double> lnRates;
		lnRates.reserve(rateEntries.size());
		for (const auto& e : rateEntries)
		{
			lnRates.push_back(e.lnRate);
		}

		auto ar = FitAr1(lnRates);
		double muHat = (std::abs(1.0 - ar.rho) > 1e-6)
			? ar.c / (1.0 - ar.rho)
			: 0.0;

		if (std::isfinite(muHat) && muHat != 0.0)
		{
			// Convert geometric mean B/s to Mb/s
			persona.meanThrMbps = std::exp(muHat) * 8.0 / 1e6;
		}
		persona.thrSigmaLn = ar.sigmaEps;
		persona.thrRho = ar.rho;
	}
}

// ======================== Public API ========================

NetPersonaFitter& NetPersonaFitter::GetInstance()
{
	static NetPersonaFitter instance;
	return instance;
}

void NetPersonaFitter::AddRequest(double ttfbS, int connReused)
{
	std::lock_guard<std::mutex> lock{mMutex};
	mRequests.push_back({ttfbS, connReused});
	if (!mAtExitRegistered)
	{
		std::atexit(AtExitHandler);
		mAtExitRegistered = true;
	}
}

void NetPersonaFitter::AddBurst(uint64_t reqId, int burstIdx,
								double durationS, std::size_t bytes,
								double gapBeforeS)
{
	std::lock_guard<std::mutex> lock{mMutex};
	mBursts.push_back({reqId, burstIdx, durationS, bytes, gapBeforeS});
}

std::size_t NetPersonaFitter::GetRequestCount() const
{
	std::lock_guard<std::mutex> lock{mMutex};
	return mRequests.size();
}

std::size_t NetPersonaFitter::GetBurstCount() const
{
	std::lock_guard<std::mutex> lock{mMutex};
	return mBursts.size();
}

bool NetPersonaFitter::GeneratePersonaJson(const std::string& basePath) const
{
	std::vector<RequestRecord> requests;
	std::vector<BurstRecord> bursts;

	{
		std::lock_guard<std::mutex> lock{mMutex};
		if (mRequests.empty() && mBursts.empty())
		{
			AAMPLOG_WARN("NetPersonaFitter: no data collected, skipping persona generation");
			return false;
		}
		requests = mRequests;
		bursts = mBursts;
	}

	Persona persona;
	FitRequests(requests, persona);
	FitBursts(bursts, persona);

	// Write persona JSON using stream formatting (no external JSON library required)
	std::string outputPath = basePath + "." + std::to_string(getpid());
	std::ofstream ofs{outputPath, std::ios::trunc};
	if (!ofs.is_open())
	{
		AAMPLOG_ERR("NetPersonaFitter: failed to open %s for writing", outputPath.c_str());
		return false;
	}

	ofs << std::setprecision(15);
	ofs << "{\n"
		<< "\t\"base_rtt_ms\": "         << persona.baseRttMs         << ",\n"
		<< "\t\"rtt_jitter_ms\": "       << persona.rttJitterMs        << ",\n"
		<< "\t\"ttfb_spike_p\": "        << persona.ttfbSpikeP         << ",\n"
		<< "\t\"ttfb_spike_ms\": "       << persona.ttfbSpikeMs        << ",\n"
		<< "\t\"mean_thr_mbps\": "       << persona.meanThrMbps        << ",\n"
		<< "\t\"thr_sigma_ln\": "        << persona.thrSigmaLn         << ",\n"
		<< "\t\"thr_rho\": "             << persona.thrRho             << ",\n"
		<< "\t\"bursts_per_segment\": "  << persona.burstsPerSegment   << ",\n"
		<< "\t\"burst_bytes_cv\": "      << persona.burstBytesCv       << ",\n"
		<< "\t\"cadence_ms\": "          << persona.cadenceMs          << ",\n"
		<< "\t\"cadence_jitter_ms\": "   << persona.cadenceJitterMs    << ",\n"
		<< "\t\"flush_jitter_ms\": "     << persona.flushJitterMs      << ",\n"
		<< "\t\"late_chunk_p\": "        << persona.lateChunkP         << ",\n"
		<< "\t\"late_chunk_extra_ms\": " << persona.lateChunkExtraMs   << ",\n"
		<< "\t\"p_conn_reuse\": "        << persona.pConnReuse         << ",\n"
		<< "\t\"new_conn_penalty_ms\": " << persona.newConnPenaltyMs   << ",\n"
		<< "\t\"capacity_drop_p\": "     << persona.capacityDropP      << ",\n"
		<< "\t\"capacity_drop_factor\": "<< persona.capacityDropFactor << ",\n"
		<< "\t\"rtt_inflation_ms\": "    << persona.rttInflationMs     << "\n"
		<< "}\n";

	if (!ofs)
	{
		AAMPLOG_ERR("NetPersonaFitter: write error for %s", outputPath.c_str());
		return false;
	}
	ofs.close();

	AAMPLOG_MIL("NetPersonaFitter: wrote persona JSON to %s (%zu requests, %zu bursts)",
				outputPath.c_str(), requests.size(), bursts.size());
	return true;
}

void NetPersonaFitter::AtExitHandler()
{
	GetInstance().GeneratePersonaJson(kDefaultBasePath);
}

} // namespace aamptrace
