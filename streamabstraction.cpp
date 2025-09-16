/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file streamabstraction.cpp
 * @brief Definition of common class functions used by fragment collectors.
 */
#include "AampStreamSinkManager.h"
#include "StreamAbstractionAAMP.h"
#include "AampUtils.h"
#include "isobmffprocessor.h"
#include "ElementaryProcessor.h"
#include "isobmffbuffer.h"
#include "AampCacheHandler.h"
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <iterator>
#include <sys/time.h>
#include <cmath>
#include "AampTSBSessionManager.h"
#include "isobmffhelper.h"
#include "AampConfig.h"
#include "SubtecFactory.hpp"
#include "AampUtils.h"

// checks if current state is going to use IFRAME ( Fragment/Playlist )
#define IS_FOR_IFRAME(rate, type) ((type == eTRACK_VIDEO) && (rate != AAMP_NORMAL_PLAY_RATE))

/* Arbitrary value to enable restamping the media segments PTS and duration with adequate precision */
static constexpr uint32_t TRICKMODE_TIMESCALE{100000};

using namespace std;


AampMediaType TrackTypeToMediaType( TrackType trackType )
{
	switch( trackType )
	{
		case eTRACK_VIDEO:
			return eMEDIATYPE_PLAYLIST_VIDEO;
		case eTRACK_AUDIO:
			return eMEDIATYPE_PLAYLIST_AUDIO;
		case eTRACK_SUBTITLE:
			return eMEDIATYPE_PLAYLIST_SUBTITLE;
		case eTRACK_AUX_AUDIO:
			return eMEDIATYPE_PLAYLIST_AUX_AUDIO;
			//case eTRACK_IFRAME:
			//	return eMEDIATYPE_PLAYLIST_IFRAME;
		default:
			return eMEDIATYPE_DEFAULT;
	}
}

/**
 * @brief Start playlist downloader loop
 */
void MediaTrack::StartPlaylistDownloaderThread()
{
	AAMPLOG_DEBUG("Starting playlist downloader for %s", name);
	if(!playlistDownloaderThreadStarted)
	{
		// Start a new thread for this track
		if(NULL == playlistDownloaderThread)
		{
			// Set thread abort flag to false and start the thread.
			abortPlaylistDownloader = false;
			playlistDownloaderThread = new std::thread(&MediaTrack::PlaylistDownloader, this);
			playlistDownloaderThreadStarted = true;
			AAMPLOG_INFO("Thread created for PlaylistDownloader [%zx]", GetPrintableThreadID(*playlistDownloaderThread));
		}
		else
		{
			AAMPLOG_ERR("Failed to start thread, already initialized for %s", name);
		}
	}
	else
	{
		AAMPLOG_INFO("Thread already running for %s", name);
	}
}

/**
 * @brief Stop playlist downloader loop
 */
void MediaTrack::StopPlaylistDownloaderThread()
{
	if ((playlistDownloaderThreadStarted) && (playlistDownloaderThread) && (playlistDownloaderThread->joinable()))
	{
		abortPlaylistDownloader = true;
		AbortWaitForPlaylistDownload();
		AbortFragmentDownloaderWait();
		playlistDownloaderThread->join();
		SAFE_DELETE(playlistDownloaderThread);
		playlistDownloaderThreadStarted = false;
		AAMPLOG_WARN("[%s] Aborted", name);
	}
}

/**
 * @brief Get string corresponding to buffer status.
 */
const char* MediaTrack::GetBufferHealthStatusString(BufferHealthStatus status)
{
	const char* ret = NULL;
	switch (status)
	{
		default:
		case BUFFER_STATUS_GREEN:
			ret = "GREEN";
			break;
		case BUFFER_STATUS_YELLOW:
			ret = "YELLOW";
			break;
		case BUFFER_STATUS_RED:
			ret = "RED";
			break;
	}
	return ret;
}

/**
 * @brief Get buffer Status of track
 */
BufferHealthStatus MediaTrack::GetBufferStatus()
{
	BufferHealthStatus bStatus = BUFFER_STATUS_GREEN;
	double bufferedTime = 0.0;
	int CachedFragmentsOrChunks = 0;
	double thresholdBuffer = AAMP_BUFFER_MONITOR_GREEN_THRESHOLD;
	class StreamAbstractionAAMP* pContext = GetContext();
	double injectedDuration = GetTotalInjectedDuration();
	if(aamp->GetLLDashServiceData()->lowLatencyMode && pContext)
	{
		bufferedTime 	    = pContext->GetBufferedDuration(); /** To align with monitorLatency use same API*/
		thresholdBuffer = AAMP_BUFFER_MONITOR_GREEN_THRESHOLD_LLD;
	}
	else if (pContext)
	{
		bufferedTime 	    = injectedDuration - pContext->GetElapsedTime();
		CachedFragmentsOrChunks = numberOfFragmentsCached ;
	}

	if ( CachedFragmentsOrChunks <= 0  && (bufferedTime <= thresholdBuffer) && pContext)
	{
		AAMPLOG_WARN("[%s] bufferedTime %f totalInjectedDuration %f elapsed time %f",
					 name, bufferedTime, injectedDuration, pContext->GetElapsedTime());
		if (bufferedTime <= 0)
		{
			bStatus = BUFFER_STATUS_RED;
		}
		else
		{
			bStatus = BUFFER_STATUS_YELLOW;
		}
	}
	return bStatus;
}


void MediaTrack::MonitorBufferHealth()
{
	UsingPlayerId playerId( aamp->mPlayerId );
	int bufferHealthMonitorDelay = GETCONFIGVALUE(eAAMPConfig_BufferHealthMonitorDelay);
	int bufferHealthMonitorInterval = GETCONFIGVALUE(eAAMPConfig_BufferHealthMonitorInterval);
	int discontinuityTimeoutValue = GETCONFIGVALUE(eAAMPConfig_DiscontinuityTimeout);
	assert(bufferHealthMonitorDelay >= bufferHealthMonitorInterval);
	unsigned int bufferMontiorScheduleTime = bufferHealthMonitorDelay - bufferHealthMonitorInterval;
	bool keepRunning = false;
	AAMPLOG_INFO("[%s] Start MonitorBufferHealth, downloads %d abort %d delay %ds interval %ds discontinuityTimeout %dms",
				 name, aamp->DownloadsAreEnabled(), abort, bufferHealthMonitorDelay, bufferHealthMonitorInterval, discontinuityTimeoutValue);
	if(aamp->DownloadsAreEnabled() && !abort)
	{
		aamp->interruptibleMsSleep(bufferMontiorScheduleTime *1000);
		keepRunning = true;
	}
	int monitorInterval = bufferHealthMonitorInterval  * 1000;
	std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
	while(keepRunning && !abort)
	{
		aamp->interruptibleMsSleep(monitorInterval);
		lock.lock();
		if (aamp->DownloadsAreEnabled() && !abort)
		{
			bufferStatus = GetBufferStatus();
			if (bufferStatus != prevBufferStatus)
			{
				AAMPLOG_WARN("aamp: track[%s] buffering %s->%s", name, GetBufferHealthStatusString(prevBufferStatus),
							 GetBufferHealthStatusString(bufferStatus));
				aamp->profiler.IncrementChangeCount(Count_BufferChange);
				prevBufferStatus = bufferStatus;
			}
			else
			{
				AAMPLOG_DEBUG(" track[%s] No Change [%s]",  name,
							  GetBufferHealthStatusString(bufferStatus));
			}
			lock.unlock();

			// We use another lock inside CheckForMediaTrackInjectionStall for synchronization
			GetContext()?GetContext()->CheckForMediaTrackInjectionStall(type):void();

			lock.lock();
			if((!aamp->pipeline_paused) && aamp->IsDiscontinuityProcessPending() && discontinuityTimeoutValue)
			{
				aamp->CheckForDiscontinuityStall((AampMediaType)type);
			}

			// If underflow occurred and cached fragments are full
			if (aamp->GetBufUnderFlowStatus() && bufferStatus == BUFFER_STATUS_GREEN && type == eTRACK_VIDEO)
			{
				// There is a chance for deadlock here
				// We hit an underflow in a scenario where its not actually an underflow
				// If track injection to GStreamer is stopped because of this special case, we can't come out of
				// buffering even if we have enough data
				if (!aamp->TrackDownloadsAreEnabled(eMEDIATYPE_VIDEO))
				{
					// This is a deadlock, buffering is active and enough-data received from GStreamer
					AAMPLOG_WARN("Possible deadlock with buffering. Enough buffers cached, un-pause pipeline!");
					aamp->StopBuffering(true);
				}
			}
		}
		else
		{
			keepRunning = false;
		}
		lock.unlock();
	}
	AAMPLOG_INFO("[%s] Exit MonitorBufferHealth, downloads %d abort %d",
				 name, aamp->DownloadsAreEnabled(), abort);
}


/**
 * @brief Task to Signal the clock to subtitle module
 */
void MediaTrack::UpdateSubtitleClockTask()
{
	// Update subtitle clock periodically until downloads are stopped or we're told to abort, starting at a faster rate until we get the first successful update so subtitles are not delayed/out of sync
	const int subtitleClockSyncIntervalMs = GETCONFIGVALUE(eAAMPConfig_SubtitleClockSyncInterval)*1000;	// rate in ms once we get first successful sync
	int warningTimeoutMs=SUBTITLE_CLOCK_ASSUMED_PLAYSTATE_TIME_MS;						// warn if synchronisation takes longer than this
	int monitorIntervalMs=0;
	bool keepRunning=(!abort && aamp->DownloadsAreEnabled());
	int timeSinceValidUpdateMs = 0;
	bool playbackStarted = false;
	bool previouslyEnabled = enabled;
#ifdef SUBTEC_VARIABLE_CLOCK_UPDATE_RATE
	int fastMonitorIntervalMs = INITIAL_SUBTITLE_CLOCK_SYNC_INTERVAL_MS;					// rate until we get first successful sync or give up
	if (fastMonitorIntervalMs>subtitleClockSyncIntervalMs)
	{
		fastMonitorIntervalMs=subtitleClockSyncIntervalMs;
	}
	// make faster retries to sync the clock until it has failed for at least this time (e.g. 20s)
	if (warningTimeoutMs<=fastMonitorIntervalMs*5)
	{
		warningTimeoutMs=fastMonitorIntervalMs*5;
		AAMPLOG_WARN("Adjusting initial startup timeout from %d to %d", SUBTITLE_CLOCK_ASSUMED_PLAYSTATE_TIME_MS, warningTimeoutMs);
	}
	monitorIntervalMs = fastMonitorIntervalMs;
	AAMPLOG_WARN("Starting UpdateSubtitleClockTask using dynamic refresh rate. DownloadsAreEnabled=%d, abort=%d, subtitleClockSyncIntervalMs=%d, fastMonitorIntervalMs=%d, warningTimeoutMs=%d",
		aamp->DownloadsAreEnabled(), abort, subtitleClockSyncIntervalMs, fastMonitorIntervalMs, warningTimeoutMs);
#else
	monitorIntervalMs=subtitleClockSyncIntervalMs;
	AAMPLOG_WARN("Starting UpdateSubtitleClockTask with fixed refresh rate. DownloadsAreEnabled=%d, abort=%d, subtitleClockSyncIntervalMs=%d, warningTimeoutMs=%d",
		aamp->DownloadsAreEnabled(), abort, subtitleClockSyncIntervalMs, warningTimeoutMs);
#endif /*SUBTEC_VARIABLE_CLOCK_UPDATE_RATE*/
	std::unique_lock<std::mutex> lock(mutex, std::defer_lock);
	while(keepRunning)
	{
		lock.lock();
		if (aamp->DownloadsAreEnabled() && !abort)
		{
			// Fetch PTS, send to Subtec
			// Enable clock sync when mediaprocessor is enabled. For QTDEMUX_OVERRIDE_ENABLED platforms, video pts received from GST is relative from playback start,
			// so we need mediaprocessor to set the base video pts so that correct pts can be signalled to subtec
			if (enabled && aamp->IsGstreamerSubsEnabled() && ISCONFIGSET(eAAMPConfig_EnableMediaProcessor))
			{
				// Note: This will fail if pipeline is not in play state, we have underflow, or if video pts is still returning 0 just after we entered play state
				if (aamp->SignalSubtitleClock())
				{
					if (!playbackStarted)
					{
						// Slow down the update rate now it's first sync'd after we enter play state
						AAMPLOG_WARN("First subtitle clock update successful. Switching to slow update rate (%d) after %d ms (if enabled)", subtitleClockSyncIntervalMs, timeSinceValidUpdateMs);
					}
					monitorIntervalMs=subtitleClockSyncIntervalMs;
					playbackStarted=true;
					timeSinceValidUpdateMs=0;
				}
				else
				{
					if( (!playbackStarted) && (timeSinceValidUpdateMs<warningTimeoutMs) )
					{
						// Underflow/paused/pts not ready/injection blocked?
						if (!aamp->pipeline_paused)
						{
							AAMPLOG_DEBUG("Subtitle clock update failed during startup; paused=%d, timetimeSinceValidUpdateMs=%d ms",
							aamp->pipeline_paused, timeSinceValidUpdateMs);
						}
					}
					else
					{
						if (!aamp->pipeline_paused)
						{
							AAMPLOG_WARN("Subtitle clock failed unexpectedly; playbackStarted=%d, timeSinceValidUpdateMs=%d ms, paused=%d, mTrackInjectionBlocked. Underflow/paused/injection blocked?",
								playbackStarted, timeSinceValidUpdateMs, aamp->pipeline_paused);
#ifdef SUBTEC_VARIABLE_CLOCK_UPDATE_RATE
							if ((timeSinceValidUpdateMs<warningTimeoutMs) && (monitorIntervalMs!=fastMonitorIntervalMs) )
							{
								AAMPLOG_WARN("Something has gone wrong after playback started. Switching to faster refresh rate (%d) after %d ms", subtitleClockSyncIntervalMs, timeSinceValidUpdateMs);
								monitorIntervalMs = fastMonitorIntervalMs;
							}
							if ((timeSinceValidUpdateMs>=warningTimeoutMs) && (monitorIntervalMs!=subtitleClockSyncIntervalMs))
							{
								AAMPLOG_WARN("Clock sync taking too long. Switching to low refresh rate (%d) after %d ms. Subtitles sync may be bad until next refresh interval.",
											 subtitleClockSyncIntervalMs, timeSinceValidUpdateMs);
								monitorIntervalMs = subtitleClockSyncIntervalMs;
							}
#endif
						}
						else
						{
							AAMPLOG_TRACE("Subtitle clock not updated in pause state. No action taken; playbackStarted=%d, timeSinceValidUpdateMs=%d ms, mTrackInjectionBlocked.",
								playbackStarted, timeSinceValidUpdateMs );
						}
					}
				}
			}
			else
			{
				if (previouslyEnabled && !enabled )
				{
					AAMPLOG_WARN("Subtitles are not active. No clock updates. Switching to low refresh rate (if enabled). enabled=%d, aamp->IsGstreamerSubsEnabled()=%d, eAAMPConfig_EnableMediaProcessor=%d",
						enabled, aamp->IsGstreamerSubsEnabled(), ISCONFIGSET(eAAMPConfig_EnableMediaProcessor));
				}
				monitorIntervalMs = subtitleClockSyncIntervalMs;
			}
		}
		else
		{
			keepRunning = false;
		}
		previouslyEnabled=enabled;
		lock.unlock();
		aamp->interruptibleMsSleep(monitorIntervalMs);
		timeSinceValidUpdateMs+=monitorIntervalMs;
	}
	AAMPLOG_WARN("Exiting UpdateSubtitleClockTask. DownloadsAreEnabled=%d, abort=%d, keepRunning=%d", aamp->DownloadsAreEnabled(), abort, keepRunning);
}

/**
 * @brief Update segment cache and inject buffer to gstreamer
 */
void MediaTrack::UpdateTSAfterInject()
{
	std::lock_guard<std::mutex> guard(mutex);
	AAMPLOG_DEBUG("[%s] Free cachedFragment[%d] numberOfFragmentsCached %d",
				  name, fragmentIdxToInject, numberOfFragmentsCached);
	mCachedFragment[fragmentIdxToInject].fragment.Free();
	fragmentIdxToInject++;
	if (fragmentIdxToInject == maxCachedFragmentsPerTrack)
	{
		fragmentIdxToInject = 0;
	}
	numberOfFragmentsCached--;
	fragmentInjected.notify_one();
}

/**
 * @brief Update segment cache and inject buffer to gstreamer
 */
void MediaTrack::UpdateTSAfterChunkInject()
{
	std::lock_guard<std::mutex> guard(mutex);
	//Free Chunk Cache Buffer
	prevDownloadStartTime = mCachedFragmentChunks[fragmentChunkIdxToInject].downloadStartTime;
	mCachedFragmentChunks[fragmentChunkIdxToInject].fragment.Free();

	parsedBufferChunk.Free();
	//memset(&parsedBufferChunk, 0x00, sizeof(AampGrowableBuffer));

	//increment Inject Index
	++fragmentChunkIdxToInject;
	fragmentChunkIdxToInject = (fragmentChunkIdxToInject) % mCachedFragmentChunksSize;
	if(numberOfFragmentChunksCached > 0) numberOfFragmentChunksCached--;

	AAMPLOG_DEBUG("[%s] updated fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d",
				  name, fragmentChunkIdxToInject, numberOfFragmentChunksCached);

	fragmentChunkInjected.notify_one();
}

/**
 * @fn InjectFragmentChunkInternal
 *
 * @param[in] mediaType - Media type of the fragment
 * @param[in] buffer - contains fragment to be processed and injected
 * @param[in] fpts - fragment PTS
 * @param[in] fdts - fragment DTS
 * @param[in] fDuration - fragment duration
 * @param[in] fragmentPTSOffset - PTS offset to be applied
 * @param[in] init - true if fragment is init fragment
 * @param[in] discontinuity - true if there is a discontinuity, false otherwise
 * @return void
 */
void MediaTrack::InjectFragmentChunkInternal(AampMediaType mediaType, AampGrowableBuffer* buffer, double fpts, double fdts, double fDuration, double fragmentPTSOffset, bool init, bool discontinuity)
{
	if (playContext)
	{
		MediaProcessor::process_fcn_t processor = [this](AampMediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
		{
			// No-op processor for chunk injection
		};
		AAMPLOG_INFO("Type[%d] position: %f duration: %f PTSOffsetSec: %f initFragment: %d size: %zu",
			type, fpts, fDuration, fragmentPTSOffset, init, buffer->GetLen());
		bool ptsError = false;
		if (!playContext->sendSegment(buffer, fpts, fDuration, fragmentPTSOffset, discontinuity, init, std::move(processor), ptsError))
		{
			AAMPLOG_INFO("Type[%d] Fragment discarded", mediaType);
		}
	}
	else
	{
		aamp->ProcessID3Metadata(buffer->GetPtr(), buffer->GetLen(), mediaType);
		AAMPLOG_DEBUG("Type[%d] fpts: %f fDuration: %f init: %d", type, fpts, fDuration, init);
		aamp->SendStreamTransfer(mediaType, buffer, fpts, fdts, fDuration, fragmentPTSOffset, init, discontinuity);
	}
}

/**
 * @brief To flush the subtitle position even if the MediaProcessor is not not enabled.
 */
void MediaTrack::FlushSubtitlePositionDuringTrackSwitch(  CachedFragment* cachedFragment )
{
	IsoBmffBuffer buffer;
	buffer.setBuffer((uint8_t *)cachedFragment->fragment.GetPtr(), cachedFragment->fragment.GetLen());
	buffer.parseBuffer();
	uint64_t currentPTS = 0;
	if(buffer.getFirstPTS(currentPTS))
	{
		double pos = (double)currentPTS / (double)aamp->GetSubTimeScale();
		aamp->FlushTrack(eMEDIATYPE_SUBTITLE,pos);
		AAMPLOG_MIL("Curr PTS %" PRIu64 " TS: %u",currentPTS,aamp->GetSubTimeScale());
	}
}

/**
 * @brief  To flush the Audio position even if the MediaProcessor is not not enabled.
 */
void  MediaTrack::FlushAudioPositionDuringTrackSwitch(  CachedFragment* cachedFragment )
{
	IsoBmffBuffer buffer;
	buffer.setBuffer((uint8_t *)cachedFragment->fragment.GetPtr(), cachedFragment->fragment.GetLen());
	buffer.parseBuffer();
	uint64_t currentPTS = 0;
	if(buffer.getFirstPTS(currentPTS))
	{
		double pos = (double)currentPTS / (double)aamp->GetAudTimeScale();
		aamp->FlushTrack(eMEDIATYPE_AUDIO,pos);
		AAMPLOG_MIL("Curr PTS %" PRIu64 " TS: %u",currentPTS,aamp->GetAudTimeScale());
	}
}
/**
 *  @brief Updates internal state after a fragment fetch
 */
void MediaTrack::UpdateTSAfterFetch(bool IsInitSegment)
{
	bool notifyCacheCompleted = false;
	class StreamAbstractionAAMP* pContext = GetContext();
	std::unique_lock<std::mutex> lock(mutex);

	CachedFragment* cachedFragment = &this->mCachedFragment[fragmentIdxToFetch];

	if (pContext)
	{
		cachedFragment->profileIndex = pContext->profileIdxForBandwidthNotification;
		pContext->UpdateStreamInfoBitrateData(cachedFragment->profileIndex, cachedFragment->cacheFragStreamInfo);
	}
	totalFetchedDuration += cachedFragment->duration;
	numberOfFragmentsCached++;
	assert(numberOfFragmentsCached <= maxCachedFragmentsPerTrack);
	currentInitialCacheDurationSeconds += cachedFragment->duration;

	if( (eTRACK_VIDEO == type)
	   && aamp->IsFragmentCachingRequired()
	   && !cachingCompleted)
	{
		const int minInitialCacheSeconds = aamp->GetInitialBufferDuration();
		if(currentInitialCacheDurationSeconds >= minInitialCacheSeconds)
		{
			AAMPLOG_WARN("##[%s] Caching Complete cacheDuration %d minInitialCacheSeconds %d##",
						 name, currentInitialCacheDurationSeconds, minInitialCacheSeconds);
			notifyCacheCompleted = true;
			cachingCompleted = true;
		}
		else if (sinkBufferIsFull && numberOfFragmentsCached == maxCachedFragmentsPerTrack)
		{
			AAMPLOG_WARN("## [%s] Cache is Full cacheDuration %d minInitialCacheSeconds %d, aborting caching!##",
						 name, currentInitialCacheDurationSeconds, minInitialCacheSeconds);
			notifyCacheCompleted = true;
			cachingCompleted = true;
		}
		else
		{
			AAMPLOG_INFO("## [%s] Caching Ongoing cacheDuration %d minInitialCacheSeconds %d##",
						 name, currentInitialCacheDurationSeconds, minInitialCacheSeconds);
		}
	}
	if(loadNewAudio && (eTRACK_AUDIO == type) && !IsInitSegment)
	{
		if(playContext)
		{
			playContext->resetPTSOnAudioSwitch(&cachedFragment->fragment, cachedFragment->position);
		}
		else
		{
			//Enters this case, when the Mediaprocessor is not enabled
			FlushAudioPositionDuringTrackSwitch( cachedFragment );
		}
		aamp->ResumeTrackInjection((AampMediaType)eMEDIATYPE_AUDIO);
		NotifyCachedAudioFragmentAvailable();
		loadNewAudio = false;
		aamp->mDisableRateCorrection = false;
	}
	if(loadNewSubtitle && (eTRACK_SUBTITLE == type) && !IsInitSegment)
	{
		if(playContext)
		{
			playContext->resetPTSOnSubtitleSwitch(&cachedFragment->fragment, cachedFragment->position);
		}
		else
		{
			//Enters this case, when the Mediaprocessor is not enabled
			FlushSubtitlePositionDuringTrackSwitch( cachedFragment );
		}
		aamp->ResumeTrackInjection((AampMediaType)eMEDIATYPE_SUBTITLE);
		NotifyCachedSubtitleFragmentAvailable();
		loadNewSubtitle = false;
		aamp->mDisableRateCorrection = false;
	}
	fragmentIdxToFetch++;
	if (fragmentIdxToFetch == maxCachedFragmentsPerTrack)
	{
		fragmentIdxToFetch = 0;
	}
	if(!IsInitSegment)
	{
		totalFragmentsDownloaded++;
	}

	fragmentFetched.notify_one();
	lock.unlock();
	if(notifyCacheCompleted)
	{
		aamp->NotifyFragmentCachingComplete();
	}
}

/**
 * @brief Process New Audio On Lang Switch
 */
void MediaTrack::LoadNewAudio(bool val)
{
	loadNewAudio = val;
}

void MediaTrack::LoadNewSubtitle(bool val)
{
	loadNewSubtitle = val;
}

/**
 *  @brief Updates internal state after a fragment fetch
 */
void MediaTrack::UpdateTSAfterChunkFetch()
{
	std::lock_guard<std::mutex> guard(mutex);

	numberOfFragmentChunksCached++;
	AAMPLOG_DEBUG("[%s] numberOfFragmentChunksCached++ [%d]", name,numberOfFragmentChunksCached);

	//this should never HIT
	assert(numberOfFragmentChunksCached <= mCachedFragmentChunksSize);

	fragmentChunkIdxToFetch = (fragmentChunkIdxToFetch+1) % mCachedFragmentChunksSize;

	AAMPLOG_DEBUG("[%s] updated fragmentChunkIdxToFetch [%d] numberOfFragmentChunksCached [%d]",
				  name, fragmentChunkIdxToFetch, numberOfFragmentChunksCached);

	totalFragmentChunksDownloaded++;
	fragmentChunkFetched.notify_one();
}

/**
 * @brief Wait until a free fragment is available.
 * @note To be called before fragment fetch by subclasses
 */
bool MediaTrack::WaitForFreeFragmentAvailable( int timeoutMs)
{
	bool ret = true;
	AAMPPlayerState state;
	int preplaybuffercount = GETCONFIGVALUE(eAAMPConfig_PrePlayBufferCount);

	if(abort)
	{
		ret = false;
	}
	else
	{
		// Still in preparation mode , not to inject any more fragments beyond capacity
		// Wait for 100ms
		std::unique_lock<std::mutex> lock(aamp->mMutexPlaystart);
		state = aamp->GetState();
		if(state == eSTATE_PREPARED && totalFragmentsDownloaded > preplaybuffercount && !aamp->IsFragmentCachingRequired())
		{
			timeoutMs = 500;
			if (std::cv_status::timeout == aamp->waitforplaystart.wait_for(lock,std::chrono::milliseconds(timeoutMs)))
			{
				AAMPLOG_TRACE("Timed out waiting for waitforplaystart");
				ret = false;
			}
		}
	}

	if (ret)
	{
		if (IsInjectionFromCachedFragmentChunks())
		{
			ret = WaitForCachedFragmentChunkInjected(timeoutMs);
		}
		else
		{
			std::unique_lock<std::mutex> lock(mutex);
			if ((maxCachedFragmentsPerTrack) && (numberOfFragmentsCached == maxCachedFragmentsPerTrack))
			{
				if (timeoutMs >= 0)
				{
					if (std::cv_status::timeout == fragmentInjected.wait_for(lock, std::chrono::milliseconds(timeoutMs)))
					{
						AAMPLOG_TRACE("Timed out waiting for fragmentInjected");
						ret = false;
					}
				}
				else
				{
					fragmentInjected.wait(lock);
				}
				if (abort)
				{
					ret = false;
				}
			}
		}
	}
	return ret;
}

/**
 *  @brief Wait till cached fragment available
 */
bool MediaTrack::WaitForCachedFragmentAvailable()
{
	std::unique_lock<std::mutex> lock(mutex);
	if ((numberOfFragmentsCached == 0) && !(abort || abortInject))
	{
		if (!eosReached)
		{
			fragmentFetched.wait(lock);
		}
	}
	bool ret = !(abort || abortInject || (numberOfFragmentsCached == 0));
	return ret;
}

/**
 *  @brief Wait until a cached fragment chunk is Injected.
 */
bool MediaTrack::WaitForCachedFragmentChunkInjected(int timeoutMs)
{
	bool ret = true;
	if(abort)
	{
		ret = false;
	}
	std::unique_lock<std::mutex> lock(mutex);
	if (ret && (numberOfFragmentChunksCached == mCachedFragmentChunksSize))
	{
		if (timeoutMs >= 0)
		{
			if (std::cv_status::timeout == fragmentChunkInjected.wait_for(lock,std::chrono::milliseconds(timeoutMs)))
			{
				AAMPLOG_DEBUG("[%s] pthread_cond_timedwait timed out", name);
				ret = false;
			}
		}
		else
		{
			AAMPLOG_DEBUG("[%s] waiting for fragmentChunkInjected condition", name);
			fragmentChunkInjected.wait(lock);
			AAMPLOG_DEBUG("[%s] wait complete for fragmentChunkInjected", name);
		}
		if (abort)
		{
			AAMPLOG_DEBUG("[%s] abort set, returning false", name);
			ret = false;
		}
	}

	AAMPLOG_DEBUG("[%s] fragmentChunkIdxToFetch = %d numberOfFragmentChunksCached %d mCachedFragmentChunksSize %zu",
				  name, fragmentChunkIdxToFetch, numberOfFragmentChunksCached, mCachedFragmentChunksSize);
	return ret;
}

/**
 *  @brief Wait till cached fragment chunk available
 */
bool MediaTrack::WaitForCachedFragmentChunkAvailable()
{
	bool ret = true;
	AAMPLOG_TRACE("DEBUG Enter");
	std::unique_lock<std::mutex> lock(mutex);

	AAMPLOG_DEBUG("[%s] Acquired MUTEX ==> fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d ret = %d abort = %d abortInject = %d ", name, fragmentChunkIdxToInject, numberOfFragmentChunksCached, ret, abort, abortInject);

	if ((numberOfFragmentChunksCached == 0) && !(abort || abortInject))
	{
		AAMPLOG_DEBUG("## [%s] Waiting for CachedFragment to be available, eosReached=%d ##", name, eosReached);

		if (!eosReached)
		{
			fragmentChunkFetched.wait(lock);
			AAMPLOG_DEBUG("[%s] wait complete for fragmentChunkFetched", name);
		}
	}

	ret = !(abort || abortInject || numberOfFragmentChunksCached == 0);
	AAMPLOG_DEBUG("[%s] fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d ret = %d abort = %d abortInject = %d",
				  name, fragmentChunkIdxToInject, numberOfFragmentChunksCached, ret, abort, abortInject);
	return ret;
}

/**
 *  @brief Abort the waiting for cached fragments and free fragment slot
 */
void MediaTrack::AbortWaitForCachedAndFreeFragment(bool immediate)
{
	std::unique_lock<std::mutex> lock(mutex);
	if (immediate)
	{
		abort = true;
		fragmentInjected.notify_one();
		AAMPLOG_DEBUG("[%s] signal fragmentChunkInjected condition", name);
		// For TSB playback, WaitForCachedFragmentChunkInject is invoked from TSBReader and CacheFragmentChunk threads
		fragmentChunkInjected.notify_all();
	}
	AAMPLOG_DEBUG("[%s] signal fragmentChunkFetched condition", name);
	fragmentChunkFetched.notify_one();
	aamp->waitforplaystart.notify_one();
	fragmentFetched.notify_one();
	lock.unlock();

	GetContext()?GetContext()->AbortWaitForDiscontinuity(): void();
}

/**
 * @brief Abort the waiting for cached fragments immediately
 */
void MediaTrack::AbortWaitForCachedFragment()
{
	std::unique_lock<std::mutex> lock(mutex);
	AAMPLOG_DEBUG("[%s] signal fragmentChunkFetched condition", name);
	fragmentChunkFetched.notify_one();

	abortInject = true;
	fragmentFetched.notify_one();
	lock.unlock();

	GetContext()?GetContext()->AbortWaitForDiscontinuity():void();
}

/**
 * @brief Abort the waiting for injected fragment chunks immediately
 */
void MediaTrack::AbortWaitForCachedFragmentChunk()
{
	std::lock_guard<std::mutex> guard(mutex);
	AAMPLOG_TRACE("[%s] signal fragmentChunkInjected condition", name);
	fragmentChunkInjected.notify_all();
}

/**
 *  @brief Process next cached fragment
 */
bool MediaTrack::CheckForDiscontinuity(CachedFragment* cachedFragment, bool& fragmentDiscarded, bool& isDiscontinuity, bool &ret)
{
	//Get Cache buffer
	bool stopInjection = false;
	StreamAbstractionAAMP* context = GetContext();
	double injectedDuration = GetTotalInjectedDuration();

	if(cachedFragment->fragment.GetPtr())
	{
		if ((cachedFragment->discontinuity || ptsError) && (AAMP_NORMAL_PLAY_RATE == aamp->rate))
		{
			bool isDiscoIgnoredForOtherTrack = aamp->IsDiscontinuityIgnoredForOtherTrack((AampMediaType)!type);
			AAMPLOG_TRACE("track %s - encountered aamp discontinuity @position - %f, isDiscoIgnoredForOtherTrack - %d ptsError %d", name, cachedFragment->position, isDiscoIgnoredForOtherTrack,ptsError );
			if (eTRACK_SUBTITLE != type)
			{
				cachedFragment->discontinuity = false;
			}
			ptsError = false;

			/* GetESChangeStatus() check is specifically added to fix an audio loss issue due to no reconfigure pipeline when there was an audio codec change for a very short period with no fragments.
			 * The totalInjectedDuration will be 0 for the very short duration periods if the single fragment is not injected or failed (due to fragment download failures).
			 * In that case, if there is an audio codec change is detected for this period, it could cause audio loss since ignoring the discontinuity to be processed since totalInjectedDuration is 0.
			 */
			/* PipelineValid is used here to avoid skipping the discontinuity if the pipeline has not been configured for the media type.
			 * This was seen with subtitles where switching to a period with subtitles enabled from one without could result in fragments being pushed
			 * to an appsrc that wasn't configured (very timing dependent). In this case we want to process the discontinuity and configure the pipeline.
			 */
			if (injectedDuration == 0 && !aamp->mpStreamAbstractionAAMP->GetESChangeStatus()&& aamp->PipelineValid((AampMediaType)type))
			{
				stopInjection = false;

				if (!isDiscoIgnoredForOtherTrack)
				{
					// Subtitles never have any discontinuity pairing logic. Ignore for it now
					if (type != eTRACK_SUBTITLE)
					{
						// set discontinuity ignored flag to check and avoid paired discontinuity processing of other track.
						aamp->SetTrackDiscontinuityIgnoredStatus((AampMediaType)type);
					}
				}
				else
				{
					AAMPLOG_WARN("discontinuity ignored for other AV track, no need to process %s track", name);
					// reset the flag when both the paired discontinuities ignored; since no buffer pushed before.
					aamp->ResetTrackDiscontinuityIgnoredStatus();
					aamp->UnblockWaitForDiscontinuityProcessToComplete();
				}
				AAMPLOG_WARN("ignoring %s discontinuity since no buffer pushed before!", name);
			}
			else if (isDiscoIgnoredForOtherTrack && !aamp->mpStreamAbstractionAAMP->GetESChangeStatus() && aamp->PipelineValid((AampMediaType)type))
			{
				AAMPLOG_WARN("discontinuity ignored for other AV track , no need to process %s track", name);
				stopInjection = false;

				// reset the flag when both the paired discontinuities ignored.
				aamp->ResetTrackDiscontinuityIgnoredStatus();
				aamp->UnblockWaitForDiscontinuityProcessToComplete();
				MediaTrack* subtitle = GetContext()?GetContext()->GetMediaTrack(eTRACK_SUBTITLE):nullptr;
				if (subtitle && subtitle->enabled)
				{
					if(subtitle->playContext)
					{
						subtitle->playContext->abortWaitForVideoPTS();
					}
				}
			}
			else
			{
				if (!aamp->PipelineValid((AampMediaType)type))
				{
					AAMPLOG_WARN("Pipeline not yet configured for %s! Process discontinuity...", name);
				}

				if(ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && (aamp->mVideoFormat == FORMAT_ISO_BMFF ))
				{
					if (context->GetESChangeStatus())
					{
						stopInjection = context->ProcessDiscontinuity(type);
					}
					else
					{
						context->ProcessDiscontinuity(type);
					}
					bool isDiscontinuityIgnoredForCurrentTrack = aamp->IsDiscontinuityIgnoredForCurrentTrack((AampMediaType)type);
					if( true != isDiscontinuityIgnoredForCurrentTrack )
					{
						isDiscontinuity = true;
						AAMPLOG_WARN("track %s discontinuity not ignored = %d - discontinuity @position - %f", name, isDiscontinuityIgnoredForCurrentTrack, cachedFragment->position);
					}
					else
					{
						isDiscontinuity = false;
						AAMPLOG_WARN("track %s - discontinuity ignored = %d continue without discontinuity @position - %f", name, isDiscontinuityIgnoredForCurrentTrack, cachedFragment->position);
					}

					if(type != eTRACK_SUBTITLE)
					{
						// Reset the discontinuity flags if we are not stopping injection
						if (!stopInjection)
						{
							context->resetDiscontinuityTrackState();
							aamp->ResetDiscontinuityInTracks();
						}
					}
				}
				else
				{
					stopInjection = context->ProcessDiscontinuity(type);
				}
			}

			if (stopInjection)
			{
				ret = false;
				discontinuityProcessed = true;
				AAMPLOG_WARN("track %s - stopping injection @position - %f", name, cachedFragment->position);
			}
			else
			{
				AAMPLOG_WARN("track %s - continuing injection", name);
			}
		}
		else if (cachedFragment->discontinuity && !ISCONFIGSET(eAAMPConfig_EnablePTSReStamp))
		{
			//Only needed when we are using the qtdemux
			SignalTrickModeDiscontinuity();
		}
	}
	return (stopInjection);
}

/**
 *  @brief Process next cached fragment chunk
 */
bool MediaTrack::ProcessFragmentChunk()
{
	class StreamAbstractionAAMP* pContext = GetContext();
	//Get Cache buffer
	CachedFragment* cachedFragment = &this->mCachedFragmentChunks[fragmentChunkIdxToInject];
	if(cachedFragment != NULL && NULL == cachedFragment->fragment.GetPtr())
	{
		if(!SignalIfEOSReached())
		{
			AAMPLOG_TRACE("[%s] Ignore NULL Chunk - cachedFragment->fragment.len %zu", name, cachedFragment->fragment.GetLen());
		}
		return false;
	}
	if(cachedFragment->initFragment)
	{
		if ((pContext) && ISCONFIGSET(eAAMPConfig_EnablePTSReStamp))
		{
			if (pContext->trickplayMode)
			{
				// If in trick mode, do trick mode PTS restamp
				TrickModePtsRestamp(cachedFragment);
			}
			else
			{
				ClearMediaHeaderDuration(cachedFragment);
			}
		}
		else if (pContext && ISCONFIGSET(eAAMPConfig_OverrideMediaHeaderDuration))
		{
			if (!pContext->trickplayMode)
			{
				ClearMediaHeaderDuration(cachedFragment);
			}
		}
		if (mSubtitleParser && type == eTRACK_SUBTITLE)
		{
			mSubtitleParser->processData(cachedFragment->fragment.GetPtr(), cachedFragment->fragment.GetLen(), cachedFragment->position, cachedFragment->duration);
		}
		if (type != eTRACK_SUBTITLE || (aamp->IsGstreamerSubsEnabled()))
		{
			AAMPLOG_INFO("Injecting init chunk for %s",name);
			InjectFragmentChunkInternal((AampMediaType)type, &cachedFragment->fragment, cachedFragment->position, cachedFragment->position, cachedFragment->duration, cachedFragment->PTSOffsetSec, cachedFragment->initFragment, cachedFragment->discontinuity);
			if (eTRACK_VIDEO == type && pContext && pContext->GetProfileCount())
			{
				pContext->NotifyBitRateUpdate(cachedFragment->profileIndex, cachedFragment->cacheFragStreamInfo, cachedFragment->position);
			}
		}
		cachedFragment->initFragment = false;
		return true;
	}
	if((cachedFragment->downloadStartTime != prevDownloadStartTime) && (unparsedBufferChunk.GetPtr() != NULL))
	{
		AAMPLOG_WARN("[%s] clean up curl chunk buffer, since  prevDownloadStartTime[%lld] != currentdownloadtime[%lld]", name,prevDownloadStartTime,cachedFragment->downloadStartTime);
		unparsedBufferChunk.Free();
	}
	size_t requiredLength = cachedFragment->fragment.GetLen() + unparsedBufferChunk.GetLen();
	AAMPLOG_DEBUG("[%s] cachedFragment->fragment.len [%zu] to unparsedBufferChunk.len [%zu] Required Len [%zu]", name, cachedFragment->fragment.GetLen(), unparsedBufferChunk.GetLen(), requiredLength);

	//Append Cache buffer to unparsed buffer for processing
	unparsedBufferChunk.AppendBytes( cachedFragment->fragment.GetPtr(), cachedFragment->fragment.GetLen() );

	//Parse Chunk Data
	IsoBmffBuffer isobuf;                   /**< Fragment Chunk buffer box parser*/
	char *unParsedBuffer = NULL;
	size_t parsedBufferSize = 0, unParsedBufferSize = 0;
	unParsedBuffer = unparsedBufferChunk.GetPtr();
	unParsedBufferSize = parsedBufferSize = unparsedBufferChunk.GetLen();
	isobuf.setBuffer(reinterpret_cast<uint8_t *>(unparsedBufferChunk.GetPtr()), unparsedBufferChunk.GetLen() );
	AAMPLOG_TRACE("[%s] Unparsed Buffer Size: %zu", name,unparsedBufferChunk.GetLen() );

	bool bParse = false;
	try
	{
		bParse = isobuf.parseBuffer();
	}
	catch( std::bad_alloc& ba)
	{
		AAMPLOG_ERR("Bad allocation: %s", ba.what() );
	}
	catch( std::exception &e)
	{
		AAMPLOG_ERR("Unhandled exception: %s", e.what() );
	}
	catch( ... )
	{
		AAMPLOG_ERR("Unknown exception");
	}
	if(!bParse)
	{
		AAMPLOG_INFO("[%s] No Box available in cache chunk: fragmentChunkIdxToInject %d", name, fragmentChunkIdxToInject);
		return true;
	}
	//Print box details
	//isobuf.printBoxes();
	uint32_t timeScale = 0;
	if(type == eTRACK_VIDEO)
	{
		timeScale = aamp->GetVidTimeScale();
	}
	else if(type == eTRACK_AUDIO)
	{
		timeScale = aamp->GetAudTimeScale();
	}
	else if (type == eTRACK_SUBTITLE)
	{
		timeScale = aamp->GetSubTimeScale();
	}
	if(!timeScale)
	{
		//FIX-ME-Read from MPD INSTEAD
		if(pContext)
		{
			timeScale = pContext->GetCurrPeriodTimeScale();
			if(!timeScale)
			{
				timeScale = 10000000.0;
				AAMPLOG_WARN("[%s] Empty timeScale!!! Using default timeScale=%d", name, timeScale);
			}
		}
		else
		{
			timeScale = 1000.0;
			AAMPLOG_WARN("[%s] Invalid play context maybe test setup, timeScale=%d", name, timeScale);
		}
	}
	double fpts = 0.0, fduration = 0.0;
	bool ret = isobuf.ParseChunkData(name, unParsedBuffer, timeScale, parsedBufferSize, unParsedBufferSize, fpts, fduration);
	if(!ret) /**  Nothing to parse */
	{
		if( noMDATCount > MAX_MDAT_NOT_FOUND_COUNT )
		{
			AAMPLOG_INFO("[%s] noMDATCount=%d ChunkIndex=%d totchunklen=%zu", name,noMDATCount, fragmentChunkIdxToInject,unParsedBufferSize);
			noMDATCount=0;
		}
		noMDATCount++;
		return true;
	}
	noMDATCount = 0;
	if(parsedBufferSize)
	{
		//Prepare parsed buffer
		parsedBufferChunk.AppendBytes( unparsedBufferChunk.GetPtr(), parsedBufferSize);
		if (ISCONFIGSET(eAAMPConfig_EnablePTSReStamp))
		{
			if (pContext && pContext->trickplayMode)
			{
				AAMPLOG_INFO("%s LLD chunk fpts = %f, absPosition = %f", name, fpts, cachedFragment->absPosition);
				fpts = cachedFragment->absPosition;
				TrickModePtsRestamp(parsedBufferChunk,fpts,fduration,cachedFragment->initFragment,cachedFragment->discontinuity);
			}
			else
			{
				int64_t ptsOffset = cachedFragment->PTSOffsetSec * cachedFragment->timeScale;
				(void)mIsoBmffHelper->RestampPts(parsedBufferChunk, ptsOffset, cachedFragment->uri,
												 name, cachedFragment->timeScale);
				fpts += cachedFragment->PTSOffsetSec;
			}
		}

		if (mSubtitleParser && type == eTRACK_SUBTITLE)
		{
			mSubtitleParser->processData(parsedBufferChunk.GetPtr(), parsedBufferChunk.GetLen(), fpts, fduration);
		}
		if (type != eTRACK_SUBTITLE || (aamp->IsGstreamerSubsEnabled()))
		{
			if( ISCONFIGSET(eAAMPConfig_CurlThroughput) )
			{
				AAMPLOG_MIL( "curl-inject type=%d", type );
			}
			AAMPLOG_INFO("Injecting chunk for %s br=%d,chunksize=%zu fpts=%f fduration=%f",name,bandwidthBitsPerSecond,parsedBufferChunk.GetLen(),fpts,fduration);
			InjectFragmentChunkInternal((AampMediaType)type,&parsedBufferChunk , fpts, fpts, fduration, cachedFragment->PTSOffsetSec);
			totalInjectedChunksDuration += fduration;
		}
	}
	// Move unparsed data sections to beginning
	//Available size remains same
	//This buffer should be released on Track cleanup
	if(unParsedBufferSize)
	{
		AAMPLOG_TRACE("[%s] unparsed[%p] unparsed_size[%zu]", name,unParsedBuffer,unParsedBufferSize);
		AampGrowableBuffer tempBuffer("tempBuffer");
		tempBuffer.AppendBytes(unParsedBuffer,unParsedBufferSize);
		unparsedBufferChunk.Free();
		unparsedBufferChunk.AppendBytes(tempBuffer.GetPtr(),tempBuffer.GetLen());
		tempBuffer.Free();
	}
	else
	{
		AAMPLOG_TRACE("[%s] Set Unparsed Buffer chunk Empty...", name);
		unparsedBufferChunk.Free();
		//memset(&unparsedBufferChunk, 0x00, sizeof(AampGrowableBuffer));
	}
	parsedBufferChunk.Free();
	return true;
}

void StreamAbstractionAAMP::ResetTrickModePtsRestamping(void)
{
	for (int i = 0; i < AAMP_TRACK_COUNT; i++)
	{
		auto track = GetMediaTrack(static_cast<TrackType>(i));
		if(nullptr != track)
		{
			track->ResetTrickModePtsRestamping();
		}
	}
}

void MediaTrack::ResetTrickModePtsRestamping(void)
{
	mTrickmodeState = TrickmodeState::UNDEF;
	mRestampedPts = 0.0;
}

void MediaTrack::TrickModePtsRestamp(AampGrowableBuffer &fragment, double &position, double &duration,
									 bool initFragment, bool  discontinuity)
{
	// Trick mode PTS restamping is supported for fast-forward and rewind
	// (not pause or slow motion)
	if (!((aamp->rate > AAMP_NORMAL_PLAY_RATE) || (aamp->rate < 0)))
	{
		AAMPLOG_WARN("Unsupported trickplay rate %f - cannot restamp", aamp->rate);
		return;
	}

	int trickPlayFPS = GETCONFIGVALUE(eAAMPConfig_VODTrickPlayFPS);
	AampTime fragmentPtsDelta = 0.0;
	AampTime inFragmentPosition = position;
	AampTime inFragmentDuration = duration;

	if (initFragment)
	{
		// Init fragment is injected after any rate change or discontinuity
		// The timescale in the ISOBMFF init segment is restamped to a value TRICKMODE_TIMESCALE to
		// enable restamping the media segment PTS and duration with adequate precision, e.g.
		// 100,000
		(void)mIsoBmffHelper->SetTimescale(fragment, TRICKMODE_TIMESCALE);
		(void)mIsoBmffHelper->ClearMediaHeaderDuration(fragment);

		if (discontinuity)
		{
			// Remember the discontinuity so that the first media segment can be handled differently
			// because it's only set on an Init fragment
			mTrickmodeState = TrickmodeState::DISCONTINUITY;
			mRestampedPts += mRestampedDuration;
		}
		else
		{
			if (TrickmodeState::UNDEF == mTrickmodeState)
			{
				mTrickmodeState = TrickmodeState::FIRST_FRAGMENT;
			}
		}
	}
	else // Media segment
	{
		switch (mTrickmodeState)
		{
			case TrickmodeState::FIRST_FRAGMENT:
				// This is the first media fragment after an init fragment (that is not a
				// discontinuity): The first restamped pts starts from 0.
				// Estimate the first fragment duration based on the rate and trickPlayFPS.
				// Subsequent durations will be based on the delta between the current fragment and
				// the last fragment. This is an estimate because we don't know how long the
				// duration should be, as there isn't a previous PTS from which to calculate a
				// delta.  Better to avoid too small a number, so limited to 0.25 seconds. GStreamer
				// works ok with this in practice.
				mRestampedDuration = MAX(duration / std::fabs(aamp->rate), 1.0 / trickPlayFPS);
				break;

			case TrickmodeState::DISCONTINUITY:
				// Assume that the restamped duration is the same as used for the previous fragment
				break;

			case TrickmodeState::STEADY:
				// Calculate the duration between the next fragment and the previous fragment and
				// divide it by the rate to determine the next pts
				fragmentPtsDelta = fabs(position - mLastFragmentPts);
				mRestampedDuration = fragmentPtsDelta / std::fabs(aamp->rate);
				mRestampedPts += mRestampedDuration;
				break;

			default:
				AAMPLOG_ERR("Unexpected trickmode state %d", static_cast<int>(mTrickmodeState));
				break;
		}
		// Transition immediately (back) to STEADY following the first frame or a discontinuity
		mTrickmodeState = TrickmodeState::STEADY;

		mLastFragmentPts = position;
		duration = mRestampedDuration.inSeconds();

		// Restamp the ISOBMFF position and duration in the media segment
		(void)mIsoBmffHelper->SetPtsAndDuration(fragment,
												static_cast<int64_t>(TRICKMODE_TIMESCALE * mRestampedPts),
												static_cast<int64_t>(TRICKMODE_TIMESCALE * mRestampedDuration));
	}
	// Update cached values for GStreamer
	position = mRestampedPts.inSeconds();

	AAMPLOG_INFO("state %d rate %f trickPlayFPS %d initFragment %d discontinuity %d "
				 "position %lfs duration %lfs restamped position %lfs duration %lfs",
				 static_cast<int>(mTrickmodeState),
				 aamp->rate, trickPlayFPS, initFragment, discontinuity,
				 inFragmentPosition.inSeconds(), inFragmentDuration.inSeconds(),
				 position, duration);
}

void MediaTrack::TrickModePtsRestamp(CachedFragment *cachedFragment)
{
	TrickModePtsRestamp(cachedFragment->fragment, cachedFragment->position, cachedFragment->duration,
						cachedFragment->initFragment, cachedFragment->discontinuity);
}

static bool isWebVttSegment( const char *buffer, size_t bufferLen )
{
	if( bufferLen>=3 && buffer[0]==(char)0xEF && buffer[1]==(char)0xBB && buffer[2]==(char)0xBF )
	{ // skip UTF-8 BOM if present
		buffer += 3;
		bufferLen -= 3;
	}
	return bufferLen>=6 && memcmp(buffer,"WEBVTT",6)==0;
}

std::string MediaTrack::RestampSubtitle( const char* buffer, size_t bufferLen, double position, double duration, double pts_offset_s )
{
	long long pts_offset_ms = pts_offset_s*1000;
	std::string str;
	if( ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp) && isWebVttSegment(buffer,bufferLen) )
	{
		const char *fin = &buffer[bufferLen];
		const char *prev = buffer;
		bool processedHeader = false;
		while( prev<fin )
		{
			const char *line_start = mystrstr( prev, fin, "\n\n" );
			if( line_start )
			{
				if( !processedHeader )
				{
					const char *localTimePtr = mystrstr(prev,line_start,"LOCAL:");
					long long localTimeMs = localTimePtr?convertHHMMSSToTime(localTimePtr+6):0;
					const char *mpegtsPtr = mystrstr(prev,line_start,"MPEGTS:");
					long long mpegts = mpegtsPtr?atoll(mpegtsPtr+7):0;
					pts_offset_ms -= localTimeMs;
					if( localTimeMs != currentLocalTimeMs  )
					{
						if( gotLocalTime )
						{
							AAMPLOG_MIL( "webvtt pts rollover" );
							ptsRollover = true;
						}
						currentLocalTimeMs = localTimeMs;
						gotLocalTime = true;
					}
					line_start += 2; // advance past \n\n
					str += "WEBVTT\nX-TIMESTAMP-MAP=LOCAL:00:00:00.000,MPEGTS:";
					str += std::to_string(mpegts);
					str += "\n\n";
					processedHeader = true;
					if( ptsRollover )
					{ // adjust by max pts ms
						pts_offset_ms += 95443717; // 0x1ffffffff/90
					}
				}
				else
				{
					line_start += 2; // advance past \n\n
					str += std::string(prev,line_start-prev);
				}
				prev = line_start;
				const char *line_end = mystrstr(line_start, fin, "\n" );
				if( line_end )
				{
					const char *line_delim = mystrstr( line_start, line_end, " --> " );
					if( line_delim )
					{ // apply pts offset by rewriting inline begin/end times
						prev = line_end;
						str += convertTimeToHHMMSS( convertHHMMSSToTime(line_start) + pts_offset_ms );
						str +=  " --> ";
						str += convertTimeToHHMMSS( convertHHMMSSToTime(line_delim+5) + pts_offset_ms );
					}
				}
			}
			else
			{ // trailing
				str += std::string(prev,fin-prev);
				prev = fin;
			}
		}
	}
	else
	{
		str = std::string(buffer,bufferLen);
	}
	printf( "***restamped caption: %s\n", str.c_str() );
	return str;
}

void MediaTrack::ClearMediaHeaderDuration(CachedFragment *fragment)
{
	(void)mIsoBmffHelper->ClearMediaHeaderDuration(fragment->fragment);
}

/**
 *  @brief Inject fragment Chunk into the gstreamer
 */
void MediaTrack::ProcessAndInjectFragment(CachedFragment *cachedFragment, bool fragmentDiscarded, bool isDiscontinuity, bool &ret )
{
	class StreamAbstractionAAMP* pContext = GetContext();
	if (aamp->GetLLDashChunkMode())
	{
		bool bIgnore = true;
		AAMPLOG_TRACE("[%s] Processing the chunk ==> fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d", name, fragmentChunkIdxToInject, numberOfFragmentChunksCached);
		if(!cachedFragment->isDummy)
		{
			bIgnore = ProcessFragmentChunk();
		}
		if(bIgnore)
		{
			AAMPLOG_TRACE("[%s] Updating the chunk inject ==> fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d", name, fragmentChunkIdxToInject, numberOfFragmentChunksCached);
			UpdateTSAfterChunkInject();
			AAMPLOG_TRACE("[%s] Updated the chunk inject ==> fragmentChunkIdxToInject = %d numberOfFragmentChunksCached %d", name, fragmentChunkIdxToInject, numberOfFragmentChunksCached);
		}
	}
	else
	{
		// Restamp 2.0 only for DASH streams
		if (ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && (eMEDIAFORMAT_DASH == aamp->mMediaFormat))
		{
			if ((pContext && pContext->trickplayMode))
			{
				TrickModePtsRestamp(cachedFragment);
			}
			else
			{
				if (!cachedFragment->initFragment)
				{
					// We could skip RestampPts when PTSOffsetSec==0 but the RestampPts log line
					// would then be missing and it is important for l2 tests
					int64_t ptsOffset = cachedFragment->PTSOffsetSec * cachedFragment->timeScale;
					(void)mIsoBmffHelper->RestampPts(cachedFragment->fragment, ptsOffset,
													 cachedFragment->uri, name,
													 cachedFragment->timeScale);
				}
				else
				{
					ClearMediaHeaderDuration(cachedFragment);
				}
			}
		}
		else if (ISCONFIGSET(eAAMPConfig_OverrideMediaHeaderDuration) &&
			(eMEDIAFORMAT_DASH == aamp->mMediaFormat))
		{
			// Only for DASH streams
			ClearMediaHeaderDuration(cachedFragment);
		}
		if ((mSubtitleParser || (aamp->IsGstreamerSubsEnabled())) && type == eTRACK_SUBTITLE)
		{
			auto ptr = cachedFragment->fragment.GetPtr();
			auto len = cachedFragment->fragment.GetLen();
			if( ISCONFIGSET(eAAMPConfig_HlsTsEnablePTSReStamp) )
			{
				while( aamp->mDownloadsEnabled )
				{
					if( pContext->mPtsOffsetMap.count(cachedFragment->discontinuityIndex)==0 )
					{
						AAMPLOG_WARN( "blocking subtitle track injection\n" );
						pContext->aamp->interruptibleMsSleep(1000);
					}
					else
					{
						auto firstElement = *pContext->mPtsOffsetMap.begin();
						cachedFragment->PTSOffsetSec = pContext->mPtsOffsetMap[cachedFragment->discontinuityIndex] - firstElement.second;
						std::string str = RestampSubtitle(
														  ptr,len,
														  cachedFragment->position,
														  cachedFragment->duration,
														  cachedFragment->PTSOffsetSec );
						cachedFragment->fragment.Clear();
						cachedFragment->fragment.AppendBytes(str.data(),str.size());
						if(mSubtitleParser)
						{
							mSubtitleParser->processData(str.data(), str.size(), cachedFragment->position, cachedFragment->duration);
						}
						break;
					}
				}
			}
			else if(mSubtitleParser)
			{ // no restamping
				mSubtitleParser->processData( ptr, len, cachedFragment->position, cachedFragment->duration);
			}
		}
		if (!cachedFragment->isDummy && (type != eTRACK_SUBTITLE || (aamp->IsGstreamerSubsEnabled())))
		{
			if(AAMP_NORMAL_PLAY_RATE==aamp->rate)
			{
				InjectFragmentInternal(cachedFragment, fragmentDiscarded, isDiscontinuity);
			}
			else
			{
				InjectFragmentInternal(cachedFragment, fragmentDiscarded, cachedFragment->discontinuity);
			}
		}
		class StreamAbstractionAAMP* pContext = GetContext();
		if (eTRACK_VIDEO == type && pContext && pContext->GetProfileCount())
		{
			pContext->NotifyBitRateUpdate(cachedFragment->profileIndex, cachedFragment->cacheFragStreamInfo, cachedFragment->position);
		}
		AAMPLOG_DEBUG("%s - injected cached fragment at pos %f dur %f", name, cachedFragment->position, cachedFragment->duration);
		if (!fragmentDiscarded)
		{
			std::lock_guard<std::mutex> lock(mTrackParamsMutex);
			totalInjectedDuration += cachedFragment->duration;
			lastInjectedPosition = cachedFragment->absPosition;
			lastInjectedDuration = cachedFragment->absPosition + cachedFragment->duration;
			mSegInjectFailCount = 0;
		}
		else
		{
			AAMPLOG_WARN("[%s] - Not updating totalInjectedDuration since fragment is Discarded", name);
			mSegInjectFailCount++;
			int SegInjectFailCount = GETCONFIGVALUE(eAAMPConfig_SegmentInjectThreshold);
			if(SegInjectFailCount <= mSegInjectFailCount)
			{
				ret	= false;
				AAMPLOG_ERR("[%s] Reached max inject failure count: %d, stopping playback", name, SegInjectFailCount);
				aamp->SendErrorEvent(AAMP_TUNE_FAILED_PTS_ERROR);
			}
		}

		// Release the memory and Update the inject
		if(IsInjectionFromCachedFragmentChunks())
		{
			UpdateTSAfterChunkInject();
		}
		else
		{
			UpdateTSAfterInject();
		}
	}
}

/**
 *  @brief Inject fragment into the gstreamer
 */
bool MediaTrack::InjectFragment()
{
	bool ret = true;
	bool isChunkMode = aamp->GetLLDashChunkMode() && (aamp->IsLocalAAMPTsbInjection() == false);
	bool isChunkBuffer = IsInjectionFromCachedFragmentChunks();
	bool lowLatency = aamp->GetLLDashServiceData()->lowLatencyMode;
	StreamAbstractionAAMP* pContext = GetContext();

	if(!isChunkMode)
	{
		aamp->BlockUntilGstreamerWantsData(NULL, 0, type);
	}
	bool notAborted = isChunkBuffer ? WaitForCachedFragmentChunkAvailable() : WaitForCachedFragmentAvailable();
	if (notAborted)
	{
		bool stopInjection = false;
		bool fragmentDiscarded = false;
		bool isDiscontinuity = false;
		CachedFragment* cachedFragment = nullptr;
		if(isChunkBuffer)
		{
			cachedFragment = &this->mCachedFragmentChunks[fragmentChunkIdxToInject];
			AAMPLOG_TRACE("[%s] fragmentChunkIdxToInject : %d Discontinuity %d ", name, fragmentChunkIdxToInject, cachedFragment->discontinuity);

		}
		else
		{
			cachedFragment = &this->mCachedFragment[fragmentIdxToInject];
			AAMPLOG_TRACE("[%s] fragmentIdxToInject : %d Discontinuity %d ", name, fragmentIdxToInject, cachedFragment->discontinuity);
		}

		AAMPLOG_TRACE("[%s] - fragmentIdxToInject %d cachedFragment %p ptr %p",
					 name, fragmentIdxToInject, cachedFragment, cachedFragment->fragment.GetPtr());

		if (cachedFragment->fragment.GetPtr())
		{
			// This is currently supported for non-LL DASH streams only at normal play rate
			if (!isChunkMode && aamp->rate == AAMP_NORMAL_PLAY_RATE)
			{
				HandleFragmentPositionJump(cachedFragment);
			}
			stopInjection = CheckForDiscontinuity(cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
			if (!stopInjection)
			{
				ProcessAndInjectFragment(cachedFragment, fragmentDiscarded, isDiscontinuity, ret);
			}
		}
		else
		{
			//EOS should not be triggered when subtitle sets its "eosReached" in any circumstances
			if (SignalIfEOSReached())
			{
				//Save the playback rate prior to sending EOS
				if(pContext != NULL)
				{
					int rate = pContext->aamp->rate;
					aamp->EndOfStreamReached((AampMediaType)type);
					/*For muxed streams, provide EOS for audio track as well since
					 * no separate MediaTrack for audio is present*/
					MediaTrack* audio = pContext->GetMediaTrack(eTRACK_AUDIO);
					if (audio && !audio->enabled && rate == AAMP_NORMAL_PLAY_RATE)
					{
						aamp->EndOfStreamReached(eMEDIATYPE_AUDIO);
					}
				}
				else
				{
					AAMPLOG_WARN("GetContext is null");  //CID:81799 - Null Return
				}
				AAMPLOG_INFO("%s EOS Signalled to pipeline", name);
			}
			else
			{
				AAMPLOG_WARN("%s - NULL ptr to inject. fragmentIdxToInject %d", name, fragmentIdxToInject);
			}
			ret = false;
		}
	}
	else
	{
		AAMPLOG_WARN("WaitForCachedFragmentAvailable %s aborted LowLatency: %d ChunkMode %d", name, lowLatency,isChunkMode);
		//EOS should not be triggered when subtitle sets its "eosReached" in any circumstances
		SignalIfEOSReached();
		ret = false;
	}
	return ret;
} // InjectFragment

/**
 *  @brief SignalIfEOSReached - Signal end-of-stream to pipeline if injector at EOS
 *
 * @return bool
 */
bool MediaTrack::SignalIfEOSReached()
{
	bool ret = false;
	//EOS should not be triggered when subtitle sets its "eosReached" in any circumstances
	if (eosReached && (eTRACK_SUBTITLE != type))
	{
		//Save the playback rate prior to sending EOS
		StreamAbstractionAAMP* pContext = GetContext();
		if(pContext != NULL)
		{
			int rate = pContext->aamp->rate;
			aamp->EndOfStreamReached((AampMediaType)type);
			/*For muxed streams, provide EOS for audio track as well since
			 * no separate MediaTrack for audio is present*/
			MediaTrack* audio = pContext->GetMediaTrack(eTRACK_AUDIO);
			if (audio && !audio->enabled && rate == AAMP_NORMAL_PLAY_RATE)
			{
				aamp->EndOfStreamReached(eMEDIATYPE_AUDIO);
			}
			ret = true;
		}
		else
		{
			AAMPLOG_WARN("GetContext is null");  //CID:81799 - Null Return
		}
	}
	return ret;
}

/**
 *  @brief Start fragment injector loop
 */
void MediaTrack::StartInjectLoop()
{

	try
	{
		std::lock_guard<std::mutex> guard(injectorStartMutex);
		if (fragmentInjectorThreadStarted)
		{
			AAMPLOG_WARN("Fragment injector thread already started");
		}
		else
		{
			abort = false;
			abortInject = false;
			discontinuityProcessed = false;

			fragmentInjectorThreadID = std::thread(&MediaTrack::RunInjectLoop, this);
			fragmentInjectorThreadStarted = true;
			AAMPLOG_INFO("Thread created for RunInjectLoop [%zx]", GetPrintableThreadID(fragmentInjectorThreadID));
		}
	}
	catch(const std::exception& e)
	{
		AAMPLOG_WARN("Failed to create FragmentInjector thread ; %s", e.what());
	}
}

/**
 * @brief Wait till the new Audio fragment cache available
 * after clearing the existing buffer
 */
void MediaTrack::WaitForCachedAudioFragmentAvailable()
{
	AAMPLOG_WARN("Enter WaitForCachedAudioFragmentAvailable");
	std::unique_lock<std::mutex> lock(audioMutex);
	audioFragmentCached.wait(lock);
	AAMPLOG_DEBUG("[%s] wait complete for audioFragmentCached", name);
}

void MediaTrack::WaitForCachedSubtitleFragmentAvailable()
{
	AAMPLOG_WARN("Enter WaitForCachedSubtitleFragmentAvailable");
	std::unique_lock<std::mutex> lock(subtitleMutex);
	subtitleFragmentCached.wait(lock);
	AAMPLOG_DEBUG("[%s] wait complete for subtitleFragmentCached", name);
}

/**
 * @brief Notify the new Audio fragment cache available
 * after clearing the existing buffer
 */
void MediaTrack::NotifyCachedAudioFragmentAvailable()
{
	std::lock_guard<std::mutex> guard(audioMutex);
	audioFragmentCached.notify_one();
}

void MediaTrack::NotifyCachedSubtitleFragmentAvailable()
{
	std::lock_guard<std::mutex> guard(subtitleMutex);
	subtitleFragmentCached.notify_one();
}

/**
 *  @brief Injection loop - use internally by injection logic
 */
void MediaTrack::RunInjectLoop()
{
	UsingPlayerId playerId( aamp->mPlayerId );
	AAMPLOG_WARN("fragment injector started. track %s", name);

	bool notifyFirstFragment = true;
	bool keepInjecting = true;
	bool lowLatency = aamp->GetLLDashServiceData()->lowLatencyMode;
	StreamAbstractionAAMP* pContext = GetContext();
	if ((AAMP_NORMAL_PLAY_RATE == aamp->rate) )
	{
		if (!bufferMonitorThreadDisabled && !bufferMonitorThreadStarted)
		{
			try
			{
				bufferMonitorThreadID = std::thread(&MediaTrack::MonitorBufferHealth, this);
				bufferMonitorThreadStarted = true;
				AAMPLOG_INFO("Thread created for MonitorBufferHealth [%zx]", GetPrintableThreadID(bufferMonitorThreadID));
			}
			catch(const std::exception& e)
			{
				AAMPLOG_WARN("Failed to create BufferHealthMonitor thread: %s", e.what());
			}
		}
		if ((type == eTRACK_SUBTITLE) && ( !UpdateSubtitleClockTaskStarted ) && aamp->IsGstreamerSubsEnabled() && ISCONFIGSET(eAAMPConfig_EnableMediaProcessor))
		{
			try
			{
				if (!ISCONFIGSET(eAAMPConfig_useRialtoSink))
				{
					subtitleClockThreadID = std::thread(&MediaTrack::UpdateSubtitleClockTask, this);
					UpdateSubtitleClockTaskStarted = true;
					AAMPLOG_INFO("Thread created for UpdateSubtitleClockTask [%zx]", GetPrintableThreadID(subtitleClockThreadID));
				}
			}
			catch(const std::exception& e)
			{
				AAMPLOG_WARN("Failed to create UpdateSubtitleClockTask thread: %s", e.what());
			}
		}
	}
	{
		std::lock_guard<std::mutex> lock(mTrackParamsMutex);
		totalInjectedDuration = 0;
		totalInjectedChunksDuration = 0;
		lastInjectedPosition = 0;
		lastInjectedDuration = 0;
	}
	while (aamp->DownloadsAreEnabled() && keepInjecting)
	{
		if(type == eTRACK_AUDIO && (loadNewAudio || refreshAudio) && !lowLatency) //TBD
		{
			WaitForCachedAudioFragmentAvailable();
		}
		if(type == eTRACK_SUBTITLE && (loadNewSubtitle || refreshSubtitles) && !lowLatency) // TBD
		{
			WaitForCachedSubtitleFragmentAvailable();
		}
		if (!InjectFragment())
		{
			if(!(loadNewAudio || refreshAudio ||loadNewSubtitle || refreshSubtitles ))
			{
				keepInjecting = false;
			}
		}
		if (notifyFirstFragment && type != eTRACK_SUBTITLE)
		{
			notifyFirstFragment = false;
			if (pContext)
			{
				pContext->NotifyFirstFragmentInjected();
			}
		}
		// Disable audio video balancing for CDVR content ..
		// CDVR Content includes eac3 audio, the duration of audio doesn't match with video
		// and hence balancing fetch/inject not needed for CDVR
		// TBD Not needed for LLD
		// Not needed for local TSB gstreamer will balance A/V - thats what it does
		if(!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback) && !aamp->IsCDVRContent() && (!aamp->mAudioOnlyPb && !aamp->mVideoOnlyPb) && !lowLatency && !aamp->IsLocalAAMPTsb())
		{
			if(pContext != NULL)
			{
				if(eTRACK_AUDIO == type)
				{
					pContext->WaitForVideoTrackCatchup();
				}
				else if (eTRACK_VIDEO == type)
				{
					pContext->ReassessAndResumeAudioTrack(false);
				}
				else if (eTRACK_SUBTITLE == type)
				{
					pContext->WaitForAudioTrackCatchup();
				}
				else if (eTRACK_AUX_AUDIO == type)
				{
					pContext->WaitForVideoTrackCatchupForAux();
				}
			}
			else
			{
				AAMPLOG_WARN("GetContext  is null");  //CID:85546 - Null Return
			}
		}
	}

	abortInject = true;
	AAMPLOG_WARN("fragment injector done. track %s", name);
}

/**
 *  @brief Stop fragment injector loop
 */
void MediaTrack::StopInjectLoop()
{
	NotifyCachedAudioFragmentAvailable();
	NotifyCachedSubtitleFragmentAvailable();
	std::lock_guard<std::mutex> guard(injectorStartMutex);
	if(fragmentInjectorThreadStarted && fragmentInjectorThreadID.joinable())
	{
		fragmentInjectorThreadID.join();
		AAMPLOG_INFO("Fragment injector thread joined");
	}
	fragmentInjectorThreadStarted = false;
}

/**
 *  @brief Check if a track is enabled
 */
bool MediaTrack::Enabled()
{
	return enabled;
}

/**
 *  @brief Get buffer to store the downloaded fragment content to cache next fragment
 */
CachedFragment* MediaTrack::GetFetchBuffer(bool initialize)
{
	/*Make sure fragmentDurationSeconds updated before invoking this*/
	CachedFragment* cachedFragment = &this->mCachedFragment[fragmentIdxToFetch];
	if(initialize)
	{
		if (cachedFragment->fragment.GetPtr() )
		{
			AAMPLOG_WARN("fragment.ptr already set - possible memory leak");
		}
		cachedFragment->fragment.Clear();
		//memset(&cachedFragment->fragment, 0x00, sizeof(AampGrowableBuffer));
	}
	return cachedFragment;
}

/**
 *  @brief Get buffer to fetch and cache next fragment chunk
 */
CachedFragment* MediaTrack::GetFetchChunkBuffer(bool initialize)
{
	if(fragmentChunkIdxToFetch <0 || fragmentChunkIdxToFetch >= mCachedFragmentChunksSize)
	{
		AAMPLOG_WARN("[%s] OUT OF RANGE => fragmentChunkIdxToFetch: %d mCachedFragmentChunksSize: %zu",name,fragmentChunkIdxToFetch, mCachedFragmentChunksSize);
		return NULL;
	}

	CachedFragment* cachedFragment = NULL;
	cachedFragment = &this->mCachedFragmentChunks[fragmentChunkIdxToFetch];

	AAMPLOG_DEBUG("[%s] fragmentChunkIdxToFetch: %d cachedFragment: %p",name, fragmentChunkIdxToFetch, cachedFragment);

	if(initialize && cachedFragment)
	{
		if (cachedFragment->fragment.GetPtr() )
		{
			AAMPLOG_WARN("[%s] fragment.ptr[%p] already set - possible memory leak (len=[%zu],avail=[%zu])",name, cachedFragment->fragment.GetPtr(), cachedFragment->fragment.GetLen(), cachedFragment->fragment.GetAvail() );
		}
		cachedFragment->fragment.Clear();
	}
	return cachedFragment;
}

/**
 * @brief Check if the fragment cache buffer is full
 * @return true if the fragment cache buffer is full, false otherwise
 */
bool MediaTrack::IsFragmentCacheFull()
{
	bool rc = false;
	std::lock_guard<std::mutex> guard(mutex);
	if(IsInjectionFromCachedFragmentChunks())
	{
		AAMPLOG_DEBUG("[%s] numberOfFragmentChunksCached %d mCachedFragmentChunksSize %zu", name, numberOfFragmentChunksCached, mCachedFragmentChunksSize);
		rc = (numberOfFragmentChunksCached == mCachedFragmentChunksSize);
	}
	else
	{
		AAMPLOG_DEBUG("[%s] numberOfFragmentsCached %d maxCachedFragmentsPerTrack %d", name, numberOfFragmentsCached, maxCachedFragmentsPerTrack);
		rc = numberOfFragmentsCached == maxCachedFragmentsPerTrack;
	}
	return rc;
}

/**
 *  @brief Set current bandwidth of track
 */
void MediaTrack::SetCurrentBandWidth(int bandwidthBps)
{
	this->bandwidthBitsPerSecond = bandwidthBps;
}

/**
 *  @brief Get profile index for TsbBandwidth
 */
int MediaTrack::GetProfileIndexForBW( BitsPerSecond mTsbBandwidth)
{
	return GetContext()?GetContext()->GetProfileIndexForBandwidth(mTsbBandwidth):0;
}

/**
 *  @brief Get current bandwidth in bps
 */
int MediaTrack::GetCurrentBandWidth()
{
	return this->bandwidthBitsPerSecond;
}


/**
 * @brief Flushes all fetched cached fragments
 * Flushes all fetched media fragments
 */
void MediaTrack::FlushFetchedFragments()
{
	std::lock_guard<std::mutex> guard(mutex);
	if (IsInjectionFromCachedFragmentChunks())
	{
		while (numberOfFragmentChunksCached)
		{
			AAMPLOG_DEBUG("[%s] Free mCachedFragmentChunks[%d] numberOfFragmentChunksCached %d", name, fragmentChunkIdxToInject, numberOfFragmentChunksCached);
			mCachedFragmentChunks[fragmentChunkIdxToInject].Clear();

			fragmentChunkIdxToInject++;
			if (fragmentChunkIdxToInject == maxCachedFragmentChunksPerTrack)
			{
				fragmentChunkIdxToInject = 0;
			}
			numberOfFragmentChunksCached--;
		}
		fragmentChunkInjected.notify_one();
	}
	else
	{
		while (numberOfFragmentsCached)
		{
			AAMPLOG_DEBUG("[%s] Free cachedFragment[%d] numberOfFragmentsCached %d", name, fragmentIdxToInject, numberOfFragmentsCached);
			mCachedFragment[fragmentIdxToInject].Clear();

			fragmentIdxToInject++;
			if (fragmentIdxToInject == maxCachedFragmentsPerTrack)
			{
				fragmentIdxToInject = 0;
			}
			numberOfFragmentsCached--;
		}
		fragmentInjected.notify_one();
	}
}

/**
 * @brief Flushes all cached fragments
 * Flushes all media fragments and resets all relevant counters
 * Only intended for use on subtitle streams
 */
void MediaTrack::FlushFragments()
{
	AAMPLOG_WARN("[%s]", name);
	if(IsInjectionFromCachedFragmentChunks())
	{
		for (int i = 0; i < maxCachedFragmentChunksPerTrack; i++)
		{
			mCachedFragmentChunks[i].Clear();
		}
		unparsedBufferChunk.Free();
		parsedBufferChunk.Free();
		fragmentChunkIdxToInject = 0;
		fragmentChunkIdxToFetch = 0;
		std::lock_guard<std::mutex> guard(mutex);
		numberOfFragmentChunksCached = 0;
		totalFragmentChunksDownloaded = 0;
		// We need to revisit if these variables should be also sync using mTrackParamsMutex
		totalInjectedChunksDuration = 0;
	}
	else
	{
		for (int i = 0; i < maxCachedFragmentsPerTrack; i++)
		{
			mCachedFragment[i].fragment.Free();
			memset(&mCachedFragment[i], 0, sizeof(CachedFragment));
		}
		fragmentIdxToInject = 0;
		fragmentIdxToFetch = 0;
		numberOfFragmentsCached = 0;
		lastInjectedDuration = 0;
		if( ( type == eTRACK_AUDIO && !loadNewAudio ) || ( type == eTRACK_SUBTITLE && !loadNewSubtitle ) )
		{
			std::lock_guard<std::mutex> lock(mTrackParamsMutex);
			totalFetchedDuration = 0;
			totalFragmentsDownloaded = 0;
			totalInjectedDuration = 0;
		}
	}
}


/**
 *  @brief OffsetTrackParams To set Track's Fetch and Inject duration after playlist update
 *  Currently intended for use on seamless audio track change
 */
void MediaTrack::OffsetTrackParams(double deltaFetchedDuration, double deltaInjectedDuration, int deltaFragmentsDownloaded)
{
	std::lock_guard<std::mutex> lock(mTrackParamsMutex);
	AAMPLOG_MIL("Before Track Change totalFetchedDuration %lf totalInjectedDuration %lf totalFragmentsDownloaded:%d", totalFetchedDuration, totalInjectedDuration, totalFragmentsDownloaded);

	totalFetchedDuration -= deltaFetchedDuration;
	// injected and fetched duration should be same
	totalInjectedDuration -= deltaInjectedDuration;
	totalFragmentsDownloaded -= deltaFragmentsDownloaded;

	AAMPLOG_MIL("New totalFetchedDuration %lf totalInjectedDuration %lf totalFragmentsDownloaded:%d", totalFetchedDuration, totalInjectedDuration, totalFragmentsDownloaded);
}

/**
 *  @brief MediaTrack Constructor
 */
MediaTrack::MediaTrack(TrackType type, PrivateInstanceAAMP* aamp, const char* name) :
		eosReached(false), enabled(false), numberOfFragmentsCached(0), numberOfFragmentChunksCached(0), fragmentIdxToInject(0), fragmentChunkIdxToInject(0),
		fragmentIdxToFetch(0), fragmentChunkIdxToFetch(0), abort(false), fragmentInjectorThreadID(), bufferMonitorThreadID(), subtitleClockThreadID(), totalFragmentsDownloaded(0), totalFragmentChunksDownloaded(0),
		fragmentInjectorThreadStarted(false), bufferMonitorThreadStarted(false), UpdateSubtitleClockTaskStarted(false), bufferMonitorThreadDisabled(false), totalInjectedDuration(0), totalInjectedChunksDuration(0), currentInitialCacheDurationSeconds(0),
		sinkBufferIsFull(false), cachingCompleted(false), fragmentDurationSeconds(0),  segDLFailCount(0),segDrmDecryptFailCount(0),mSegInjectFailCount(0),
		bufferStatus(BUFFER_STATUS_GREEN), prevBufferStatus(BUFFER_STATUS_GREEN),
		bandwidthBitsPerSecond(0), totalFetchedDuration(0),
		discontinuityProcessed(false), ptsError(false), mCachedFragment(NULL), name(name), type(type), aamp(aamp),
		mutex(), fragmentFetched(), fragmentInjected(), abortInject(false),
		mSubtitleParser(), refreshSubtitles(false), refreshAudio(false), maxCachedFragmentsPerTrack(0),
		mCachedFragmentChunks{}, unparsedBufferChunk{"unparsedBufferChunk"}, parsedBufferChunk{"parsedBufferChunk"}, fragmentChunkFetched(), fragmentChunkInjected(), maxCachedFragmentChunksPerTrack(0),
		noMDATCount(0), loadNewAudio(false), audioFragmentCached(), audioMutex(), loadNewSubtitle(false), subtitleFragmentCached(), subtitleMutex(),
		abortPlaylistDownloader(true), playlistDownloaderThreadStarted(false), plDownloadWait()
		,dwnldMutex(), playlistDownloaderThread(NULL), fragmentCollectorWaitingForPlaylistUpdate(false)
		,frDownloadWait(),prevDownloadStartTime(-1)
		,playContext(nullptr), seamlessAudioSwitchInProgress(false), lastInjectedPosition(0), lastInjectedDuration(0), seamlessSubtitleSwitchInProgress(false)
		,mIsLocalTSBInjection(false), mCachedFragmentChunksSize(0)
		,mIsoBmffHelper(std::make_shared<IsoBmffHelper>())
		,mLastFragmentPts(0), mRestampedPts(0), mRestampedDuration(0), mTrickmodeState(TrickmodeState::UNDEF)
		,mTrackParamsMutex(), mCheckForRampdown(false)
		,gotLocalTime(false),ptsRollover(false),currentLocalTimeMs(0)
{
	maxCachedFragmentsPerTrack = GETCONFIGVALUE(eAAMPConfig_MaxFragmentCached);
	mCachedFragment = new CachedFragment[(maxCachedFragmentsPerTrack) ? maxCachedFragmentsPerTrack : 1];

	maxCachedFragmentChunksPerTrack = GETCONFIGVALUE(eAAMPConfig_MaxFragmentChunkCached);
	SetCachedFragmentChunksSize((aamp->GetLLDashChunkMode()) ? maxCachedFragmentChunksPerTrack : maxCachedFragmentsPerTrack);
}


/**
 *  @brief MediaTrack Destructor
 */
MediaTrack::~MediaTrack()
{
	if (bufferMonitorThreadStarted)
	{
		bufferMonitorThreadID.join();
		{
			AAMPLOG_TRACE("joined bufferMonitorThreadID");
		}
	}
	if ((UpdateSubtitleClockTaskStarted) && (type == eTRACK_SUBTITLE))
	{
		AAMPLOG_TRACE("joining subtitleClockThreadID for UpdateSubtitleClockTask");
		if (subtitleClockThreadID.joinable())
		{
			subtitleClockThreadID.join();
			AAMPLOG_TRACE("joined subtitleClockThreadID for UpdateSubtitleClockTask");
		}
		else
		{
			AAMPLOG_ERR("Unable to join subtitleClockThreadID for UpdateSubtitleClockTask!");
		}
	}

	SAFE_DELETE_ARRAY(mCachedFragment);
}

/**
 *  @brief Unblock track if caught up with video or downloads are stopped
 */
void StreamAbstractionAAMP::ReassessAndResumeAudioTrack(bool abort)
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	MediaTrack *aux = GetMediaTrack(eTRACK_AUX_AUDIO);
	if( audio && video )
	{
		std::lock_guard<std::mutex> guard(mLock);
		double audioDuration = audio->GetTotalInjectedDuration();
		double videoDuration = video->GetTotalInjectedDuration();
		if(audioDuration < (videoDuration + (2 * video->fragmentDurationSeconds)) || !aamp->DownloadsAreEnabled() || video->IsDiscontinuityProcessed() || abort || video->IsAtEndOfTrack())
		{
			mCond.notify_one();
		}
		if (aux && aux->enabled)
		{
			double auxDuration = aux->GetTotalInjectedDuration();
			if (auxDuration < (videoDuration + (2 * video->fragmentDurationSeconds)) || !aamp->DownloadsAreEnabled() || video->IsDiscontinuityProcessed() || abort || video->IsAtEndOfTrack())
			{
				mAuxCond.notify_one();
			}
		}
	}
}


/**
 * @brief Blocks aux track injection until caught up with video track.
 *        Used internally by injection logic
 */
void StreamAbstractionAAMP::WaitForVideoTrackCatchup()
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if( video != NULL)
	{
		std::unique_lock<std::mutex> lock(mLock);
		double audioDuration = audio->GetTotalInjectedDuration();
		double videoDuration = video->GetTotalInjectedDuration();
		while ((audioDuration > (videoDuration + video->fragmentDurationSeconds)) && aamp->DownloadsAreEnabled() && !audio->IsDiscontinuityProcessed() && !video->IsInjectionAborted() && !(video->IsAtEndOfTrack()))
		{
			if (mTrackState == eDISCONTINUITY_IN_VIDEO)
			{
				AAMPLOG_WARN("Skipping WaitForVideoTrackCatchup as video is processing a discontinuity");
				break;
			}

			if (std::cv_status::no_timeout == mCond.wait_for(lock, std::chrono::milliseconds(100)))
			{
				break;
			}
			// Update video and audio duration after wait
			audioDuration = audio->GetTotalInjectedDuration();
			videoDuration = video->GetTotalInjectedDuration();
		}
	}
}


/**
 * @brief StreamAbstractionAAMP constructor.
 */
StreamAbstractionAAMP::StreamAbstractionAAMP(PrivateInstanceAAMP* aamp, id3_callback_t mID3Handler):
		trickplayMode(false), currentProfileIndex(0), mCurrentBandwidth(0),currentAudioProfileIndex(-1),currentTextTrackProfileIndex(-1),
		mTsbBandwidth(0),mNwConsistencyBypass(true), profileIdxForBandwidthNotification(0),
		hasDrm(false), mIsAtLivePoint(false), mESChangeStatus(false),mAudiostateChangeCount(0),
		mNetworkDownDetected(false), mTotalPausedDurationMS(0), mIsPaused(false), mProgramStartTime(-1),
		mStartTimeStamp(-1),mLastPausedTimeStamp(-1), aamp(aamp),
		mIsPlaybackStalled(false), mTuneType(), mLock(),
		mCond(), mLastVideoFragCheckedForABR(0), mLastVideoFragParsedTimeMS(0),
		mSubCond(), mAudioTracks(), mTextTracks(),mABRHighBufferCounter(0),mABRLowBufferCounter(0),mMaxBufferCountCheck(0),
		mStateLock(), mStateCond(), mTrackState(eDISCONTINUITY_FREE),
		mRampDownLimit(-1), mRampDownCount(0),mABRMaxBuffer(0), mABRCacheLength(0), mABRMinBuffer(0), mABRNwConsistency(0),
		mBitrateReason(eAAMP_BITRATE_CHANGE_BY_TUNE),
		mAudioTrackIndex(), mTextTrackIndex(),
		mAuxCond(), mFwdAudioToAux(false),
		mAudioTracksAll(), mTextTracksAll(),
		mTsbMaxBitrateProfileIndex(-1),mUpdateReason(false),
		mPTSOffset(0.0),
		mID3Handler{mID3Handler}
{
	mLastVideoFragParsedTimeMS = aamp_GetCurrentTimeMS();
	AAMPLOG_TRACE("StreamAbstractionAAMP");
	mMaxBufferCountCheck = GETCONFIGVALUE(eAAMPConfig_ABRCacheLength);
	mABRCacheLength = mMaxBufferCountCheck;
	mABRBufferCounter = GETCONFIGVALUE(eAAMPConfig_ABRBufferCounter);
	mABRMaxBuffer = GETCONFIGVALUE(eAAMPConfig_MaxABRNWBufferRampUp);
	mABRMinBuffer = GETCONFIGVALUE(eAAMPConfig_MinABRNWBufferRampDown);
	mABRNwConsistency = GETCONFIGVALUE(eAAMPConfig_ABRNWConsistency);
	aamp->mhAbrManager.setDefaultInitBitrate(aamp->GetDefaultBitrate());

	BitsPerSecond ibitrate = aamp->GetIframeBitrate();
	if (ibitrate > 0)
	{
		aamp->mhAbrManager.setDefaultIframeBitrate(ibitrate);
	}
	mRampDownLimit = GETCONFIGVALUE(eAAMPConfig_RampDownLimit);
	if (!aamp->IsNewTune())
	{
		mBitrateReason = (aamp->rate != AAMP_NORMAL_PLAY_RATE) ? eAAMP_BITRATE_CHANGE_BY_TRICKPLAY : eAAMP_BITRATE_CHANGE_BY_SEEK;
	}
}


/**
 *  @brief StreamAbstractionAAMP destructor.
 */
StreamAbstractionAAMP::~StreamAbstractionAAMP()
{
	AAMPLOG_INFO("Exit StreamAbstractionAAMP");
}

/**
 *  @brief Get the last video fragment parsed time.
 */
double StreamAbstractionAAMP::LastVideoFragParsedTimeMS(void)
{
	return mLastVideoFragParsedTimeMS;
}

/**
 *  @brief Get the desired profile to start fetching.
 */
int StreamAbstractionAAMP::GetDesiredProfile(bool getMidProfile)
{
	int desiredProfileIndex = 0;
	if(GetProfileCount())
	{
		if (this->UseIframeTrack() && ABRManager::INVALID_PROFILE != aamp->mhAbrManager.getLowestIframeProfile())
		{
			desiredProfileIndex = GetIframeTrack();
		}
		else
		{
			desiredProfileIndex = aamp->mhAbrManager.getInitialProfileIndex(getMidProfile);
		}
		profileIdxForBandwidthNotification = desiredProfileIndex;
		MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
		if(video)
		{
			StreamInfo* streamInfo = GetStreamInfo(profileIdxForBandwidthNotification);
			if(streamInfo != NULL)
			{
				video->SetCurrentBandWidth( (int)streamInfo->bandwidthBitsPerSecond);
			}
			else
			{
				AAMPLOG_WARN("GetStreamInfo is null");  //CID:81678 - Null Returns
			}
		}
		else
		{
			AAMPLOG_TRACE("video track NULL");
		}
		AAMPLOG_DEBUG("profileIdxForBandwidthNotification updated to %d ", profileIdxForBandwidthNotification);
	}
	return desiredProfileIndex;
}

/**
 *   @brief Get profile index of highest bandwidth
 *
 *   @return Profile highest BW profile index
 */
int StreamAbstractionAAMP::GetMaxBWProfile()
{
	int ret = 0;
	if(aamp->IsFogTSBSupported() && mTsbMaxBitrateProfileIndex >= 0)
	{
		ret = mTsbMaxBitrateProfileIndex;
	}
	else
	{
		ret =  aamp->mhAbrManager.getMaxBandwidthProfile();
	}
	return ret;
}

/**
 *   @brief Notify bitrate updates to application.
 *          Used internally by injection logic
 */
void StreamAbstractionAAMP::NotifyBitRateUpdate(int profileIndex, const StreamInfo &cacheFragStreamInfo, double position)
{
	AAMPLOG_TRACE("[DEBUG]:stream Info bps(%ld) w(%d) h(%d) fr(%f) profileIndex %d aamp->GetPersistedProfileIndex() %d", cacheFragStreamInfo.bandwidthBitsPerSecond, cacheFragStreamInfo.resolution.width, cacheFragStreamInfo.resolution.height, cacheFragStreamInfo.resolution.framerate,profileIndex,aamp->GetPersistedProfileIndex());
	if (profileIndex != aamp->GetPersistedProfileIndex() && cacheFragStreamInfo.bandwidthBitsPerSecond != 0)
	{
		StreamInfo* streamInfo = GetStreamInfo(GetMaxBWProfile());
		if(streamInfo != NULL)
		{
			bool lGetBWIndex = false;
			if(aamp->IsTuneTypeNew && ((cacheFragStreamInfo.bandwidthBitsPerSecond == streamInfo->bandwidthBitsPerSecond) || !aamp->CheckABREnabled()))
			{
				MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
				AAMPLOG_WARN("NotifyBitRateUpdate: Max BitRate: %" BITSPERSECOND_FORMAT ", timetotop: %f", cacheFragStreamInfo.bandwidthBitsPerSecond, video->GetTotalInjectedDuration());
				aamp->IsTuneTypeNew = false;
				lGetBWIndex = true;
			}

			// Send bitrate notification
			aamp->profiler.IncrementChangeCount(Count_BitrateChange);
			aamp->NotifyBitRateChangeEvent( (int)cacheFragStreamInfo.bandwidthBitsPerSecond,
										   cacheFragStreamInfo.reason, cacheFragStreamInfo.resolution.width,
										   cacheFragStreamInfo.resolution.height, cacheFragStreamInfo.resolution.framerate, position, lGetBWIndex);
			// Store the profile , compare it before sending it . This avoids sending of event after trickplay if same bitrate
			aamp->SetPersistedProfileIndex(profileIndex);
		}
		else
		{
			AAMPLOG_WARN("StreamInfo  is null");  //CID:82200 - Null Returns
		}
	}
}

/**
 *  @brief Check if Initial Fragment Caching is supported
 */
bool StreamAbstractionAAMP::IsInitialCachingSupported()
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	return (video && video->enabled);
}

/**
 *  @brief Function to update stream info of current fetched fragment
 */
void StreamAbstractionAAMP::UpdateStreamInfoBitrateData(int profileIndex, StreamInfo &cacheFragStreamInfo)
{
	StreamInfo* streamInfo = GetStreamInfo(profileIndex);

	if (streamInfo)
	{
		cacheFragStreamInfo.bandwidthBitsPerSecond = streamInfo->bandwidthBitsPerSecond;
		cacheFragStreamInfo.reason = mBitrateReason;
		cacheFragStreamInfo.resolution.height = streamInfo->resolution.height;
		cacheFragStreamInfo.resolution.framerate = streamInfo->resolution.framerate;
		cacheFragStreamInfo.resolution.width = streamInfo->resolution.width;
		//AAMPLOG_WARN("stream Info bps(%ld) w(%d) h(%d) fr(%f)", cacheFragStreamInfo.bandwidthBitsPerSecond, cacheFragStreamInfo.resolution.width, cacheFragStreamInfo.resolution.height, cacheFragStreamInfo.resolution.framerate);
	}
}


/**
 *  @brief Update profile state based on bandwidth of fragments downloaded.
 */
void StreamAbstractionAAMP::UpdateProfileBasedOnFragmentDownloaded(void)
{
	// This function checks for bandwidth change based on the fragment url from FOG
	int desiredProfileIndex = 0;
	if (mCurrentBandwidth != mTsbBandwidth)
	{
		// a) Check if network bandwidth changed from starting bw
		// b) Check if netwwork bandwidth is different from persisted bandwidth( needed for first time reporting)
		// find the profile for the newbandwidth
		desiredProfileIndex = GetMediaTrack(eTRACK_VIDEO)->GetProfileIndexForBW(mTsbBandwidth);
		mCurrentBandwidth = mTsbBandwidth;
		StreamInfo* streamInfo = GetStreamInfo(profileIdxForBandwidthNotification);
		if (profileIdxForBandwidthNotification != desiredProfileIndex)
		{
			if(streamInfo != NULL)
			{
				profileIdxForBandwidthNotification = desiredProfileIndex;
				GetMediaTrack(eTRACK_VIDEO)->SetCurrentBandWidth( (int)streamInfo->bandwidthBitsPerSecond);
				mBitrateReason = eAAMP_BITRATE_CHANGE_BY_FOG_ABR;
			}
			else
			{
				AAMPLOG_WARN("GetStreamInfo is null");  //CID:84179 - Null Returns
			}
		}
	}
}

/**
 *  @brief Update rampdown or Up profile  reason
 */
void StreamAbstractionAAMP::UpdateRampUpOrDownProfileReason(void)
{
	mBitrateReason = eAAMP_BITRATE_CHANGE_BY_RAMPDOWN;
	if(mUpdateReason && aamp->IsFogTSBSupported())
	{
		mBitrateReason = eAAMP_BITRATE_CHANGE_BY_FOG_ABR;
		mUpdateReason = false;
	}
}

/**
 *  @brief Get Desired Profile based on Buffer availability
 */
void StreamAbstractionAAMP::GetDesiredProfileOnBuffer(int currProfileIndex, int &newProfileIndex)
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);

	double bufferValue = GetBufferValue(video);
	double minBufferNeeded ;
	if(bufferValue > 0)
	{
		if(aamp->GetLLDashServiceData()->lowLatencyMode)
		{
			minBufferNeeded	= mABRMinBuffer;
		}
		else
		{
			minBufferNeeded = video->fragmentDurationSeconds + aamp->mNetworkTimeoutMs/1000;
		}
		aamp->mhAbrManager.GetDesiredProfileOnBuffer(currProfileIndex,newProfileIndex,bufferValue,minBufferNeeded);
	}
	//When buffer goes zero, no need to ramp up - Switch directly to 0th profile ,inorder to build buffer
	else
	{
		AAMPLOG_WARN("Switch to index 0; buffer is about to drain :Buffer %lf !!",bufferValue);
		newProfileIndex = 0;
	}
}

/**
 *  @brief Get Desired Profile on steady state
 */
void StreamAbstractionAAMP::GetDesiredProfileOnSteadyState(int currProfileIndex, int &newProfileIndex, long nwBandwidth)
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	double bufferValue = GetBufferValue(video);
	//long currBandwidth = GetStreamInfo(currProfileIndex)->bandwidthBitsPerSecond;
	if(bufferValue > 0 && currProfileIndex == newProfileIndex)
	{
		AAMPLOG_INFO("buffer:%f currProf:%d nwBW:%ld",bufferValue,currProfileIndex,nwBandwidth);
		if(bufferValue > mABRMaxBuffer && !aamp->GetLLDashServiceData()->lowLatencyMode)
		{
			mABRHighBufferCounter++;
			mABRLowBufferCounter = 0 ;
			if(mABRHighBufferCounter > mMaxBufferCountCheck)
			{
				int nProfileIdx =  aamp->mhAbrManager.getRampedUpProfileIndex(currProfileIndex);
				long newBandwidth = GetStreamInfo(nProfileIdx)->bandwidthBitsPerSecond;
				HybridABRManager::BitrateChangeReason mhBitrateReason;
				mhBitrateReason = (HybridABRManager::BitrateChangeReason) mBitrateReason;
				aamp->mhAbrManager.CheckRampupFromSteadyState(currProfileIndex,newProfileIndex,nwBandwidth,bufferValue,newBandwidth,mhBitrateReason,mMaxBufferCountCheck);
				mBitrateReason = (BitrateChangeReason) mhBitrateReason;
				mABRHighBufferCounter = 0;
			}
		}
		// steady state ,with no ABR cache available to determine actual bandwidth
		// this state can happen due to timeouts
		// Adding delta check: When bandwidth is higher than currentprofile bandwidth but insufficient to download both audio and video simultaneously, a delta less than 2000 kbps indicates a need for steady state rampdown.
		if(bufferValue < mABRMinBuffer && !video->IsInjectionAborted())
		{
			if(aamp->GetLLDashServiceData()->lowLatencyMode || nwBandwidth == -1)
			{
				mABRLowBufferCounter++;
				mABRHighBufferCounter = 0;
				HybridABRManager::BitrateChangeReason mhBitrateReason;
				mhBitrateReason = (HybridABRManager::BitrateChangeReason) mBitrateReason;
				aamp->mhAbrManager.CheckRampdownFromSteadyState(currProfileIndex,newProfileIndex,mhBitrateReason,mABRLowBufferCounter);
				mBitrateReason = (BitrateChangeReason) mhBitrateReason;
				mABRLowBufferCounter = (mABRLowBufferCounter >= mABRBufferCounter)? 0 : mABRLowBufferCounter ;
			}
		}
	}
	else
	{
		mABRLowBufferCounter = 0 ;
		mABRHighBufferCounter = 0;
	}
}

/**
 *  @brief Configure download timeouts based on buffer
 */
void StreamAbstractionAAMP::ConfigureTimeoutOnBuffer()
{
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);

	if(video && video->enabled)
	{
		// If buffer is high , set high timeout , not to fail the download
		// If buffer is low , set timeout less than the buffer availability
		double vBufferDuration = video->GetBufferedDuration();
		if(vBufferDuration > 0)
		{
			int timeoutMs = vBufferDuration*1000;
			if(vBufferDuration < mABRMaxBuffer)
			{
				timeoutMs = aamp->mNetworkTimeoutMs;
			}
			else
			{	// enough buffer available
				timeoutMs = std::min(timeoutMs/2, mABRMaxBuffer*1000 );
				timeoutMs = std::max(timeoutMs , aamp->mNetworkTimeoutMs);
			}
			aamp->SetCurlTimeout(timeoutMs,eCURLINSTANCE_VIDEO);
			AAMPLOG_INFO("Setting Video timeout to :%d %f",timeoutMs,vBufferDuration);
		}
	}
	if(audio && audio->enabled)
	{
		// If buffer is high , set high timeout , not to fail the download
		// If buffer is low , set timeout less than the buffer availability
		double aBufferDuration = audio->GetBufferedDuration();
		if(aBufferDuration > 0)
		{
			int timeoutMs = aBufferDuration*1000;
			if(aBufferDuration < mABRMaxBuffer)
			{
				timeoutMs = aamp->mNetworkTimeoutMs;
			}
			else
			{
				timeoutMs = std::min(timeoutMs/2, mABRMaxBuffer*1000 );
				timeoutMs = std::max(timeoutMs , aamp->mNetworkTimeoutMs);
			}
			aamp->SetCurlTimeout(timeoutMs,eCURLINSTANCE_AUDIO);
			AAMPLOG_INFO("Setting Audio timeout to :%d %f",timeoutMs,aBufferDuration);
		}
	}
}


/**
 *  @brief Update rampdown profile on network failure
 */
double StreamAbstractionAAMP::GetBufferValue(MediaTrack *track)
{
	double bufferValue = 0.0;
	if (track)
	{
		bufferValue = track->GetBufferedDuration();
		if (aamp->IsLocalAAMPTsb() && track->IsLocalTSBInjection()) /**< Update buffer value based on manifest endDelta if it is LOCAL TSB LLD playback*/
		{
			AampTSBSessionManager *tsbSessionManager = aamp->GetTSBSessionManager();
			if(tsbSessionManager)
			{
				double manifestEndDelta = tsbSessionManager->GetManifestEndDelta();
				bufferValue = (manifestEndDelta + aamp->mLiveOffset); /**< Buffer should be calculated from live offset*/
				bufferValue += track->fragmentDurationSeconds; /**< Adjust with last fragment; One fragment may be downloading and not yet completed*/
				AAMPLOG_INFO("Inverse Buffer (%.02lf)sec based on TSB end point delta (%.02lf)sec and live offset (%.02lf)sec and fragmentDuration for adjust (%.02lf)sec !!",
							 bufferValue, manifestEndDelta, aamp->mLiveOffset, track->fragmentDurationSeconds);
				if(bufferValue < 0) /** Correct the inverse buffer; it may become -ve*/
				{
					bufferValue = 0;
				}
			}
			else
			{
				AAMPLOG_ERR("tsbSessionManager is NULL for LocalTSB!! Returning buffer value as %.02lf !!",bufferValue);
			}
		}
	}
	else
	{
		AAMPLOG_WARN("Video is NULL!! Returning buffer value as %.02lf !!",bufferValue);
	}
	return bufferValue;
}

/**
 *  @brief Get desired profile based on cached duration
 */
int StreamAbstractionAAMP::GetDesiredProfileBasedOnCache(void)
{
	int desiredProfileIndex = currentProfileIndex;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if(video != NULL)
	{
		double bufferValue = GetBufferValue(video);
		if(aamp->GetLLDashServiceData()->lowLatencyMode && bufferValue < AAMP_LLDABR_MIN_BUFFER_VALUE && !video->IsLocalTSBInjection())
		{
			//InsufficientBufferRule: Buffer is empty
			AAMPLOG_WARN("Switch to index 0; buffer is about to drain :Buffer %lf !!",bufferValue);
			desiredProfileIndex = 0;
			if(currentProfileIndex != desiredProfileIndex)
			{
				mBitrateReason = eAAMP_BITRATE_CHANGE_BY_BUFFER_EMPTY;
			}
			return desiredProfileIndex;
		}
		if (this->UseIframeTrack())
		{
			int tmpIframeProfile = GetIframeTrack();
			if(tmpIframeProfile != ABRManager::INVALID_PROFILE)
			{
				if (currentProfileIndex != tmpIframeProfile)
				{
					mBitrateReason = eAAMP_BITRATE_CHANGE_BY_ABR;
				}
				desiredProfileIndex = tmpIframeProfile;
			}
		}
		/*In live, fog takes care of ABR, and cache updating is not based only on bandwidth,
		 * but also depends on fragment availability in CDN*/
		else
		{
			long currentBandwidth = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
			long networkBandwidth = aamp->GetCurrentlyAvailableBandwidth();
			int nwConsistencyCnt = (mNwConsistencyBypass)?1:mABRNwConsistency;
			if(aamp->GetLLDashServiceData()->lowLatencyMode)
			{
				/** Avoid bypass for LLD so that buffer can be build up*/
				nwConsistencyCnt = mABRNwConsistency;
			}

			// Ramp up/down (do ABR)
			desiredProfileIndex = aamp->mhAbrManager.getProfileIndexByBitrateRampUpOrDown(currentProfileIndex,
																						  currentBandwidth, networkBandwidth, nwConsistencyCnt);
			AAMP_LogLevel logLevel = eLOGLEVEL_INFO;
			if(aamp->IsTuneTypeNew)
			{
				logLevel = eLOGLEVEL_MIL;
			}

			AAMPLOG(logLevel,"currBW:%ld NwBW=%ld currProf:%d desiredProf:%d ,Buffer:%lf",currentBandwidth,networkBandwidth,currentProfileIndex,desiredProfileIndex,bufferValue);

			if (currentProfileIndex != desiredProfileIndex)
			{
				// There is a chance that desiredProfileIndex is reset in below GetDesiredProfileOnBuffer call
				// Since bitrate notification will not be triggered in this case, its fine
				mBitrateReason = eAAMP_BITRATE_CHANGE_BY_ABR;
			}
			if(!mNwConsistencyBypass && ISCONFIGSET(eAAMPConfig_ABRBufferCheckEnabled))
			{
				// Checking if frequent profile change happening
				if(currentProfileIndex != desiredProfileIndex)
				{
					GetDesiredProfileOnBuffer(currentProfileIndex, desiredProfileIndex);
				}

				// Now check for Fixed BitRate for longer time(valley)
				GetDesiredProfileOnSteadyState(currentProfileIndex, desiredProfileIndex, networkBandwidth);

				// After ABR is done , next configure the timeouts for next downloads based on buffer
				ConfigureTimeoutOnBuffer();
			}
		}
		// only for first call, consistency check is ignored
		mNwConsistencyBypass = false;
	}
	else
	{
		AAMPLOG_WARN("video is null");  //CID:84160 - Null Returns
	}
	return desiredProfileIndex;
}


/**
 *  @brief Rampdown profile
 */
bool StreamAbstractionAAMP::RampDownProfile(int http_error)
{
	bool ret = false;
	int desiredProfileIndex = currentProfileIndex;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if (this->UseIframeTrack())
	{
		//We use only second last and lowest profiles for iframes
		int lowestIframeProfile = aamp->mhAbrManager.getLowestIframeProfile();
		if (ABRManager::INVALID_PROFILE != lowestIframeProfile)
		{
			desiredProfileIndex = lowestIframeProfile;
		}
		else
		{
			AAMPLOG_WARN("lowestIframeProfile Invalid - Stream does not has an iframe track!! ");
		}
	}
	else if (video)
	{
		double bufferValue = GetBufferValue(video);
		// Let's keep things simple! This function is invoked when we want to rampdown, which we could either do in single or multiple steps
		// If buffer is high, rampdown in single steps
		// If buffer is less, rampdown in multiple steps based on buffer available
		// If buffer is zero, we can't rampdown in multiple steps and the only way is either:
		// 1. Rampdown to the lowest profile directly (if this also fails, will lead to playback failure or skipped content)
		// 2. Rampdown in single steps (here we're already rebuffering or not yet streaming)
		// Recommend option 2, unless good reason is found to rampdown to lowest profile directly.
		if (bufferValue == 0 || bufferValue > mABRMaxBuffer)
		{
			// Rampdown in single steps
			desiredProfileIndex = aamp->mhAbrManager.getRampedDownProfileIndex(currentProfileIndex);
		}
		else if (bufferValue > 0)
		{
			// If buffer is available, rampdown based on buffer
			long desiredBw = aamp->mhAbrManager.FragmentfailureRampdown(bufferValue, currentProfileIndex);
			if (desiredBw > 0)
			{
				desiredProfileIndex = GetProfileIndexForBandwidth(desiredBw);
			}
			else
			{
				AAMPLOG_ERR("desiredBw received is 0, which is not expected.. Rampdown failed!!");
			}
		}
	}
	if (desiredProfileIndex != currentProfileIndex)
	{
		AAMPAbrInfo stAbrInfo = {};

		stAbrInfo.abrCalledFor = AAMPAbrFragmentDownloadFailed;
		stAbrInfo.currentProfileIndex = currentProfileIndex;
		stAbrInfo.desiredProfileIndex = desiredProfileIndex;
		StreamInfo* streamInfodesired = GetStreamInfo(desiredProfileIndex);
		StreamInfo* streamInfocurrent = GetStreamInfo(currentProfileIndex);
		if((streamInfocurrent != NULL) && (streamInfodesired != NULL))   //CID:160715 - Forward null
		{
			stAbrInfo.currentBandwidth = streamInfocurrent->bandwidthBitsPerSecond;
			stAbrInfo.desiredBandwidth = streamInfodesired->bandwidthBitsPerSecond;
			stAbrInfo.networkBandwidth = aamp->GetCurrentlyAvailableBandwidth();
			stAbrInfo.errorType = AAMPNetworkErrorHttp;
			stAbrInfo.errorCode = http_error;

			AampLogManager::LogABRInfo(&stAbrInfo);

			aamp->UpdateVideoEndMetrics(stAbrInfo);

			if(ISCONFIGSET(eAAMPConfig_ABRBufferCheckEnabled))
			{
				// After Rampdown, configure the timeouts for next downloads based on buffer
				ConfigureTimeoutOnBuffer();
			}

			this->currentProfileIndex = desiredProfileIndex;
			profileIdxForBandwidthNotification = desiredProfileIndex;
			AAMPLOG_DEBUG(" profileIdxForBandwidthNotification updated to %d ",  profileIdxForBandwidthNotification);
			ret = true;
			long newBW = GetStreamInfo(profileIdxForBandwidthNotification)->bandwidthBitsPerSecond;
			if(video)
			{
				video->SetCurrentBandWidth( (int)newBW );
				aamp->ResetCurrentlyAvailableBandwidth(newBW,false,profileIdxForBandwidthNotification);
				mBitrateReason = eAAMP_BITRATE_CHANGE_BY_RAMPDOWN;

				// Send abr notification
				video->ABRProfileChanged();
				mABRLowBufferCounter = 0 ;
				mABRHighBufferCounter = 0;
			}
			else
			{
				AAMPLOG_WARN("Video is null");
			}
		}
		else
		{
			AAMPLOG_WARN("GetStreamInfo is null");  //CID:84132 - Null Returns
		}
	}

	return ret;
}

/**
 *  @brief Check whether the current profile is lowest.
 */
bool StreamAbstractionAAMP::IsLowestProfile(int currentProfileIndex)
{
	bool ret = false;

	if (UseIframeTrack())
	{
		if (currentProfileIndex == aamp->mhAbrManager.getLowestIframeProfile())
		{
			ret = true;
		}
	}
	else
	{
		ret = aamp->mhAbrManager.isProfileIndexBitrateLowest(currentProfileIndex);
	}

	return ret;
}

/**
 *  @brief Convert custom curl errors to original
 */
int StreamAbstractionAAMP::getOriginalCurlError(int http_error)
{
	int ret = http_error;

	if (http_error >= PARTIAL_FILE_CONNECTIVITY_AAMP && http_error <= PARTIAL_FILE_START_STALL_TIMEOUT_AAMP)
	{
		if (http_error == OPERATION_TIMEOUT_CONNECTIVITY_AAMP)
		{
			ret = CURLE_OPERATION_TIMEDOUT;
		}
		else
		{
			ret = CURLE_PARTIAL_FILE;
		}
	}

	// return original error code
	return ret;
}


/**
 *  @brief Check for rampdown profile.
 */
bool StreamAbstractionAAMP::CheckForRampDownProfile(int http_error)
{
	bool retValue = false;

	if (!aamp->CheckABREnabled())
	{
		return retValue;
	}

	// If lowest profile reached, then no need to check for ramp up/down for timeout cases, instead skip the failed fragment and jump to next fragment to download.
	if (GetABRMode() == ABRMode::ABR_MANAGER && !IsLowestProfile(currentProfileIndex))
	{
		http_error = getOriginalCurlError(http_error);

		if (http_error == 404 || http_error == 403 || http_error == 500 || http_error == 503 || http_error == CURLE_PARTIAL_FILE)
		{
			if (RampDownProfile(http_error))
			{
				AAMPLOG_INFO("StreamAbstractionAAMP: Condition Rampdown Success");
				retValue = true;
			}
		}
		// For timeout, rampdown in single steps might not be enough
		else if (http_error == CURLE_OPERATION_TIMEDOUT)
		{
			if (UpdateProfileBasedOnFragmentCache())
			{
				retValue = true;
			}
			else if (RampDownProfile(http_error))
			{
				retValue = true;
			}
		}
	}

	if ((true == retValue) && (mRampDownLimit > 0))
	{
		mRampDownCount++;
	}

	return retValue;
}

/**
 *  @brief Checks and update profile based on bandwidth.
 */
void StreamAbstractionAAMP::CheckForProfileChange(void)
{
	switch (GetABRMode())
	{
		case ABRMode::FOG_TSB:
			// This is for FOG based download , where bandwidth is calculated based on downloaded fragment file name
			// No profile change will be done or manifest download triggered based on profilechange
			UpdateProfileBasedOnFragmentDownloaded();
			break;

		case ABRMode::ABR_MANAGER:
		{
			MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
			if(video != NULL)
			{
				UpdateProfileBasedOnFragmentCache();
			}
			else
			{
				AAMPLOG_WARN("Video is null");  //CID:82070 - Null Returns
			}
			break;
		}

		default:
			AAMPLOG_ERR("ABR mode not supported");
			break;
	}
}

/**
 *  @brief Get iframe track index.
 *         This shall be called only after UpdateIframeTracks() is done
 */
int StreamAbstractionAAMP::GetIframeTrack()
{
	return aamp->mhAbrManager.getDesiredIframeProfile();
}

/**
 *   @brief Update iframe tracks.
 *          Subclasses shall invoke this after StreamInfo is populated .
 */
void StreamAbstractionAAMP::UpdateIframeTracks()
{
	aamp->mhAbrManager.updateProfile();
}


/**
 *  @brief Function called when playback is paused to update related flags.
 */
void StreamAbstractionAAMP::NotifyPlaybackPaused(bool paused)
{
	std::lock_guard<std::mutex> guard(mLock);
	mIsPaused = paused;
	if (paused)
	{
		mIsAtLivePoint = false;
		mLastPausedTimeStamp = aamp_GetCurrentTimeMS();
	}
	else
	{
		if(-1 != mLastPausedTimeStamp)
		{
			mTotalPausedDurationMS += (aamp_GetCurrentTimeMS() - mLastPausedTimeStamp);
			mLastPausedTimeStamp = -1;
		}
		else
		{
			AAMPLOG_WARN("StreamAbstractionAAMP: mLastPausedTimeStamp -1");
		}
	}
}


/**
 *  @brief Check if player caches are running dry.
 */
bool StreamAbstractionAAMP::CheckIfPlayerRunningDry()
{
	MediaTrack *videoTrack = GetMediaTrack(eTRACK_VIDEO);
	MediaTrack *audioTrack = GetMediaTrack(eTRACK_AUDIO);

	if (!audioTrack || !videoTrack)
	{
		return false;
	}
	bool videoBufferIsEmpty = videoTrack->numberOfFragmentsCached == 0 && aamp->IsSinkCacheEmpty(eMEDIATYPE_VIDEO);
	bool audioBufferIsEmpty = (audioTrack->Enabled() ? (audioTrack->numberOfFragmentsCached == 0) : true) && aamp->IsSinkCacheEmpty(eMEDIATYPE_AUDIO);
	if (videoBufferIsEmpty || audioBufferIsEmpty) /* Changed the condition from '&&' to '||', because if video getting stalled it doesn't need to wait until audio become dry */
	{
		AAMPLOG_WARN("StreamAbstractionAAMP: Stall detected. Buffer status is RED!");
		return true;
	}
	return false;
}

/**
 *  @brief Update profile based on fragment cache.
 */
bool StreamAbstractionAAMP::UpdateProfileBasedOnFragmentCache()
{
	bool retVal = false;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	int desiredProfileIndex = currentProfileIndex;
	double totalFetchedDuration = video->GetTotalFetchedDuration();
	long availBW = aamp->GetCurrentlyAvailableBandwidth();
	bool checkProfileChange = aamp->mhAbrManager.CheckProfileChange(totalFetchedDuration,currentProfileIndex,availBW);
	//For LLD, it's necessary to initiate a rampdown process when there is a consistent download delay in order to construct the buffer.
	if (aamp->GetLLDashServiceData()->lowLatencyMode && !checkProfileChange && (aamp->mDownloadDelay >= (int)(floor(aamp->mLiveOffset / 2))))
	{
		desiredProfileIndex = aamp->mhAbrManager.getRampedDownProfileIndex(currentProfileIndex);
		AAMPLOG_INFO("ProfileChange Due to Download Delay %lf totalFetchedDuration ,%d aamp->mDownloadDelay %lf aamp->mLiveOffset %d desiredProfileIndex",totalFetchedDuration,aamp->mDownloadDelay,aamp->mLiveOffset,desiredProfileIndex);
		aamp->mDownloadDelay = 0;
	}

	if(checkProfileChange)
	{
		desiredProfileIndex = GetDesiredProfileBasedOnCache();
	}

	if (desiredProfileIndex != currentProfileIndex)
	{
#if 0 /* Commented since the same is supported via AampLogManager::LogABRInfo */
		AAMPLOG_WARN("**aamp changing profile: %d->%d [%ld->%ld]",
					 currentProfileIndex, desiredProfileIndex,
					 GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond,
					 GetStreamInfo(desiredProfileIndex)->bandwidthBitsPerSecond);
#else
		AAMPAbrInfo stAbrInfo = {};

		stAbrInfo.abrCalledFor = AAMPAbrBandwidthUpdate;
		stAbrInfo.currentProfileIndex = currentProfileIndex;
		stAbrInfo.desiredProfileIndex = desiredProfileIndex;
		stAbrInfo.currentBandwidth = GetStreamInfo(currentProfileIndex)->bandwidthBitsPerSecond;
		stAbrInfo.desiredBandwidth = GetStreamInfo(desiredProfileIndex)->bandwidthBitsPerSecond;
		stAbrInfo.networkBandwidth = aamp->GetCurrentlyAvailableBandwidth();
		stAbrInfo.errorType = AAMPNetworkErrorNone;

		AampLogManager::LogABRInfo(&stAbrInfo);
		aamp->UpdateVideoEndMetrics(stAbrInfo);
#endif /* 0 */

		this->currentProfileIndex = desiredProfileIndex;
		profileIdxForBandwidthNotification = desiredProfileIndex;
		AAMPLOG_DEBUG(" profileIdxForBandwidthNotification updated to %d ",  profileIdxForBandwidthNotification);
		video->ABRProfileChanged();
		long newBW = GetStreamInfo(profileIdxForBandwidthNotification)->bandwidthBitsPerSecond;
		video->SetCurrentBandWidth((int)newBW);
		aamp->ResetCurrentlyAvailableBandwidth(newBW,false,profileIdxForBandwidthNotification);
		mABRLowBufferCounter = 0 ;
		mABRHighBufferCounter = 0;
		retVal = true;
	}
	else
	{
		/* No profile change. */
	}

	return retVal;
}

/**
 *  @brief Check if playback has stalled and update related flags.
 */
void StreamAbstractionAAMP::CheckForPlaybackStall(bool fragmentParsed)
{
	if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
	{
		return;
	}
	if (fragmentParsed)
	{
		mLastVideoFragParsedTimeMS = aamp_GetCurrentTimeMS();
		if (mIsPlaybackStalled)
		{
			mIsPlaybackStalled = false;
		}
	}
	else
	{
		/** Need to confirm if we are stalled here */
		double timeElapsedSinceLastFragment = (aamp_GetCurrentTimeMS() - mLastVideoFragParsedTimeMS);

		// We have not received a new fragment for a long time, check for cache empty required for dash
		MediaTrack* mediatrack = GetMediaTrack(eTRACK_VIDEO);
		if(mediatrack != NULL)
		{
			int stalltimeout = GETCONFIGVALUE(eAAMPConfig_StallTimeoutMS);
			if (!mNetworkDownDetected && (timeElapsedSinceLastFragment > stalltimeout) && mediatrack->numberOfFragmentsCached == 0)
			{
				AAMPLOG_INFO("StreamAbstractionAAMP: Didn't download a new fragment for a long time(%f) and cache empty!", timeElapsedSinceLastFragment);
				mIsPlaybackStalled = true;
				if (CheckIfPlayerRunningDry())
				{
					AAMPLOG_WARN("StreamAbstractionAAMP: Stall detected!. Time elapsed since fragment parsed(%f), caches are all empty!", timeElapsedSinceLastFragment);
					aamp->SetFlushFdsNeededInCurlStore(true);
					aamp->SendStalledErrorEvent();
				}
			}
		}
		else
		{
			AAMPLOG_WARN("GetMediaTrack  is null");  //CID:85383 - Null Returns
		}
	}
}


/**
 *  @brief MediaTracks shall call this to notify first fragment is injected.
 */
void StreamAbstractionAAMP::NotifyFirstFragmentInjected()
{
	std::lock_guard<std::mutex> guard(mLock);
	mIsPaused = false;
	mLastPausedTimeStamp = -1;
	mTotalPausedDurationMS = 0;
	mStartTimeStamp = aamp_GetCurrentTimeMS();
}

/**
 *  @brief Get elapsed time of play-back.
 */
double StreamAbstractionAAMP::GetElapsedTime()
{
	double elapsedTime;
	std::lock_guard<std::mutex> guard(mLock);
	AAMPLOG_DEBUG("StreamAbstractionAAMP:mStartTimeStamp %lld mTotalPausedDurationMS %lld mLastPausedTimeStamp %lld", mStartTimeStamp, mTotalPausedDurationMS, mLastPausedTimeStamp);
	if (!mIsPaused)
	{
		elapsedTime = (double)(aamp_GetCurrentTimeMS() - mStartTimeStamp - mTotalPausedDurationMS) / 1000;
	}
	else
	{
		elapsedTime = (double)(mLastPausedTimeStamp - mStartTimeStamp - mTotalPausedDurationMS) / 1000;
	}
	return elapsedTime;
}

/**
 *  @brief Get the bitrate of current video profile selected.
 */
BitsPerSecond StreamAbstractionAAMP::GetVideoBitrate(void)
{
	BitsPerSecond bitrate = 0;
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);

	if (video && video->enabled)
	{
		AampTSBSessionManager* tsbSessionManager = aamp->GetTSBSessionManager();
		if (video->IsLocalTSBInjection() && tsbSessionManager)
		{
			// Return the video bitrate from TSB session manager
			bitrate = tsbSessionManager->GetVideoBitrate();
		}
		else
		{
			bitrate = video->GetCurrentBandWidth();
		}
	}
	return bitrate;
}

/**
 *  @brief Get the bitrate of current audio profile selected.
 */
BitsPerSecond StreamAbstractionAAMP::GetAudioBitrate(void)
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	return ((audio && audio->enabled) ? (audio->GetCurrentBandWidth()) : 0);
}


/**
 *  @brief Check if current stream is muxed
 */
bool StreamAbstractionAAMP::IsMuxedStream()
{
	bool ret = false;

	if ((!ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback)) && (AAMP_NORMAL_PLAY_RATE == aamp->rate))
	{
		MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
		MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
		if (!audio || !video || !audio->enabled || !video->enabled)
		{
			ret = true;
		}
	}
	return ret;
}

/**
 *   @brief Set AudioTrack info from Muxed stream
 *
 *   @param[in] vector AudioTrack info
 *   @return void
 */
void StreamAbstractionAAMP::SetAudioTrackInfoFromMuxedStream(std::vector<AudioTrackInfo>& vector)
{
	if( vector.size()>0 )
	{ // copy track info from mpegts PMT
		mAudioTracks = vector;
		aamp->NotifyAudioTracksChanged();
	}
}


/**
 *   @brief Waits subtitle track injection until caught up with muxed/audio track.
 *          Used internally by injection logic
 */
void StreamAbstractionAAMP::WaitForAudioTrackCatchup()
{
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	if (audio && !audio->enabled)
	{ // muxed a/v
		audio = GetMediaTrack(eTRACK_VIDEO);
	}
	if( audio && subtitle )
	{
		std::unique_lock<std::mutex> lock(mLock);
		double audioDuration = audio->GetTotalInjectedDuration();
		double subtitleDuration = subtitle->GetTotalInjectedDuration();
		//Allow subtitles to be ahead by 15 seconds compared to audio
		while ((subtitleDuration > (audioDuration + audio->fragmentDurationSeconds + 15.0)) && aamp->DownloadsAreEnabled() && !subtitle->IsDiscontinuityProcessed() && !audio->IsInjectionAborted())
		{
			AAMPLOG_DEBUG("Blocked on Inside mSubCond with sub:%f and audio:%f", subtitleDuration, audioDuration);
			if (std::cv_status::no_timeout == mSubCond.wait_for(lock, std::chrono::milliseconds(100)))
			{
				break;
			}
			audioDuration = audio->GetTotalInjectedDuration();
			subtitleDuration = subtitle->GetTotalInjectedDuration();
		}
	}
}

/**
 *  @brief Unblock subtitle track injector if downloads are stopped
 */
void StreamAbstractionAAMP::AbortWaitForAudioTrackCatchup(bool force)
{
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	if (subtitle && subtitle->enabled)
	{
		std::lock_guard<std::mutex> guard(mLock);
		if (force || !aamp->DownloadsAreEnabled())
		{
			mSubCond.notify_one();
		}
	}
}

/**
 *  @brief Send a MUTE/UNMUTE packet to the subtitle renderer
 */
void StreamAbstractionAAMP::MuteSubtitles(bool mute)
{
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		subtitle->mSubtitleParser->mute(mute);
	}
}

/**
 *  @brief Checks if streamer reached end of stream
 */
bool StreamAbstractionAAMP::IsEOSReached()
{
	bool eos = true;
	if(!aamp->IsLocalAAMPTsb())
	{
		for (int i = 0 ; i < AAMP_TRACK_COUNT; i++)
		{
			// For determining EOS we will Ignore the subtitle track
			if ((TrackType)i == eTRACK_SUBTITLE)
			{
				continue;
			}

			MediaTrack *track = GetMediaTrack((TrackType) i);
			if (track && track->enabled)
			{
				eos = eos && track->IsAtEndOfTrack();
				if (!eos)
				{
					AAMPLOG_WARN("EOS not seen by track: %s, skip check for rest of the tracks", track->name);
					aamp->ResetEOSSignalledFlag();
					break;
				}
			}
		}
	}
	else
	{
		AampTSBSessionManager* tsbSessionManager = aamp->GetTSBSessionManager();
		if(tsbSessionManager)
		{
			for (int i = 0 ; i < AAMP_TRACK_COUNT; i++)
			{
				// For determining EOS we will Ignore the subtitle track
				if ((TrackType)i == eTRACK_SUBTITLE)
					continue;
				// Implement Track enabled and proper EOS logic for Discontinuities
			}
		}
	}
	return eos;
}

/**
 *  @brief Function to returns last injected fragment position
 */
double StreamAbstractionAAMP::GetLastInjectedFragmentPosition()
{
	// We get the position of video, we use video position for most of our position related things
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	double pos = 0;
	if (video)
	{
		pos = video->GetTotalInjectedDuration();
	}
	AAMPLOG_INFO("Last Injected fragment Position : %f", pos);
	return pos;
}

/**
 * @brief Set local AAMP TSB injection flag
 */
void MediaTrack::SetLocalTSBInjection(bool value)
{
	mIsLocalTSBInjection.store(value);
	AAMPLOG_INFO("isLocalAampTsbInjection %d", mIsLocalTSBInjection.load());
}

/**
 * @brief Is injection from local AAMP TSB
 *
 * @return true if injection is from local AAMP TSB, false otherwise
 */
bool MediaTrack::IsLocalTSBInjection()
{
	return mIsLocalTSBInjection.load();
}

/**
 * @brief Function to Resume track downloader
 */
void StreamAbstractionAAMP::ResumeTrackDownloadsHandler( )
{
	aamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
}

/**
 * @brief Function to Stop track downloader
 */
void StreamAbstractionAAMP::StopTrackDownloadsHandler( )
{
	aamp->StopTrackDownloads(eMEDIATYPE_SUBTITLE);
}

/**
 * @brief Function to Send VTT Cue Data as event
 */
void StreamAbstractionAAMP::SendVTTCueDataHandler(VTTCue* cueData)
{
	aamp->SendVTTCueDataAsEvent(cueData);
}

/**
 * @brief Function to Get the seek position current playback position in seconds
 */
void StreamAbstractionAAMP::GetPlayerPositionsHandler(long long& getPositionMS, double& seekPositionSeconds)
{
    getPositionMS = aamp->GetPositionMs();
    seekPositionSeconds = aamp->seek_pos_seconds;
}

/**
 * @brief Function to initialize the player related callbacks
 */
void StreamAbstractionAAMP::InitializePlayerCallbacks(PlayerCallbacks& callbacks)
{
	callbacks.resumeTrackDownloads_CB = std::bind(&StreamAbstractionAAMP::ResumeTrackDownloadsHandler, this);
	callbacks.stopTrackDownloads_CB = std::bind(&StreamAbstractionAAMP::StopTrackDownloadsHandler, this);
	callbacks.sendVTTCueData_CB = std::bind(&StreamAbstractionAAMP::SendVTTCueDataHandler, this, std::placeholders::_1);
	callbacks.getPlayerPositions_CB = std::bind(&StreamAbstractionAAMP::GetPlayerPositionsHandler, this, std::placeholders::_1, std::placeholders::_2);
}

/**
 * @brief Function to initialize the create subtitle parser instance & player related callbacks
 */
std::unique_ptr<SubtitleParser> StreamAbstractionAAMP::RegisterSubtitleParser_CB(std::string mimeType, bool isExpectedMimeType)
{
	SubtitleMimeType type = eSUB_TYPE_UNKNOWN;

	AAMPLOG_INFO("RegisterSubtitleParser_CB: mimeType %s", mimeType.c_str());

	if (!mimeType.compare("text/vtt"))
		type = eSUB_TYPE_WEBVTT;
	else if (!mimeType.compare("application/ttml+xml") ||
			!mimeType.compare("application/mp4"))
		type = eSUB_TYPE_TTML;

	return RegisterSubtitleParser_CB(type, isExpectedMimeType);
}

/**
 * @brief Function to initialize the create subtitle parser instance & player related callbacks
 */
std::unique_ptr<SubtitleParser> StreamAbstractionAAMP::RegisterSubtitleParser_CB(SubtitleMimeType mimeType, bool isExpectedMimeType) {
    int width = 0, height = 0;
    bool webVTTCueListenersRegistered = false, isWebVTTNativeConfigured = false, resumeTrackDownload = false;
    PlayerCallbacks playerCallBack = {};

	if(isExpectedMimeType)
	{
		webVTTCueListenersRegistered = aamp->WebVTTCueListenersRegistered();
		isWebVTTNativeConfigured = ISCONFIGSET(eAAMPConfig_WebVTTNative);
	}

    this->InitializePlayerCallbacks(playerCallBack);
    aamp->GetPlayerVideoSize(width, height);

    std::unique_ptr<SubtitleParser> subtitleParser = SubtecFactory::createSubtitleParser(mimeType, width, height, webVTTCueListenersRegistered, isWebVTTNativeConfigured, resumeTrackDownload);
    if (subtitleParser) {
        subtitleParser->RegisterCallback(playerCallBack);
        if (resumeTrackDownload) {
            aamp->ResumeTrackDownloads(eMEDIATYPE_SUBTITLE);
        }
    }
    return subtitleParser;
}

/**
 *  @brief To check for discontinuity in future fragments.
 */
bool MediaTrack::CheckForFutureDiscontinuity(double &cachedDuration)
{
	bool ret = false;
	cachedDuration = 0;
	int index = 0;
	int count = 0;
	int maxFrags = 0;
	CachedFragment *pCachedFragment = NULL;

	std::lock_guard<std::mutex> guard(mutex);

	if (IsInjectionFromCachedFragmentChunks())
	{
		index = fragmentChunkIdxToInject;
		count = numberOfFragmentChunksCached;
		maxFrags = maxCachedFragmentChunksPerTrack;
		pCachedFragment = mCachedFragmentChunks;
	}
	else
	{
		index = fragmentIdxToInject;
		count = numberOfFragmentsCached;
		maxFrags = maxCachedFragmentsPerTrack;
		pCachedFragment = mCachedFragment;
	}

	while (count > 0)
	{
		if (!ret)
		{
			ret = ret || pCachedFragment[index].discontinuity;
			if (ret)
			{
				AAMPLOG_WARN("Found discontinuity for track %s at index: %d and position - %f", name, index, pCachedFragment[index].position);
			}
		}
		cachedDuration += pCachedFragment[index].duration;
		if (++index == maxFrags)
		{
			index = 0;
		}
		count--;
	}
	AAMPLOG_WARN("track %s numberOfFragmentsCached - %d, cachedDuration - %f", name, IsInjectionFromCachedFragmentChunks() ? numberOfFragmentChunksCached : numberOfFragmentsCached, cachedDuration);

	return ret;
}

/**
 *  @brief Called if sink buffer is full
 */
void MediaTrack::OnSinkBufferFull()
{
	//check if we should stop initial caching here
	if(sinkBufferIsFull)
	{
		return;
	}

	bool notifyCacheCompleted = false;
	bool cachingCompletedFlag = false;
	{
		{
			std::lock_guard<std::mutex> guard(mutex);
			sinkBufferIsFull = true;
			cachingCompletedFlag = cachingCompleted;
		}
		
		// check if cache buffer is full and caching was needed
		if (IsFragmentCacheFull() && (eTRACK_VIDEO == type) &&
			aamp->IsFragmentCachingRequired() && !cachingCompletedFlag)
		{
			std::lock_guard<std::mutex> guard(mutex);
			AAMPLOG_WARN("## [%s] Cache is Full cacheDuration %d minInitialCacheSeconds %d, aborting caching!##",
						name, currentInitialCacheDurationSeconds, aamp->GetInitialBufferDuration());
			notifyCacheCompleted = true;
			cachingCompleted = true;
		}
	}

	if(notifyCacheCompleted)
	{
		aamp->NotifyFragmentCachingComplete();
	}
}

/**
 *  @brief Function to reset the paired discontinuity.
 */
void StreamAbstractionAAMP::resetDiscontinuityTrackState()
{
	mTrackState = eDISCONTINUITY_FREE;
}

/**
 *  @brief Function to process discontinuity.
 */
bool StreamAbstractionAAMP::ProcessDiscontinuity(TrackType type)
{
	bool ret = true;
	MediaTrackDiscontinuityState state = eDISCONTINUITY_FREE;
	bool isMuxedAndAudioDiscoIgnored = false;

	std::unique_lock<std::mutex> lock(mStateLock);
	if (type == eTRACK_VIDEO)
	{
		state = eDISCONTINUITY_IN_VIDEO;

		/*For muxed streams, give discontinuity for audio track as well*/
		MediaTrack* audio = GetMediaTrack(eTRACK_AUDIO);
		if (audio && !audio->enabled)
		{
			mTrackState = (MediaTrackDiscontinuityState) (mTrackState | eDISCONTINUITY_IN_BOTH);
			ret = aamp->Discontinuity(eMEDIATYPE_AUDIO, false);

			/* In muxed stream, if discontinuity-EOS processing for audio track failed, then set the "mProcessingDiscontinuity" flag of audio to true if video track discontinuity succeeded.
			 * In this case, no need to reset mTrackState by removing audio track, because need to process the video track discontinuity-EOS process since its a muxed stream.
			 */
			if (ret == false)
			{
				AAMPLOG_WARN("muxed track audio discontinuity/EOS processing ignored!");
				isMuxedAndAudioDiscoIgnored = true;
			}
		}
	}
	else if (type == eTRACK_AUDIO)
	{
		state = eDISCONTINUITY_IN_AUDIO;
	}
	// bypass discontinuity check for auxiliary audio for now
	else if (type == eTRACK_AUX_AUDIO)
	{
		aamp->Discontinuity(eMEDIATYPE_AUX_AUDIO, false);
	}
	else if (type == eTRACK_SUBTITLE)
	{
		ret=true;
	}

	if (state != eDISCONTINUITY_FREE)
	{
		bool aborted = false;
		bool wait = false;
		mTrackState = (MediaTrackDiscontinuityState) (mTrackState | state);

		AAMPLOG_MIL("mTrackState:%d!", mTrackState);

		if (mTrackState == state)
		{
			wait = true;
			AAMPLOG_MIL("track[%d] Going into wait for processing discontinuity in other track!", type);
			mStateCond.wait(lock);
			MediaTrack *track = GetMediaTrack(type);
			if (track && track->IsInjectionAborted())
			{
				//AbortWaitForDiscontinuity called, don't push discontinuity
				//Just exit with ret = true to avoid InjectFragmentInternal
				aborted = true;
			}
			else if (type == eTRACK_AUDIO)
			{
				//AbortWaitForDiscontinuity() will be triggered by video first, check video injection aborted
				MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
				if (video && video->IsInjectionAborted())
				{
					aborted = true;
				}
			}

			//Check if mTrackState was reset from CheckForMediaTrackInjectionStall
			if ((!ISCONFIGSET(eAAMPConfig_RetuneForUnpairDiscontinuity) || type == eTRACK_AUDIO) && (!aborted && ((mTrackState & state) != state)))
			{
				//Ignore discontinuity
				ret = false;
				aborted = true;
			}
		}
		AAMPLOG_MIL("track[%d] mTrackState:%d wait:%d aborted:%d", type, mTrackState, wait, aborted);
		// We can't ensure that mTrackState == eDISCONTINUITY_IN_BOTH after wait, because
		// if Discontinuity() returns false, we need to reset the track bit from mTrackState
		if (mTrackState == eDISCONTINUITY_IN_BOTH || (wait && !aborted))
		{
			lock.unlock();

			ret = aamp->Discontinuity((AampMediaType) type, false);
			//Discontinuity ignored, so we need to remove state from mTrackState
			if (ret == false)
			{
				mTrackState = (MediaTrackDiscontinuityState) (mTrackState & ~state);
				AAMPLOG_WARN("track:%d discontinuity processing ignored! reset mTrackState to: %d!", type, mTrackState);
				aamp->UnblockWaitForDiscontinuityProcessToComplete();
			}
			else if (isMuxedAndAudioDiscoIgnored && type == eTRACK_VIDEO)
			{
				// In muxed stream, set the audio track's mProcessingDiscontinuity flag to true to unblock the ProcessPendingDiscontinuity if video track discontinuity-EOS processing succeeded
				AAMPLOG_MIL("set muxed track audio discontinuity flag to true since video discontinuity processing succeeded.");
				aamp->Discontinuity((AampMediaType) eTRACK_AUDIO, true);
			}

			lock.lock();
			mStateCond.notify_one();
		}
	}

	return ret;
}

/**
 *  @brief Function to abort any wait for discontinuity by injector threads.
 */
void StreamAbstractionAAMP::AbortWaitForDiscontinuity()
{
	//Release injector thread blocked in ProcessDiscontinuity
	std::lock_guard<std::mutex> guard(mStateLock);
	mStateCond.notify_one();
}

/**
 *  @brief Function to check if any media tracks are stalled on discontinuity.
 */
void StreamAbstractionAAMP::CheckForMediaTrackInjectionStall(TrackType type)
{
	MediaTrackDiscontinuityState state = eDISCONTINUITY_FREE;
	MediaTrack *track = GetMediaTrack(type);
	MediaTrack *otherTrack = NULL;
	bool bProcessFlag = false;

	if (type == eTRACK_AUDIO)
	{
		otherTrack = GetMediaTrack(eTRACK_VIDEO);
		state = eDISCONTINUITY_IN_AUDIO;
	}
	else if (type == eTRACK_VIDEO)
	{
		otherTrack = GetMediaTrack(eTRACK_AUDIO);
		state = eDISCONTINUITY_IN_VIDEO;
	}

	// If both tracks are available and enabled, then only check required
	if (track && track->enabled && otherTrack && otherTrack->enabled)
	{
		std::lock_guard<std::mutex> guard(mStateLock);
		if (mTrackState == eDISCONTINUITY_IN_VIDEO || mTrackState == eDISCONTINUITY_IN_AUDIO)
		{
			bool isDiscontinuitySeen = mTrackState & state;
			if (isDiscontinuitySeen)
			{
				double cachedDuration = 0;
				bool isDiscontinuityPresent;
				double duration = track->GetTotalInjectedDuration();
				double otherTrackDuration = otherTrack->GetTotalInjectedDuration();
				double diff = otherTrackDuration - duration;
				AAMPLOG_WARN("Discontinuity encountered in track:%d with injectedDuration:%f and other track injectedDuration:%f, other.fragmentDurationSeconds:%f, diff:%f",
							 type, duration, otherTrackDuration, otherTrack->fragmentDurationSeconds, diff);
				if (otherTrackDuration >= duration)
				{
					//Check for future discontinuity
					isDiscontinuityPresent = otherTrack->CheckForFutureDiscontinuity(cachedDuration);
					if (isDiscontinuityPresent)
					{
						//Scenario - video wait on discontinuity, and audio has a future discontinuity
						if (type == eTRACK_VIDEO)
						{
							AAMPLOG_WARN("For discontinuity in track:%d, other track has injectedDuration:%f and future discontinuity, signal mCond var!",
										 type, otherTrackDuration);
							std::lock_guard<std::mutex> guardLock(mLock);
							mCond.notify_one();
						}
					}
					// If discontinuity is not seen in future fragments or if the unblocked track has finished more than 2 * fragmentDurationSeconds,
					// unblock this track.
					// If the last downloaded content was an init fragment (likely due to an ABR change) the fragment duration  will be 0 before the
					// condition is checked. So the following condition may be falsely satisfied in that case, leading to ignoring discontinuity process
					// that is actually required.
					else if ((otherTrack->fragmentDurationSeconds > 0) && ((diff + cachedDuration) > (2 * otherTrack->fragmentDurationSeconds)))
					{
						AAMPLOG_WARN("Discontinuity in track:%d does not have a discontinuity in other track (diff: %f, injectedDuration: %f, cachedDuration: %f)",
									 type, diff, otherTrackDuration, cachedDuration);
						bProcessFlag = true;
					}
				}
				// Current track injected duration goes very huge value with the below cases
				// 1. When the EOS for earlier discontinuity missed to processing due to singular discontinuity or some edge case missing
				// 2. When there is no EOS processed message for the previous discontinuity seen from the pipeline.
				// In that case the diff value will go to negative and this CheckForMediaTrackInjectionStall() continuously called
				// until stall happens from outside or explicitly aamp_stop() to be called from XRE or Apps,
				// so need to control the stalling as soon as possible for the negative diff case from here.
				else if ((diff < 0) && (otherTrack->fragmentDurationSeconds > 0) && (abs(diff) > (2 * otherTrack->fragmentDurationSeconds)))
				{
					AAMPLOG_WARN("Discontinuity in track:%d does not have a discontinuity in other track (diff is negative: %f, injectedDuration: %f)",
								 type, diff, otherTrackDuration);
					(void)otherTrack->CheckForFutureDiscontinuity(cachedDuration); // called just to get the value of cachedDuration of the track.
					bProcessFlag = true;
				}

				if (bProcessFlag)
				{
					if (ISCONFIGSET(eAAMPConfig_RetuneForUnpairDiscontinuity) && type != eTRACK_AUDIO)
					{
						if(aamp->GetBufUnderFlowStatus())
						{
							AAMPLOG_WARN("Schedule retune since for discontinuity in track:%d other track doesn't have a discontinuity (diff: %f, injectedDuration: %f, cachedDuration: %f)",
										 type, diff, otherTrackDuration, cachedDuration);
							aamp->ScheduleRetune(eSTALL_AFTER_DISCONTINUITY, (AampMediaType) type);
						}
						else
						{
							//Check for PTS change for 1 second
							aamp->CheckForDiscontinuityStall((AampMediaType) type);
						}
					}
					else
					{
						AAMPLOG_WARN("Ignoring discontinuity in track:%d since other track doesn't have a discontinuity (diff: %f, injectedDuration: %f, cachedDuration: %f)",
									 type, diff, otherTrackDuration, cachedDuration);
						// special case handling for CDVR streams having SCTE signals
						// During the period transition, the audio track detected the discontinuity, but the Player didn’t detect discontinuity for the video track within the expected time frame.
						// So the Audio track is exiting from the discontinuity process due to singular discontinuity condition,
						// but after that, the video track encountered discontinuity and ended in a deadlock due to no more discontinuity in audio to match with it.
						if(aamp->IsDashAsset())
						{
							AAMPLOG_WARN("Ignoring discontinuity in DASH period for track:%d",type);
							aamp->SetTrackDiscontinuityIgnoredStatus((AampMediaType)type);
						}
						mTrackState = (MediaTrackDiscontinuityState) (mTrackState & ~state);
						mStateCond.notify_one();
					}
				}
			}
		}
	}
}

/**
 *  @brief Check for ramp down limit reached by player
 */
bool StreamAbstractionAAMP::CheckForRampDownLimitReached()
{
	bool ret = false;
	// Check rampdownlimit reached when the value is set,
	// limit will be -1 by default, function will return false to attempt rampdown.
	if ((mRampDownCount >= mRampDownLimit) && (mRampDownLimit >= 0))
	{
		ret = true;
		mRampDownCount = 0;
		AAMPLOG_WARN("Rampdown limit reached, Limit is %d", mRampDownLimit);
	}
	return ret;
}

/**
 *  @brief Get buffered video duration in seconds
 */
double StreamAbstractionAAMP::GetBufferedVideoDurationSec()
{
	double bufferValue = -1.0;
	// do not support trickplay track
	if(AAMP_NORMAL_PLAY_RATE != aamp->rate)
	{
		return bufferValue;
	}
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if(video)
	{
		bufferValue = GetBufferValue(video);
	}
	return bufferValue;
}

/**
 *  @brief Get buffered audio duration in seconds
 */
double StreamAbstractionAAMP::GetBufferedAudioDurationSec()
{
	double bufferValue = -1.0;
	// do not support trickplay track
	if(AAMP_NORMAL_PLAY_RATE != aamp->rate)
	{
		return bufferValue;
	}
	MediaTrack *audio = GetMediaTrack(eTRACK_AUDIO);
	if(audio)
	{
		bufferValue = GetBufferValue(audio);
	}
	return bufferValue;
}

/**
 *  @brief Get current audio track information
 */
bool StreamAbstractionAAMP::GetCurrentAudioTrack(AudioTrackInfo &audioTrack)
{
	bool bFound = false;
	if (!mAudioTrackIndex.empty())
	{
		for (auto it = mAudioTracks.begin(); it != mAudioTracks.end(); it++)
		{
			if (it->index == mAudioTrackIndex)
			{
				audioTrack = *it;
				bFound = true;
			}
		}
	}
	return bFound;
}


/**
 *   @brief Get current text track
 */
bool StreamAbstractionAAMP::GetCurrentTextTrack(TextTrackInfo &textTrack)
{
	bool bFound = false;
	if (!mTextTrackIndex.empty())
	{
		for (auto it = mTextTracks.begin(); it != mTextTracks.end(); it++)
		{
			if (it->index == mTextTrackIndex)
			{
				textTrack = *it;
				bFound = true;
			}
		}
	}
	return bFound;
}
/**
*   @brief verify in-band CC availability for a stream.
*/
bool StreamAbstractionAAMP::isInBandCcAvailable()
{
	bool inBandCC = false;
	for (auto it = mTextTracks.begin(); it != mTextTracks.end(); it++)
	{
		// Use the rendition to identify whether the stream has inband CC or not.
		// Note that this field is not mandatory but most streams have this field.
		// Note that adaptation set for text track may not be present for inband CC(608/708) streams.
		if(!it->rendition.empty())
		{
			AAMPLOG_INFO("Accessibility:%s",it->rendition.c_str());
			inBandCC = true;
			break;
		}
	}
	return inBandCC;
}
/**
 *   @brief Get current audio track
 */
int StreamAbstractionAAMP::GetAudioTrack()
{
	int index = -1;
	if (!mAudioTrackIndex.empty())
	{
		for (auto it = mAudioTracks.begin(); it != mAudioTracks.end(); it++)
		{
			if (it->index == mAudioTrackIndex)
			{
				index = (int)std::distance(mAudioTracks.begin(), it);
			}
		}
	}
	return index;
}

/**
 *  @brief Get current text track
 */
int StreamAbstractionAAMP::GetTextTrack()
{
	int index = -1;
	if (!mTextTrackIndex.empty())
	{
		for (auto it = mTextTracks.begin(); it != mTextTracks.end(); it++)
		{
			if (it->index == mTextTrackIndex)
			{
				index = (int)std::distance(mTextTracks.begin(), it);
			}
		}
	}
	return index;
}

/**
 *  @brief Refresh subtitle track
 */
void StreamAbstractionAAMP::RefreshSubtitles()
{
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		AAMPLOG_WARN("Setting refreshSubtitles");
		subtitle->refreshSubtitles = true;
		subtitle->AbortWaitForCachedAndFreeFragment(true);
	}
}


void StreamAbstractionAAMP::WaitForVideoTrackCatchupForAux()
{
	MediaTrack *aux = GetMediaTrack(eTRACK_AUX_AUDIO);
	MediaTrack *video = GetMediaTrack(eTRACK_VIDEO);
	if( aux && video )
	{
		std::unique_lock<std::mutex> lock(mLock);
		double auxDuration = aux->GetTotalInjectedDuration();
		double videoDuration = video->GetTotalInjectedDuration();

		while ((auxDuration > (videoDuration + video->fragmentDurationSeconds)) && aamp->DownloadsAreEnabled() && !aux->IsDiscontinuityProcessed() && !video->IsInjectionAborted() && !(video->IsAtEndOfTrack()))
		{
			if (mTrackState == eDISCONTINUITY_IN_VIDEO)
			{
				AAMPLOG_WARN("Skipping WaitForVideoTrackCatchupForAux as video is processing a discontinuity");
				break;
			}

			if (std::cv_status::no_timeout == mAuxCond.wait_for(lock, std::chrono::milliseconds(100)))
			{
				break;
			}
			auxDuration = aux->GetTotalInjectedDuration();
			videoDuration = video->GetTotalInjectedDuration();
		}
	}
}

/**
 * @fn GetPreferredLiveOffsetFromConfig
 * @brief check if current stream have 4K content
 * @retval true on success
 */
bool StreamAbstractionAAMP::GetPreferredLiveOffsetFromConfig()
{
	bool stream4K = false;
	do
	{
		int height = 0;
		BitsPerSecond bandwidth = 0;

		/** Update Live Offset with default or configured liveOffset*/
		aamp->UpdateLiveOffset();

		/**< 1. Is it CDVR or iVOD? not required live Offset correction*/
		if(!aamp->IsLiveAdjustRequired())
		{
			/** 4K live offset not applicable for CDVR/IVOD */
			stream4K = false;
			break;
		}

		/**< 2. Check whether it is 4K stream or not*/
		stream4K =  Is4KStream(height, bandwidth);
		if (!stream4K)
		{
			/**Not a 4K */
			break;
		}

		/**< 3. 4K disabled by user? **/
		if (ISCONFIGSET(eAAMPConfig_Disable4K) )
		{
			AAMPLOG_WARN("4K playback disabled by User!!");
			break;
		}

		/**< 4. maxbitrate should be less than 4K bitrate? */
		BitsPerSecond maxBitrate = aamp->GetMaximumBitrate();
		if (bandwidth > maxBitrate)
		{
			AAMPLOG_WARN("Maxbitrate (%" BITSPERSECOND_FORMAT ") set by user is less than 4K bitrate (%" BITSPERSECOND_FORMAT ");", maxBitrate, bandwidth);
			stream4K = false;
			break;
		}

		/**< 5. If display resolution check enabled and resolution available then it should be grater than 4K profile */
		if (ISCONFIGSET(eAAMPConfig_LimitResolution) && aamp->mDisplayHeight == 0)
		{
			AAMPLOG_WARN("Ignoring display resolution check due to invalid display height 0");
		}
		else
		{
			if (ISCONFIGSET(eAAMPConfig_LimitResolution) && aamp->mDisplayHeight < height  )
			{
				AAMPLOG_WARN("Display resolution (%d) doesn't support the 4K resolution (%d)", aamp->mDisplayHeight, height);
				stream4K = false;
				break;
			}
		}

		/** 4K stream and 4K support is found ; Use 4K live offset if provided*/
		if (GETCONFIGOWNER(eAAMPConfig_LiveOffset4K) > AAMP_DEFAULT_SETTING)
		{
			/**Update live Offset with 4K stream live offset configured*/
			aamp->mLiveOffset = GETCONFIGVALUE(eAAMPConfig_LiveOffset4K);
			if(aamp->mBufferFor4kRampup != 0)
			{
				SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_MaxABRNWBufferRampUp,aamp->mBufferFor4kRampup);
				SETCONFIGVALUE(AAMP_TUNE_SETTING,eAAMPConfig_MinABRNWBufferRampDown,aamp->mBufferFor4kRampdown);
				aamp->LoadAampAbrConfig();
			}
			AAMPLOG_INFO("Updated live offset for 4K stream %lf", aamp->mLiveOffset);
			stream4K = true;
		}
	} while(0);
	return stream4K;
}

/**
 * @brief Set the text style of the subtitle to the options passed
 *
 * @param[in] - options - reference to the Json string that contains the information
 * @return - true indicating successful operation in passing options to the parser
 */
bool StreamAbstractionAAMP::SetTextStyle(const std::string &options)
{
	bool retVal = false;
	MediaTrack *subtitle = GetMediaTrack(eTRACK_SUBTITLE);
	// If embedded subtitles enabled
	if (subtitle && subtitle->enabled && subtitle->mSubtitleParser)
	{
		AAMPLOG_INFO("Calling SubtitleParser::SetTextStyle(%s)", options.c_str());
		subtitle->mSubtitleParser->setTextStyle(options);
		retVal = true;
	}
	return retVal;
}
/**
 * @brief Whether we are playing at live point or not.
 *
 * @param[in] - seekPosition - seek position in seconds
 * @return true if we are at live point.
 */
bool StreamAbstractionAAMP::IsStreamerAtLivePoint(double seekPosition)
{
	if(mIsAtLivePoint)
	{
		double endPos = aamp->culledSeconds+aamp->durationSeconds;
		if(seekPosition > (endPos-aamp->mLiveOffset))
		{
			AAMPLOG_INFO("SeekPostion[%lf] is greater than endPos[%lf]-mLiveOffset(%lf) i.e:%lf",seekPosition,endPos,aamp->mLiveOffset,endPos-aamp->mLiveOffset);
		}
		else
		{
			AAMPLOG_INFO("SeekPostion[%lf] is within range of endPos[%lf]-mLiveOffset(%lf) i.e:%lf",seekPosition,endPos,aamp->mLiveOffset,endPos-aamp->mLiveOffset);
			mIsAtLivePoint = false;
		}
	}
	return mIsAtLivePoint;
}

/**
 * @brief Whether we seeked to live offset range or not.
 *
 * @param[in] - seekPosition - seek position in seconds
 * @return true if we seeked to live.
 */
bool StreamAbstractionAAMP::IsSeekedToLive(double seekPosition)
{
	bool ret = false;
	double endPos = aamp->culledSeconds + aamp->durationSeconds;

	// if case seekPosition is in live range, endPos-mLiveOffset to endPos.
	// else if endPos updates after seek cmd received or seekPosition < endPos-LiveOffset, seekPosition sent by App won't be in live range.
	if (ceil(endPos - seekPosition) <= aamp->mLiveOffset)
	{
		AAMPLOG_WARN("SeekPostion[%lf] is in live range, endPos[%lf]-mLiveOffset(%lf) i.e:%lf",seekPosition,endPos,aamp->mLiveOffset,endPos-aamp->mLiveOffset);
		ret = true;
	}
	else
	{
		AAMPLOG_INFO("SeekPostion[%lf] is not in live range, endPos[%lf]-mLiveOffset(%lf) i.e:%lf",seekPosition,endPos,aamp->mLiveOffset,endPos-aamp->mLiveOffset);
	}
	return ret;
}

/**
 * @fn SetVideoPlaybackRate
 * @brief Set the Video playback rate
 *
 * @param[in] rate - play rate
 *
 * Note: A common abstraction object is used for recording the live edge to TSB, and playing back from TSB.
 * For this reason we only want to adjust the MediaProcessors speed when playing back from TSB.
 */
void StreamAbstractionAAMP::SetVideoPlaybackRate(float rate)
{
	MediaTrack *track = GetMediaTrack(eTRACK_VIDEO);
	if (track && track->enabled)
	{
		track->playContext->setRate(rate, PlayMode_normal);
	}
}

/**
 * @brief Initialize ISOBMFF Media Processor
 * @param[in] passThroughMode - true if processor should skip parsing PTS and flush
 */
void StreamAbstractionAAMP::InitializeMediaProcessor(bool passThroughMode)
{
	std::shared_ptr<IsoBmffProcessor> peerAudioProcessor = nullptr;
	std::shared_ptr<IsoBmffProcessor> peerSubtitleProcessor = nullptr;
	std::shared_ptr<MediaProcessor> subtitleESProcessor = nullptr;
	StreamOutputFormat videoFormat, audioFormat, auxAudioFormat, subtitleFormat;
	GetStreamFormat(videoFormat, audioFormat, auxAudioFormat, subtitleFormat);
	for (int i = eMEDIATYPE_SUBTITLE; i >= eMEDIATYPE_VIDEO; i--)
	{
		MediaTrack *track = GetMediaTrack((TrackType) i);
		// Some tracks can get enabled later during playback, example subtitle tracks in ad->content transition. Avoid overwriting playContext instance
		if(track && track->enabled && track->playContext == nullptr)
		{
			AAMPLOG_WARN("StreamAbstractionAAMP : Track[%s] - FORMAT_ISO_BMFF", track->name);

			if(eMEDIATYPE_SUBTITLE != i)
			{
				std::shared_ptr<IsoBmffProcessor> processor = std::make_shared<IsoBmffProcessor>(aamp, mID3Handler, (IsoBmffProcessorType) i,
																passThroughMode, peerAudioProcessor.get(), peerSubtitleProcessor.get());
				track->SourceFormat(FORMAT_ISO_BMFF);
				track->playContext = std::static_pointer_cast<MediaProcessor>(processor);
				track->playContext->setRate(aamp->rate, PlayMode_normal);
				if(eMEDIATYPE_AUDIO == i)
				{
					peerAudioProcessor = std::move(processor);
				}
				else if (eMEDIATYPE_VIDEO == i && subtitleESProcessor)
				{
					processor->addPeerListener(subtitleESProcessor.get());
				}
			}
			else
			{
				if(FORMAT_SUBTITLE_MP4 == subtitleFormat)
				{
					peerSubtitleProcessor = std::make_shared<IsoBmffProcessor>(aamp, nullptr, (IsoBmffProcessorType) i, passThroughMode, nullptr, nullptr);
					track->playContext = std::static_pointer_cast<MediaProcessor>(peerSubtitleProcessor);
					track->playContext->setRate(aamp->rate, PlayMode_normal);
				}
				else
				{
					subtitleESProcessor = std::make_shared<ElementaryProcessor>(aamp);
					track->playContext = subtitleESProcessor;
				}

				// If video playcontext is already created, attach subtitle processor to it.
				MediaTrack *videoTrack = GetMediaTrack(eTRACK_VIDEO);
				if (videoTrack && videoTrack->enabled && videoTrack->playContext)
				{
					std::static_pointer_cast<IsoBmffProcessor> (videoTrack->playContext)->setPeerSubtitleProcessor(peerSubtitleProcessor.get());
				}
			}
		}
	}
}

/**
 * @brief Returns playlist type of track
 */
AampMediaType MediaTrack::GetPlaylistMediaTypeFromTrack(TrackType type, bool isIframe)
{
	AampMediaType playlistType = eMEDIATYPE_MANIFEST;
	// For DASH, return playlist type as manifest
	if(eMEDIAFORMAT_DASH != aamp->mMediaFormat)
	{
		if(isIframe)
		{
			playlistType = eMEDIATYPE_PLAYLIST_IFRAME;
		}
		else if (type == eTRACK_AUDIO )
		{
			playlistType = eMEDIATYPE_PLAYLIST_AUDIO;
		}
		else if (type == eTRACK_SUBTITLE)
		{
			playlistType = eMEDIATYPE_PLAYLIST_SUBTITLE;
		}
		else if (type == eTRACK_AUX_AUDIO)
		{
			playlistType = eMEDIATYPE_PLAYLIST_AUX_AUDIO;
		}
		else if (type == eTRACK_VIDEO)
		{
			playlistType = eMEDIATYPE_PLAYLIST_VIDEO;
		}
	}
	return playlistType;
}


/**
 * @brief Notify playlist downloader threads of tracks
 */
void StreamAbstractionAAMP::DisablePlaylistDownloads()
{
	for (int i = 0 ; i < AAMP_TRACK_COUNT; i++)
	{
		MediaTrack *track = GetMediaTrack((TrackType) i);
		if (track && track->enabled)
		{
			track->AbortWaitForPlaylistDownload();
			track->AbortFragmentDownloaderWait();
		}
	}
}

/**
 * @brief Abort wait for playlist download
 */
void MediaTrack::AbortWaitForPlaylistDownload()
{
	std::unique_lock<std::mutex> lock(dwnldMutex);
	if(playlistDownloaderThreadStarted)
	{
		plDownloadWait.notify_one();
	}
	else
	{
		AAMPLOG_ERR("[%s] Playlist downloader thread not started", name);
	}
}

/**
 * @brief Wait until timeout is reached or interrupted
 */
void MediaTrack::EnterTimedWaitForPlaylistRefresh(int timeInMs)
{
	if(timeInMs > 0 && aamp->DownloadsAreEnabled())
	{
		std::unique_lock<std::mutex> lock(dwnldMutex);
		if(plDownloadWait.wait_for(lock, std::chrono::milliseconds(timeInMs)) == std::cv_status::timeout)
		{
			AAMPLOG_TRACE("[%s] timeout exceeded %d", name, timeInMs); // make it trace
		}
		else
		{
			AAMPLOG_TRACE("[%s] Signalled conditional wait", name); // TRACE
		}
	}
}

/**
 * @brief Abort fragment downloader wait
 */
void MediaTrack::AbortFragmentDownloaderWait()
{
	std::unique_lock<std::mutex> lock(dwnldMutex);
	if(fragmentCollectorWaitingForPlaylistUpdate)
	{
		frDownloadWait.notify_one();
	}
}

/**
 * @brief Wait for playlist download and update
 */
void MediaTrack::WaitForManifestUpdate()
{
	if(aamp->DownloadsAreEnabled() && fragmentCollectorWaitingForPlaylistUpdate)
	{
		std::unique_lock<std::mutex> lock(dwnldMutex);
		AAMPLOG_INFO("[%s] Waiting for manifest update", name);
		frDownloadWait.wait(lock);
	}
	fragmentCollectorWaitingForPlaylistUpdate = false;
	AAMPLOG_INFO("Exit");
}

/**
 * @brief Playlist downloader
 */
void MediaTrack::PlaylistDownloader()
{
	AampMediaType mediaType = GetPlaylistMediaTypeFromTrack(type, IS_FOR_IFRAME(aamp->rate,type));
	std::string trackName = GetMediaTypeName(mediaType);
	int updateDuration = 0, liveRefreshTimeOutInMs = 0 ;
	updateDuration = GetDefaultDurationBetweenPlaylistUpdates();
	long long lastPlaylistDownloadTimeMS = 0;
	bool quickPlaylistDownload = false;
	bool firstTimeDownload = true;
	long minUpdateDuration = 0, maxSegDuration = 0,availTimeOffMs=0;

	// abortPlaylistDownloader is by default true, sets as "false" when thread initializes
	// This supports Single download mode for VOD and looped mode for Live (always runs in thread)
	if(abortPlaylistDownloader)
	{
		// Playlist downloader called one time, For VOD content profile changes
		AAMPLOG_INFO("Downloading playlist : %s", name);
	}
	else
	{
		// Playlist downloader called in loop mode
		AAMPLOG_WARN("[%s] : Enter, track '%s'", trackName.c_str(), name);
		AAMPLOG_INFO("[%s] Playlist download timeout : %d", trackName.c_str(), updateDuration);
	}

	if( aamp->GetLLDashServiceData()->lowLatencyMode )
	{
		minUpdateDuration = GetMinUpdateDuration();
		maxSegDuration = (long)(aamp->GetLLDashServiceData()->fragmentDuration*1000);
		availTimeOffMs = (long)((aamp->GetLLDashServiceData()->availabilityTimeOffset)*1000);

		AAMPLOG_INFO("LL-DASH [%s] maxSegDuration=i[%d]d[%f] minUpdateDuration=[%d]d[%f],availTimeOff=%d]d[%f]",
					 name,(int)maxSegDuration,(double)maxSegDuration,(int)minUpdateDuration,(double)minUpdateDuration,(int)availTimeOffMs,(double)availTimeOffMs);
	}

	/* DOWNLOADER LOOP */
	do
	{
		/* TIMEOUT WAIT LOGIC
		 *
		 * Skipping this for VOD contents.
		 * Hits : When player attempts ABR, Player rampdown for retry logic
		 * 			Subtitle language change is requested
		 * quickPlaylistDownload is enabled under above cases for live refresh.
		 *
		 */
		if(aamp->DownloadsAreEnabled() && aamp->IsLive() && !quickPlaylistDownload)
		{
			lastPlaylistDownloadTimeMS = GetLastPlaylistDownloadTime();
			liveRefreshTimeOutInMs = updateDuration - (int)(aamp_GetCurrentTimeMS() - lastPlaylistDownloadTimeMS);
			if(liveRefreshTimeOutInMs <= 0 && aamp->IsLive() && aamp->rate > 0)
			{
				AAMPLOG_DEBUG("[%s] Refreshing playlist as it exceeded download timeout : %d", trackName.c_str(), updateDuration);
			}
			else
			{
				// For DASH first time download, always take maximum time to download enough fragments from different tracks
				// Else calculate wait time based on buffer
				if (firstTimeDownload && (eMEDIAFORMAT_DASH == aamp->mMediaFormat))
				{
					if(aamp->GetLLDashServiceData()->lowLatencyMode)
					{
						if((minUpdateDuration > 0) &&
							(minUpdateDuration > availTimeOffMs) &&
							(minUpdateDuration < MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS) )
						{
							liveRefreshTimeOutInMs = (int)(minUpdateDuration-availTimeOffMs);
						}
						else if(maxSegDuration > 0 && maxSegDuration > availTimeOffMs)
						{
							liveRefreshTimeOutInMs = (int)(maxSegDuration - availTimeOffMs);
						}
						else
						{
							liveRefreshTimeOutInMs = MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
						}
					}
					else
					{
						liveRefreshTimeOutInMs = MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
					}
				}
				else
				{
					liveRefreshTimeOutInMs = WaitTimeBasedOnBufferAvailable();
				}
				AAMPLOG_INFO("Refreshing playlist at %d ", liveRefreshTimeOutInMs);
				// Intricate timing issues exist, where we could enter timed wait even though we signalled
				// AbortWaitForPlaylistDownload from switch audio track. Seamless audio switch needs to happen immediately
				if (!seamlessAudioSwitchInProgress)
				{
					EnterTimedWaitForPlaylistRefresh(liveRefreshTimeOutInMs);
				}
			}
			firstTimeDownload = false;
		}

		/* PLAYLIST DOWNLOAD LOGIC
		 *
		 * Proceed if downloads are enabled.
		 *
		 */
		if(aamp->DownloadsAreEnabled())
		{
			AampGrowableBuffer manifest("download-PlaylistManifest");
			// reset quickPlaylistDownload for live playlist
			quickPlaylistDownload = false;
			std::string manifestUrl = GetPlaylistUrl();
			// take the original url before it gets changed in GetFile
			std::string effectiveUrl = GetEffectivePlaylistUrl();
			bool gotManifest = false;
			int http_error = 0;
			double downloadTime;
			manifest.Clear();

			/*
			 *
			 * FOR HLS, This should be called here
			 * FOR DASH, after getting MPD doc
			 *
			 */
			if(eMEDIAFORMAT_DASH != aamp->mMediaFormat)
			{
				long long lastPlaylistDownloadTime = aamp_GetCurrentTimeMS();
				SetLastPlaylistDownloadTime(lastPlaylistDownloadTime);
			}

			if (aamp->getAampCacheHandler()->RetrieveFromPlaylistCache(manifestUrl, &manifest, effectiveUrl,mediaType))
			{
				gotManifest = true;
				AAMPLOG_INFO("manifest[%s] retrieved from cache", trackName.c_str());
			}
			else
			{
				AampCurlInstance curlInstance = aamp->GetPlaylistCurlInstance(mediaType, false);
				// Enable downloads of mediaType if disabled
				if(!aamp->mMediaDownloadsEnabled[mediaType])
				{
					AAMPLOG_INFO("[%s] Re-enabling media download", trackName.c_str());
					aamp->EnableMediaDownloads(mediaType);
				}
				gotManifest = aamp->GetFile(manifestUrl, mediaType, &manifest, effectiveUrl, &http_error, &downloadTime, NULL, curlInstance, true );
				if(seamlessAudioSwitchInProgress && (manifestUrl != GetPlaylistUrl()))
				{
					//new Playlist updated in mid.
					quickPlaylistDownload = true;
					//To avoid below signaling to fragment collector
					continue;
				}

				//update videoend info
				aamp->UpdateVideoEndMetrics(mediaType,0,http_error,effectiveUrl,downloadTime);
			}

			if(gotManifest)
			{
				if(eMEDIAFORMAT_DASH == aamp->mMediaFormat)
				{
					aamp->mManifestUrl = effectiveUrl;
				}
				else
				{
					// HLS or HLS_MP4
					// Set effective URL, else fragments will be mapped from old url
					SetEffectivePlaylistUrl(effectiveUrl);
				}
			}

			// Index playlist and update track informations.
			ProcessPlaylist(manifest, http_error);

			// HTTP Response header needs to be sent to app when:
			// 1. HTTP header response event listener is available
			// 2. HTTP header response values are present
			// 3. Manifest refresh has happened during Live
			// Sending response after ProcessPlaylist is done
			if (aamp->IsEventListenerAvailable(AAMP_EVENT_HTTP_RESPONSE_HEADER) && gotManifest && !aamp->httpHeaderResponses.empty() && aamp->IsLive())
			{
				aamp->SendHTTPHeaderResponse();
			}

			if(fragmentCollectorWaitingForPlaylistUpdate && gotManifest)
			{
				// (gotManifest => false) If manifest download failed due to ABR request from HLS, don't abort wait.
				// DASH waits for manifest update only at EOS from all tracks, proceed only with fresh manifest.
				// Signal fragment collector to abort it's wait for playlist process
				AbortFragmentDownloaderWait();
			}

			// Check whether downloads are still enabled after processing playlist
			if (aamp->DownloadsAreEnabled())
			{
				if (!aamp->mMediaDownloadsEnabled[mediaType])
				{
					AAMPLOG_ERR("[%s] Aborted playlist download by callback, retrying..", trackName.c_str());
					// Download playlist without any wait
					quickPlaylistDownload = true;
				}
			}
			else // if downloads disabled
			{
				AAMPLOG_ERR("[%s] : Downloads are disabled, exiting", trackName.c_str());
				abortPlaylistDownloader = true;
			}
		}
		else
		{
			AAMPLOG_ERR("[%s] : Downloads are disabled, exiting", trackName.c_str());
			abortPlaylistDownloader = true;
		}
	} while (!abortPlaylistDownloader && aamp->IsLive());
	// abortPlaylistDownloader is true by default, made for VOD playlist.
	// Loop runs for Live manifests, closes at dynamic => static transition

	AAMPLOG_WARN("[%s] : Exit", trackName.c_str());
}
/**
 * @brief Wait time for playlist refresh based on buffer available
 *
 * @return minDelayBetweenPlaylistUpdates - wait time for playlist refresh
 */
int MediaTrack::WaitTimeBasedOnBufferAvailable()
{
	long long lastPlaylistDownloadTimeMS = GetLastPlaylistDownloadTime();
	int minDelayBetweenPlaylistUpdates = 0;
	if (lastPlaylistDownloadTimeMS)
	{
		int timeSinceLastPlaylistDownload = (int)(aamp_GetCurrentTimeMS() - lastPlaylistDownloadTimeMS);
		long long currentPlayPosition = aamp->GetPositionMilliseconds();
		long long endPositionAvailable = (aamp->culledSeconds + aamp->durationSeconds)*1000;
		bool lowLatencyMode = aamp->GetLLDashServiceData()->lowLatencyMode;
		// playTarget value will vary if TSB is full and trickplay is attempted. Cant use for buffer calculation
		// So using the endposition in playlist - Current playing position to get the buffer availability
		long bufferAvailable = (endPositionAvailable - currentPlayPosition);
		//Get Minimum update duration in milliseconds
		long minUpdateDuration = GetMinUpdateDuration();
		minDelayBetweenPlaylistUpdates = MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
		// when target duration is high value(>Max delay)  but buffer is available just above the max update inteval,then go with max delay between playlist refresh.
		if(bufferAvailable < (2* MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS))
		{
			if ((minUpdateDuration > 0) && (bufferAvailable  > minUpdateDuration))
			{
				//1.If buffer Available is > 2*minUpdateDuration , may be 1.0 times also can be set ???
				//2.If buffer is between 2*target & mMinUpdateDurationMs
				float mFactor=0.0f;
				if (lowLatencyMode)
				{
					mFactor = (bufferAvailable  > (minUpdateDuration * 2)) ? (float)(minUpdateDuration/1000) : 0.5;
				}
				else
				{
					mFactor = (bufferAvailable  > (minUpdateDuration * 2)) ? 1.5 : 0.5;
				}
				minDelayBetweenPlaylistUpdates = (int)(mFactor * minUpdateDuration);
			}
			// if buffer < targetDuration && buffer < MaxDelayInterval
			else
			{
				// if bufferAvailable is less than targetDuration ,its in RED alert . Close to freeze
				// need to refresh soon ..
				minDelayBetweenPlaylistUpdates = (bufferAvailable) ? (int)(bufferAvailable / 3) : MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS; //500ms

				// limit the logs when buffer is low
				{
					static int bufferlowCnt;
					if((bufferlowCnt++ & 5) == 0)
					{
						AAMPLOG_WARN("Buffer is running low(%ld).Refreshing playlist(%d).PlayPosition(%lld) End(%lld)",
									 bufferAvailable,minDelayBetweenPlaylistUpdates,currentPlayPosition,endPositionAvailable);
					}
				}
			}
		}

		// First cap max limit ..
		// remove already consumed time from last update
		// if time interval goes negative, limit to min value
		// restrict to Max delay interval
		if (minDelayBetweenPlaylistUpdates > MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS)
		{
			minDelayBetweenPlaylistUpdates = MAX_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
		}

		// adjust with last refreshed time interval
		minDelayBetweenPlaylistUpdates -= timeSinceLastPlaylistDownload;

		if(minDelayBetweenPlaylistUpdates < MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS)
		{
			if (lowLatencyMode)
			{
				long availTimeOffMs = (long)((aamp->GetLLDashServiceData()->availabilityTimeOffset)*1000);
				long maxSegDuration = (long)(aamp->GetLLDashServiceData()->fragmentDuration*1000);
				if(minUpdateDuration > 0 && minUpdateDuration < maxSegDuration)
				{
					minDelayBetweenPlaylistUpdates = (int)minUpdateDuration;
				}
				else if(minUpdateDuration > 0 && minUpdateDuration > availTimeOffMs)
				{
					minDelayBetweenPlaylistUpdates = (int)(minUpdateDuration-availTimeOffMs);
				}
				else if (maxSegDuration > 0 && maxSegDuration > availTimeOffMs)
				{
					minDelayBetweenPlaylistUpdates = (int)(maxSegDuration-availTimeOffMs);
				}
				else
				{
					// minimum of 500 mSec needed to avoid too frequent download.
					minDelayBetweenPlaylistUpdates = MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
				}
				if(minDelayBetweenPlaylistUpdates < MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS)
				{
					// minimum of 500 mSec needed to avoid too frequent download.
					minDelayBetweenPlaylistUpdates = MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
				}
			}
			else
			{
				// minimum of 500 mSec needed to avoid too frequent download.
				minDelayBetweenPlaylistUpdates = MIN_DELAY_BETWEEN_PLAYLIST_UPDATE_MS;
			}
		}
		AAMPLOG_INFO("aamp playlist end refresh bufferMs(%ld) delay(%d) delta(%d) End(%lld) PlayPosition(%lld)",
					 bufferAvailable,minDelayBetweenPlaylistUpdates,timeSinceLastPlaylistDownload,endPositionAvailable,currentPlayPosition);
	}
	return minDelayBetweenPlaylistUpdates;
}

/**
 * @brief Get total fragment injected duration
 *
 * @return Total duration in seconds
 */
double MediaTrack::GetTotalInjectedDuration()
{
	std::lock_guard<std::mutex> lock(mTrackParamsMutex);
	double ret = totalInjectedDuration;
	if (aamp->GetLLDashChunkMode())
	{
		ret = totalInjectedChunksDuration;
	}
	return ret;
}

/**
 * @brief update total fragment injected duration
 *
 * @return void
 */
void MediaTrack::UpdateInjectedDuration(double surplusDuration)
{
	totalInjectedDuration -= surplusDuration ;
}

/**
 * @brief SetCachedFragmentChunksSize - Setter for fragment chunks cache size
 *
 * @param[in] size Size for fragment chunks cache
 */
void MediaTrack::SetCachedFragmentChunksSize(size_t size)
{
	if (size > 0 && size <= maxCachedFragmentChunksPerTrack)
	{
		AAMPLOG_TRACE("Set mCachedFragmentChunks size:%zu successfully", size);
		mCachedFragmentChunksSize = size;
	}
	else
	{
		AAMPLOG_ERR("Failed to set size:%zu", size);
	}
}

/**
 * Handles the fragment position jump for the media track.
 *
 * This function is responsible for handling the fragment position jump for the media track.
 * It calculates the delta between the last injected fragment end position and the current fragment position,
 * and updates the total injected duration accordingly.
 *
 * @param cachedFragment pointer to the cached fragment.
 */
void MediaTrack::HandleFragmentPositionJump(CachedFragment* cachedFragment)
{
	// Not tested for HLS_MP4 and HLS, hence limiting to DASH for now.
	if ((lastInjectedDuration > 0) && (aamp->mMediaFormat == eMEDIAFORMAT_DASH))
	{
		// Find the delta between the last injected fragment end position and the current fragment position
		double positionDelta = (cachedFragment->absPosition - lastInjectedDuration);
		// There is a delta which implies a fragment might have been skipped
		// Here we are comparing against absolute position, so discontinuous periods have no effect
		if (positionDelta > 0)
		{
			// Update the total injected duration
			{
				std::lock_guard<std::mutex> lock(mTrackParamsMutex);
				totalInjectedDuration += positionDelta;
			}
			if (type != eTRACK_SUBTITLE)
			{
				AAMPLOG_WARN("[%s] Found a positionDelta (%lf) between lastInjectedDuration (%lf) and new fragment absPosition (%lf)",
						name, positionDelta, lastInjectedDuration, cachedFragment->absPosition);
			}
		}
	}
}

bool MediaTrack::IsInjectionFromCachedFragmentChunks()
{
	// CachedFragmentChunks is used for LL-DASH and for any content if AAMP TSB is enabled
	bool isLLDashChunkMode = aamp->GetLLDashChunkMode();
	bool aampTsbEnabled = aamp->IsLocalAAMPTsb();
	bool isInjectionFromCachedFragmentChunks = isLLDashChunkMode || aampTsbEnabled;

	AAMPLOG_TRACE("[%s] isLLDashChunkMode %d aampTsbEnabled %d ret %d",
				  name, isLLDashChunkMode, aampTsbEnabled, isInjectionFromCachedFragmentChunks);
	return isInjectionFromCachedFragmentChunks;
}

/**
 *   @brief Re-initializes the injection
 *   @param[in] rate - play rate
 */	
void StreamAbstractionAAMP::ReinitializeInjection(double rate) 
{
	clearFirstPTS();							//Clears the mFirstPTS value to trigger update of first PTS
	SetTrickplayMode(rate);
	ResetTrickModePtsRestamping();
	if (!aamp->GetLLDashChunkMode())
	{
		SetVideoPlaybackRate(rate);
	}
}
