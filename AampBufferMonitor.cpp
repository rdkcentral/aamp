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
		if (mediaType == eMEDIATYPE_VIDEO)
		{
			sVideoStatus = status;
		}
		else if (mediaType == eMEDIATYPE_AUDIO)
		{
			sAudioStatus = status;
		}

		// If either track goes RED, declare underflow and pause pipeline.
		if (status == BUFFER_STATUS_RED)
		{
			if (!isUnderflowActive)
			{
				if (mediaType == eMEDIATYPE_VIDEO)
				{
					AAMPLOG_WARN("AampBufferMonitor [video] underflow detected; pausing pipeline.");
				}
				else if (mediaType == eMEDIATYPE_AUDIO)
				{
					AAMPLOG_WARN("AampBufferMonitor [audio] underflow detected; pausing pipeline.");
				}
				// Notify start of buffering/underflow and pause the pipeline.
				aamp->SendBufferChangeEvent(true);
				(void)aamp->PausePipeline(true, false);
			}
		}
		// If underflow is active and neither track is RED, resume.
		else if (status == BUFFER_STATUS_GREEN)
		{
			if (isUnderflowActive && sVideoStatus != BUFFER_STATUS_RED && sAudioStatus != BUFFER_STATUS_RED)
			{
				if (mediaType == eMEDIATYPE_VIDEO)
				{
					AAMPLOG_WARN("AampBufferMonitor [video] buffer recovered; resuming pipeline.");
				}
				else if (mediaType == eMEDIATYPE_AUDIO)
				{
					AAMPLOG_WARN("AampBufferMonitor [audio] buffer recovered; resuming pipeline.");
				}
				(void)aamp->PausePipeline(false, false);
				aamp->UpdateSubtitleTimestamp();
				aamp->SendBufferChangeEvent(false);
			}
		}
		// YELLOW is informational; no direct pipeline control.
	}
}
} // namespace aamp
