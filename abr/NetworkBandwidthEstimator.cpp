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
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <optional>
#include <string>
#include <ctime>

#include "NetworkBandwidthEstimator.h"

static const double epsilon = 1e-6;
static const double BLEND_WEIGHT_HARMONIC = 0.6; // 60% harmonic, 40% EWMA
static constexpr size_t MAX_HISTORY = 24; // how far back in rolling window samples to consider for bandwidth estimate

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
double GetMedian( std::vector<double> &values )
{
	if( values.empty() )
	{
		return 0.0;
	}

	const size_t n = values.size();
	const size_t mid = n / 2;
	if( n % 2 )
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

Sample::Sample( const CurlInfo &curlInfo ) : m_curlInfo(curlInfo)
{ // compute derived values
	m_payload_download_time_seconds = std::max(epsilon, curlInfo.m_total_time_seconds - curlInfo.m_time_to_first_byte_seconds);

	m_payload_bytes_per_second = static_cast<double>(curlInfo.m_size_download_bytes) / m_payload_download_time_seconds;
}

double Sample::GetTimeToFirstByteSeconds() const
{
	return m_curlInfo.m_time_to_first_byte_seconds;
}

double Sample::GetPayloadBytesPerSecond() const
{
	return m_payload_bytes_per_second;
}

double Sample::GetTotalTimeSeconds() const
{
	return m_curlInfo.m_total_time_seconds;
}

/**
 * @brief Recompute median TTFB and harmonic mean from history; this requires iterating through all recent samples
 */
void NetworkBandwidthEstimator::RecomputeHarmonicMeanAndMedianTTFB()
{ // Overhead = median TTFB from all samples
	std::vector<double> ttfb;
	ttfb.reserve(m_history.size());
	for( const auto& s : m_history )
	{
		ttfb.push_back(s.GetTimeToFirstByteSeconds() );
	}
	m_estimated_TTFB_seconds = GetMedian(ttfb);
	
	// Harmonic mean of throughput over last harmonic_window samples
	const size_t n = m_history.size();
	const size_t start = (n > harmonic_window) ? (n - harmonic_window) : 0;
	double denominator = 0.0;
	size_t count = 0;
	for( size_t i = start; i < n; i++ )
	{
		const double payloadBytesPerSecond = m_history[i].GetPayloadBytesPerSecond();
		if( payloadBytesPerSecond > epsilon )
		{
			denominator += 1.0/payloadBytesPerSecond;
			count++;
		}
	}
	m_harmonic_BytesPerSecond = (count > 0 && denominator > 0.0) ? (static_cast<double>(count) / denominator) : 0.0;
}

void NetworkBandwidthEstimator::UpdateDownloadMetrics( const CurlInfo &curlInfo )
{
	Sample sample(curlInfo);
	// extract derived payload_bytes_per_second from sample
	const double payload_bytes_per_second = sample.GetPayloadBytesPerSecond();
	m_history.emplace_back(sample);
	
	if (m_history.size() > MAX_HISTORY)
	{ // Trim history to avoid unbounded growth
		m_history.erase(m_history.begin(), m_history.begin() + (m_history.size() - MAX_HISTORY));
	}
	
	// EWMA updates
	if( m_EWMA_fast_BytesPerSecond > 0.0 )
	{
		m_EWMA_fast_BytesPerSecond = ALPHA_FAST * payload_bytes_per_second + (1.0 - ALPHA_FAST) * m_EWMA_fast_BytesPerSecond;
	}
	else
	{
		m_EWMA_fast_BytesPerSecond = payload_bytes_per_second;
	}
	if( m_EWMA_slow_BytesPerSecond > 0.0 )
	{
		m_EWMA_slow_BytesPerSecond = ALPHA_SLOW * payload_bytes_per_second + (1.0 - ALPHA_SLOW) * m_EWMA_slow_BytesPerSecond;
	}
	else
	{
		m_EWMA_slow_BytesPerSecond = payload_bytes_per_second;
	}
	RecomputeHarmonicMeanAndMedianTTFB();
}

/**
 * @brief return current robust throughput estimate (bytes/s), buffer-agnostic
 */
double NetworkBandwidthEstimator::GetThroughputBytesPerSecond() const
{
	double EWMA_min = (m_EWMA_fast_BytesPerSecond > 0.0 && m_EWMA_slow_BytesPerSecond > 0.0)
	? std::min(m_EWMA_fast_BytesPerSecond, m_EWMA_slow_BytesPerSecond)
	: std::max(m_EWMA_fast_BytesPerSecond, m_EWMA_slow_BytesPerSecond);
	if( EWMA_min > 0.0 )
	{
		if (m_harmonic_BytesPerSecond > 0.0)
		{
			return BLEND_WEIGHT_HARMONIC * m_harmonic_BytesPerSecond + (1.0 - BLEND_WEIGHT_HARMONIC) * EWMA_min;
		}
		else
		{
			return EWMA_min;
		}
	}
	else
	{
		return m_harmonic_BytesPerSecond;
	}
}

/**
 * @brief return current overhead (TTFB) estimate (seconds)
 */
double NetworkBandwidthEstimator::GetTimeToFirstByteSeconds() const
{
	return m_estimated_TTFB_seconds;
}

/**
 * @brief predict completion time for a new segment
 */
double NetworkBandwidthEstimator::GetPredictedDownloadTimeSeconds(size_t segment_size_bytes) const
{
	const double throughput = GetThroughputBytesPerSecond();
	if( throughput > 0.0 )
	{
		return m_estimated_TTFB_seconds + (static_cast<double>(segment_size_bytes) / throughput);
	}
	else
	{ // no history data to make estimate
		return 0.0;
	}
}

DownloadContext::DownloadContext() = default;

DownloadContext::~DownloadContext() = default;

void DownloadContext::Reset( const double now )
{
	m_ewma_bytes_per_second = 0.0;
	m_dltotal  = 0;
	m_dlnow_prev = 0;
	m_time_prev = now;
}

double DownloadContext::GetEstimatedRemainingTime() const
{
	double rc = 0.0;
	const size_t remaining_bytes = m_dltotal - m_dlnow_prev;
	if( m_ewma_bytes_per_second > 0.0 )
	{
		rc = remaining_bytes / m_ewma_bytes_per_second;
	}
	return rc;
}

double DownloadContext::GetEstimatedThroughputBytesPerSecond() const
{
	return m_ewma_bytes_per_second;
}

/**
 * @param dltotal total bytes to download
 * @param dlnow downloaded bytes so far
 */
bool DownloadContext::xferinfo( const double now, size_t dltotal, size_t dlnow )
{
	bool rc = false;
	const size_t delta_bytes = dlnow - m_dlnow_prev;
	const double delta_time = now - m_time_prev;
	m_dltotal = dltotal;
	if( delta_bytes > 0 )
	{ // some data has trickled in
		if( delta_time > epsilon )
		{
			const double Bps = static_cast<double>(delta_bytes)/delta_time;
			if( m_ewma_bytes_per_second > 0.0 )
			{
				m_ewma_bytes_per_second = m_ewma_short_window_weight * Bps + (1.0 - m_ewma_short_window_weight) * m_ewma_bytes_per_second;
			}
			else
			{
				m_ewma_bytes_per_second = Bps;
			}
			m_time_prev = now;
			m_dlnow_prev = dlnow;
			rc = true;
		}
	}
	return rc;
}
