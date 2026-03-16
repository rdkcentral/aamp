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
 * @file FragmentCacheDescriptor.h
 * @brief Descriptor structure for unified fragment caching API
 */

#ifndef FRAGMENT_CACHE_DESCRIPTOR_H
#define FRAGMENT_CACHE_DESCRIPTOR_H

#include "AampMediaType.h"
#include <cstdint>
#include <string>
#include <vector>

/**
 * @struct FragmentCacheDescriptor
 * @brief Unified descriptor for fragment caching operations
 *        Supports both full-fragment downloads and chunked injection (LL-DASH)
 */
struct FragmentCacheDescriptor
{
	// ========================================
	// Data Source (mutually exclusive based on mode)
	// ========================================
	
	/**
	 * @brief Chunk mode payload pointer (ephemeral CURL callback buffer)
	 *        COPY REQUIRED - buffer is temporary and owned by CURL
	 */
	const char* chunkPayload;
	
	/**
	 * @brief Fragment mode buffer (for ZERO-COPY move)
	 *        Ownership transferred via std::move() to CachedFragment
	 */
	std::vector<uint8_t>* downloadBuffer;
	
	/**
	 * @brief Size of payload in bytes
	 */
	size_t payloadSize;
	
	// ========================================
	// Fragment Metadata
	// ========================================
	
	/**
	 * @brief Fragment URL (for logging/debug)
	 */
	std::string url;
	
	/**
	 * @brief Position in playlist (seconds)
	 */
	double position;
	
	/**
	 * @brief Fragment duration (seconds)
	 */
	double duration;
	
	/**
	 * @brief Absolute position (seconds) - for live: epoch time, for VOD: period-relative
	 */
	double absolutePosition;
	
	/**
	 * @brief Timescale from manifest
	 */
	uint32_t timeScale;
	
	/**
	 * @brief PTS offset to apply for this segment (seconds)
	 */
	double ptsOffsetSec;
	
	// ========================================
	// Type Information
	// ========================================
	
	/**
	 * @brief Media type (video/audio/subtitle/etc)
	 */
	AampMediaType mediaType;
	
	/**
	 * @brief CURL instance identifier
	 */
	unsigned int curlInstance;
	
	/**
	 * @brief Byte range (NULL if not used)
	 */
	const char* range;
	
	/**
	 * @brief Profile index for bitrate tracking
	 */
	int profileIndex;
	
	// ========================================
	// Behavioral Control Flags
	// ========================================
	
	/**
	 * @brief Init vs media segment
	 */
	bool isInitSegment;
	
	/**
	 * @brief PTS discontinuity flag
	 */
	bool isDiscontinuity;
	
	/**
	 * @brief Chunk mode (true) vs fragment mode (false)
	 *        Controls buffer routing and copy behavior
	 */
	bool isChunkMode;
	
	/**
	 * @brief Skip init segment parsing (timescale extraction)
	 *        Typically true for chunk mode
	 */
	bool skipInitSegmentParsing;
	
	/**
	 * @brief Ad playback indicator
	 */
	bool playingAd;
	
	// ========================================
	// Timing
	// ========================================
	
	/**
	 * @brief Download start time (epoch milliseconds)
	 *        Uses uint64_t for platform consistency (was long long)
	 */
	uint64_t downloadStartTime;
	
	/**
	 * @brief Default constructor
	 */
	FragmentCacheDescriptor()
		: chunkPayload(nullptr)
		, downloadBuffer(nullptr)
		, payloadSize(0)
		, url()
		, position(0.0)
		, duration(0.0)
		, absolutePosition(0.0)
		, timeScale(0)
		, ptsOffsetSec(0.0)
		, mediaType(eMEDIATYPE_DEFAULT)
		, curlInstance(0)
		, range(nullptr)
		, profileIndex(0)
		, isInitSegment(false)
		, isDiscontinuity(false)
		, isChunkMode(false)
		, skipInitSegmentParsing(false)
		, playingAd(false)
		, downloadStartTime(0)
	{
	}
};

#endif // FRAGMENT_CACHE_DESCRIPTOR_H
