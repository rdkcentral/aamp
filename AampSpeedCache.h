/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file AampSpeedCache.h
 * @brief SpeedCache struct used by low-latency DASH ABR speed estimation
 *
 * Shared by priv_aamp.h, abr/abr.cpp and support/aampabr/HybridABRManager.cpp
 * to avoid duplicate definitions and the need to pull in priv_aamp.h for ABR-only users.
 */

#ifndef AAMP_SPEED_CACHE_H
#define AAMP_SPEED_CACHE_H

#include <vector>
#include <utility>

/**
 * @struct SpeedCache
 * @brief Stores the information for cache speed
 */
struct SpeedCache
{
	long last_sample_time_val;
	long prev_dlnow;
	long prevSampleTotalDownloaded;
	long totalDownloaded;
	long speed_now;
	long start_val;
	bool bStart;

	double totalWeight;
	double weightedBitsPerSecond;
	std::vector< std::pair<double,long> > mChunkSpeedData;

	SpeedCache() : last_sample_time_val(0), prev_dlnow(0), prevSampleTotalDownloaded(0), totalDownloaded(0), speed_now(0), start_val(0), bStart(false), totalWeight(0), weightedBitsPerSecond(0), mChunkSpeedData()
	{
	}
};

#endif /* AAMP_SPEED_CACHE_H */
