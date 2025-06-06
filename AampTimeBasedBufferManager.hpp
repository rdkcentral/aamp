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

#include <mutex>
#include <stdexcept>
#include <iostream>
#include "priv_aamp.h"

namespace aamp
{
	/**
	 * @class AampTimeBasedBufferManager
	 * @brief A class for managing a time-based media buffer.
	 */
	class AampTimeBasedBufferManager
	{
	public:
		/**
		 * @brief Constructor for AampTimeBasedBufferManager.
		 *
		 * @param[in] maxBufferTime Maximum buffer time in seconds.
		 * @param[in] trickPlayMultiplier Multiplier for trick play mode (default is 1.0).
		 */
		AampTimeBasedBufferManager(int maxBufferTime, double trickPlayMultiplier = 1.0, AampMediaType mediaType = eMEDIATYPE_DEFAULT);

		/**
		 * @brief Default Destructor for AampTimeBasedBufferManager.
		 */
		~AampTimeBasedBufferManager() = default;

		/**
		 * @brief Populate a specified amount of time to the buffer.
		 *
		 * @param[in] fragmentDuration Duration of the fragment in seconds.
		 */
		void PopulateBuffer(double fragmentDuration);

		/**
		 * @brief Consume a specified amount of time from the buffer.
		 *
		 * @param[in] timeToConsume Amount of time to consume from the buffer in seconds.
		 */
		void ConsumeBuffer(double timeToConsume);

		/**
		 * @brief Check if the buffer is full.
		 *
		 * @return True if buffer is full, false otherwise.
		 */
		bool IsFull() const;

	private:
		int maxBufferTime;
		double currentBufferTime;
		double trickPlayMultiplier;
		mutable std::mutex mutex;
		AampMediaType mediaType; // Media type for which this buffer is used
	};
} // namespace aamp
