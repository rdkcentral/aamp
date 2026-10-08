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
 * @file net_persona_fitter.h
 * @brief Fits a network persona JSON from in-memory request/burst trace data
 *
 * Purpose: C++ re-implementation of simnet/simnet/persona_fit.py. Accumulates
 * request and burst records from NetTrace::FlushPersona() and, on demand,
 * computes a 19-field persona JSON describing RTT, throughput, cadence,
 * and burst characteristics for the LL-DASH network simulator (simnet).
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

// Installed alongside this public header (see simnet/CMakeLists.txt); resolved
// via an include path rather than a relative one so installed consumers compile.
#include "AampTimingHistogram.h"

namespace aamptrace {

/**
 * @struct RequestRecord
 * @brief Lightweight record of per-request metrics relevant to persona fitting
 */
struct RequestRecord {
	double ttfbS{0.0};		///< Time to first byte (seconds)
	int connReused{0};		///< 1 if connection was reused, 0 otherwise
};

/**
 * @struct BurstRecord
 * @brief Lightweight record of per-burst metrics relevant to persona fitting
 */
struct BurstRecord {
	uint64_t reqId{0};		///< Parent request identifier
	int burstIdx{0};		///< Burst index within the request
	double durationS{0.0};	///< Burst duration (seconds, >= 1 ms floor)
	std::size_t bytes{0};	///< Bytes received in this burst
	double gapBeforeS{0.0};	///< Idle gap preceding this burst (seconds)
};

/**
 * @class NetPersonaFitter
 * @brief Accumulates network trace data and generates a persona JSON file
 *
 * Purpose: Provides a process-wide singleton that collects request/burst
 * records from NetTrace::FlushPersona() calls. When GeneratePersonaJson() is
 * invoked (typically at player Stop()), it performs statistical fitting
 * identical to persona_fit.py and writes the result as JSON.
 *
 * Thread Safety: All public methods are protected by an internal mutex.
 */
class NetPersonaFitter {
public:
	/// Default output path for persona JSON (PID is appended at write time)
	static constexpr const char* kDefaultBasePath = "/tmp/aamp_net_persona.json";

	/**
	 * @brief Access the process-wide singleton instance
	 * @return Reference to the singleton NetPersonaFitter
	 */
	static NetPersonaFitter& GetInstance();

	/// Non-copyable, non-movable
	NetPersonaFitter(const NetPersonaFitter&) = delete;
	NetPersonaFitter& operator=(const NetPersonaFitter&) = delete;

	/**
	 * @brief Record a completed HTTP request's metrics
	 *
	 * Streaming O(1) accumulators are always updated. The full RequestRecord is
	 * appended (for the file-based persona) only when keepRecord is true.
	 *
	 * @param[in] ttfbS Time to first byte (seconds)
	 * @param[in] connReused 1 if connection was reused, 0 otherwise
	 * @param[in] keepRecord Append the full record for file-persona fitting
	 */
	void AddRequest(double ttfbS, int connReused, bool keepRecord = true);

	/**
	 * @brief Record a single burst from a completed request
	 *
	 * Streaming O(1) accumulators are always updated. The full BurstRecord is
	 * appended (for the file-based persona) only when keepRecord is true.
	 *
	 * @param[in] reqId Parent request identifier
	 * @param[in] burstIdx Burst index within the request
	 * @param[in] durationS Burst duration (seconds)
	 * @param[in] bytes Bytes received in this burst
	 * @param[in] gapBeforeS Idle gap preceding this burst (seconds)
	 * @param[in] keepRecord Append the full record for file-persona fitting
	 */
	void AddBurst(uint64_t reqId, int burstIdx,
				  double durationS, std::size_t bytes, double gapBeforeS,
				  bool keepRecord = true);

	/**
	 * @brief Record a completed request's burst-group summary (O(1))
	 *
	 * Purpose: Feeds the per-request burst count and byte-size distribution into
	 * the fixed-memory histograms backing the bursts_per_segment and
	 * burst_bytes_cv persona fields. Unlike AddBurst(), which is order- and
	 * group-independent, these fields require correct per-request grouping;
	 * the caller (NetTrace::FlushPersona) already owns one request's complete burst
	 * set, so it summarizes it in a single atomic call. This keeps the grouping
	 * correct even when multiple media tracks flush concurrently, with bounded
	 * memory (no per-request map retained).
	 *
	 * Mirrors FitBursts(): requests with < 2 bursts contribute a 0.0 CV; a
	 * request whose mean byte size is non-positive is skipped for CV.
	 *
	 * @param[in] burstCount Number of bursts in the request (>= 1)
	 * @param[in] bytesSum   Sum of per-burst byte counts
	 * @param[in] bytesSumSq Sum of per-burst byte counts squared
	 */
	void AddRequestBurstSummary(std::size_t burstCount, double bytesSum, double bytesSumSq);

	/**
	 * @brief Build a minimal persona JSON from O(1) streaming accumulators
	 *
	 * Purpose: Produces a single-line JSON string from fixed-memory histograms
	 * and running sums. It omits order-dependent fields such as throughput
	 * autocorrelation and the static default fields. Intended for inline logging
	 * without creating any file.
	 *
	 * @return Compact JSON string, or an empty string if no data was collected
	 */
	std::string BuildMinimalPersonaJson() const;

	/**
	 * @brief Reset the O(1) streaming accumulators to start a fresh session
	 */
	void ResetStreaming();

	/**
	 * @brief Fit persona model and write JSON to disk
	 *
	 * Purpose: Performs statistical fitting on accumulated data and writes
	 * a 19-field persona JSON. The output filename is suffixed with the
	 * process ID (e.g., /tmp/aamp_net_persona.json.12345).
	 *
	 * Note: This method is deliberately non-const. It swaps out (consumes)
	 * the accumulated request/burst vectors in O(1) under the mutex, then
	 * performs O(N) statistical fitting lock-free. After the first call the
	 * vectors are empty; subsequent calls (e.g., the atexit safety-net) will
	 * log nothing and return false without noisy warnings.
	 *
	 * @param[in] basePath Base output path (PID is appended)
	 * @return true if JSON was written successfully, false on error or
	 *         insufficient data
	 */
	bool GeneratePersonaJson(const std::string& basePath);

	/**
	 * @brief Atomically finalize the aggregate session (exactly-once)
	 *
	 * Purpose: End-of-session counterpart used when the last active player
	 * stops. Under a single mutex acquisition it snapshots the inline persona,
	 * consumes the file-persona records, and clears all state; the file persona
	 * (when @p basePath is non-empty) is then written outside the lock.
	 *
	 * Exactly-once semantics: because the snapshot and reset are atomic, a
	 * concurrent last-stopper that calls this after the accumulators have been
	 * cleared observes no data and returns an empty string without emitting or
	 * writing anything. Callers must therefore log the returned JSON only when
	 * it is non-empty.
	 *
	 * @param[in] basePath Base output path for the file persona (PID appended),
	 *                     or an empty string to skip the file persona.
	 * @return Inline minimal persona JSON, or an empty string when there was no
	 *         streaming data (e.g. already finalized by a concurrent caller).
	 */
	std::string FinalizeSession(const std::string& basePath);

	/**
	 * @brief Return the number of accumulated request records
	 * @return Request count
	 */
	std::size_t GetRequestCount() const;

	/**
	 * @brief Return the number of accumulated burst records
	 * @return Burst count
	 */
	std::size_t GetBurstCount() const;

	/**
	 * @brief Reset all accumulated state to a fresh, first-use condition
	 *
	 * Purpose: Clears the file-persona record vectors, the one-shot
	 * generation guard, and the streaming accumulators. Primarily used by
	 * tests to isolate the process-wide singleton between cases.
	 */
	void Reset();

private:
	NetPersonaFitter() = default;

	/**
	 * @brief AtExit callback — writes persona JSON on process exit
	 *
	 * Purpose: Safety net for abrupt termination. Ensures persona data
	 * is persisted even if Stop() is never called. Registered once on
	 * the first AddRequest() call.
	 */
	static void AtExitHandler();

	/// Lock-free body of BuildMinimalPersonaJson(); caller must hold mMutex.
	std::string BuildMinimalPersonaJsonLocked() const;

	/// Lock-free body of ResetStreaming(); caller must hold mMutex.
	void ResetStreamingLocked();

	/// Lock-free full reset (records, generation guard, streaming accumulators);
	/// caller must hold mMutex.
	void ResetLocked();

	/// Fit the consumed records and write the file persona (PID appended to
	/// basePath). Does no locking; operates only on its own arguments.
	bool WritePersonaFile(const std::vector<RequestRecord>& requests,
						  const std::vector<BurstRecord>& bursts,
						  const std::string& basePath) const;

	mutable std::mutex mMutex;				///< Protects all mutable state below
	std::vector<RequestRecord> mRequests;	///< Consumed (swapped out) on first GeneratePersonaJson call
	std::vector<BurstRecord> mBursts;		///< Consumed (swapped out) on first GeneratePersonaJson call
	bool mAtExitRegistered{false};			///< True after atexit() has been registered
	bool mGenerated{false};					///< True after GeneratePersonaJson has successfully run once

	// O(1) streaming accumulators for the inline minimal persona (bounded space).
	// Request-side: connection reuse fraction.
	std::size_t mStreamReqCount{0};			///< Number of requests seen since last reset
	std::size_t mStreamReuseCount{0};		///< Number of reused-connection requests
	// TTFB distribution — fixed-memory histograms backing the median/percentile
	// RTT fields (base_rtt_ms, rtt_jitter_ms, new_conn_penalty_ms) so these can
	// be produced from the inline minimal persona without retaining raw samples.
	static constexpr double kTtfbBucketMs = 1.0;	///< TTFB histogram resolution (ms)
	static constexpr double kTtfbMaxMs = 3000.0;	///< TTFB histogram upper edge (ms)
	AampTimingHistogram mStreamAllTtfbHist{kTtfbBucketMs, kTtfbMaxMs};		///< All TTFB samples
	AampTimingHistogram mStreamReusedTtfbHist{kTtfbBucketMs, kTtfbMaxMs};	///< Reused-conn TTFB samples
	AampTimingHistogram mStreamFreshTtfbHist{kTtfbBucketMs, kTtfbMaxMs};		///< Fresh-conn TTFB samples
	// Running sum/sumSq for the RobustStd sample-std fallback (IQR <= 0 case).
	double mStreamAllTtfbSum{0.0};			///< Sum of all TTFB (ms)
	double mStreamAllTtfbSumSq{0.0};		///< Sum of all TTFB^2 (ms^2)
	double mStreamReusedTtfbSum{0.0};		///< Sum of reused-conn TTFB (ms)
	double mStreamReusedTtfbSumSq{0.0};		///< Sum of reused-conn TTFB^2 (ms^2)
	// Burst-shape distributions — fixed-memory histograms backing the median
	// burst fields (bursts_per_segment, burst_bytes_cv), populated once per
	// request by AddRequestBurstSummary().
	static constexpr double kBurstsPerSegBucket = 1.0;		///< Burst-count resolution
	static constexpr double kBurstsPerSegMax = 1024.0;		///< Burst-count upper edge
	static constexpr double kBurstCvBucket = 0.01;			///< CV resolution
	static constexpr double kBurstCvMax = 10.0;				///< CV upper edge
	AampTimingHistogram mStreamBurstsPerSegHist{kBurstsPerSegBucket, kBurstsPerSegMax};	///< Per-request burst counts
	AampTimingHistogram mStreamBurstCvHist{kBurstCvBucket, kBurstCvMax};				///< Per-request byte-size CVs
	// Inter-burst gap distribution (ms) — backs the tail fields late_chunk_p and
	// late_chunk_extra_ms, which count/average gaps above a dynamic threshold.
	static constexpr double kGapBucketMs = 5.0;			///< Gap histogram resolution (ms)
	static constexpr double kGapMaxMs = 10000.0;		///< Gap histogram upper edge (ms)
	AampTimingHistogram mStreamAllGapHist{kGapBucketMs, kGapMaxMs};	///< All inter-burst gaps (ms)
	// Burst throughput: geometric mean and log-normal spread of per-burst rate.
	std::size_t mStreamLnRateN{0};			///< Count of bursts with a positive rate
	double mStreamLnRateSum{0.0};			///< Sum of ln(rate) over bursts
	double mStreamLnRateSumSq{0.0};			///< Sum of ln(rate)^2 over bursts
	// Inter-burst gaps within the guard band (0.10..0.50 s) for cadence.
	std::size_t mStreamGuardGapN{0};		///< Count of guard-band gaps
	double mStreamGuardGapSum{0.0};			///< Sum of guard-band gaps (seconds)
	double mStreamGuardGapSumSq{0.0};		///< Sum of guard-band gaps^2
	// All gaps — fallback cadence source when no guard-band gaps were seen.
	std::size_t mStreamAllGapN{0};			///< Count of all gaps
	double mStreamAllGapSum{0.0};			///< Sum of all gaps (seconds)
	double mStreamAllGapSumSq{0.0};			///< Sum of all gaps^2
};

} // namespace aamptrace
