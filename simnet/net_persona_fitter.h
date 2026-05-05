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
 * request and burst records from NetTrace::FlushCsv() and, on demand,
 * computes a 19-field persona JSON describing RTT, throughput, cadence,
 * and burst characteristics for the LL-DASH network simulator (simnet).
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

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
 * records from NetTrace::FlushCsv() calls. When GeneratePersonaJson() is
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
	 * @param[in] ttfbS Time to first byte (seconds)
	 * @param[in] connReused 1 if connection was reused, 0 otherwise
	 */
	void AddRequest(double ttfbS, int connReused);

	/**
	 * @brief Record a single burst from a completed request
	 * @param[in] reqId Parent request identifier
	 * @param[in] burstIdx Burst index within the request
	 * @param[in] durationS Burst duration (seconds)
	 * @param[in] bytes Bytes received in this burst
	 * @param[in] gapBeforeS Idle gap preceding this burst (seconds)
	 */
	void AddBurst(uint64_t reqId, int burstIdx,
				  double durationS, std::size_t bytes, double gapBeforeS);

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
	 * @brief Return the number of accumulated request records
	 * @return Request count
	 */
	std::size_t GetRequestCount() const;

	/**
	 * @brief Return the number of accumulated burst records
	 * @return Burst count
	 */
	std::size_t GetBurstCount() const;

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

	mutable std::mutex mMutex;				///< Protects all mutable state below
	std::vector<RequestRecord> mRequests;	///< Consumed (swapped out) on first GeneratePersonaJson call
	std::vector<BurstRecord> mBursts;		///< Consumed (swapped out) on first GeneratePersonaJson call
	bool mAtExitRegistered{false};			///< True after atexit() has been registered
	bool mGenerated{false};					///< True after GeneratePersonaJson has successfully run once
};

} // namespace aamptrace
