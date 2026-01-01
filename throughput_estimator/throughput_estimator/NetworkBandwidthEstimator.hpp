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
#ifndef NETWORK_BANDWIDTH_ESTIMATOR
#define NETWORK_BANDWIDTH_ESTIMATOR

#include <cstddef>
#include <vector>

double GetCurrentTimeMonotonicSeconds( void );

class CurlInfo
{
public:
	size_t size_download_bytes;
	double total_time_seconds;
	double time_to_first_byte_seconds;
};

/**
 * @brief encapsulate performance information for a given http download
 */
class Sample
{
private:
	CurlInfo curlInfo;
	
	// total_time - time_to_first_byte
	double payload_download_time_seconds = 0.0;
	
	// size_download / payload_download_time
	double payload_bytes_per_second = 0.0;
	
public:
	/**
	 * @brief constructor - populate sample metrics
	 * @param curlInfo  Curl Handle profiling data
	 */
	Sample( const CurlInfo &curlInfo );
	double getTimeToFirstByteSeconds( void ) const;
	double getPayloadBytesPerSecond( void ) const;
	double getTotalTimeSeconds( void ) const;
};

/**
 * @brief abstract network bandwith state and prediction logic
 */
class NetworkBandwidthEstimator
{
private:
	// Rolling history & stats
	std::vector<Sample> history;
	
	// Robust per-request overhead Time to First Byte (TTFB) estimate
	double estimated_TTFB_seconds = 0.0; // median TTFB - computed brute force
	
	// Robust throughput estimates (bytes/s)
	double EWMA_fast_BytesPerSecond = 0.0; // reacts quickly
	double EWMA_slow_BytesPerSecond = 0.0; // stable
	double harmonic_BytesPerSecond = 0.0;  // conservative
	
	// Exponentially Weighted Moving Average (EWMA) tuning
	static constexpr double ALPHA_FAST = 0.5;
	static constexpr double ALPHA_SLOW = 0.2;
	// Harmonic mean over last N samples
	static constexpr size_t harmonic_window = 8;
	
	/**
	 * @brief Recompute median TTFB and harmonic mean from history; this requires iterating through all recent samples
	 */
	void RecomputeHarmonicMeanAndMedianTTFB( void );
	
public:
	NetworkBandwidthEstimator() = default;
	
	double UpdateDownloadMetrics( const CurlInfo &curl );
	
	/**
	 * @brief return current robust throughput estimate (bytes/s), buffer-agnostic
	 */
	double GetThroughputBytesPerSecond() const;
	
	/**
	 * @brief return current overhead (TTFB) estimate (seconds)
	 */
	double GetTimeToFirstByteSeconds() const;
	
	/**
	 * @brief predict completion time for a new segment
	 */
	double GetPredictedDownloadTimeSeconds(size_t segment_size_bytes) const;
};

class DownloadContext
{
private:
	FILE *f = NULL; // logging
	static constexpr double ewma_short_window_weight = 0.4;
	double ewma_bytes_per_second = 0.0;
	size_t dlnow_prev = 0;
	double time_prev = 0.0;
	
public:
	explicit DownloadContext( FILE *f );
	
	/**
	 * @param dltotal total bytes to download
	 * @param dlnow downloaded bytes so far
	 */
	int xferinfo( size_t dltotal, size_t dlnow );
};
#endif
