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
#ifndef AAMP_ABR_BANDWIDTH_ESTIMATOR_BASE_H
#define AAMP_ABR_BANDWIDTH_ESTIMATOR_BASE_H

#include <cstddef>

#include "AampMediaType.h"

#define DEFAULT_ABR_CHUNK_CACHE_LENGTH	10					/**< Default ABR chunk cache length */

/**
 * @brief Plain Old Data (POD) structure for profiling information from a
 * given CURL instance.
 */
struct DownloadMetrics
{
	std::size_t m_size_download_bytes; // CURLINFO_SIZE_DOWNLOAD
	double m_total_time_seconds; // CURLINFO_TOTAL_TIME
	double m_time_to_first_byte_seconds; // CURLINFO_STARTTRANSFER_TIME
};

/**
 * @brief POD structure for in-flight progress updates (xferinfo-style).
 */
struct DownloadProgressInfo
{
	double m_now_seconds;
	std::size_t m_total_bytes;
	std::size_t m_now_bytes;
};

/**
 * @brief Optional configuration for bandwidth estimators.
 *
 * Not all estimators use every field. Estimators that do not support
 * configuration should ignore it.
 */
struct BandwidthEstimatorConfig
{
	int mAbrCacheLife;
	int mAbrCacheLength;
	int mAbrCacheOutlier;
	std::size_t mLowLatencyCacheLength;

	/**
	 * @brief Default constructor.
	 */
	BandwidthEstimatorConfig()
		: mAbrCacheLife(0),
		  mAbrCacheLength(0),
		  mAbrCacheOutlier(0),
		  mLowLatencyCacheLength(0)
	{
	}

	/**
	 * @brief Parameterized constructor.
	 */
	BandwidthEstimatorConfig(
		int abrCacheLife,
		int abrCacheLength,
		int abrCacheOutlier,
		std::size_t lowLatencyCacheLength)
		: mAbrCacheLife(abrCacheLife),
		  mAbrCacheLength(abrCacheLength),
		  mAbrCacheOutlier(abrCacheOutlier),
		  mLowLatencyCacheLength(lowLatencyCacheLength)
	{
	}
};

/**
 * @brief Base interface for bandwidth estimation algorithms.
 */
class BandwidthEstimatorBase
{
public:
	virtual ~BandwidthEstimatorBase() {}

	/**
	 * @brief Human-readable algorithm name.
	 */
	virtual const char* GetNetworkEstimatorName() const = 0;

	/**
	 * @brief Reset all internal state.
	 */
	virtual void Reset() = 0;

	/**
	 * @brief Add a bandwidth sample (bits per second).
	 */
	virtual void AddBandwidthSample(BitsPerSecond downloadbps, bool lowLatencyMode) = 0;

	/**
	 * @brief Add download metrics derived from curl (optional).
	 */
	virtual void UpdateDownloadMetrics(const DownloadMetrics &metrics ) = 0;

	/**
	 * @brief Provide in-flight download progress updates (optional).
	 */
	virtual void UpdateDownloadProgress(const DownloadProgressInfo &progressInfo)
	{
	}

	/**
	 * @brief Update estimator configuration (optional).
	 *
	 * Default implementation is a no-op.
	 */
	virtual void SetConfig(const BandwidthEstimatorConfig &config)
	{
	}

	/**
	 * @brief Return current bandwidth estimate (bits per second).
	 *
	 * @return Estimated bandwidth in bits per second, or -1 if unavailable.
	 */
	virtual BitsPerSecond GetBandwidthBitsPerSecond() = 0;

	/**
	 * @brief Return current robust throughput estimate (bytes/s).
	 */
	virtual double GetThroughputBytesPerSecond() = 0;

	/**
	 * @brief Return current overhead (TTFB) estimate (seconds).
	 */
	virtual double GetTimeToFirstByteSeconds() = 0;

	/**
	 * @brief Predict download completion time for a segment.
	 *
	 * Predicts how long it will take (wall-clock time) to download a
	 * segment of the given size using the estimator's current state.
	 *
	 * Units:
	 * - @p segment_size_bytes: bytes
	 * - return value: seconds
	 *
	 * Semantics:
	 * - Implementations may include protocol/connection overhead (e.g.
	 *   time-to-first-byte) in addition to payload transfer time.
	 * - If there is insufficient data to estimate bandwidth, or the
	 *   estimate is not usable (e.g. <= 0), implementations should return
	 *   0.0 to indicate "unavailable".
	 */
	virtual double GetPredictedDownloadTimeSeconds(std::size_t segment_size_bytes) = 0;

	/**
	 * @brief Reset only the currently available bandwidth estimate.
	 */
	virtual void ResetCurrentlyAvailableBandwidth()
	{
	}
};

#endif
