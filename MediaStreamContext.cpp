/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 RDK Management
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
 * @file MediaStreamContext.cpp
 * @brief Handles operations on Media streams
 */

#include "MediaStreamContext.h"
#include "AampMemoryUtils.h"
#include "isobmff/isobmffbuffer.h"
#include "AampCacheHandler.h"
#include "AampTSBSessionManager.h"
#include "AampMPDUtils.h"

/**
 *  @brief Receives cached fragment and injects to sink.
 */
void MediaStreamContext::InjectFragmentInternal(CachedFragment* cachedFragment, bool &fragmentDiscarded,bool isDiscontinuity)
{
	assert(!aamp->GetLLDashChunkMode());

	if(playContext)
	{
		MediaProcessor::process_fcn_t processor = [this](AampMediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
		{
		};
		fragmentDiscarded = !playContext->sendSegment( &cachedFragment->fragment, cachedFragment->position,
														cachedFragment->duration, cachedFragment->PTSOffsetSec, isDiscontinuity, cachedFragment->initFragment, processor, ptsError);
	}
	else
	{
		aamp->ProcessID3Metadata(cachedFragment->fragment.GetPtr(), cachedFragment->fragment.GetLen(), (AampMediaType) type);
		AAMPLOG_DEBUG("Type[%d] cachedFragment->position: %f cachedFragment->duration: %f cachedFragment->initFragment: %d", type, cachedFragment->position,cachedFragment->duration,cachedFragment->initFragment);
		aamp->SendStreamTransfer((AampMediaType)type, &cachedFragment->fragment,
		cachedFragment->position, cachedFragment->position, cachedFragment->duration, cachedFragment->PTSOffsetSec, cachedFragment->initFragment, cachedFragment->discontinuity);
	}

	fragmentDiscarded = false;
} // InjectFragmentInternal

/**
 *  @brief Fetch and cache a fragment
 */
bool MediaStreamContext::CacheFragment(std::string fragmentUrl, unsigned int curlInstance, double position, double fragmentDurationS, const char *range, bool initSegment, bool discontinuity, bool playingAd, uint32_t scale)
{
	bool ret = false;
	AAMPLOG_INFO("Type[%d] position(before restamp) %f discontinuity %d scale %u duration %f mPTSOffsetSec %f absTime %lf fragmentUrl %s", type, position, discontinuity, scale, fragmentDurationS, GetContext()->mPTSOffset.inSeconds(), mActiveDownloadInfo->absolutePosition, fragmentUrl.c_str());

	ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia(mediaType, initSegment);
	CachedFragment *cachedFragment = GetFetchBuffer(true);
	BitsPerSecond bitrate = 0;
	double downloadTimeS = 0;
	AampMediaType actualType = (AampMediaType)(initSegment ? (eMEDIATYPE_INIT_VIDEO + mediaType) : mediaType); // Need to revisit the logic

	cachedFragment->type = actualType;
	cachedFragment->initFragment = initSegment;
	cachedFragment->timeScale = fragmentDescriptor.TimeScale;
	cachedFragment->uri = fragmentUrl; // For debug output
	cachedFragment->absPosition = 0;
	if (mActiveDownloadInfo)
	{
		cachedFragment->absPosition = mActiveDownloadInfo->absolutePosition;
		cachedFragment->timeScale = mActiveDownloadInfo->timeScale;
		cachedFragment->PTSOffsetSec = mActiveDownloadInfo->ptsOffset.inSeconds();
		/* The value of PTSOffsetSec in the context can get updated at the start of a period before
		 * the last segment from the previous period has been injected, hence we copy it
		 */
		if (ISCONFIGSET(eAAMPConfig_EnablePTSReStamp))
		{
			// apply pts offset to position which ends up getting put into gst_buffer in sendHelper
			position += mActiveDownloadInfo->ptsOffset.inSeconds();
			AAMPLOG_INFO("Type[%d] position after restamp = %fs", type, position);
		}
	}
	else
	{
		AAMPLOG_WARN("mActiveDownloadInfo is NULL");
	}

	if (initSegment && discontinuity)
	{
		setDiscontinuityState(true);
	}

	if (!initSegment && mDownloadedFragment.GetPtr())
	{
		ret = true;
		cachedFragment->fragment.Replace(&mDownloadedFragment);
	}
	else
	{
		std::string effectiveUrl;
		int iFogError = -1;
		int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play
		bool bReadfromcache = false;
		if (initSegment)
		{
			ret = bReadfromcache = aamp->getAampCacheHandler()->RetrieveFromInitFragmentCache(fragmentUrl,&cachedFragment->fragment,effectiveUrl);
		}
		if (!bReadfromcache)
		{
			AampMPDDownloader *dnldInstance = aamp->GetMPDDownloader();
			int maxInitDownloadTimeMS = 0;
			if ((aamp->IsLocalAAMPTsb()) && (dnldInstance))
			{
				//Calculate the time remaining for the fragment to be available in the timeshift buffer window
				//         A                                     B                        C
				// --------|-------------------------------------|------------------------|
				// AC represents timeshiftBufferDepth in MPD; B is absolute time position of fragment and
				// C is MPD publishTime(absolute time). So AC - (C-B) gives the time remaining for the
				//fragment to be available in the timeshift buffer window
				maxInitDownloadTimeMS = aamp->mTsbDepthMs - (dnldInstance->GetPublishTime() - (fragmentTime * 1000));
				AAMPLOG_INFO("maxInitDownloadTimeMS %d, initSegment %d, mTsbDepthMs %d, GetPublishTime %llu(ms), fragmentTime %f(s) ",
					maxInitDownloadTimeMS, initSegment, aamp->mTsbDepthMs, (unsigned long long)dnldInstance->GetPublishTime(), fragmentTime);
			}

			ret = aamp->GetFile(fragmentUrl, actualType, mTempFragment.get(), effectiveUrl, &httpErrorCode, &downloadTimeS, range, curlInstance, true/*resetBuffer*/,  &bitrate, &iFogError, fragmentDurationS, bucketType, maxInitDownloadTimeMS);
			if (initSegment && ret)
			{
				aamp->getAampCacheHandler()->InsertToInitFragCache(fragmentUrl, mTempFragment.get(), effectiveUrl, actualType);
			}
			if (ret)
			{
				cachedFragment->fragment = *mTempFragment;
				mTempFragment->Free();
			}
		}

		if (iCurrentRate != AAMP_NORMAL_PLAY_RATE)
		{
			if (actualType == eMEDIATYPE_VIDEO)
			{
				actualType = eMEDIATYPE_IFRAME;
			}
			else if (actualType == eMEDIATYPE_INIT_VIDEO)
			{
				actualType = eMEDIATYPE_INIT_IFRAME;
			}
		}
		else
		{
			if ((actualType == eMEDIATYPE_INIT_VIDEO || actualType == eMEDIATYPE_INIT_AUDIO || actualType == eMEDIATYPE_INIT_SUBTITLE) && ret) // Only if init fragment successful or available from cache
			{
				// To read track_id from the init fragments to check if there any mismatch.
				// A mismatch in track_id is not handled in the gstreamer version 1.10.4
				// But is handled in the latest version (1.18.5),
				// so upon upgrade to it or introduced a patch in qtdemux,
				// this portion can be reverted
				IsoBmffBuffer buffer;
				buffer.setBuffer((uint8_t *)cachedFragment->fragment.GetPtr(), cachedFragment->fragment.GetLen());
				buffer.parseBuffer();
				uint32_t track_id = 0;
				buffer.getTrack_id(track_id);
				if (buffer.isInitSegment())
				{
					uint32_t timeScale = 0;
					buffer.getTimeScale(timeScale);
					if (actualType == eMEDIATYPE_INIT_VIDEO)
					{
						AAMPLOG_INFO("Video TimeScale [%d]", timeScale);
						aamp->SetVidTimeScale(timeScale);
					}
					else if (actualType == eMEDIATYPE_INIT_AUDIO)
					{
						AAMPLOG_INFO("Audio TimeScale  [%d]", timeScale);
						aamp->SetAudTimeScale(timeScale);
					}
					else if (actualType == eMEDIATYPE_INIT_SUBTITLE)
					{
						AAMPLOG_INFO("Subtitle TimeScale  [%d]", timeScale);
						aamp->SetSubTimeScale(timeScale);
					}
				}
			}
		}

		if (!bReadfromcache)
		{
			// update videoend info
			aamp->UpdateVideoEndMetrics(actualType, bitrate ? bitrate : fragmentDescriptor.Bandwidth, (iFogError > 0 ? iFogError : httpErrorCode), effectiveUrl, fragmentDurationS, downloadTimeS);
		}
	}

	mCheckForRampdown = false;
	if (ret && (bitrate > 0 && bitrate != fragmentDescriptor.Bandwidth))
	{
		AAMPLOG_INFO("Bitrate changed from %u to %ld", fragmentDescriptor.Bandwidth, bitrate);
		fragmentDescriptor.Bandwidth = (uint32_t)bitrate;
		context->SetTsbBandwidth(bitrate);
		context->mUpdateReason = true;
		mDownloadedFragment.Replace(&cachedFragment->fragment);
		ret = false;
	}
	return ret;
}

/**
 *  @brief Cache Fragment Chunk
 */
bool MediaStreamContext::CacheFragmentChunk(AampMediaType actualType, char *ptr, size_t size, std::string remoteUrl, long long dnldStartTime)
{
	AAMPLOG_DEBUG("[%s] Chunk Buffer Length %zu Remote URL %s", name, size, remoteUrl.c_str());

	bool ret = true;
	if (WaitForCachedFragmentChunkInjected())
	{
		CachedFragment *cachedFragment = NULL;
		cachedFragment = GetFetchChunkBuffer(true);
		if (NULL == cachedFragment)
		{
			AAMPLOG_WARN("[%s] Something Went wrong - Can't get FetchChunkBuffer", name);
			return false;
		}
		cachedFragment->absPosition = 0;
		cachedFragment->type = actualType;
		cachedFragment->downloadStartTime = dnldStartTime;
		cachedFragment->fragment.AppendBytes(ptr, size);
		cachedFragment->timeScale = fragmentDescriptor.TimeScale;
		cachedFragment->uri = remoteUrl;
		if (mActiveDownloadInfo)
		{
			cachedFragment->absPosition = mActiveDownloadInfo->absolutePosition;
			cachedFragment->timeScale = mActiveDownloadInfo->timeScale;
		}
		/* The value of PTSOffsetSec in the context can get updated at the start of a period before
		 * the last segment from the previous period has been injected, hence we copy it
		 */
		cachedFragment->PTSOffsetSec = GetContext()->mPTSOffset.inSeconds();

		AAMPLOG_TRACE("[%s] cachedFragment %p ptr %p", name, cachedFragment, cachedFragment->fragment.GetPtr());
		UpdateTSAfterChunkFetch();
	}
	else
	{
		AAMPLOG_TRACE("[%s] WaitForCachedFragmentChunkInjected aborted", name);
		ret = false;
	}
	return ret;
}

/**
 *  @brief Function to update skip duration on PTS restamp
 */
void MediaStreamContext::updateSkipPoint(double position, double duration )
{
	if(ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && (aamp->mVideoFormat == FORMAT_ISO_BMFF ))
	{
		if(playContext)
		{
			playContext->updateSkipPoint(position,duration);
		}
	}
}

/**
 *  @brief Function to set discontinuity state
 */
 void MediaStreamContext::setDiscontinuityState(bool isDiscontinuity)
 {
	if(ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && (aamp->mVideoFormat == FORMAT_ISO_BMFF ))
	{
		if(playContext)
		{
			playContext->setDiscontinuityState(isDiscontinuity);
		}
	}
 }

 /**
 *  @brief Function to abort wait for video PTS
 */
 void MediaStreamContext::abortWaitForVideoPTS()
 {
	if(ISCONFIGSET(eAAMPConfig_EnablePTSReStamp) && (aamp->mVideoFormat == FORMAT_ISO_BMFF ))
	{
		if(playContext)
		{
			AAMPLOG_WARN(" %s abort waiting for video PTS arrival",name );
		  	playContext->abortWaitForVideoPTS();
		}
	}
 }

/**
 *  @brief Listener to ABR profile change
 */
void MediaStreamContext::ABRProfileChanged(void)
{
	// TODO: Use this lock across all the functions which uses shared variables
	AcquireMediaStreamContextLock();
	struct ProfileInfo profileMap = context->GetAdaptationSetAndRepresentationIndicesForProfile(context->currentProfileIndex);
	// Get AdaptationSet Index and Representation Index from the corresponding profile
	int adaptIdxFromProfile = profileMap.adaptationSetIndex;
	int reprIdxFromProfile = profileMap.representationIndex;
	if (!((adaptationSetIdx == adaptIdxFromProfile) && (representationIndex == reprIdxFromProfile)))
	{
		const IAdaptationSet *pNewAdaptationSet = context->GetAdaptationSetAtIndex(adaptIdxFromProfile);
		IRepresentation *pNewRepresentation = pNewAdaptationSet->GetRepresentation().at(reprIdxFromProfile);
		if(representation != NULL)
		{
			AAMPLOG_WARN("StreamAbstractionAAMP_MPD: ABR %dx%d[%d] -> %dx%d[%d]",
					representation->GetWidth(), representation->GetHeight(), representation->GetBandwidth(),
					pNewRepresentation->GetWidth(), pNewRepresentation->GetHeight(), pNewRepresentation->GetBandwidth());
			adaptationSetIdx = adaptIdxFromProfile;
			adaptationSet = pNewAdaptationSet;
			adaptationSetId = adaptationSet->GetId();
			representationIndex = reprIdxFromProfile;
			representation = pNewRepresentation;

			dash::mpd::IMPD *mpd = context->GetMPD();
			IPeriod *period = context->GetPeriod();
			fragmentDescriptor.ClearMatchingBaseUrl();
			fragmentDescriptor.AppendMatchingBaseUrl( &mpd->GetBaseUrls() );
			fragmentDescriptor.AppendMatchingBaseUrl( &period->GetBaseURLs() );
			fragmentDescriptor.AppendMatchingBaseUrl( &adaptationSet->GetBaseURLs() );
			fragmentDescriptor.AppendMatchingBaseUrl( &representation->GetBaseURLs() );

			fragmentDescriptor.Bandwidth = representation->GetBandwidth();
			fragmentDescriptor.RepresentationID.assign(representation->GetId());
			// Update timescale when video profile changes in ABR
			SegmentTemplates segmentTemplates (representation->GetSegmentTemplate(), adaptationSet->GetSegmentTemplate());
			if (segmentTemplates.HasSegmentTemplate())
			{
				fragmentDescriptor.TimeScale = segmentTemplates.GetTimescale();
			}
			profileChanged = true;
		}
		else
		{
			AAMPLOG_WARN("representation is null");  //CID:83962 - Null Returns
		}
	}
	else
	{
		AAMPLOG_DEBUG("StreamAbstractionAAMP_MPD:: Not switching ABR %dx%d[%d] ",
				representation->GetWidth(), representation->GetHeight(), representation->GetBandwidth());
	}
	ReleaseMediaStreamContextLock();
}

/**
 * @brief Get duration of buffer
 */
double MediaStreamContext::GetBufferedDuration()
{
	double bufferedDuration=0;
	double position = aamp->GetPositionMs() / 1000.00;
	AAMPLOG_INFO("[%s] lastDownloadedPosition %lfs position %lfs prevFirstPeriodStartTime %llds",
		GetMediaTypeName(mediaType),
		lastDownloadedPosition.load(),
		position,
		aamp->prevFirstPeriodStartTime);
	if(lastDownloadedPosition >= position)
	{
		// If player faces buffering, this will be 0
		bufferedDuration = lastDownloadedPosition - position;
		AAMPLOG_TRACE("[%s] bufferedDuration %fs lastDownloadedPosition %lfs position %lfs",
			GetMediaTypeName(mediaType),
			bufferedDuration,
			lastDownloadedPosition.load(),
			position);
	}
	else if( lastDownloadedPosition < aamp->prevFirstPeriodStartTime )
	{
		//When Player is rolling from IVOD window to Linear
		position = aamp->prevFirstPeriodStartTime - position;
		aamp->prevFirstPeriodStartTime = 0;
		bufferedDuration = lastDownloadedPosition - position;
		AAMPLOG_TRACE("[%s] bufferedDuration %fs lastDownloadedPosition %lfs position %lfs prevFirstPeriodStartTime %llds",
			GetMediaTypeName(mediaType),
			bufferedDuration,
			lastDownloadedPosition.load(),
			position,
			aamp->prevFirstPeriodStartTime);
	}
	else
	{
		// This avoids negative buffer, expecting
		// lastDownloadedPosition never exceeds position in normal case.
		// Other case happens when contents are yet to be injected.
		lastDownloadedPosition = 0;
		bufferedDuration = lastDownloadedPosition;
	}
	AAMPLOG_INFO("[%s] bufferedDuration %fs",
		GetMediaTypeName(mediaType),
		bufferedDuration);
	return bufferedDuration;
}

/**
 * @brief Notify discontinuity during trick-mode as PTS re-stamping is done in sink
 */
void MediaStreamContext::SignalTrickModeDiscontinuity()
{
	aamp->SignalTrickModeDiscontinuity();
}

/**
 * @brief Returns if the end of track reached.
 */
bool MediaStreamContext::IsAtEndOfTrack()
{
	return eosReached;
}

/**
 * @brief Returns the MPD playlist URL
 */
std::string& MediaStreamContext::GetPlaylistUrl()
{
	return mPlaylistUrl;
}

/**
 * @brief Returns the MPD original playlist URL
 */
std::string& MediaStreamContext::GetEffectivePlaylistUrl()
{
	return mEffectiveUrl;
}

/**
 * @brief Sets the HLS original playlist URL
 */
void MediaStreamContext::SetEffectivePlaylistUrl(std::string url)
{
	mEffectiveUrl = url;
}

/**
 * @brief Returns last playlist download time
 */
long long MediaStreamContext::GetLastPlaylistDownloadTime()
{
	return (long long) context->mLastPlaylistDownloadTimeMs;
}

/**
 * @brief Sets last playlist download time
 */
void MediaStreamContext::SetLastPlaylistDownloadTime(long long time)
{
	context->mLastPlaylistDownloadTimeMs = time;
}

/**
 * @brief Returns minimum playlist update duration in Ms
 */
long MediaStreamContext::GetMinUpdateDuration()
{
	return (long) context->GetMinUpdateDuration();
}

/**
 * @brief Returns default playlist update duration in Ms
 */
int MediaStreamContext::GetDefaultDurationBetweenPlaylistUpdates()
{
	return DEFAULT_INTERVAL_BETWEEN_PLAYLIST_UPDATES_MS;
}


/**
 * @fn CacheTsbFragment
 * @param fragment TSB fragment pointer
 * @retval true on success
 */
bool MediaStreamContext::CacheTsbFragment(std::shared_ptr<CachedFragment> fragment)
{
	// FN_TRACE_F_MPD( __FUNCTION__ );
	std::lock_guard<std::mutex> lock(fetchChunkBufferMutex);
	bool ret = false;
	if(fragment->fragment.GetPtr() && WaitForCachedFragmentChunkInjected())
	{
		AAMPLOG_TRACE("Type[%s] fragmentTime %f discontinuity %d duration %f initFragment:%d", name, fragment->position, fragment->discontinuity, fragment->duration, fragment->initFragment);
		CachedFragment* cachedFragment = GetFetchChunkBuffer(true);
		if(cachedFragment->fragment.GetPtr())
		{
			// If following log is coming, possible memory leak. Need to clear the data first before slot reuse.
			AAMPLOG_WARN("Fetch buffer has junk data, Need to free this up");
		}
		cachedFragment->fragment.Clear();
		cachedFragment->Copy(fragment.get(), fragment->fragment.GetLen());
		if(cachedFragment->fragment.GetPtr() && cachedFragment->fragment.GetLen() > 0)
		{
			ret = true;
			UpdateTSAfterChunkFetch();
		}
		else
		{
			AAMPLOG_TRACE("Empty fragment, not injecting");
			cachedFragment->fragment.Free();
		}
	}
	else
	{
		AAMPLOG_WARN("[%s] Failed to update inject", name);
	}
	return ret;
}

/**
 * @fn OnFragmentDownloadSucess
 * @brief Function called on fragment download success
 * @param[in] downloadInfo - download information
 */
void MediaStreamContext::OnFragmentDownloadSucess(DownloadInfoPtr dlInfo)
{
	if (nullptr == mActiveDownloadInfo || nullptr == dlInfo)
	{
		AAMPLOG_WARN("OnFragmentDownloadSucess: mActiveDownloadInfo or dlInfo is NULL");
		return;
	}

	// Get active buffer
	CachedFragment *cachedFragment = GetFetchBuffer(false);
	mActiveDownloadInfo = nullptr;
	AampTSBSessionManager *tsbSessionManager = aamp->GetTSBSessionManager();

	auto CheckEos = [this, &tsbSessionManager]()
	{
		return IsLocalTSBInjection() &&
			   AAMP_NORMAL_PLAY_RATE == aamp->rate &&
			   !aamp->pipeline_paused &&
			   eTUNETYPE_SEEKTOLIVE == context->mTuneType &&
			   tsbSessionManager &&
			   tsbSessionManager->GetTsbReader((AampMediaType)type) &&
			   tsbSessionManager->GetTsbReader((AampMediaType)type)->IsEos();
	};

	cachedFragment->position = dlInfo->pts;
	cachedFragment->duration = dlInfo->fragmentDurationSec;
	cachedFragment->discontinuity = dlInfo->isDiscontinuity;
	segDLFailCount = 0;
	// Update the last downloaded position for buffered duration calculation
	lastDownloadedPosition = dlInfo->absolutePosition + dlInfo->fragmentDurationSec;
	AAMPLOG_INFO("[%s] lastDownloadedPosition %lfs fragmentTime %lfs",
				 GetMediaTypeName(dlInfo->mediaType),
				 lastDownloadedPosition.load(),
				 dlInfo->absolutePosition);
	if ((eTRACK_VIDEO == type) && (!dlInfo->isInitSegment))
	{
		// reset count on video fragment success
		context->mRampDownCount = 0;
	}

	if(tsbSessionManager && cachedFragment->fragment.GetLen())
	{
		std::shared_ptr<CachedFragment> fragmentToTsbSessionMgr = std::make_shared<CachedFragment>();
		fragmentToTsbSessionMgr->Copy(cachedFragment, cachedFragment->fragment.GetLen());
		if(fragmentToTsbSessionMgr->initFragment)
		{
			fragmentToTsbSessionMgr->profileIndex = GetContext()->profileIdxForBandwidthNotification;
			GetContext()->UpdateStreamInfoBitrateData(fragmentToTsbSessionMgr->profileIndex, fragmentToTsbSessionMgr->cacheFragStreamInfo);
		}
		fragmentToTsbSessionMgr->cacheFragStreamInfo.bandwidthBitsPerSecond = fragmentDescriptor.Bandwidth;

		if (CheckEos())
		{
			// A reader EOS check is performed after downloading live edge segment
			// If reader is at EOS, inject the missing live segment directly
			AAMPLOG_INFO("Reader at EOS, Pushing last downloaded data");
			tsbSessionManager->GetTsbReader((AampMediaType)type)->CheckForWaitIfReaderDone();
			// If reader is at EOS, inject the last data in AAMP TSB
			if (aamp->GetLLDashChunkMode())
			{
				CacheTsbFragment(fragmentToTsbSessionMgr);
			}
			SetLocalTSBInjection(false);
		}
		else if (fragmentToTsbSessionMgr->initFragment && !IsLocalTSBInjection() && !aamp->pipeline_paused)
		{
			// In chunk mode, media segments are added to the chunk cache in the SSL callback, but init segments are added here
			if (aamp->GetLLDashChunkMode())
			{
				CacheTsbFragment(fragmentToTsbSessionMgr);
			}
		}
		tsbSessionManager->EnqueueWrite(dlInfo->url, fragmentToTsbSessionMgr, context->GetPeriod()->GetId());
	}
	// Added the duplicate conditional statements, to log only for localAAMPTSB cases.
	else if (tsbSessionManager && cachedFragment->fragment.GetLen() == 0)
	{
		AAMPLOG_WARN("Type[%d] Empty cachedFragment ignored!! fragmentUrl %s fragmentTime %f discontinuity %d scale %u duration %f", type, dlInfo->url.c_str(), dlInfo->pts, dlInfo->isDiscontinuity, dlInfo->timeScale, dlInfo->fragmentDurationSec);
	}
	else if (aamp->GetLLDashChunkMode() && dlInfo->isInitSegment)
	{
		std::shared_ptr<CachedFragment> fragmentToTsbSessionMgr = std::make_shared<CachedFragment>();
		fragmentToTsbSessionMgr->Copy(cachedFragment, cachedFragment->fragment.GetLen());
		if (fragmentToTsbSessionMgr->initFragment)
		{
			fragmentToTsbSessionMgr->profileIndex = GetContext()->profileIdxForBandwidthNotification;
			GetContext()->UpdateStreamInfoBitrateData(fragmentToTsbSessionMgr->profileIndex, fragmentToTsbSessionMgr->cacheFragStreamInfo);
		}
		fragmentToTsbSessionMgr->cacheFragStreamInfo.bandwidthBitsPerSecond = fragmentDescriptor.Bandwidth;
		CacheTsbFragment(fragmentToTsbSessionMgr);
	}

	// If playing back from local TSB, or pending playing back from local TSB as paused, but not paused due to underflow
	if (tsbSessionManager &&
		(IsLocalTSBInjection() || (aamp->pipeline_paused && !aamp->GetBufUnderFlowStatus())))
	{
		AAMPLOG_TRACE("[%s] cachedFragment %p ptr %p not injecting IsLocalTSBInjection %d, aamp->pipeline_paused %d, aamp->GetBufUnderFlowStatus() %d",
			name, cachedFragment, cachedFragment->fragment.GetPtr(), IsLocalTSBInjection(), aamp->pipeline_paused, aamp->GetBufUnderFlowStatus());
		cachedFragment->fragment.Free();
		auto timeBasedBufferManager = GetTimeBasedBufferManager();
		if(timeBasedBufferManager)
		{
			timeBasedBufferManager->ConsumeBuffer(cachedFragment->duration);
		}
	}
	else
	{
		// Update buffer index after fetch for injection
		UpdateTSAfterFetch(dlInfo->isInitSegment);

		// With AAMP TSB enabled, the chunk cache is used for any content type (SLD or LLD)
		// When playing live SLD content, the fragment is written to the regular cache and to the chunk cache
		if(tsbSessionManager && !IsLocalTSBInjection() && !aamp->GetLLDashChunkMode())
		{
			std::shared_ptr<CachedFragment> fragmentToCache = std::make_shared<CachedFragment>();
			fragmentToCache->Copy(cachedFragment, cachedFragment->fragment.GetLen());
			CacheTsbFragment(fragmentToCache);
		}

		// If injection is from chunk buffer, remove the fragment for injection
		if(IsInjectionFromCachedFragmentChunks())
		{
			UpdateTSAfterInject();
			auto timeBasedBufferManager = GetTimeBasedBufferManager();
			if(timeBasedBufferManager)
			{
				timeBasedBufferManager->ConsumeBuffer(cachedFragment->duration);
			}
		}
	}

	if (aamp->IsLive())
	{
		GetContext()->CheckForPlaybackStall(true);
	}
	if ((!GetContext()->trickplayMode) && (eMEDIATYPE_VIDEO == dlInfo->mediaType) && !failAdjacentSegment && !dlInfo->isInitSegment)
	{
		// Check for ABR profile change
		// ABR is performed from TrackWorker thread to ensure the profile change is done in the same thread
		if (aamp->CheckABREnabled())
		{
			GetContext()->CheckForProfileChange();
		}
	}
}

/**
 * @fn OnFragmentDownloadFailed
 * @brief Callback on fragment download failure
 * @param[in] downloadInfo - download information
 */
void MediaStreamContext::OnFragmentDownloadFailed(DownloadInfoPtr dlInfo)
{

	if (mActiveDownloadInfo == nullptr || dlInfo == nullptr)
	{
		AAMPLOG_WARN("OnFragmentDownloadFailed: mActiveDownloadInfo or dlInfo is NULL");
		return;
	}

	// Get active buffer
	CachedFragment *cachedFragment = GetFetchBuffer(false);
	mActiveDownloadInfo = nullptr;
	AAMPLOG_INFO("fragment fetch failed - Free cachedFragment for %d", cachedFragment->type);
	cachedFragment->fragment.Free();
	if (aamp->DownloadsAreEnabled())
	{
		AAMPLOG_WARN("%sfragment fetch failed -- fragmentUrl %s", (dlInfo->isInitSegment) ? "Init " : " ", dlInfo->url.c_str());
		if (mSkipSegmentOnError)
		{
			// Skip segment on error, and increase fail count
			if (httpErrorCode != 502)
			{
				segDLFailCount += 1;
			}
		}
		else
		{
			// Rampdown already attempted on same segment
			// Reset flag for next fetch
			mSkipSegmentOnError = true;
		}
		int FragmentDownloadFailThreshold = GETCONFIGVALUE(eAAMPConfig_FragmentDownloadFailThreshold);
		if (FragmentDownloadFailThreshold <= segDLFailCount)
		{
			if (!dlInfo->isPlayingAd) // If playingAd, we are invalidating the current Ad in onAdEvent().
			{
				if (!dlInfo->isInitSegment)
				{
					if (type != eTRACK_SUBTITLE) // Avoid sending error for failure to download subtitle fragments
					{
						AAMPLOG_ERR("%s Not able to download fragments; reached failure threshold sending tune failed event", name);
						abortWaitForVideoPTS();
						aamp->SetFlushFdsNeededInCurlStore(true);
						aamp->SendDownloadErrorEvent(AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, httpErrorCode);
					}
				}
				else
				{
					// When rampdown limit is not specified, init segment will be ramped down, this will
					AAMPLOG_ERR("%s Not able to download init fragments; reached failure threshold sending tune failed event", name);
					abortWaitForVideoPTS();
					aamp->SetFlushFdsNeededInCurlStore(true);

					aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, httpErrorCode);
				}
			}
		}
		// Profile RampDown check and rampdown is needed only for Video . If audio fragment download fails
		// should continue with next fragment,no retry needed .
		else if ((eTRACK_VIDEO == type) && !ISCONFIGSET(eAAMPConfig_AudioOnlyPlayback) && !(context->CheckForRampDownLimitReached()))
		{
			// Attempt rampdown
			// ABR is performed from TrackWorker thread to ensure the profile change is done in the same thread
			if (context->CheckForRampDownProfile(httpErrorCode))
			{
				mCheckForRampdown = true;
				if (!dlInfo->isInitSegment)
				{
					// Rampdown attempt success, download same segment from lower profile.
					mSkipSegmentOnError = false;
				}
				AAMPLOG_WARN("StreamAbstractionAAMP_MPD::Error while fetching fragment:%s, failedCount:%d. decrementing profile",
							 dlInfo->url.c_str(), segDLFailCount);

				// Submit job to download same fragment from lower profile and push it to the front of the fetch queue.
				// To ensure the init fragment is downloaded from the lower profile, we need to push it to the front of the fetch queue
				// This is done from onFragmentDownloadFailed() from context.
				aamp->GetAampTrackWorkerManager()->GetWorker(dlInfo->mediaType)->RescheduleActiveJob();
			}
			else
			{
				if (!dlInfo->isPlayingAd && dlInfo->isInitSegment && httpErrorCode != 502)
				{
					// Already at lowest profile, send error event for init fragment.
					AAMPLOG_ERR("Not able to download init fragments; reached failure threshold sending tune failed event");
					abortWaitForVideoPTS();
					aamp->SetFlushFdsNeededInCurlStore(true);
					aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, httpErrorCode);
				}
				else
				{
					AAMPLOG_WARN("%s StreamAbstractionAAMP_MPD::Already at the lowest profile, skipping segment at pos:%lf dur:%lf disc:%d", name, dlInfo->pts, dlInfo->fragmentDurationSec, dlInfo->isDiscontinuity);
					if (!dlInfo->isInitSegment)
						updateSkipPoint((dlInfo->pts + dlInfo->fragmentDurationSec), dlInfo->fragmentDurationSec);
					auto timeBasedBufferManager = GetTimeBasedBufferManager();
					if(timeBasedBufferManager)
					{
						// Consume the buffer for the segment duration as segment is skipped
						timeBasedBufferManager->ConsumeBuffer(dlInfo->fragmentDurationSec);
					}
					context->mRampDownCount = 0;
				}
			}
		}
		else if (AampLogManager::isLogworthyErrorCode(httpErrorCode))
		{
			AAMPLOG_ERR("StreamAbstractionAAMP_MPD::Error on fetching %s fragment. failedCount:%d", name, segDLFailCount);
			if (dlInfo->isInitSegment)
			{
				// For init fragment, rampdown limit is reached. Send error event.
				if (!dlInfo->isPlayingAd && httpErrorCode != 502)
				{
					abortWaitForVideoPTS();
					aamp->SetFlushFdsNeededInCurlStore(true);
					aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, httpErrorCode);
				}
			}
			else
			{
				updateSkipPoint((dlInfo->pts + dlInfo->fragmentDurationSec), dlInfo->fragmentDurationSec);
			}
		}
	}
}

/**
 * @fn DownloadFragment
 * @brief Download submitted fragment
 * @param[in] downloadInfo - download information
 *
 * @return true on success
 */
bool MediaStreamContext::DownloadFragment(DownloadInfoPtr dlInfo)
{
	bool retval = true;
	std::string fragmentUrl;

	// Now construct the fragment URL
	if (!dlInfo)
	{
		AAMPLOG_WARN("DownloadFragment called with NULL downloadInfo");
		return false;
	}

	URLInfo urlInfo;
	if (dlInfo->urlList.size() > 0)
	{
		// Asses the current bandwidth and get the appropriate URLInfo from the map with resolved URLs
		if (dlInfo->urlList.find(fragmentDescriptor.Bandwidth) != dlInfo->urlList.end())
		{
			urlInfo = dlInfo->urlList[fragmentDescriptor.Bandwidth];
		}
		if (urlInfo.url.empty() && dlInfo->urlList.size() > 0)
		{
			// If the fragment URL is not found in the map, then use the first URL in the map
			AAMPLOG_WARN("Fragment URL not found in the map, using the first URL in the map");
			urlInfo = dlInfo->urlList.begin()->second;
		}
	}

	// Handle change in bandwidth for segmentBase streams, so need to load new range
	if((dlInfo->bandwidth != fragmentDescriptor.Bandwidth) && IDX.GetPtr() && urlInfo.range.empty())
	{
		// If the bandwidth is different, then set the range
		if (dlInfo->bandwidth > 0)
		{
			dlInfo->fragmentOffset = 0;
			dlInfo->fragmentOffset++; // first byte following packed index
			if (IDX.GetPtr() )
			{
				unsigned int firstOffset;
				ParseSegmentIndexBox(
										IDX.GetPtr(),
										IDX.GetLen(),
										0,
										NULL,
										NULL,
										&firstOffset);
				dlInfo->fragmentOffset += firstOffset;
			}
			if (dlInfo->fragmentOffset != 0 && IDX.GetPtr() )
			{
				unsigned int referenced_size;
				float fragmentDuration;
				AAMPLOG_INFO("current fragmentIndex = %d", dlInfo->fragmentIndex);
				//Find the offset of previous fragment in new representation
				for (int i = 0; i < dlInfo->fragmentIndex; i++)
				{
					if (ParseSegmentIndexBox(
												IDX.GetPtr(),
												IDX.GetLen(),
												i,
												&referenced_size,
												&fragmentDuration,
												NULL))
					{
						dlInfo->fragmentOffset += referenced_size;
					}
				}
			}
			unsigned int referenced_size;
			float fragmentDuration;
			if (ParseSegmentIndexBox(
										IDX.GetPtr(),
										IDX.GetLen(),
										dlInfo->fragmentIndex,
										&referenced_size,
										&fragmentDuration,
										NULL) )
			{
				char range[MAX_RANGE_STRING_CHARS];
				snprintf(range, sizeof(range), "%" PRIu64 "-%" PRIu64 "", dlInfo->fragmentOffset, dlInfo->fragmentOffset + referenced_size - 1);
				AAMPLOG_INFO("%s [%s]",GetMediaTypeName(dlInfo->mediaType), range);
				urlInfo.range = range;
				dlInfo->fragmentDurationSec = fragmentDuration;
			}
		}
		if(!urlInfo.range.empty())
		{
			// If the range is not empty, then set the range
			dlInfo->range = urlInfo.range;
		}
	}

	if (urlInfo.url.empty())
	{
		AAMPLOG_WARN("Fragment URL is empty");
		retval = false;
	}
	else
	{
		dlInfo->url = urlInfo.url;
	}

	AAMPLOG_INFO("[%s] DownloadFragment: %s;%s", name, dlInfo->url.c_str(), dlInfo->range.c_str());

	if (dlInfo->isInitSegment)
	{
		if (!(initialization.empty()) && (0 == initialization.compare(dlInfo->url)) && !dlInfo->isDiscontinuity)
		{
			AAMPLOG_TRACE("We have pushed the same initialization segment for %s skipping", GetMediaTypeName(dlInfo->mediaType));
			return retval;
		}
		else
		{
			initialization = std::string(dlInfo->url);
		}

		double lastInjectedAbsDuration = GetLastInjectedFragmentDuration();
		if((lastInjectedAbsDuration > 0) && !dlInfo->isDiscontinuity)
		{
			// Reset the absolute position to the last injected position for profile change
			AAMPLOG_TRACE("Setting absolute position to last injected position: %lf", lastInjectedAbsDuration);
			dlInfo->absolutePosition = lastInjectedAbsDuration;
		}
	}

	if (retval && aamp->DownloadsAreEnabled())
	{
		if (dlInfo->failoverContentSegment)
		{
			if (mediaType == eMEDIATYPE_VIDEO)
			{
				// Attempt rampdown
				int http_code = 404;
				retval = false;
				if (GetContext()->CheckForRampDownProfile(http_code))
				{
					AAMPLOG_WARN("RampDownProfile Due to failover Content %" PRIu64 " Number %lf FDT", dlInfo->fragmentNumber, dlInfo->pts);
					this->mCheckForRampdown = true;
					// Rampdown attempt success, download same segment from lower profile.
					this->mSkipSegmentOnError = false;
				}
				else
				{
					AAMPLOG_WARN("Already at the lowest profile, skipping segment due to failover");
					GetContext()->mRampDownCount = 0;
				}
				return retval;
			}
		}
		if (!mActiveDownloadInfo)
		{
			// Assign the new download info to mActiveDownloadInfo
			mActiveDownloadInfo = dlInfo;
		}

		int maxCachedFragmentsPerTrack = GETCONFIGVALUE(eAAMPConfig_MaxFragmentCached); // Max cached fragments per track
		PrivAAMPState state = aamp->GetState();
		int timeOut = -1;
		bool lowLatency = aamp->GetLLDashServiceData()->lowLatencyMode;
		// Wait for free fragment only if the number of fragments cached is equal to the max cached fragments per track
		if (numberOfFragmentsCached == maxCachedFragmentsPerTrack)
		{
			timeOut = MAX_WAIT_TIMEOUT_MS;
			while (!WaitForFreeFragmentAvailable(timeOut) && aamp->DownloadsAreEnabled())
			{
				AAMPLOG_TRACE("Waiting for free fragment");
			}
		}
		if (lowLatency)
		{
			while (!aamp->TrackDownloadsAreEnabled(dlInfo->mediaType) && aamp->DownloadsAreEnabled())
			{
				AAMPLOG_TRACE("Waiting for need-data signal");
			}
		}
		retval = CacheFragment(dlInfo->url, dlInfo->curlInstance, dlInfo->pts, dlInfo->fragmentDurationSec, dlInfo->range.c_str(), dlInfo->isInitSegment, dlInfo->isDiscontinuity, dlInfo->isPlayingAd, dlInfo->timeScale);
	}

	return retval;
}
