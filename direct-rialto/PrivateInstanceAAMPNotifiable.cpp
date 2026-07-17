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
#include <cinttypes>

// ---------------------------------------------------------------------------
// Parameter structs for async task dispatch.
//
// Each void IStreamSinkNotifiable method schedules its work on the AAMP
// scheduler thread via ScheduleAsyncTask rather than calling PrivateInstanceAAMP
// directly.  This breaks the potential AB/BA deadlock where the Rialto
// callback thread holds a Rialto lock and tries to acquire an AAMP lock
// (mLock / mStreamLock) while the AAMP thread holds the same AAMP lock and
// waits for a Rialto call to return.
//
// Methods with extra parameters beyond the AAMP pointer use a heap-allocated
// struct carried through the void * arg.  Methods with no extra parameters
// pass m_aamp directly as the arg.
// ---------------------------------------------------------------------------

namespace {

struct NotifyFirstFrameReceivedArgs
{
	PrivateInstanceAAMP *aamp;
	unsigned long ccDecoderHandle;
};

struct NotifyFirstBufferProcessedArgs
{
	PrivateInstanceAAMP *aamp;
	std::string videoRectangle;
};

struct MonitorProgressArgs
{
	PrivateInstanceAAMP *aamp;
	bool sync;
	bool beginningOfStream;
};

struct NotifySpeedChangedArgs
{
	PrivateInstanceAAMP *aamp;
	float rate;
	bool changeState;
};

struct NotifyBufferUnderflowArgs
{
	PrivateInstanceAAMP *aamp;
	AampMediaType type;
};

struct SendMonitorAvEventArgs
{
	PrivateInstanceAAMP *aamp;
	std::string status;
	int64_t videoPositionMs;
	int64_t audioPositionMs;
	uint64_t timeInStateMs;
	uint64_t droppedFrames;
};

} // namespace

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
	auto *args = new NotifyFirstFrameReceivedArgs{m_aamp, ccDecoderHandle};
	m_aamp->ScheduleAsyncTask([](void *p) -> int {
		auto *a = static_cast<NotifyFirstFrameReceivedArgs *>(p);
		a->aamp->NotifyFirstFrameReceived(a->ccDecoderHandle);
		delete a;
		return 0;
	}, args, "NotifyFirstFrameReceived");
}

void PrivateInstanceAAMPNotifiable::NotifyFirstBufferProcessed(
	const std::string &videoRectangle)
{
	AAMPLOG_TRACE("videoRectangle=%s", videoRectangle.c_str());
	auto *args = new NotifyFirstBufferProcessedArgs{m_aamp, videoRectangle};
	m_aamp->ScheduleAsyncTask([](void *p) -> int {
		auto *a = static_cast<NotifyFirstBufferProcessedArgs *>(p);
		a->aamp->NotifyFirstBufferProcessed(a->videoRectangle);
		delete a;
		return 0;
	}, args, "NotifyFirstBufferProcessed");
}

void PrivateInstanceAAMPNotifiable::NotifyFirstVideoFrameDisplayed()
{
	AAMPLOG_TRACE("NotifyFirstVideoFrameDisplayed");
	m_aamp->ScheduleAsyncTask([](void *p) -> int {
		static_cast<PrivateInstanceAAMP *>(p)->NotifyFirstVideoFrameDisplayed();
		return 0;
	}, m_aamp, "NotifyFirstVideoFrameDisplayed");
}

void PrivateInstanceAAMPNotifiable::LogFirstFrame()
{
	AAMPLOG_TRACE("LogFirstFrame");
	m_aamp->ScheduleAsyncTask([](void *p) -> int {
		static_cast<PrivateInstanceAAMP *>(p)->LogFirstFrame();
		return 0;
	}, m_aamp, "LogFirstFrame");
}

void PrivateInstanceAAMPNotifiable::LogTuneComplete()
{
	AAMPLOG_TRACE("LogTuneComplete");
	m_aamp->ScheduleAsyncTask([](void *p) -> int {
		static_cast<PrivateInstanceAAMP *>(p)->LogTuneComplete();
		return 0;
	}, m_aamp, "LogTuneComplete");
}

void PrivateInstanceAAMPNotifiable::NotifyEOSReached()
{
	AAMPLOG_TRACE("NotifyEOSReached");
	m_aamp->ScheduleAsyncTask([](void *p) -> int {
		static_cast<PrivateInstanceAAMP *>(p)->NotifyEOSReached();
		return 0;
	}, m_aamp, "NotifyEOSReached");
}

void PrivateInstanceAAMPNotifiable::MonitorProgress(
	bool sync, bool beginningOfStream)
{
	AAMPLOG_TRACE("sync=%d beginningOfStream=%d", sync, beginningOfStream);
	auto *args = new MonitorProgressArgs{m_aamp, sync, beginningOfStream};
	m_aamp->ScheduleAsyncTask([](void *p) -> int {
		auto *a = static_cast<MonitorProgressArgs *>(p);
		a->aamp->MonitorProgress(a->sync, a->beginningOfStream);
		delete a;
		return 0;
	}, args, "MonitorProgress");
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
	auto *args = new NotifySpeedChangedArgs{m_aamp, rate, changeState};
	m_aamp->ScheduleAsyncTask([](void *p) -> int {
		auto *a = static_cast<NotifySpeedChangedArgs *>(p);
		a->aamp->NotifySpeedChanged(a->rate, a->changeState);
		delete a;
		return 0;
	}, args, "NotifySpeedChanged");
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
	auto *args = new NotifyBufferUnderflowArgs{m_aamp, type};
	m_aamp->ScheduleAsyncTask([](void *p) -> int {
		auto *a = static_cast<NotifyBufferUnderflowArgs *>(p);
		if (a->aamp->mConfig->IsConfigSet(eAAMPConfig_EnableAampUnderflowMonitor))
		{
			AAMPLOG_INFO(
				"Underflow will be handled by AampUnderflowMonitor, "
				"skipping retune for mediaType=%d",
				static_cast<int>(a->type));
		}
		else
		{
			a->aamp->ScheduleRetune(eGST_ERROR_UNDERFLOW, a->type);
		}
		delete a;
		return 0;
	}, args, "NotifyBufferUnderflow");
}

void PrivateInstanceAAMPNotifiable::SendMonitorAvEvent(
	const std::string &status,
	int64_t videoPositionMs,
	int64_t audioPositionMs,
	uint64_t timeInStateMs,
	uint64_t droppedFrames)
{
	AAMPLOG_TRACE("status=%s videoMs=%" PRId64 " audioMs=%" PRId64
		" timeInStateMs=%" PRIu64 " dropped=%" PRIu64,
		status.c_str(), videoPositionMs, audioPositionMs,
		timeInStateMs, droppedFrames);
	auto *args = new SendMonitorAvEventArgs{
		m_aamp, status, videoPositionMs, audioPositionMs,
		timeInStateMs, droppedFrames};
	m_aamp->ScheduleAsyncTask([](void *p) -> int {
		auto *a = static_cast<SendMonitorAvEventArgs *>(p);
		a->aamp->SendMonitorAvEvent(
			a->status, a->videoPositionMs, a->audioPositionMs,
			a->timeInStateMs, a->droppedFrames);
		delete a;
		return 0;
	}, args, "SendMonitorAvEvent");
}
