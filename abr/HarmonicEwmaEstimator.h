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
#ifndef THROUGHPUT_ESTIMATOR_HARMONIC_EWMA_ESTIMATOR_H
#define THROUGHPUT_ESTIMATOR_HARMONIC_EWMA_ESTIMATOR_H

#include <cstddef>
#include <vector>

#include "BandwidthEstimatorBase.h"

/**
 * @brief Compute the median of the given values in-place.
 *
 * This function may reorder the contents of @p values for performance
 * reasons (for example, by using partial sorting algorithms). Callers
 * must not rely on the original order of @p values after this call.
 *
 * @param values Vector of numeric samples to compute the median from.
 *        The vector is modified and its element order is not preserved.
 *
 * @return The median value computed from @p values.
 */
double GetMedian(std::vector<double> &values);
double GetCurrentTimeMonotonicSeconds();

/**
 * @brief encapsulate performance information for a given http download
 */
class Sample
{
private:
	DownloadMetrics m_downloadMetrics;

	// total_time - time_to_first_byte
	double m_payload_download_time_seconds = 0.0;

	// size_download / payload_download_time
	double m_payload_bytes_per_second = 0.0;

public:
	/**
	 * @brief constructor - populate sample metrics
	 * @param downloadMetrics  Download profiling data
	 */
	Sample(const DownloadMetrics &downloadMetrics);
	double GetTimeToFirstByteSeconds() const;
	double GetPayloadBytesPerSecond() const;
	double GetTotalTimeSeconds() const;
};

class DownloadContext
{
private:
	static constexpr double m_ewma_short_window_weight = 0.4;
	double m_ewma_bytes_per_second = 0.0;
	std::size_t m_dltotal = 0;
	std::size_t m_dlnow_prev = 0;
	double m_time_prev = 0.0;

public:
	void Reset(const double now);

	double GetEstimatedRemainingTime() const;
	double GetEstimatedThroughputBytesPerSecond() const;

	bool xferinfo(const double now, std::size_t dltotal, std::size_t dlnow);
};

/**
 * @class HarmonicEwmaEstimator
 * @brief Maintains network bandwidth state and predicts download performance.
 */
class HarmonicEwmaEstimator
	: public BandwidthEstimatorBase
{
private:
	DownloadContext m_progressContext;
	bool m_progressContextValid = false;
	double m_progressBytesPerSecond = 0.0;
	double m_progressLastNowSeconds = 0.0;
	std::size_t m_progressLastNowBytes = 0;
	std::size_t m_progressLastTotalBytes = 0;
	bool m_progressHasSample = false;

	// Rolling history & stats
	std::vector<Sample> m_history;

	// Robust per-request overhead Time to First Byte (TTFB) estimate
	double m_estimated_TTFB_seconds = 0.0; // median TTFB - computed brute force

	// Robust throughput estimates (bytes/s)
	double m_ewma_fast_BytesPerSecond = 0.0; // reacts quickly
	double m_ewma_slow_BytesPerSecond = 0.0; // stable
	double m_harmonic_BytesPerSecond = 0.0;	 // conservative

	// Exponentially Weighted Moving Average (EWMA) tuning
	static constexpr double ALPHA_FAST = 0.5;
	static constexpr double ALPHA_SLOW = 0.2;

	// Harmonic mean over last N samples.
	static constexpr std::size_t harmonic_window = 8;

	void RecomputeHarmonicMeanAndMedianTTFB();

public:
	HarmonicEwmaEstimator() = default;

	const char *GetNetworkEstimatorName() const override;
	void Reset() override;
	void AddBandwidthSample(BitsPerSecond downloadbps, bool lowLatencyMode) override;
	void UpdateDownloadMetrics(const DownloadMetrics &curl) override;
	void UpdateDownloadProgress(const DownloadProgressInfo &progressInfo) override;
	BitsPerSecond GetBandwidthBitsPerSecond() override;
	double GetThroughputBytesPerSecond() override;
	double GetTimeToFirstByteSeconds() override;
	double GetPredictedDownloadTimeSeconds(std::size_t segment_size_bytes) override;
	void ResetCurrentlyAvailableBandwidth() override;
};

#endif
