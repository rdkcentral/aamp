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
 * @file PrivateInstanceAAMPNotifiable.cpp
 * @brief Forwards IStreamSinkNotifiable calls to PrivateInstanceAAMP.
 */

#include "PrivateInstanceAAMPNotifiable.h"
#include "priv_aamp.h"
#include "AampLogManager.h"

PrivateInstanceAAMPNotifiable::PrivateInstanceAAMPNotifiable(
	PrivateInstanceAAMP *aamp) noexcept
	: m_aamp{aamp}
{
}

void PrivateInstanceAAMPNotifiable::ChangeAamp(
	PrivateInstanceAAMP *newAamp) noexcept
{
	AAMPLOG_TRACE("newAamp=%p", newAamp);
	m_aamp = newAamp;
}

void PrivateInstanceAAMPNotifiable::NotifyFirstFrameReceived(
	unsigned long ccDecoderHandle)
{
	AAMPLOG_TRACE("ccDecoderHandle=%lu", ccDecoderHandle);
	m_aamp->NotifyFirstFrameReceived(ccDecoderHandle);
}

void PrivateInstanceAAMPNotifiable::NotifyFirstBufferProcessed(
	const std::string &videoRectangle)
{
	AAMPLOG_TRACE("videoRectangle=%s", videoRectangle.c_str());
	m_aamp->NotifyFirstBufferProcessed(videoRectangle);
}

void PrivateInstanceAAMPNotifiable::NotifyFirstVideoFrameDisplayed()
{
	AAMPLOG_TRACE("NotifyFirstVideoFrameDisplayed");
	m_aamp->NotifyFirstVideoFrameDisplayed();
}

void PrivateInstanceAAMPNotifiable::LogFirstFrame()
{
	AAMPLOG_TRACE("LogFirstFrame");
	m_aamp->LogFirstFrame();
}

void PrivateInstanceAAMPNotifiable::LogTuneComplete()
{
	AAMPLOG_TRACE("LogTuneComplete");
	m_aamp->LogTuneComplete();
}

void PrivateInstanceAAMPNotifiable::NotifyEOSReached()
{
	AAMPLOG_TRACE("NotifyEOSReached");
	m_aamp->NotifyEOSReached();
}

void PrivateInstanceAAMPNotifiable::MonitorProgress(
	bool sync, bool beginningOfStream)
{
	AAMPLOG_TRACE("sync=%d beginningOfStream=%d", sync, beginningOfStream);
	m_aamp->MonitorProgress(sync, beginningOfStream);
}

double PrivateInstanceAAMPNotifiable::GetProgressReportIntervalSeconds()
{
	double intervalSeconds = 0.0;
	if (m_aamp == nullptr || m_aamp->mConfig == nullptr)
	{
		AAMPLOG_WARN("AAMP or config is null while reading progress interval");
	}
	else
	{
		intervalSeconds =
			m_aamp->mConfig->GetConfigValue(eAAMPConfig_ReportProgressInterval);
	}
	AAMPLOG_TRACE("intervalSeconds=%f", intervalSeconds);
	return intervalSeconds;
}

void PrivateInstanceAAMPNotifiable::NotifySpeedChanged(
	float rate, bool changeState)
{
	AAMPLOG_TRACE("rate=%f changeState=%d", rate, changeState);
	m_aamp->NotifySpeedChanged(rate, changeState);
}

AAMPPlayerState PrivateInstanceAAMPNotifiable::GetState()
{
	AAMPPlayerState state = m_aamp->GetState();
	AAMPLOG_TRACE("state=%d", static_cast<int>(state));
	return state;
}

void PrivateInstanceAAMPNotifiable::NotifyBufferUnderflow(AampMediaType type)
{
	AAMPLOG_TRACE("type=%d", static_cast<int>(type));
	if (m_aamp->mConfig->IsConfigSet(eAAMPConfig_EnableAampUnderflowMonitor))
	{
		AAMPLOG_INFO(
			"Underflow will be handled by AampUnderflowMonitor, "
			"skipping retune for mediaType=%d",
			static_cast<int>(type));
	}
	else
	{
		m_aamp->ScheduleRetune(eGST_ERROR_UNDERFLOW, type);
	}
}
