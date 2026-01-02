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
#ifndef THROUGHPUT_ESTIMATOR_NETWORK_BANDWIDTH_ESTIMATOR_HPP
#define THROUGHPUT_ESTIMATOR_NETWORK_BANDWIDTH_ESTIMATOR_HPP

#include <cstddef>
#include <ostream>
#include <vector>


double GetMedian( std::vector<double> &values );
double GetCurrentTimeMonotonicSeconds( void );

/**
 * @brief Plain Old Data (POD) structure for profiling information from a given CURL instance
 */
struct CurlInfo
{
	size_t m_size_download_bytes; // CURLINFO_SIZE_DOWNLOAD
	double m_total_time_seconds; // CURLINFO_TOTAL_TIME
	double m_time_to_first_byte_seconds; // CURLINFO_STARTTRANSFER_TIME
};

/**
 * @brief encapsulate performance information for a given http download
 */
class Sample
{
private:
	CurlInfo m_curlInfo;
	
	// total_time - time_to_first_byte
	double m_payload_download_time_seconds = 0.0;
	
	// size_download / payload_download_time
	double m_payload_bytes_per_second = 0.0;
	
public:
	/**
	 * @brief constructor - populate sample metrics
	 * @param curlInfo  Curl Handle profiling data
	 */
	Sample( const CurlInfo &curlInfo );
	double GetTimeToFirstByteSeconds( void ) const;
	double GetPayloadBytesPerSecond( void ) const;
	double GetTotalTimeSeconds( void ) const;
};

/**
 * @class NetworkBandwidthEstimator
 * @brief Maintains network bandwidth state and predicts download performance.
 *
 * This class collects per-request download samples (size, total time and
 * time-to-first-byte) and derives robust throughput estimates that can be
 * consumed by Adaptive Bitrate (ABR) decision logic. It maintains two
 * Exponentially Weighted Moving Average (EWMA) filters over the measured
 * payload throughput: a fast EWMA that reacts quickly to changes and a
 * slow EWMA that provides a more stable baseline.
 *
 * In addition, it computes a conservative harmonic-mean throughput over a
 * sliding window of recent samples and blends this with the EWMA-based
 * estimates. The combination of EWMA smoothing and harmonic mean blending
 * yields a bandwidth estimate that is responsive to real network changes
 * while remaining resilient to outliers, and is used to predict segment
 * download times and guide ABR bitrate selection.
 */
class NetworkBandwidthEstimator
{
private:
	// Rolling history & stats
	std::vector<Sample> m_history;
	
	// Robust per-request overhead Time to First Byte (TTFB) estimate
	double m_estimated_TTFB_seconds = 0.0; // median TTFB - computed brute force
	
	// Robust throughput estimates (bytes/s)
	double m_EWMA_fast_BytesPerSecond = 0.0; // reacts quickly
	double m_EWMA_slow_BytesPerSecond = 0.0; // stable
	double m_harmonic_BytesPerSecond = 0.0;  // conservative
	
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
	
	void UpdateDownloadMetrics( const CurlInfo &curl );
	
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
	/**
	 * @brief Smoothing factor for short-window EWMA throughput estimate.
	 *
	 * This weight acts as the alpha parameter in an Exponentially Weighted
	 * Moving Average that tracks the instantaneous download rate reported
	 * by periodic progress callbacks (see xferinfo()).
	 *
	 * A value of 0.4 biases the estimate toward the most recent progress
	 * interval (real-time behavior) while still retaining enough history
	 * to dampen noise from very short spikes or stalls. This trade-off
	 * was chosen to make the remaining-time and throughput estimates react
	 * quickly to genuine bandwidth changes without causing large UI jumps
	 * between successive callbacks.
	 */
	static constexpr double m_ewma_short_window_weight = 0.4;
	double m_ewma_bytes_per_second = 0.0;
	size_t m_dltotal = 0;
	size_t m_dlnow_prev = 0;
	double m_time_prev = 0.0;
	std::ostream* m_progress_stream = nullptr; ///< Optional output stream for progress logging
	
public:
	DownloadContext();
	~DownloadContext();
	void Reset( const double now );
	
	/**
	 * @brief Set an optional output stream for progress logging
	 * @param stream Pointer to output stream, or nullptr to disable logging
	 */
	void SetProgressStream(std::ostream* stream);
	
	double GetEstimatedRemainingTime( void );
	double GetEstimatedThroughputBytesPerSecond( void );
	
	/**
	 * @brief monitor download progress
	 * @note: name and parameters are based on CURLOPT_XFERINFOFUNCTION
	 *
	 * @param dltotal total bytes to download
	 * @param dlnow downloaded bytes so far
	 * @retval true if more bytes have been transferred since last call
	 */
	bool xferinfo( const double now, size_t dltotal, size_t dlnow );
};
#endif
