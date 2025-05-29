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

/**
 * @file SocUtils.cpp
 */
#include "SocUtils.h"
#include "SocInterface.h"
#include "InterfacePlayerRDK.h"
#include <assert.h>

namespace SocUtils
{
	static std::shared_ptr<SocInterface> socInterface = SocInterface::CreateSocInterface();
	/**
	 * @brief Checks if AppSrc should be used for progressive playback.
	 *
	 * This function queries the SOC interface to determine whether AppSrc
	 * should be used for handling progressive playback.
	 *
	 * @return true if AppSrc is used, false otherwise.
	 */
	bool UseAppSrcForProgressivePlayback( void )
	{
		return socInterface->UseAppSrc();
	}

	/**
	 * @brief Determines if AC-4 audio format is supported.
	 *
	 * This function checks the SOC interface for AC-4 support and also verifies
	 * if the codec is supported at the InterfacePlayerRDK level.
	 *
	 * @return true if AC-4 is supported, false otherwise.
	 */
	bool IsSupportedAC4( void )
	{
		bool disableAc = socInterface->IsSupportedAC4();
		return (disableAc || (!InterfacePlayerRDK::IsCodecSupported("ac-4")));
	}

	/**
	 * @brief Determines if AC-3 audio format is supported.
	 *
	 * This function checks whether the AC-3 codec is supported by InterfacePlayerRDK.
	 *
	 * @return true if AC-3 is supported, false otherwise.
	 */
	bool IsSupportedAC3( void )
	{
		return (!InterfacePlayerRDK::IsCodecSupported("ac-3"));
	}

	/**
	 * @brief Checks if Westeros sink is used.
	 *
	 * This function queries the SOC interface to determine whether the Westeros sink
	 * is enabled for rendering video.
	 *
	 * @return true if Westeros sink is used, false otherwise.
	 */
	bool UseWesterosSink( void )
	{
		return socInterface->UseWesterosSink();
	}

	/**
	 * @brief Determines if audio fragment synchronization is supported.
	 *
	 * Queries the SOC interface to check if audio fragment sync is supported.
	 *
	 * @return true if audio fragment sync is supported, false otherwise.
	 */
	bool IsAudioFragmentSyncSupported( void )
	{
		return socInterface->IsAudioFragmentSyncSupported();
	}

	/**
	 * @brief Checks if live latency correction is enabled.
	 *
	 * This function queries the SOC interface to determine whether live latency
	 * correction is enabled.
	 *
	 * @return true if live latency correction is enabled, false otherwise.
	 */
	bool EnableLiveLatencyCorrection( void )
	{
		return socInterface->EnableLiveLatencyCorrection();
	}

	/**
	 * @brief Retrieves the number of required queued frames.
	 *
	 * Queries the SOC interface to get the required number of frames
	 * that should be queued for smooth playback.
	 *
	 * @return The required number of queued frames.
	 */
	int RequiredQueuedFrames( void )
	{
		return socInterface->RequiredQueuedFrames();
	}

	/**
	 * @brief Checks if PTS (Presentation Timestamp) re-stamping is enabled.
	 *
	 * This function queries the SOC interface to determine whether
	 * PTS re-stamping is enabled.
	 *
	 * @return true if PTS re-stamping is enabled, false otherwise.
	 */
	bool EnablePTSRestamp(void)
	{
		return socInterface->EnablePTSRestamp();
	}
	/**
	 * @brief Resets segment event flags during trickplay transitions.
	 *
	 * Manages segment event tracking for trickplay scenarios without disrupting seekplay or advertisements.
	 */
	bool ResetNewSegmentEvent()
	{
		return socInterface->ResetNewSegmentEvent();
	}

}
