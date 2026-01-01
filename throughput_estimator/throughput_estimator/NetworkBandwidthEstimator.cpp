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
static const double BLEND_WEIGHT_HARMONIC = 0.6; // 60% harmonic, 40% EWMA

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
static double median( const std::vector<double> &values )
{
	if( values.empty() )
	{
		return 0.0;
	}
	else
	{
		std::vector<double> v(values);
		std::sort(v.begin(), v.end());
		const size_t n = v.size();
		return (n % 2) ? v[n/2] : 0.5 * (v[n/2 - 1] + v[n/2]);
	}
}

Sample::Sample( const CurlInfo &curlInfo ) : m_curlInfo(curlInfo)
{ // compute derived values
	m_payload_download_time_seconds = std::max(epsilon, curlInfo.m_total_time_seconds - curlInfo.m_time_to_first_byte_seconds);

	m_payload_bytes_per_second = static_cast<double>(curlInfo.m_size_download_bytes) / m_payload_download_time_seconds;
}

double Sample::getTimeToFirstByteSeconds( void ) const
{
	return m_curlInfo.m_time_to_first_byte_seconds;
}

double Sample::getPayloadBytesPerSecond( void ) const
{
	return m_payload_bytes_per_second;
}

double Sample::getTotalTimeSeconds( void ) const
{
	return m_curlInfo.m_total_time_seconds;
}

/**
 * @brief Recompute median TTFB and harmonic mean from history; this requires iterating through all recent samples
 */
void NetworkBandwidthEstimator::RecomputeHarmonicMeanAndMedianTTFB()
{ // Overhead = median TTFB from all samples
	std::vector<double> ttfbs;
	ttfbs.reserve(m_history.size());
	for( const auto& s : m_history )
	{
		ttfbs.push_back(s.getTimeToFirstByteSeconds() );
	}
	m_estimated_TTFB_seconds = median(ttfbs);
	
	// Harmonic mean of throughput over last harmonic_window samples
	const size_t n = m_history.size();
	const size_t start = (n > harmonic_window) ? (n - harmonic_window) : 0;
	double denominator = 0.0;
	size_t count = 0;
	for( size_t i = start; i < n; i++ )
	{
		const double payloadBytesPerSecond = m_history[i].getPayloadBytesPerSecond();
		if( payloadBytesPerSecond > epsilon )
		{
			denominator += 1.0/payloadBytesPerSecond;
			count++;
		}
	}
	m_harmonic_BytesPerSecond = (count > 0 && denominator > 0.0) ? (static_cast<double>(count) / denominator) : 0.0;
}

double NetworkBandwidthEstimator::UpdateDownloadMetrics( const CurlInfo &curlInfo )
{
	Sample sample(curlInfo);
	const double payload_bytes_per_second = sample.getPayloadBytesPerSecond();
	const double total_time_seconds = sample.getTotalTimeSeconds();
	m_history.push_back(std::move(sample));
	
	const size_t MAX_HISTORY = 24;
	if (m_history.size() > MAX_HISTORY)
	{ // Trim history to avoid unbounded growth
		m_history.erase(m_history.begin(), m_history.begin() + (m_history.size() - MAX_HISTORY));
	}
	
	// EWMA updates
	if( m_EWMA_fast_BytesPerSecond <= 0.0 )
	{
		m_EWMA_fast_BytesPerSecond = payload_bytes_per_second;
	}
	else
	{
		m_EWMA_fast_BytesPerSecond = ALPHA_FAST * payload_bytes_per_second + (1.0 - ALPHA_FAST) * m_EWMA_fast_BytesPerSecond;
	}
	if( m_EWMA_slow_BytesPerSecond <= 0.0 )
	{
		m_EWMA_slow_BytesPerSecond = payload_bytes_per_second;
	}
	else
	{
		m_EWMA_slow_BytesPerSecond = ALPHA_SLOW * payload_bytes_per_second + (1.0 - ALPHA_SLOW) * m_EWMA_slow_BytesPerSecond;
	}
	RecomputeHarmonicMeanAndMedianTTFB();
	return total_time_seconds;
}

/**
 * @brief return current robust throughput estimate (bytes/s), buffer-agnostic
 */
double NetworkBandwidthEstimator::GetThroughputBytesPerSecond() const
{
	double EWMA_min = (m_EWMA_fast_BytesPerSecond > 0.0 && m_EWMA_slow_BytesPerSecond > 0.0)
	? std::min(m_EWMA_fast_BytesPerSecond, m_EWMA_slow_BytesPerSecond)
	: std::max(m_EWMA_fast_BytesPerSecond, m_EWMA_slow_BytesPerSecond);
	if (EWMA_min <= 0.0)
	{
		return m_harmonic_BytesPerSecond;
	}
	if (m_harmonic_BytesPerSecond <= 0.0)
	{
		return EWMA_min;
	}
	return BLEND_WEIGHT_HARMONIC * m_harmonic_BytesPerSecond + (1.0 - BLEND_WEIGHT_HARMONIC) * EWMA_min;
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
	if( throughput >= 1.0 )
	{
		return m_estimated_TTFB_seconds + (static_cast<double>(segment_size_bytes) / throughput);
	}
	else
	{ // we have no history data to make estimate
		return 0.0;
	}
}

DownloadContext::DownloadContext( const char *logPath )
{
	mLogFile = fopen(logPath,"wb");
	if( mLogFile )
	{
		fprintf( mLogFile, "Time,Pct,dlnow,dltotal,Bps,est remaining(s)\n" );
	}
}

DownloadContext::~DownloadContext()
{
	if( mLogFile )
	{
		fclose( mLogFile );
	}
}

void DownloadContext::Reset( void )
{
	if( mLogFile )
	{
		fprintf( mLogFile, "\n%f,%f\n", GetCurrentTimeMonotonicSeconds(), 0.0 );
	}
	m_ewma_bytes_per_second = 0.0;
	m_dlnow_prev = 0;
	m_time_prev = 0.0;
}

/**
 * @param dltotal total bytes to download
 * @param dlnow downloaded bytes so far
 */
int DownloadContext::xferinfo( size_t dltotal, size_t dlnow )
{
	const double now = GetCurrentTimeMonotonicSeconds();
	if( m_time_prev > 0.0 && now > m_time_prev )
	{
		const size_t delta_bytes = dlnow - m_dlnow_prev;
		const double delta_time = now - m_time_prev;
		if (delta_time > epsilon && delta_bytes > 0)
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
		}
	}
	if( dlnow>m_dlnow_prev && m_ewma_bytes_per_second > 0.0 )
	{
		const size_t remaining_bytes = dltotal - dlnow;
		const double remaining_time_estimate = remaining_bytes / m_ewma_bytes_per_second;
		if( mLogFile )
		{
			fprintf( mLogFile, "%f,%zu,%zu,%zu,%f,%f\n",
					now,
					(dltotal>0)?(100*dlnow/dltotal):0,
					dlnow,
					dltotal,
					m_ewma_bytes_per_second,
					remaining_time_estimate );
		}
	}
	m_dlnow_prev = dlnow;
	m_time_prev = now;
	return 0; // continue
	// returning 1 aborts transfer
}
