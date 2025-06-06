/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#ifndef AAMP_TIME_BASED_BUFFER_MANAGER_CPP
#define AAMP_TIME_BASED_BUFFER_MANAGER_CPP

#include "AampTimeBasedBufferManager.hpp"

namespace aamp
{
	/**
	 * @brief Constructor for AampTimeBasedBufferManager.
	 *
	 * @param[in] maxBufferTime Maximum buffer time in seconds.
	 * @param[in] trickPlayMultiplier Multiplier for trick play mode (default is 1.0).
	 * @param[in] mediaType Media type for which this buffer is used.
	 */
	AampTimeBasedBufferManager::AampTimeBasedBufferManager(int maxBufferTime, double trickPlayMultiplier, AampMediaType mediaType)
		: maxBufferTime(maxBufferTime),
		  currentBufferTime(0),
		  trickPlayMultiplier(trickPlayMultiplier),
		  mediaType(mediaType)
	{
		AAMPLOG_DEBUG("[%s] maxBufferTime: %d, trickPlayMultiplier: %f", GetMediaTypeName(mediaType), maxBufferTime, trickPlayMultiplier);
		if (maxBufferTime <= 0 || trickPlayMultiplier <= 0)
		{
			throw std::invalid_argument("maxBufferTime and trickPlayMultiplier must be positive.");
		}
	}

	/**
	 * @brief Populate a specified amount of time to the buffer.
	 *
	 * @param[in] fragmentDuration Duration of the fragment in seconds.
	 */
	void AampTimeBasedBufferManager::PopulateBuffer(double fragmentDuration)
	{
		if (fragmentDuration < 0)
		{
			throw std::invalid_argument("Fragment duration must be non-negative.");
		}

		std::lock_guard<std::mutex> lock(mutex);
		currentBufferTime += fragmentDuration;
		AAMPLOG_DEBUG("[%s] Buffer time: %f", GetMediaTypeName(mediaType), currentBufferTime);
	}

	/**
	 * @brief Consume a specified amount of time from the buffer.
	 *
	 * @param[in] timeToConsume Amount of time to consume from the buffer in seconds.
	 */
	void AampTimeBasedBufferManager::ConsumeBuffer(double timeToConsume)
	{
		if (timeToConsume < 0)
		{
			throw std::invalid_argument("Time to consume must be non-negative.");
		}

		std::lock_guard<std::mutex> lock(mutex);
		currentBufferTime -= timeToConsume;
		AAMPLOG_DEBUG("[%s] Buffer time: %f\n", GetMediaTypeName(mediaType), currentBufferTime);
		if (currentBufferTime < 0)
		{
			currentBufferTime = 0;
		}
	}

	/**
	 * @brief Check if the buffer is full.
	 *
	 * @return True if buffer is full, false otherwise.
	 */
	bool AampTimeBasedBufferManager::IsFull() const
	{
		std::lock_guard<std::mutex> lock(mutex);
		bool ret = (currentBufferTime >= (maxBufferTime * trickPlayMultiplier));
		if (ret)
		{
			AAMPLOG_DEBUG("[%s] Buffer is full. Current buffer time: %f, Max buffer time: %d, Trick play multiplier: %f", GetMediaTypeName(mediaType), currentBufferTime, maxBufferTime, trickPlayMultiplier);
		}
		else
		{
			AAMPLOG_DEBUG("[%s] Buffer is not full. Current buffer time: %f, Max buffer time: %d, Trick play multiplier: %f", GetMediaTypeName(mediaType), currentBufferTime, maxBufferTime, trickPlayMultiplier);
		}
		return ret;
	}
} // namespace aamp

#endif // AAMP_TIME_BASED_BUFFER_MANAGER_CPP
