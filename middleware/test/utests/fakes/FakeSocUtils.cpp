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

#include "middleware/SocUtils.h"

namespace SocUtils
{
	void InitializePlatformConfigs()
	{
	}
	bool UseAppSrcForProgressivePlayback( void )
	{
		return false;
	}
	bool IsSupportedAC4( void )
	{
		return false;
	}
	bool UseWesterosSink( void )
	{
		return false;
	}
	bool IsAudioFragmentSyncSupported( void )
	{
		return false;
	}
	bool EnableLiveLatencyCorrection( void )
	{
		return false;
	}
	bool DisableAC3( void )
	{
		return false;
	}

	bool IsSupportedAC3()
	{
		return false;
	}

	int RequiredQueuedFrames( void )
	{
		return 0;
	}
	bool EnablePTSRestamp(void)
	{
		return false;
	}

	bool ResetNewSegmentEvent()
	{
		return false;
	}
}
