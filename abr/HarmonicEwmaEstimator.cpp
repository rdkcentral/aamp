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
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <optional>

#include "HarmonicEwmaEstimator.h"

static constexpr double epsilon = 1e-6;
static constexpr double BLEND_WEIGHT_HARMONIC = 0.6; // 60% harmonic, 40% EWMA
static constexpr size_t MAX_HISTORY = 24;			 // how far back in rolling window samples to consider for bandwidth estimate

/**
 * @brief get clock time as a floating point monotonic value
 */
double GetCurrentTimeMonotonicSeconds()
{
	using clock = std::chrono::steady_clock;
	return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

/**
 * @brief given a vector of floating point values, retrieve the median value
 * This function uses std::nth_element for O(n) time complexity instead of sorting.
 * For performance reasons, we don't make a copy of input, and so order in values may change as side effect of calling this function.
 *
 * @param values Input vector of floating point values; order may change after calling
 * @return The median value, or 0.0 if the input vector is empty
 */
double GetMedian(std::vector<double> &values)
{
	if (values.empty())
	{
		return 0.0;
	}

	const size_t n = values.size();
	const size_t mid = n / 2;
	if (n % 2)
	{ // Odd number of elements - find the middle element
		std::nth_element(values.begin(), values.begin() + mid, values.end());
		return values[mid];
	}
	else
	{ // Even number of elements - average the two middle elements
		// Find the element at mid position
		std::nth_element(values.begin(), values.begin() + mid, values.end());
		const double upper = values[mid];

		// Find the element at mid-1 position (the max of the lower half)
		std::nth_element(values.begin(), values.begin() + (mid - 1), values.end());
		const double lower = values[mid - 1];

		return 0.5 * (lower + upper);
	}
}

/**
 * @brief Recompute median TTFB and harmonic mean from history; this requires iterating through all recent samples
 */
void HarmonicEwmaEstimator::RecomputeHarmonicMeanAndMedianTTFB()
{
	// Overhead = median TTFB from all samples
	std::vector<double> ttfb;
	ttfb.reserve(m_history.size());
	for (const auto &s : m_history)
	{
		ttfb.push_back(s.GetTimeToFirstByteSeconds());
	}
	m_estimated_TTFB_seconds = GetMedian(ttfb);

	// Harmonic mean of throughput over last harmonic_window samples
	const size_t n = m_history.size();
	const size_t start = (n > harmonic_window) ? (n - harmonic_window) : 0;
	double denominator = 0.0;
	size_t count = 0;
	for (size_t i = start; i < n; i++)
	{
		const double payloadBytesPerSecond =
			m_history[i].GetPayloadBytesPerSecond();
		if (payloadBytesPerSecond > epsilon)
		{
			denominator += 1.0 / payloadBytesPerSecond;
			count++;
		}
	}
	m_harmonic_BytesPerSecond = (count > 0 && denominator > 0.0)
									? (static_cast<double>(count) / denominator)
									: 0.0;
}

/**
 * @brief Add download metrics derived from curl (optional).
 * @param[in] downloadMetrics Download metrics.
 */
void HarmonicEwmaEstimator::UpdateDownloadMetrics(const DownloadMetrics &downloadMetrics)
{
	Sample sample(downloadMetrics);
	const double payload_bytes_per_second = sample.GetPayloadBytesPerSecond();
	m_history.emplace_back(sample);

	if (m_history.size() > MAX_HISTORY)
	{ // Trim history to avoid unbounded growth
		m_history.erase(
			m_history.begin(),
			m_history.begin() + (m_history.size() - MAX_HISTORY));
	}

	// EWMA updates
	if (m_ewma_fast_BytesPerSecond > 0.0)
	{
		m_ewma_fast_BytesPerSecond = ALPHA_FAST * payload_bytes_per_second +
									 (1.0 - ALPHA_FAST) * m_ewma_fast_BytesPerSecond;
	}
	else
	{
		m_ewma_fast_BytesPerSecond = payload_bytes_per_second;
	}
	if (m_ewma_slow_BytesPerSecond > 0.0)
	{
		m_ewma_slow_BytesPerSecond = ALPHA_SLOW * payload_bytes_per_second +
									 (1.0 - ALPHA_SLOW) * m_ewma_slow_BytesPerSecond;
	}
	else
	{
		m_ewma_slow_BytesPerSecond = payload_bytes_per_second;
	}
	RecomputeHarmonicMeanAndMedianTTFB();
}

/**
 * @brief Human-readable algorithm name.
 * @return Algorithm name.
 */
const char *HarmonicEwmaEstimator::GetNetworkEstimatorName() const
{
	return "Harmonic_Mean_Exponentially_Weighted_Moving_Average_Blend";
}

/**
 * @brief Reset internal state.
 */
void HarmonicEwmaEstimator::Reset()
{
	m_history.clear();
	m_estimated_TTFB_seconds = 0.0;
	m_ewma_fast_BytesPerSecond = 0.0;
	m_ewma_slow_BytesPerSecond = 0.0;
	m_harmonic_BytesPerSecond = 0.0;
	m_progressContextValid = false;
	m_progressBytesPerSecond = 0.0;
	m_progressLastNowSeconds = 0.0;
	m_progressLastNowBytes = 0;
	m_progressLastTotalBytes = 0;
	m_progressHasSample = false;
}

/**
 * @brief Add a bandwidth sample.
 * @param[in] downloadbps Download bandwidth in bits per second.
 * @param[in] lowLatencyMode Whether low latency mode is enabled.
 */
void HarmonicEwmaEstimator::AddBandwidthSample(BitsPerSecond downloadbps, bool lowLatencyMode)
{
	(void)downloadbps;
	(void)lowLatencyMode;
}

/**
 * @brief Provide in-flight download progress updates (optional).
 * @param[in] progressInfo Download progress information.
 */
void HarmonicEwmaEstimator::UpdateDownloadProgress(
	const DownloadProgressInfo &progressInfo)
{
	const double now = progressInfo.m_now_seconds;
	const std::size_t dltotal = progressInfo.m_total_bytes;
	const std::size_t dlnow = progressInfo.m_now_bytes;

	if (!m_progressContextValid)
	{
		m_progressContext.Reset(now);
		m_progressContextValid = true;
	}
	else if (now < m_progressLastNowSeconds || dlnow < m_progressLastNowBytes)
	{
		m_progressContext.Reset(now);
		m_progressBytesPerSecond = 0.0;
		m_progressHasSample = false;
	}
	else if (dltotal != 0 &&
			 dltotal != m_progressLastTotalBytes &&
			 dlnow == 0)
	{
		m_progressContext.Reset(now);
		m_progressBytesPerSecond = 0.0;
		m_progressHasSample = false;
	}

	if (m_progressContext.xferinfo(now, dltotal, dlnow))
	{
		m_progressBytesPerSecond =
			m_progressContext.GetEstimatedThroughputBytesPerSecond();
		m_progressHasSample = (m_progressBytesPerSecond > 0.0);
	}

	m_progressLastNowSeconds = now;
	m_progressLastNowBytes = dlnow;
	m_progressLastTotalBytes = dltotal;
}

/**
 * @brief Get current bandwidth estimate.
 * @return Estimated bandwidth in bits per second, or -1 if unavailable.
 */
BitsPerSecond HarmonicEwmaEstimator::GetBandwidthBitsPerSecond()
{
	const double throughputBytesPerSecond = GetThroughputBytesPerSecond();
	// Convert to bits per second and address edge case of zero/negative throughput
	if (throughputBytesPerSecond <= 0.0)
	{
		return static_cast<BitsPerSecond>(-1);
	}
	return static_cast<BitsPerSecond>(throughputBytesPerSecond * 8.0);
}

/**
 * @brief Get current robust throughput estimate.
 * @return Estimated throughput in bytes per second.
 */
double HarmonicEwmaEstimator::GetThroughputBytesPerSecond()
{
	double ewma_min = (m_ewma_fast_BytesPerSecond > 0.0 &&
					   m_ewma_slow_BytesPerSecond > 0.0)
						  ? std::min(m_ewma_fast_BytesPerSecond, m_ewma_slow_BytesPerSecond)
						  : std::max(m_ewma_fast_BytesPerSecond, m_ewma_slow_BytesPerSecond);
	if (ewma_min > 0.0)
	{
		if (m_harmonic_BytesPerSecond > 0.0)
		{
			const double blended =
				BLEND_WEIGHT_HARMONIC * m_harmonic_BytesPerSecond +
				(1.0 - BLEND_WEIGHT_HARMONIC) * ewma_min;
			return (m_progressHasSample)
					   ? std::min(blended, m_progressBytesPerSecond)
					   : blended;
		}
		else
		{
			return (m_progressHasSample)
					   ? std::min(ewma_min, m_progressBytesPerSecond)
					   : ewma_min;
		}
	}
	else
	{
		const double baseline = m_harmonic_BytesPerSecond;
		if (baseline > 0.0)
		{
			return (m_progressHasSample)
					   ? std::min(baseline, m_progressBytesPerSecond)
					   : baseline;
		}
		return (m_progressHasSample) ? m_progressBytesPerSecond : 0.0;
	}
}

/**
 * @brief Get current overhead (TTFB) estimate.
 * @return Estimated time to first byte in seconds.
 */
double HarmonicEwmaEstimator::GetTimeToFirstByteSeconds()
{
	return m_estimated_TTFB_seconds;
}

/**
 * @brief Predict completion time for a new segment.
 * @param[in] segment_size_bytes Size of the segment in bytes.
 * @return Predicted download time in seconds.
 */
double HarmonicEwmaEstimator::GetPredictedDownloadTimeSeconds(
	size_t segment_size_bytes)
{
	const double throughput = GetThroughputBytesPerSecond();
	if (throughput > 0.0)
	{
		return m_estimated_TTFB_seconds +
			   (static_cast<double>(segment_size_bytes) / throughput);
	}
	else
	{
		return 0.0;
	}
}

/**
 * @brief Reset only the currently available bandwidth estimate.
 *
 * Called after every profile change to prevent stale high-bandwidth estimates
 * from triggering an immediate ramp-up.  After this call,
 * GetBandwidthBitsPerSecond() returns -1 until the next UpdateDownloadMetrics()
 * completes, matching the behaviour of RollingMedianOutlierEstimator::Reset-
 * CurrentlyAvailableBandwidth() which clears its rolling cache.
 *
 * m_history is deliberately preserved: the EWMA accumulators and harmonic mean
 * are recomputed from that history on the very next UpdateDownloadMetrics()
 * call, so the estimator reconverges quickly without discarding prior data.
 */
void HarmonicEwmaEstimator::ResetCurrentlyAvailableBandwidth()
{
	m_ewma_fast_BytesPerSecond = 0.0;
	m_ewma_slow_BytesPerSecond = 0.0;
	m_harmonic_BytesPerSecond  = 0.0;
	m_progressHasSample        = false;
	m_progressBytesPerSecond   = 0.0;
}

/**
 * @brief Reset the download context.
 * @param now Current time in seconds.
 */
void DownloadContext::Reset(const double now)
{
	m_ewma_bytes_per_second = 0.0;
	m_dltotal = 0;
	m_dlnow_prev = 0;
	m_time_prev = now;
}

/**
 * @brief Get estimated remaining time for download.
 * @return Estimated remaining time in seconds.
 */
double DownloadContext::GetEstimatedRemainingTime() const
{
	double remaining_time_seconds = 0.0;
	const size_t remaining_bytes = m_dltotal - m_dlnow_prev;
	if (m_ewma_bytes_per_second > 0.0)
	{
		remaining_time_seconds = remaining_bytes / m_ewma_bytes_per_second;
	}
	return remaining_time_seconds;
}

/**
 * @brief Get estimated throughput in bytes per second.
 * @return Estimated throughput in bytes per second.
 */
double DownloadContext::GetEstimatedThroughputBytesPerSecond() const
{
	return m_ewma_bytes_per_second;
}

/**
 * @brief Update transfer information.
 * @param now Current time in seconds.
 * @param dltotal Total bytes to download.
 * @param dlnow Bytes downloaded so far.
 * @return True if the context was updated, false otherwise.
 */
bool DownloadContext::xferinfo(const double now, size_t dltotal, size_t dlnow)
{
	bool rc = false;
	const size_t delta_bytes = dlnow - m_dlnow_prev;
	const double delta_time = now - m_time_prev;
	m_dltotal = dltotal;
	if (delta_bytes > 0)
	{ // some data has trickled in
		if (delta_time > epsilon)
		{
			const double bytesPerSecond =
				static_cast<double>(delta_bytes) / delta_time;
			if (m_ewma_bytes_per_second > 0.0)
			{
				m_ewma_bytes_per_second = m_ewma_short_window_weight *
											  bytesPerSecond +
										  (1.0 - m_ewma_short_window_weight) *
											  m_ewma_bytes_per_second;
			}
			else
			{
				m_ewma_bytes_per_second = bytesPerSecond;
			}
			m_time_prev = now;
			m_dlnow_prev = dlnow;
			rc = true;
		}
	}
	return rc;
}

/**
 * @brief Construct a Sample from DownloadMetrics
 * @param downloadMetrics Download metrics.
 */
Sample::Sample(const DownloadMetrics &downloadMetrics) : m_downloadMetrics(downloadMetrics)
{
	// compute derived values
	m_payload_download_time_seconds = std::max(
		epsilon,
		downloadMetrics.m_total_time_seconds - downloadMetrics.m_time_to_first_byte_seconds);

	m_payload_bytes_per_second =
		static_cast<double>(downloadMetrics.m_size_download_bytes) /
		m_payload_download_time_seconds;
}

/**
 * @brief Get time to first byte from sample.
 * @return Time to first byte in seconds.
 */
double Sample::GetTimeToFirstByteSeconds() const
{
	return m_downloadMetrics.m_time_to_first_byte_seconds;
}

/**
 * @brief Get payload bytes per second from sample.
 * @return Payload bytes per second.
 */
double Sample::GetPayloadBytesPerSecond() const
{
	return m_payload_bytes_per_second;
}

/**
 * @brief Get total time from sample.
 * @return Total download time in seconds.
 */
double Sample::GetTotalTimeSeconds() const
{
	return m_downloadMetrics.m_total_time_seconds;
}
