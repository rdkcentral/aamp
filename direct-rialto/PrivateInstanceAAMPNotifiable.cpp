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
#include "StreamAbstractionAAMP.h"
#include "AampStreamSinkManager.h"
#include "AampLogManager.h"
#include <cinttypes>
#include <memory>
#include <utility>

// ---------------------------------------------------------------------------
// Async task dispatch.
//
// Each void IStreamSinkNotifiable method schedules its work on the AAMP
// scheduler thread via ScheduleAsyncTask rather than calling PrivateInstanceAAMP
// directly.  This breaks the potential AB/BA deadlock where the Rialto
// callback thread holds a Rialto lock and tries to acquire an AAMP lock
// (mLock / mStreamLock) while the AAMP thread holds the same AAMP lock and
// waits for a Rialto call to return.
//
// Methods with extra parameters beyond the AAMP pointer go through
// ScheduleOwned(), which carries them in a heap-allocated task owned by a
// unique_ptr.  Methods with no extra parameters pass the AAMP pointer
// directly as the arg.
// ---------------------------------------------------------------------------

namespace {

/**
 * @brief Run @p work(aamp) on the AAMP scheduler thread.
 *
 * The captured state is freed after the task has run, or here if the
 * scheduler refused the task.  A task the scheduler accepts but later drops
 * unrun (it clears its queue on stop) is not freed: AsyncTaskObj has no
 * destroy hook.
 */
template <typename Work>
void ScheduleOwned(PrivateInstanceAAMP *aamp, const char *name, Work work)
{
	struct Task
	{
		PrivateInstanceAAMP *aamp;
		Work work;
	};

	auto task = std::make_unique<Task>(Task{aamp, std::move(work)});
	const int id = aamp->ScheduleAsyncTask([](void *p) -> int {
		std::unique_ptr<Task> owned(static_cast<Task *>(p));
		owned->work(owned->aamp);
		return 0;
	}, task.get(), name);
	if (id != AAMP_TASK_ID_INVALID)
	{
		// The task now owns it.
		task.release();
	}
}

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
	m_aamp.store(newAamp, std::memory_order_release);
}

PrivateInstanceAAMP *PrivateInstanceAAMPNotifiable::Target(
	const char *caller) const
{
	PrivateInstanceAAMP *aamp = m_aamp.load(std::memory_order_acquire);
	if (aamp == nullptr)
	{
		AAMPLOG_WARN("%s: no AAMP instance - notification dropped", caller);
	}
	return aamp;
}

void PrivateInstanceAAMPNotifiable::NotifyFirstFrameReceived(
	unsigned long ccDecoderHandle)
{
	AAMPLOG_TRACE("ccDecoderHandle=%lu", ccDecoderHandle);
	PrivateInstanceAAMP *aamp = Target("NotifyFirstFrameReceived");
	if (aamp == nullptr)
	{
		return;
	}
	ScheduleOwned(aamp, "NotifyFirstFrameReceived",
		[ccDecoderHandle](PrivateInstanceAAMP *target) {
			target->NotifyFirstFrameReceived(ccDecoderHandle);
		});
}

void PrivateInstanceAAMPNotifiable::NotifyFirstBufferProcessed(
	const std::string &videoRectangle)
{
	AAMPLOG_TRACE("videoRectangle=%s", videoRectangle.c_str());
	PrivateInstanceAAMP *aamp = Target("NotifyFirstBufferProcessed");
	if (aamp == nullptr)
	{
		return;
	}
	ScheduleOwned(aamp, "NotifyFirstBufferProcessed",
		[videoRectangle](PrivateInstanceAAMP *target) {
			target->NotifyFirstBufferProcessed(videoRectangle);
		});
}

void PrivateInstanceAAMPNotifiable::NotifyFirstVideoFrameDisplayed()
{
	AAMPLOG_TRACE("NotifyFirstVideoFrameDisplayed");
	PrivateInstanceAAMP *aamp = Target("NotifyFirstVideoFrameDisplayed");
	if (aamp == nullptr)
	{
		return;
	}
	aamp->ScheduleAsyncTask([](void *p) -> int {
		static_cast<PrivateInstanceAAMP *>(p)->NotifyFirstVideoFrameDisplayed();
		return 0;
	}, aamp, "NotifyFirstVideoFrameDisplayed");
}

void PrivateInstanceAAMPNotifiable::LogFirstFrame()
{
	AAMPLOG_TRACE("LogFirstFrame");
	PrivateInstanceAAMP *aamp = Target("LogFirstFrame");
	if (aamp == nullptr)
	{
		return;
	}
	aamp->ScheduleAsyncTask([](void *p) -> int {
		static_cast<PrivateInstanceAAMP *>(p)->LogFirstFrame();
		return 0;
	}, aamp, "LogFirstFrame");
}

void PrivateInstanceAAMPNotifiable::LogTuneComplete()
{
	AAMPLOG_TRACE("LogTuneComplete");
	PrivateInstanceAAMP *aamp = Target("LogTuneComplete");
	if (aamp == nullptr)
	{
		return;
	}
	aamp->ScheduleAsyncTask([](void *p) -> int {
		static_cast<PrivateInstanceAAMP *>(p)->LogTuneComplete();
		return 0;
	}, aamp, "LogTuneComplete");
}

void PrivateInstanceAAMPNotifiable::NotifyEOSReached()
{
	AAMPLOG_TRACE("NotifyEOSReached");
	PrivateInstanceAAMP *aamp = Target("NotifyEOSReached");
	if (aamp == nullptr)
	{
		return;
	}
	aamp->ScheduleAsyncTask([](void *p) -> int {
		static_cast<PrivateInstanceAAMP *>(p)->NotifyEOSReached();
		return 0;
	}, aamp, "NotifyEOSReached");
}

void PrivateInstanceAAMPNotifiable::MonitorProgress(
	bool sync, bool beginningOfStream)
{
	AAMPLOG_TRACE("sync=%d beginningOfStream=%d", sync, beginningOfStream);
	PrivateInstanceAAMP *aamp = Target("MonitorProgress");
	if (aamp == nullptr)
	{
		return;
	}
	ScheduleOwned(aamp, "MonitorProgress",
		[sync, beginningOfStream](PrivateInstanceAAMP *target) {
			target->MonitorProgress(sync, beginningOfStream);
		});
}

double PrivateInstanceAAMPNotifiable::GetProgressReportIntervalSeconds()
{
	double intervalSeconds = 0.0;
	PrivateInstanceAAMP *aamp = m_aamp.load(std::memory_order_acquire);
	if (aamp == nullptr || aamp->mConfig == nullptr)
	{
		AAMPLOG_WARN("AAMP or config is null while reading progress interval");
	}
	else
	{
		intervalSeconds =
			aamp->mConfig->GetConfigValue(eAAMPConfig_ReportProgressInterval);
	}
	AAMPLOG_TRACE("intervalSeconds=%f", intervalSeconds);
	return intervalSeconds;
}

void PrivateInstanceAAMPNotifiable::NotifySpeedChanged(
	float rate, bool changeState)
{
	AAMPLOG_TRACE("rate=%f changeState=%d", rate, changeState);
	PrivateInstanceAAMP *aamp = Target("NotifySpeedChanged");
	if (aamp == nullptr)
	{
		return;
	}
	ScheduleOwned(aamp, "NotifySpeedChanged",
		[rate, changeState](PrivateInstanceAAMP *target) {
			target->NotifySpeedChanged(rate, changeState);
		});
}

AAMPPlayerState PrivateInstanceAAMPNotifiable::GetState()
{
	AAMPPlayerState state = eSTATE_IDLE;
	if (PrivateInstanceAAMP *aamp = Target("GetState"))
	{
		state = aamp->GetState();
	}
	AAMPLOG_TRACE("state=%d", static_cast<int>(state));
	return state;
}

void PrivateInstanceAAMPNotifiable::NotifyBufferUnderflow(AampMediaType type)
{
	AAMPLOG_TRACE("type=%d", static_cast<int>(type));
	PrivateInstanceAAMP *aamp = Target("NotifyBufferUnderflow");
	if (aamp == nullptr)
	{
		return;
	}
	ScheduleOwned(aamp, "NotifyBufferUnderflow",
		[type](PrivateInstanceAAMP *target) {
			if (target->mConfig &&
			    target->mConfig->IsConfigSet(eAAMPConfig_EnableAampUnderflowMonitor))
			{
				AAMPLOG_INFO(
					"Underflow will be handled by AampUnderflowMonitor, "
					"skipping retune for mediaType=%d",
					static_cast<int>(type));
			}
			else
			{
				target->ScheduleRetune(eGST_ERROR_UNDERFLOW, type);
			}
		});
}

void PrivateInstanceAAMPNotifiable::NotifyPlaybackError(
	AAMPTuneFailure failure, const std::string &description,
	bool isRetryEnabled)
{
	AAMPLOG_TRACE("failure=%d isRetryEnabled=%d description=%s",
		static_cast<int>(failure), isRetryEnabled, description.c_str());
	PrivateInstanceAAMP *aamp = Target("NotifyPlaybackError");
	if (aamp == nullptr)
	{
		return;
	}
	ScheduleOwned(aamp, "NotifyPlaybackError",
		[failure, description, isRetryEnabled](PrivateInstanceAAMP *target) {
			target->SendErrorEvent(failure, description.c_str(), isRetryEnabled);
		});
}

void PrivateInstanceAAMPNotifiable::NotifyOutputProtectionRecovered()
{
	AAMPLOG_TRACE("NotifyOutputProtectionRecovered");
	PrivateInstanceAAMP *aamp = Target("NotifyOutputProtectionRecovered");
	if (aamp == nullptr)
	{
		return;
	}
	aamp->ScheduleAsyncTask([](void *p) -> int {
		auto *target = static_cast<PrivateInstanceAAMP *>(p);
		// Mute the sink directly (transient, sink-scoped), not aamp's own
		// SetVideoMute() - that sets the persistent video_muted flag, which
		// TuneHelper() would then re-apply forever on the retuned pipeline.
		StreamSink *sink = AampStreamSinkManager::GetInstance().GetStreamSink(target);
		if (sink)
		{
			sink->SetVideoMute(true);
		}
		target->ScheduleRetune(eGST_ERROR_OUTPUT_PROTECTION_ERROR, eMEDIATYPE_VIDEO);
		return 0;
	}, aamp, "NotifyOutputProtectionRecovered");
}

// CompleteDiscontinuityDataDeliverForPTSRestamp() and
// NotifyPipelinePausedToUnderflowMonitor() are called synchronously, unlike
// the notifications above.  Those are deferred via ScheduleAsyncTask because
// they originate on a Rialto IPC callback thread that may be holding a
// Rialto-side lock while AAMP holds its own lock (AB/BA deadlock risk).
// These two instead run on AAMP's own fragment-injector thread, and the
// GStreamer reference (AAMPGstPlayer::Discontinuity()) makes the equivalent
// calls synchronously from the same call chain, so this adds no new hazard.
// Note PrivateInstanceAAMP::Discontinuity() still holds mLock across the
// sink call: re-entering AAMP here is safe because mLock is recursive and
// both targets are leaf operations that take only their own mutex
// (mDiscoCompleteLock / mUnderflowMonitorMutex) and never call back out.
void PrivateInstanceAAMPNotifiable::CompleteDiscontinuityDataDeliverForPTSRestamp(
	AampMediaType type)
{
	AAMPLOG_TRACE("type=%d", static_cast<int>(type));
	PrivateInstanceAAMP *aamp = Target("CompleteDiscontinuityDataDeliverForPTSRestamp");
	if (aamp == nullptr)
	{
		return;
	}
	aamp->CompleteDiscontinuityDataDeliverForPTSRestamp(type);
}

void PrivateInstanceAAMPNotifiable::NotifyPipelinePausedToUnderflowMonitor()
{
	AAMPLOG_TRACE("NotifyPipelinePausedToUnderflowMonitor");
	PrivateInstanceAAMP *aamp = Target("NotifyPipelinePausedToUnderflowMonitor");
	if (aamp == nullptr)
	{
		return;
	}
	// Read without mStreamLock, matching the GStreamer reference; the null
	// check guards the common teardown race, not object lifetime.
	if (aamp->mpStreamAbstractionAAMP)
	{
		aamp->mpStreamAbstractionAAMP->NotifyPipelinePausedToUnderflowMonitor();
	}
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
	PrivateInstanceAAMP *aamp = Target("SendMonitorAvEvent");
	if (aamp == nullptr)
	{
		return;
	}
	ScheduleOwned(aamp, "SendMonitorAvEvent",
		[status, videoPositionMs, audioPositionMs, timeInStateMs,
		 droppedFrames](PrivateInstanceAAMP *target) {
			target->SendMonitorAvEvent(status, videoPositionMs,
				audioPositionMs, timeInStateMs, droppedFrames);
		});
}
