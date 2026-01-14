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
 * @file AampBufferMonitor.h
 * @brief Portable underflow monitor driven by player buffer status
 */

#ifndef __AAMP_BUFFER_MONITOR_H__
#define __AAMP_BUFFER_MONITOR_H__

#include "AampMediaType.h"
#include "StreamAbstractionAAMP.h"
#include "priv_aamp.h"

namespace aamp
{
/**
 * @class AampBufferMonitor
 * @brief Detects underflow using player-derived buffer status and controls pipeline pause/resume.
 *
 * This class avoids SoC-specific underflow signaling by relying on injected duration
 * versus elapsed playback time to infer true pipeline underflow and recovery.
 */
class AampBufferMonitor
{
public:
	/**
	 * @brief Handle buffer status transition for a media track.
	 *
	 * @param[in] aamp		Private AAMP instance.
	 * @param[in] mediaType	Media type of the track (video/audio).
	 * @param[in] status		Computed buffer health status.
	 */
	static void HandleStatus(PrivateInstanceAAMP* aamp, AampMediaType mediaType, BufferHealthStatus status);
};
} // namespace aamp

#endif // __AAMP_BUFFER_MONITOR_H__
