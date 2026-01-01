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

#include "NetworkBandwidthEstimator.hpp"

static const double epsilon = 1e-6;
static const double blend_weight_harmonic = 0.6; // 60% harmonic, 40% EWMA

/**
 * @brief get clock time as a floating point monotonic value
 */
double GetCurrentTimeMonotonicSeconds( void )
{
	timespec ts{};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

/**
 * @brief given a vector of floating point values, retrieve the median value
 */
static double median(std::vector<double> v)
{
	if( v.empty() )
	{
		return 0.0;
	}
	else
	{
		std::sort(v.begin(), v.end());
		const size_t n = v.size();
		return (n % 2) ? v[n/2] : 0.5 * (v[n/2 - 1] + v[n/2]);
	}
}

Sample::Sample( const CurlInfo &curlInfo )
{
	this->curlInfo = curlInfo;

	// compute derived values
	payload_download_time_seconds = std::max(epsilon, curlInfo.total_time_seconds - curlInfo.time_to_first_byte_seconds);

	payload_bytes_per_second = static_cast<double>(curlInfo.size_download_bytes) / payload_download_time_seconds;
}

double Sample::getTimeToFirstByteSeconds( void ) const
{
	return curlInfo.time_to_first_byte_seconds;
}

double Sample::getPayloadBytesPerSecond( void ) const
{
	return payload_bytes_per_second;
}

double Sample::getTotalTimeSeconds( void ) const
{
	return curlInfo.total_time_seconds;
}

/**
 * @brief Recompute median TTFB and harmonic mean from history; this requires iterating through all recent samples
 */
void NetworkBandwidthEstimator::RecomputeHarmonicMeanAndMedianTTFB()
{ // Overhead = median TTFB from all samples
	std::vector<double> ttfbs;
	ttfbs.reserve(history.size());
	for( const auto& s : history )
	{
		ttfbs.push_back(s.getTimeToFirstByteSeconds() );
	}
	estimated_TTFB_seconds = median(ttfbs);
	
	// Harmonic mean of throughput over last harmonic_window samples
	const size_t n = history.size();
	const size_t start = (n > harmonic_window) ? (n - harmonic_window) : 0;
	double denominator = 0.0;
	size_t count = 0;
	for( size_t i = start; i < n; i++ )
	{
		const double payloadBytesPerSecond = history[i].getPayloadBytesPerSecond();
		if( payloadBytesPerSecond > epsilon )
		{
			denominator += 1.0/payloadBytesPerSecond;
			count++;
		}
	}
	harmonic_BytesPerSecond = (count > 0 && denominator > 0.0) ? (static_cast<double>(count) / denominator) : 0.0;
}

double NetworkBandwidthEstimator::UpdateDownloadMetrics( const CurlInfo &curlInfo )
{
	Sample sample(curlInfo);
	const double payload_bytes_per_second = sample.getPayloadBytesPerSecond();
	const double total_time_seconds = sample.getTotalTimeSeconds();
	history.push_back(std::move(sample));
	
	const size_t MAX_HISTORY = 24;
	if (history.size() > MAX_HISTORY)
	{ // Trim history to avoid unbounded growth
		history.erase(history.begin(), history.begin() + (history.size() - MAX_HISTORY));
	}
	
	// EWMA updates
	if( EWMA_fast_BytesPerSecond <= 0.0 )
	{
		EWMA_fast_BytesPerSecond = payload_bytes_per_second;
	}
	else
	{
		EWMA_fast_BytesPerSecond = ALPHA_FAST * payload_bytes_per_second + (1.0 - ALPHA_FAST) * EWMA_fast_BytesPerSecond;
	}
	if( EWMA_slow_BytesPerSecond <= 0.0 )
	{
		EWMA_slow_BytesPerSecond = payload_bytes_per_second;
	}
	else
	{
		EWMA_slow_BytesPerSecond = ALPHA_SLOW * payload_bytes_per_second + (1.0 - ALPHA_SLOW) * EWMA_slow_BytesPerSecond;
	}
	RecomputeHarmonicMeanAndMedianTTFB();
	return total_time_seconds;
}

/**
 * @brief return current robust throughput estimate (bytes/s), buffer-agnostic
 */
double NetworkBandwidthEstimator::GetThroughputBytesPerSecond() const
{
	double EWMA_min = (EWMA_fast_BytesPerSecond > 0.0 && EWMA_slow_BytesPerSecond > 0.0)
	? std::min(EWMA_fast_BytesPerSecond, EWMA_slow_BytesPerSecond)
	: std::max(EWMA_fast_BytesPerSecond, EWMA_slow_BytesPerSecond);
	if (EWMA_min <= 0.0)
	{
		return harmonic_BytesPerSecond;
	}
	if (harmonic_BytesPerSecond <= 0.0)
	{
		return EWMA_min;
	}
	return blend_weight_harmonic * harmonic_BytesPerSecond + (1.0 - blend_weight_harmonic) * EWMA_min;
}

/**
 * @brief return current overhead (TTFB) estimate (seconds)
 */
double NetworkBandwidthEstimator::GetTimeToFirstByteSeconds() const
{
	return estimated_TTFB_seconds;
}

/**
 * @brief predict completion time for a new segment
 */
double NetworkBandwidthEstimator::GetPredictedDownloadTimeSeconds(size_t segment_size_bytes) const
{
	const double throughput = GetThroughputBytesPerSecond();
	if( throughput >= 1.0 )
	{
		return estimated_TTFB_seconds + (static_cast<double>(segment_size_bytes) / throughput);
	}
	else
	{ // we have no history data to make estimate
		return 0.0;
	}
}

DownloadContext::DownloadContext( FILE *f )
{
	this->f = f;
}

/**
 * @param dltotal total bytes to download
 * @param dlnow downloaded bytes so far
 */
int DownloadContext::xferinfo( size_t dltotal, size_t dlnow )
{
	const double now = GetCurrentTimeMonotonicSeconds();
	if( time_prev > 0.0 && now > time_prev )
	{
		const auto delta_bytes = dlnow - dlnow_prev;
		const auto delta_time = now - time_prev;
		if (delta_time > epsilon && delta_bytes > 0)
		{
			const double Bps = static_cast<double>(delta_bytes)/delta_time;
			if( ewma_bytes_per_second <= 0.0 )
			{
				ewma_bytes_per_second = Bps;
			}
			else
			{
				ewma_bytes_per_second =
				ewma_short_window_weight * Bps +
				(1.0 - ewma_short_window_weight) * ewma_bytes_per_second;
			}
		}
	}
	if( dlnow>dlnow_prev )
	{
		const auto remaining_bytes = dltotal - dlnow;
		const double remaining_time_estimate = remaining_bytes / ewma_bytes_per_second;
		fprintf( f, "%f,%ld,%ld,%ld,%f,%f\n",
				now,
				(dltotal>0)?(100*dlnow/dltotal):0,
				dlnow,
				dltotal,
				ewma_bytes_per_second,
				remaining_time_estimate );
	}
	dlnow_prev = dlnow;
	time_prev = now;
	return 0; // continue
	// returning 1 aborts transfer
}
