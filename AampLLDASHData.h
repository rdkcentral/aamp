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
	bool lowLatencyMode;        	/**< LL Playback mode enabled */
	bool strictSpecConformance; 	/**< Check for Strict LL Dash spec conformance*/
	double availabilityTimeOffset;  	/**< LL Availability Time Offset */
	bool availabilityTimeComplete;  	/**< LL Availability Time Complete */
	int targetLatency;          	/**< Target Latency of playback */
	int minLatency;             	/**< Minimum Latency of playback */
	int maxLatency;             	/**< Maximum Latency of playback */
	int latencyThreshold;       	/**< Latency when play rate correction kicks-in */
	double minPlaybackRate;     	/**< Minimum playback rate for playback */
	double maxPlaybackRate;     	/**< Maximum playback rate for playback */
	bool isSegTimeLineBased;		/**< Indicates is stream is segmenttimeline based */
	double fragmentDuration;		/**< Maximum Fragment Duration */
	UtcTiming utcTiming;		/**< Server UTC timings */

	AampLLDashServiceData() : lowLatencyMode(false),
			strictSpecConformance(false),
			availabilityTimeOffset(0.0),
			availabilityTimeComplete(false),
			targetLatency(0),
			minLatency(0),
			maxLatency(0),
			latencyThreshold(0),
			minPlaybackRate(0.0),
			maxPlaybackRate(0.0),
			isSegTimeLineBased(false),
			fragmentDuration(0.0),
			utcTiming(eUTC_HTTP_INVALID)
			{
			}
	/**< API to reset the Data*/
	void clear(void)
	{
		lowLatencyMode =false;
		strictSpecConformance = false;
		availabilityTimeOffset = 0.0;
		availabilityTimeComplete = false;
		targetLatency = 0;
		minLatency = 0;
		maxLatency = 0;
		latencyThreshold = 0;
		minPlaybackRate = 0.0;
		maxPlaybackRate = 0.0;
		isSegTimeLineBased = false;
		fragmentDuration = 0.0; 
		utcTiming = eUTC_HTTP_INVALID;
	}
};

#endif /* __AAMP_LL_DASH_DATA_H__ */
