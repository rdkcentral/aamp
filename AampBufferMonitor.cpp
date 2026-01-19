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
 * @file AampBufferMonitor.cpp
 * @brief Implementation of portable underflow monitor.
 */

#include "AampBufferMonitor.h"
#include "AampUtils.h"
#include <mutex>

namespace aamp
{
static const char* BufferStatusStr(BufferHealthStatus status)
{
	switch (status)
	{
		case BUFFER_STATUS_GREEN: return "GREEN";
		case BUFFER_STATUS_YELLOW: return "YELLOW";
		case BUFFER_STATUS_RED: return "RED";
		default: return "UNKNOWN";
	}
}

void AampBufferMonitor::HandleStatus(PrivateInstanceAAMP* aamp, AampMediaType mediaType, BufferHealthStatus status)
{
	if (!aamp)
	{
		return;
	}

	// Aggregate audio/video statuses to avoid thrash and ensure consistent pause/resume.
	static std::mutex sMutex;
	static BufferHealthStatus sVideoStatus = BUFFER_STATUS_GREEN;
	static BufferHealthStatus sAudioStatus = BUFFER_STATUS_GREEN;

	const bool isUnderflowActive = aamp->GetBufUnderFlowStatus();

	{
		std::lock_guard<std::mutex> lock(sMutex);
		if(mediaType == eMEDIATYPE_VIDEO)
		{
			sVideoStatus = status;
		}
		else if(mediaType == eMEDIATYPE_AUDIO)
		{
			sAudioStatus = status;
		}

		// Fetch latest buffered time for this media type
		double bufferedTimeSec = aamp->GetBufferedTime(mediaType);
		AAMPLOG_INFO("AampBufferMonitor: track=%s, buffered=%.2f, status=%s, underflowActive=%d", 
			mediaType == eMEDIATYPE_VIDEO ? "video" : "audio", bufferedTimeSec, BufferStatusStr(status), isUnderflowActive ? 1 : 0);
		// Underflow detection should start only once pipeline is in playing state.
		// Allow resume checks even when not playing only if underflow is already active.
		AAMPPlayerState playerState = aamp->GetState();
		if (playerState != eSTATE_PLAYING && !isUnderflowActive)
		{
			AAMPLOG_INFO("AampBufferMonitor: pipeline not playing, skipping underflow check.");
			return;
		}

		// If buffer reaches 0 or status goes RED, declare underflow and pause.
		if (status == BUFFER_STATUS_RED || bufferedTimeSec <= 0.0)
		{
			if (!isUnderflowActive)
			{
				if (mediaType == eMEDIATYPE_VIDEO)
				{
					AAMPLOG_INFO("AampBufferMonitor [video] underflow detected; pausing pipeline.");
				}
				else if (mediaType == eMEDIATYPE_AUDIO)
				{
					AAMPLOG_INFO("AampBufferMonitor [audio] underflow detected; pausing pipeline.");
				}
				// Notify start of buffering/underflow and pause the pipeline.
			//	aamp->SendBufferChangeEvent(true);
			//	(void)aamp->PausePipeline(true, false);
			}
		}
		// Resume condition: underflow active, neither track RED, and buffered time >= 1s.
		else
		{
			const double resumeThresholdSec = 1.0;
			if (isUnderflowActive && sVideoStatus != BUFFER_STATUS_RED && sAudioStatus != BUFFER_STATUS_RED && bufferedTimeSec >= resumeThresholdSec)
			{
				if (mediaType == eMEDIATYPE_VIDEO)
				{
					AAMPLOG_INFO("AampBufferMonitor [video] buffer >= %.2fs; resuming pipeline.", resumeThresholdSec);
				}
				else if (mediaType == eMEDIATYPE_AUDIO)
				{
					AAMPLOG_INFO("AampBufferMonitor [audio] buffer >= %.2fs; resuming pipeline.", resumeThresholdSec);
				}
			//	(void)aamp->PausePipeline(false, false);
			//	aamp->UpdateSubtitleTimestamp();
			//	aamp->SendBufferChangeEvent(false);
			}
		}
		// YELLOW is informational; no direct pipeline control.
	}
}
} // namespace aamp
