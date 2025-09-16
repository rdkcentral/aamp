/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#ifndef AAMP_MOCK_AAMP_PRIV_AAMP_H
#define AAMP_MOCK_AAMP_PRIV_AAMP_H

#include <gmock/gmock.h>
#include "priv_aamp.h"

class MockPrivateInstanceAAMP
{
public:
    MOCK_METHOD(double, RecalculatePTS, (AampMediaType mediaType, const void *ptr, size_t len));
    
	MOCK_METHOD(void, Stop, (bool sendStateChangeEvent));

	MOCK_METHOD(void, StartPausePositionMonitoring, (long long pausePositionMilliseconds));

	MOCK_METHOD(void, StopPausePositionMonitoring, (std::string reason));

	MOCK_METHOD(AAMPPlayerState, GetState, ());

	MOCK_METHOD(void, SetState, (AAMPPlayerState state));

	MOCK_METHOD(bool, GetFile, (std::string remoteUrl, AampMediaType mediaType, AampGrowableBuffer *buffer, std::string& effectiveUrl,
				int * http_error, double *downloadTime, const char *range, unsigned int curlInstance,
				bool resetBuffer, BitsPerSecond *bitrate, int * fogError,
				double fragmentDurationSeconds, ProfilerBucketType bucketType, int maxInitDownloadTimeMS));
	MOCK_METHOD(void, SetStreamFormat, (StreamOutputFormat videoFormat, StreamOutputFormat audioFormat, StreamOutputFormat auxFormat));

	MOCK_METHOD(std::string, GetAvailableAudioTracks, (bool allTrack));
	MOCK_METHOD(int,GetAudioTrack,());
	MOCK_METHOD(void, SendErrorEvent, (AAMPTuneFailure, const char *, bool, int32_t, int32_t, int32_t, const std::string &));
	MOCK_METHOD(void, SendStreamTransfer, (AampMediaType, AampGrowableBuffer*, double, double, double, double, bool, bool));
	MOCK_METHOD(bool, SendStreamCopy, (AampMediaType, const void *, size_t, double, double, double));
	MOCK_METHOD(MediaFormat,GetMediaFormatTypeEnum,());
	MOCK_METHOD(long long, GetPositionMs, ());
	MOCK_METHOD(long long, GetPositionMilliseconds, ());
	MOCK_METHOD(int, ScheduleAsyncTask, (IdleTask task, void *arg, std::string taskName));
	MOCK_METHOD(bool, RemoveAsyncTask, (int taskId));

	MOCK_METHOD(const std::string &, GetSessionId, ());

	MOCK_METHOD(std::shared_ptr<TSB::Store>, GetTSBStore, (const TSB::Store::Config& config, TSB::LogFunction logger, TSB::LogLevel level));

	MOCK_METHOD(void, FoundEventBreak, (const std::string &adBreakId, uint64_t startMS, EventBreakInfo brInfo));
	MOCK_METHOD(void, SaveNewTimedMetadata, (long long timeMS, const char* id, double durationMS));
	MOCK_METHOD(bool, DownloadsAreEnabled, ());
	MOCK_METHOD(void, SendAdResolvedEvent, (const std::string &adId, bool status, uint64_t startMS, uint64_t durationMs, AAMPCDAIError errorCode));
	MOCK_METHOD(uint32_t, GetAudTimeScale, ());
	MOCK_METHOD(uint32_t, GetVidTimeScale, ());
	MOCK_METHOD(void, ProcessID3Metadata, (char *, size_t , AampMediaType , uint64_t ));
	MOCK_METHOD(void, SetPauseOnStartPlayback, (bool enable));
	MOCK_METHOD(bool, isDecryptClearSamplesRequired, ());
	MOCK_METHOD(long long, DurationFromStartOfPlaybackMs, ());
	MOCK_METHOD(bool, IsLocalAAMPTsbInjection, ());
	MOCK_METHOD(void, UpdateLocalAAMPTsbInjection, ());
	MOCK_METHOD(bool,  GetLLDashAdjustSpeed, ());
	MOCK_METHOD(double, GetLLDashCurrentPlayBackRate, ());
	MOCK_METHOD(void, StopDownloads, ());
	MOCK_METHOD(void, ResumeDownloads, ());
	MOCK_METHOD(void, DisableDownloads, ());
	MOCK_METHOD(void, TuneHelper, (TuneType tuneType, bool seekWhilePaused));
	MOCK_METHOD(AampTSBSessionManager*, GetTSBSessionManager, ());
	MOCK_METHOD(void, NotifyOnEnteringLive, ());

	MOCK_METHOD(void, SendAdPlacementEvent, (AAMPEventType, const std::string &, uint32_t, uint64_t, uint32_t, uint32_t, bool, long));
	MOCK_METHOD(void, SendAdReservationEvent, (AAMPEventType, const std::string &, uint64_t, uint64_t, bool));
	MOCK_METHOD(void, CalculateTrickModePositionEOS, ());
	MOCK_METHOD(void, BlockUntilGstreamerWantsData, (void(*cb)(void), int , int ));
	MOCK_METHOD(void, WaitForDiscontinuityProcessToComplete, ());
	MOCK_METHOD(double, GetLivePlayPosition, ());
	MOCK_METHOD(bool, GetLLDashChunkMode, ());
	MOCK_METHOD(void, SetLLDashChunkMode, (bool enable));
	MOCK_METHOD(void, NotifySpeedChanged, (float rate, bool changeState));
};

extern MockPrivateInstanceAAMP *g_mockPrivateInstanceAAMP;

#endif /* AAMP_MOCK_AAMP_PRIV_AAMP_H */
