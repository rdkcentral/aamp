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
// net_trace.h
#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <atomic>
#include <mutex>
#include <fstream>
#include <chrono>
#include <cmath>
#include <cstdint>
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>   // getpid()
#endif

namespace aamptrace {

static inline double now_monotonic_s() {
	using C = std::chrono::steady_clock;
	auto t = C::now().time_since_epoch();
	return std::chrono::duration<double>(t).count();
}

/**
 * @struct Burst
 * @brief Represents a single data burst within a network request
 * 
 * Purpose: Captures timing and size metrics for individual bursts of data received
 * during a network download. Bursts are separated by idle gaps in the write callback stream.
 */
struct Burst {
	int    index = 0;            ///< Burst index within the request
	double startTime = 0.0;      ///< Burst start time relative to request start (seconds)
	double duration = 0.0;       ///< Burst duration (seconds)
	size_t bytes = 0;            ///< Total bytes received in this burst
	double gapBefore = 0.0;      ///< Idle gap before this burst (seconds)
	bool   isLate = false;       ///< True if gapBefore exceeds late threshold
};

/**
 * @class NetTrace
 * @brief Network activity tracer for AAMP HTTP requests
 * 
 * Purpose: Instruments network downloads to capture detailed timing, burst patterns,
 * and throughput metrics. Records data ingress timing from curl callbacks and produces
 * CSV output for performance analysis and network persona modeling.
 * 
 * Thread Safety: Individual NetTrace instances are not thread-safe. File I/O is
 * protected by internal mutex in the shared FileState singleton.
 */
class NetTrace {
public:
	/// Minimum non-zero duration assigned to any burst (1 ms)
	static constexpr double kMinBurstDurS = 0.001;
	
	/**
	 * @brief Construct a network trace recorder for a single HTTP request
	 * 
	 * @param[in] req_id Unique request identifier
	 * @param[in] url_path URL path component (after domain)
	 * @param[in] media_type Media type string ("video", "audio", "manifest", etc.)
	 * @param[in] chunked_hdr_seen True if Transfer-Encoding: chunked header is present
	 * @param[in] gap_threshold_s Minimum idle time to split bursts (seconds)
	 * @param[in] late_gap_extra_s_threshold Gap threshold to mark bursts as "late" (seconds)
	 */
	explicit NetTrace(uint64_t req_id,
					  std::string_view url_path,
					  std::string_view media_type,
					  bool chunked_hdr_seen,
					  double gap_threshold_s,
					  double late_gap_extra_s_threshold)
	: mReqId(req_id),
	mUrlPath(url_path),
	mMediaType(media_type),
	mGapThresholdS(gap_threshold_s),
	mLateExtraThresholdS(late_gap_extra_s_threshold),
	mT0(now_monotonic_s()),
	mChunkedHdrSeen(chunked_hdr_seen) {}
	
	/**
	 * @brief Mark this request as using chunked transfer encoding
	 * 
	 * Purpose: Called from curl header callback when Transfer-Encoding: chunked is detected.
	 * This metadata is recorded in the request CSV.
	 */
	void MarkChunked() { mChunkedHdrSeen = true; }
	
	/**
	 * @brief Record data ingress from curl write callback
	 * 
	 * Purpose: Tracks byte arrival timing and automatically splits bursts when idle
	 * gaps exceed the configured threshold. Must be called for each write callback.
	 * 
	 * @param[in] num_bytes Number of bytes received in this callback
	 * @param[in] t_now_s Current monotonic timestamp (seconds)
	 * @return True if a new burst was started, false if continuing existing burst
	 */
	bool OnWrite(size_t num_bytes, double t_now_s) {
		if (mFirstPayloadTimeS < 0) mFirstPayloadTimeS = t_now_s;
		bool new_burst = false;
		if (!mInBurst) {
			OpenBurst(t_now_s, /*gap_before*/ mLastEndTimeS > 0 ? std::max(0.0, t_now_s - mLastEndTimeS) : 0.0);
			new_burst = true;
		} else {
			// detect split if there's an idle gap mid-write stream
			double idle = mLastCbTimeS > 0 ? std::max(0.0, t_now_s - mLastCbTimeS) : 0.0;
			if (idle > mGapThresholdS) { // close previous, open new
				// Close the previous burst at the *current* ingress time to avoid 0-span bursts.
				CloseBurst(t_now_s);
				OpenBurst(t_now_s, idle);
				new_burst = true;
			}
		}
		// account
		if (!mBursts.empty()) {
			mBursts.back().bytes += num_bytes;
		}
		mLastCbTimeS = t_now_s;
		return new_burst;
	}
	
	/**
	 * @brief Finalize burst recording at end of data transfer
	 * 
	 * Purpose: Closes the final burst and applies minimum duration floor if needed.
	 * Must be called after all write callbacks complete.
	 */
	void OnCompleteBytes() {
		if (mInBurst) {
			// If we only saw a single callback, mLastCbTimeS can equal open time.
			// Use a minimal non-zero duration floor.
			double t_end = mLastCbTimeS;
			if (t_end <= 0.0) t_end = now_monotonic_s();
			CloseBurst(t_end);
		}
	}
	
	/**
	 * @brief Record curl timing metrics after request completion
	 * 
	 * Purpose: Captures HTTP/TCP/TLS timing details from curl_easy_getinfo() calls.
	 * These timings are written to the request CSV row.
	 * 
	 * @param[in] name_s DNS lookup time (seconds)
	 * @param[in] connect_s TCP connect time (seconds)
	 * @param[in] appconnect_s TLS handshake time (seconds)
	 * @param[in] pre_xfer_s Time until transfer ready (seconds)
	 * @param[in] start_xfer_s Time to first byte / TTFB (seconds)
	 * @param[in] total_s Total request time (seconds)
	 * @param[in] http_code HTTP response code
	 * @param[in] conn_reused True if connection was reused from pool
	 * @param[in] primary_ip Server IP address
	 * @param[in] local_port Local port number
	 * @param[in] bytes_total Total bytes transferred
	 */
	void SetCurlTimings(double name_s, double connect_s, double appconnect_s,
						  double pre_xfer_s, double start_xfer_s, double total_s,
						  long http_code, bool conn_reused,
						  const std::string& primary_ip, long local_port,
						  size_t bytes_total) {
		mNameS = name_s; mConnectS = connect_s; mAppconnectS = appconnect_s;
		mPreXferS = pre_xfer_s; mStartXferS = start_xfer_s; mTotalS = total_s;
		mHttpCode = http_code; mConnReused = conn_reused ? 1 : 0;
		mPrimaryIp = primary_ip; mLocalPort = local_port;
		mBytesTotal = bytes_total;
		mTotalDoneTimeS = now_monotonic_s();
	}
	
	/**
	 * @brief Write collected metrics to CSV files
	 * 
	 * Purpose: Outputs one row to the requests CSV (aggregated metrics) and
	 * multiple rows to the bursts CSV (per-burst details). Files are created
	 * with headers on first write and appended thereafter.
	 * 
	 * Thread Safety: Protected by mutex in shared FileState.
	 */
	void FlushCsv() {
		EnsureFilesOpen();
		auto& state = GetFileState();
		std::lock_guard<std::mutex> g(state.mutex);
		
		// aggregate
		double gap_time_s = 0, burst_time_s = 0; int late_count = 0; size_t bytes = 0;
		for (auto& b : mBursts) {
			gap_time_s   += b.gapBefore;
			burst_time_s += b.duration;
			bytes        += b.bytes;
			late_count   += b.isLate ? 1 : 0;
		}
		double avg_burst_rate_Bps = (burst_time_s > 0) ? (static_cast<double>(bytes) / burst_time_s) : 0.0;
		
		// request row
		state.req_ofs <<
		mReqId << ',' << mT0 << ',' << mUrlPath << ',' << mMediaType << ',' <<
		mBytesTotal << ',' << mHttpCode << ',' << mConnReused << ',' <<
		mPrimaryIp << ',' << mLocalPort << ',' <<
		mStartXferS << ',' << mTotalS << ',' <<
		mNameS << ',' << mConnectS << ',' << mAppconnectS << ',' <<
		mPreXferS << ',' << mRedirectS << ',' <<
		(mChunkedHdrSeen ? 1 : 0) << ',' <<
		gap_time_s << ',' << burst_time_s << ',' <<
		mBursts.size() << ',' << late_count << ',' << avg_burst_rate_Bps <<
		'\n';
		
		// burst rows
		for (auto& b : mBursts) {
			state.burst_ofs << mReqId << ',' << b.index << ',' << b.startTime << ',' <<
			b.duration << ',' << b.bytes << ',' << b.gapBefore << ',' <<
			(b.isLate ? "late" : "normal") << '\n';
		}
		state.req_ofs.flush();
		state.burst_ofs.flush();
	}
	
	/**
	 * @brief Reclassify bursts as "late" based on expected cadence
	 * 
	 * Purpose: Optional post-processing to mark bursts with gaps exceeding
	 * cadence + 2*jitter as late. Overrides the constructor's late threshold.
	 * 
	 * @param[in] cadence_s Expected inter-burst cadence (seconds)
	 * @param[in] jitter_s Expected jitter/variance (seconds)
	 */
	void ClassifyGaps(double cadence_s, double jitter_s) {
		double late_thr = cadence_s + 2.0*std::max(0.010, jitter_s);
		for (auto& b : mBursts) {
			if (b.gapBefore > late_thr) b.isLate = true;
		}
	}
	
	/**
	 * @brief Configure per-process CSV file paths
	 * 
	 * Purpose: Appends process ID to file paths to prevent cross-process interleaving.
	 * For example: /tmp/aamp_net_requests.csv becomes /tmp/aamp_net_requests.csv.12345
	 * 
	 * Must be called once before any NetTrace objects flush data. Typically called
	 * during initialization via std::call_once.
	 * 
	 * @param[in] req_path Base path for requests CSV file
	 * @param[in] burst_path Base path for bursts CSV file
	 * 
	 * Thread Safety: Protected by internal mutex.
	 */
	static void SetPathsWithPid(const std::string& req_path, const std::string& burst_path) {
		pid_t pid = getpid();
		auto& state = GetFileState();
		std::lock_guard<std::mutex> g(state.mutex);
		state.req_path = req_path + "." + std::to_string(pid);
		state.burst_path = burst_path + "." + std::to_string(pid);
	}
	
private:
	// Meyer's singleton pattern for shared file state
	struct FileState {
		std::mutex mutex;
		std::string req_path = "/tmp/aamp_net_requests.csv";
		std::string burst_path = "/tmp/aamp_net_bursts.csv";
		std::ofstream req_ofs;
		std::ofstream burst_ofs;
	};
	
	/**
	 * @brief Access the singleton FileState instance
	 * 
	 * Purpose: Provides thread-safe access to shared file state using Meyer's singleton.
	 * All NetTrace instances write to the same CSV files within a process.
	 * 
	 * @return Reference to the singleton FileState instance
	 */
	static FileState& GetFileState() {
		static FileState state;
		return state;
	}
	
	/**
	 * @brief Start a new burst record
	 * 
	 * Purpose: Creates a new Burst entry and marks it as late if the gap before
	 * exceeds the configured threshold. Called when first write callback arrives
	 * or when idle gap exceeds gap_threshold_s.
	 * 
	 * @param[in] t_start_abs_s Absolute monotonic start time (seconds)
	 * @param[in] gap_before_s Idle gap duration before this burst (seconds)
	 */
	void OpenBurst(double t_start_abs_s, double gap_before_s) {
		mInBurst = true;
		Burst b;
		b.index = int(mBursts.size());
		b.startTime = t_start_abs_s - mT0;
		b.gapBefore = gap_before_s;
		// Mark burst as late if gap exceeds the configured threshold
		b.isLate = (gap_before_s > mLateExtraThresholdS);
		mBursts.push_back(b);
	}
	
	/**
	 * @brief Close the current burst record
	 * 
	 * Purpose: Finalizes the active burst by computing its duration with a minimum
	 * floor to avoid zero-duration bursts. Updates mLastEndTimeS for gap calculation.
	 * 
	 * @param[in] t_end_abs_s Absolute monotonic end time (seconds)
	 */
	void CloseBurst(double t_end_abs_s) {
		if (!mInBurst || mBursts.empty()) return;
		auto &b = mBursts.back();
		// Floor durations to avoid zero-time bursts from single write callbacks.
		double raw = t_end_abs_s - (mT0 + b.startTime);
		if (raw < kMinBurstDurS) raw = kMinBurstDurS;
		b.duration = raw;
		mLastEndTimeS = t_end_abs_s;
		mInBurst = false;
	}
	
	/**
	 * @brief Open CSV output files if not already open
	 * 
	 * Purpose: Lazily opens requests and bursts CSV files and writes headers if
	 * files are new. Uses append mode to preserve existing data. Thread-safe via
	 * mutex protection.
	 * 
	 * Thread Safety: Protected by FileState::mutex
	 */
	static void EnsureFilesOpen() {
		auto& state = GetFileState();
		std::lock_guard<std::mutex> g(state.mutex);
		if (!state.req_ofs.is_open()) {
			state.req_ofs.open(state.req_path, std::ios::app);
			if (state.req_ofs.tellp() == 0) {
				state.req_ofs << "req_id,when_start_s,url_path,media_type,bytes_total,http_code,conn_reused,primary_ip,local_port,ttfb_s,total_s,namelookup_s,connect_s,appconnect_s,pretransfer_s,redirect_s,chunked,gap_time_s,burst_time_s,burst_count,late_gap_count,avg_burst_rate_Bps\n";
			}
		}
		if (!state.burst_ofs.is_open()) {
			state.burst_ofs.open(state.burst_path, std::ios::app);
			if (state.burst_ofs.tellp() == 0) {
				state.burst_ofs << "req_id,burst_idx,t_start_s,duration_s,bytes,gap_before_s,class\n";
			}
		}
	}
	
	// request identity
	uint64_t mReqId;
	std::string mUrlPath, mMediaType;
	double mT0;
	bool mChunkedHdrSeen;
	
	// write/burst state
	bool   mInBurst = false;
	double mLastCbTimeS = 0.0;
	double mLastEndTimeS = 0.0;
	double mFirstPayloadTimeS = -1.0;
	double mGapThresholdS;
	double mLateExtraThresholdS;
	std::vector<Burst> mBursts;
	
	// curl results
	long   mHttpCode = -1;
	int    mConnReused = 0;
	std::string mPrimaryIp;
	long   mLocalPort = 0;
	size_t mBytesTotal = 0;
	double mNameS=0, mConnectS=0, mAppconnectS=0, mPreXferS=0, mStartXferS=0, mTotalS=0, mRedirectS=0;
	double mTotalDoneTimeS = 0.0;
};

} // namespace aamptrace
