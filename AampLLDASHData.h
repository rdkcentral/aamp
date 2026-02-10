/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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

/**************************************
* @file AampLLDASHData.h
* @brief Data types for AAMP LL DASH
**************************************/

#ifndef __AAMP_LL_DASH_DATA_H__
#define __AAMP_LL_DASH_DATA_H__

/**
 * @brief To store Low Latency Service configurations
 */
struct AampLLDashServiceData {
	bool lowLatencyMode = false;				/**< LL Playback mode enabled */
	bool strictSpecConformance = false;			/**< Check for Strict LL Dash spec conformance*/
	double availabilityTimeOffset = 0.0;		/**< LL Availability Time Offset */
	bool availabilityTimeComplete = false;		/**< LL Availability Time Complete */
	int targetLatency = 0;						/**< Target Latency of playback */
	int minLatency = 0;							/**< Minimum Latency of playback */
	int maxLatency = 0;							/**< Maximum Latency of playback */
	int latencyThreshold = 0;					/**< Latency when play rate correction kicks-in */
	double minPlaybackRate = 0.0;				/**< Minimum playback rate for playback */
	double maxPlaybackRate = 0.0;				/**< Maximum playback rate for playback */
	bool isSegTimeLineBased = false;			/**< Indicates is stream is segmenttimeline based */
	double fragmentDuration = 0.0;				/**< Maximum Fragment Duration */
	UtcTiming utcTiming = eUTC_HTTP_INVALID;	/**< Server UTC timings */

	void clear()
	{
		*this = {};
	}
};

#endif /* __AAMP_LL_DASH_DATA_H__ */
