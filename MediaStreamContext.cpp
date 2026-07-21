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
#include "AampUtils.h"
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

	if(ISCONFIGSET(eAAMPConfig_SuppressDecode))
	{
		fragmentDiscarded = false;
		return;
	}

	if(playContext)
	{
		MediaProcessor::process_fcn_t processor = [this](AampMediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
		{
		};
		fragmentDiscarded = !playContext->sendSegment(std::move(cachedFragment->fragment), cachedFragment->position,
														cachedFragment->duration, cachedFragment->PTSOffsetSec, isDiscontinuity, cachedFragment->initFragment, std::move(processor), ptsError);
	}
	else
	{
		aamp->ProcessID3Metadata(cachedFragment->fragment, (AampMediaType) type);
		AAMPLOG_DEBUG("Type[%d] cachedFragment->position: %f cachedFragment->duration: %f cachedFragment->initFragment: %d", type, cachedFragment->position,cachedFragment->duration,cachedFragment->initFragment);
		aamp->SendStreamTransfer((AampMediaType)type, cachedFragment->fragment,
		cachedFragment->position, cachedFragment->position, cachedFragment->duration, cachedFragment->PTSOffsetSec, cachedFragment->initFragment, cachedFragment->discontinuity);
		fragmentDiscarded = false;
	}
} // InjectFragmentInternal

/**
 *  @brief Fetch and cache a fragment
 */
bool MediaStreamContext::CacheFragment(std::string fragmentUrl, unsigned int curlInstance, double position, double fragmentDurationS, const char *range, bool initSegment, bool discontinuity, bool playingAd, uint32_t scale)
{
	bool ret = false;
	AAMPLOG_INFO("Type[%d] position(before restamp) %f discontinuity %d scale %u duration %f mPTSOffsetSec %f absTime %lf fragmentUrl %s", type, position, discontinuity, scale, fragmentDurationS, GetContext()->mPTSOffset.inSeconds(), mActiveDownloadInfo->absolutePosition, fragmentUrl.c_str());

	fragmentDurationSeconds = fragmentDurationS;
	ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia(mediaType, initSegment);
	mStagingFragment.Clear();
	CachedFragment *cachedFragment = &mStagingFragment;
	BitsPerSecond bitrate = 0;
	double downloadTimeS = 0;
	AampMediaType actualType = (AampMediaType)(initSegment ? (eMEDIATYPE_INIT_VIDEO + mediaType) : mediaType); // Need to revisit the logic

	PopulateCommonMetadata(cachedFragment, fragmentUrl, actualType, 0, initSegment, discontinuity);
	cachedFragment->timeScale = fragmentDescriptor.TimeScale;
	cachedFragment->absPosition = 0;
	if (mActiveDownloadInfo)
	{
		cachedFragment->absPosition = mActiveDownloadInfo->absolutePosition;
		cachedFragment->timeScale = mActiveDownloadInfo->timeScale;
		cachedFragment->PTSOffsetSec = mActiveDownloadInfo->ptsOffset.inSeconds();
	}
	else
	{
		AAMPLOG_WARN("mActiveDownloadInfo is NULL");
	}

	if (initSegment && discontinuity)
	{
		setDiscontinuityState(true);
	}

	if (!initSegment && !mDownloadedFragment.empty())
	{
		ret = true;
		TransferFragmentBuffer(cachedFragment, nullptr, &mDownloadedFragment, 0, false);

	}
	else
	{
		std::string effectiveUrl;
		int iFogError = -1;
		int iCurrentRate = aamp->rate; //  Store it as back up, As sometimes by the time File is downloaded, rate might have changed due to user initiated Trick-Play
		bool bReadfromcache = false;
		if (initSegment)
		{
			ret = bReadfromcache = aamp->getAampCacheHandler()->RetrieveFromInitFragmentCache(fragmentUrl,cachedFragment->fragment,effectiveUrl);
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

			ret = aamp->GetFile(fragmentUrl, actualType, mTempFragment, effectiveUrl, httpErrorCode, &downloadTimeS, range, curlInstance, true/*resetBuffer*/,  &bitrate, &iFogError, fragmentDurationS, bucketType, maxInitDownloadTimeMS);
			if (initSegment && ret)
			{
				aamp->getAampCacheHandler()->InsertToInitFragCache(fragmentUrl, mTempFragment, effectiveUrl, actualType);
			}
			if (ret)
			{
				TransferFragmentBuffer(cachedFragment, nullptr, &mTempFragment, 0, false);
			}
		}

		// Extract timescale from init segments (video, audio, subtitle)
		// Note: Legacy code also extracted track_id here for mismatch detection,
		// but that value was never used. Omitted in favour of the helper.
		uint32_t initTimeScale = ProcessInitSegmentIfNeeded(cachedFragment, initSegment && ret);
		if (initTimeScale != 0)
		{
			if (actualType == eMEDIATYPE_INIT_VIDEO)
			{
				AAMPLOG_INFO("Video TimeScale [%d]", initTimeScale);
				aamp->SetVidTimeScale(initTimeScale);
			}
			else if (actualType == eMEDIATYPE_INIT_AUDIO)
			{
				AAMPLOG_INFO("Audio TimeScale  [%d]", initTimeScale);
				aamp->SetAudTimeScale(initTimeScale);
			}
			else if (actualType == eMEDIATYPE_INIT_SUBTITLE)
			{
				AAMPLOG_INFO("Subtitle TimeScale  [%d]", initTimeScale);
				aamp->SetSubTimeScale(initTimeScale);
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

		if (!bReadfromcache)
		{
			// update videoend info
			aamp->UpdateVideoEndMetrics(actualType, bitrate ? bitrate : fragmentDescriptor.Bandwidth, (iFogError > 0 ? iFogError : httpErrorCode), effectiveUrl, fragmentDurationS, downloadTimeS);
		}
	}

	mCheckForRampdown = false;
	if (ret && (bitrate > 0 && bitrate != fragmentDescriptor.Bandwidth))
	{
		AAMPLOG_INFO("Bitrate changed from %" BITSPERSECOND_FORMAT " to %" BITSPERSECOND_FORMAT "",
					 fragmentDescriptor.Bandwidth, bitrate);
		fragmentDescriptor.Bandwidth = (uint32_t)bitrate;
		context->SetTsbBandwidth(bitrate);
		context->mUpdateReason = true;
		mDownloadedFragment = std::move(cachedFragment->fragment);
		aamp_utils::ClearAndRelease(cachedFragment->fragment);
		ret = false;
	}
	return ret;
}

/**
 *  @brief Cache Fragment Chunk
 */
bool MediaStreamContext::CacheFragmentChunk(AampMediaType actualType, const uint8_t *ptr, size_t size, std::string remoteUrl, uint64_t dnldStartTime, uint64_t durationInTicks)
{
	AAMPLOG_DEBUG("[%s] Chunk Buffer Length %zu Remote URL %s", name, size, remoteUrl.c_str());

	if (ptr == NULL && size > 0)
	{
		AAMPLOG_WARN("[%s] Null fragment pointer with non-zero size %zu", name, size);
		return false;
	}
	bool ret = true;
	if (WaitForCachedFragmentInjected())
	{
		CachedFragment *cachedFragment = NULL;
		cachedFragment = GetFetchBuffer(true);
		if (NULL == cachedFragment)
		{
			AAMPLOG_WARN("[%s] Something Went wrong - Can't get FetchChunkBuffer", name);
			return false;
		}
		PopulateCommonMetadata(cachedFragment, std::move(remoteUrl), actualType, 0, false, false);
		TransferFragmentBuffer(cachedFragment, ptr, nullptr, size, true);
		cachedFragment->absPosition = 0;
		cachedFragment->downloadStartTime = dnldStartTime;

		cachedFragment->timeScale = fragmentDescriptor.TimeScale;
		if (mActiveDownloadInfo)
		{
			cachedFragment->absPosition = mActiveDownloadInfo->absolutePosition;
			cachedFragment->timeScale = mActiveDownloadInfo->timeScale;
			cachedFragment->duration = (double)durationInTicks / (double)cachedFragment->timeScale;
			mActiveDownloadInfo->chunkDurationSec += cachedFragment->duration;
			// Only update when absPosition is set to avoid messing up the values.
			if (cachedFragment->absPosition > 0)
			{
				AAMPLOG_DEBUG("[%s] Updating last downloaded position[chunkDuration:%f]. Previous: %f, New: %f",
					name, mActiveDownloadInfo->chunkDurationSec, lastDownloadedPosition.load(),
					cachedFragment->absPosition + mActiveDownloadInfo->chunkDurationSec);
				lastDownloadedPosition.store(cachedFragment->absPosition + mActiveDownloadInfo->chunkDurationSec);
				if (eTRACK_VIDEO == type)
				{
					// Notify the underflow monitor for LL-DASH chunks.
					// Paused-state gating to 0.0f is handled inside
					// NotifyVideoFragmentToUnderflowMonitor under its mutex.
					GetContext()->NotifyVideoFragmentToUnderflowMonitor(
						cachedFragment->absPosition + mActiveDownloadInfo->chunkDurationSec,
						aamp->rate);
					// Notify the latency monitor so it can wake its worker early on
					// danger-buffer onset rather than waiting for the next scheduled poll.
					{
						const double bufferMs = aamp->GetMinAVBufferedDurationSecs() * 1000.0;
						if (bufferMs >= 0.0)
						{
							GetContext()->NotifyBufferLevelToLatencyMonitor(bufferMs);
						}
					}
				}
			}
		}
		/* The value of PTSOffsetSec in the context can get updated at the start of a period before
		 * the last segment from the previous period has been injected, hence we copy it
		 */
		cachedFragment->PTSOffsetSec = GetContext()->mPTSOffset.inSeconds();

		AAMPLOG_TRACE("[%s] cachedFragment %p ptr %p", name, cachedFragment, cachedFragment->fragment.data());
		UpdateTSAfterFetch();
	}
	else
	{
		AAMPLOG_TRACE("[%s] WaitForCachedFragmentInjected aborted", name);
		ret = false;
	}
	return ret;
}

/**
 *  @brief Unified fragment caching implementation
 *  @note Phase 2: Stub implementation - will be fully implemented in Phase 3
 */
bool MediaStreamContext::CacheFragmentData(const FragmentCacheDescriptor& desc)
{
	// Phase 2 stub: Not yet implemented
	// This will be implemented in Phase 3 with unified logic
	AAMPLOG_WARN("[%s] CacheFragmentData() called but not yet implemented (Phase 2 stub)", name);
	return false;
}

/**
 *  @brief Transfer buffer data into a CachedFragment.
 *
 *  In chunk mode the data is assigned (copied) from the ephemeral CURL
 *  callback pointer into the CachedFragment.
 *  In fragment mode the download buffer is moved (zero-copy) into the cached
 *  fragment, leaving the source empty.
 *
 *  @param[out] cached         Destination CachedFragment.
 *  @param[in]  chunkPayload   Chunk data pointer (chunk mode only).
 *  @param[in]  downloadBuffer Source vector buffer (fragment mode only).
 *  @param[in]  payloadSize    Chunk payload size in bytes.
 *  @param[in]  isChunkMode    true = assign from raw pointer, false = move from download buffer.
 */
void MediaStreamContext::TransferFragmentBuffer(CachedFragment* cached,
		const uint8_t* chunkPayload,
		std::vector<uint8_t>* downloadBuffer,
		size_t payloadSize,
		bool isChunkMode)
{
	if (isChunkMode)
	{
		if (payloadSize == 0 || chunkPayload == nullptr)
		{
			cached->fragment.clear();
			return;
		}

		cached->fragment.assign(chunkPayload, chunkPayload + payloadSize);
	}
	else
	{
		if (downloadBuffer)
		{
			cached->fragment = std::move(*downloadBuffer);
			aamp_utils::ClearAndRelease(*downloadBuffer);
		}
	}
}

/**
 *  @brief Populate common metadata fields shared by fragment and chunk paths.
 *
 *  CRITICAL: This helper intentionally does NOT set position, duration, or
 *  absPosition.  Those fields are lifecycle-dependent:
 *    - Fragment mode: set later by OnFragmentDownloadSuccess
 *    - Chunk mode: set by the caller immediately after this helper returns
 *
 *  @param[out] cached          Destination CachedFragment.
 *  @param[in]  url             Fragment URL (moved into cached->uri).
 *  @param[in]  mediaType       AampMediaType of this fragment.
 *  @param[in]  profileIndex    ABR profile index.
 *  @param[in]  isInitSegment   true for init segments.
 *  @param[in]  isDiscontinuity true when a PTS discontinuity precedes this fragment.
 */
void MediaStreamContext::PopulateCommonMetadata(CachedFragment* cached,
                                                std::string url,
                                                AampMediaType mediaType,
                                                int profileIndex,
                                                bool isInitSegment,
                                                bool isDiscontinuity)
{
	cached->type = mediaType;
	cached->initFragment = isInitSegment;
	cached->uri = std::move(url);
	cached->profileIndex = profileIndex;
	cached->discontinuity = isDiscontinuity;
}

/**
 *  @brief Parse an init segment and extract the timescale.
 *
 *  When isInitSegment is true the cached fragment buffer is parsed as ISO BMFF.
 *  If a valid timescale is found it is extracted and returned to the caller,
 *  which is responsible for applying it to the appropriate AAMP track
 *  (video, audio, or subtitle). This function is a no-op for non-init segments
 *  or non-init media types and returns 0 in those cases.
 *
 *  @param[in] cached        CachedFragment containing the init segment data.
 *  @param[in] isInitSegment true if this fragment is an init segment.
 *  @return Extracted timescale, or 0 if not applicable or extraction failed.
 */
uint32_t MediaStreamContext::ProcessInitSegmentIfNeeded(const CachedFragment* cached,
                                                        bool isInitSegment)
{
	if (!isInitSegment)
	{
		return 0;
	}

	AampMediaType actualType = cached->type;

	if (actualType != eMEDIATYPE_INIT_VIDEO &&
		actualType != eMEDIATYPE_INIT_AUDIO &&
		actualType != eMEDIATYPE_INIT_SUBTITLE)
	{
		AAMPLOG_TRACE("Skipping init segment processing for type %d", actualType);
		return 0;
	}

	IsoBmffBuffer buffer;
	buffer.setBuffer(cached->fragment);
	if (!buffer.parseBuffer())
	{
		AAMPLOG_WARN("Failed to parse init segment buffer (type %d, size %zu)",
			actualType, cached->fragment.size());
		return 0;
	}

	if (buffer.isInitSegment())
	{
		uint32_t timeScale = 0;
		if (buffer.getTimeScale(timeScale))
		{
			return timeScale;
		}
	}
	return 0;
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
	std::lock_guard<std::recursive_mutex> lock(mMediaStreamContextMutex);
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
	mEffectiveUrl = std::move(url);
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
bool MediaStreamContext::CacheTsbFragment(std::shared_ptr<CachedFragment>&& fragment)
{
	// FN_TRACE_F_MPD( __FUNCTION__ );
	std::lock_guard<std::mutex> lock(fetchChunkBufferMutex);
	bool ret = false;
	if(!fragment->fragment.empty() && WaitForCachedFragmentInjected())
	{
		AAMPLOG_TRACE("Type[%s] fragmentTime %f discontinuity %d duration %f initFragment:%d", name, fragment->position, fragment->discontinuity, fragment->duration, fragment->initFragment);
		CachedFragment* cachedFragment = GetFetchBuffer(true);
		if(!cachedFragment)
		{
			AAMPLOG_ERR("[%s] GetFetchBuffer returned null", name);
			return false;
		}
		if(!cachedFragment->fragment.empty())
		{
			// Slot was not cleared after previous use; the assignment below will overwrite and release the old data.
			AAMPLOG_WARN("Fetch buffer has junk data; previous slot was not cleared after use");
		}
		*cachedFragment = std::move(*fragment);
		if(!cachedFragment->fragment.empty())
		{
			ret = true;
			UpdateTSAfterFetch();
		}
		else
		{
			AAMPLOG_TRACE("Empty fragment, not injecting");
			aamp_utils::ClearAndRelease(cachedFragment->fragment);
		}
	}
	else
	{
		AAMPLOG_WARN("[%s] Failed to update inject", name);
	}
	return ret;
}

/**
 * @fn CacheStagingFragmentForInjection
 * @brief Copy the staging fragment into a chunk-cache slot and signal the
 *        inject thread (non-LLD DASH path only).
 *
 *  Pre-populates profileIndex and cacheFragStreamInfo on the slot before
 *  handing it to the inject thread.  UpdateTSAfterFetchStats runs after this
 *  call on the separate mStagingFragment object, so without this population
 *  the chunk slot would carry zeroed cacheFragStreamInfo, causing
 *  NotifyBitRateUpdate to skip AAMP_EVENT_BITRATE_CHANGED.
 */
void MediaStreamContext::CacheStagingFragmentForInjection()
{
	std::shared_ptr<CachedFragment> fragmentToCache = std::make_shared<CachedFragment>();
	fragmentToCache->Copy(mStagingFragment);
	if (auto* pContext = GetContext())
	{
		fragmentToCache->profileIndex = pContext->profileIdxForBandwidthNotification;
		pContext->UpdateStreamInfoBitrateData(fragmentToCache->profileIndex,
											 fragmentToCache->cacheFragStreamInfo);
	}
	CacheTsbFragment(std::move(fragmentToCache));
}

/**
 * @fn OnFragmentDownloadSuccess
 * @brief Function called on fragment download success
 * @param[in] downloadInfo - download information
 */
void MediaStreamContext::OnFragmentDownloadSuccess(DownloadInfoPtr dlInfo)
{
	if (nullptr == mActiveDownloadInfo || nullptr == dlInfo || !aamp->DownloadsAreEnabled() || abort)
	{
		AAMPLOG_WARN("mActiveDownloadInfo or dlInfo is NULL or downloads are disabled. DownloadsAreEnabled=%d abort=%d",
			aamp->DownloadsAreEnabled(), abort);
		return;
	}

	// Get staging fragment populated by CacheFragment
	CachedFragment *cachedFragment = &mStagingFragment;
	mActiveDownloadInfo = nullptr;
	AampTSBSessionManager *tsbSessionManager = aamp->GetTSBSessionManager();

	auto CheckEos = [this, &tsbSessionManager]()
	{
		return IsLocalTSBInjection() &&
			   AAMP_NORMAL_PLAY_RATE == aamp->rate &&
			   // Allow EOS detection when playback is not paused, or when pause is due to buffer underflow
			   (!aamp->mSinkPaused.load() || aamp->GetBufUnderFlowStatus()) &&
			   eTUNETYPE_SEEKTOLIVE == context->mTuneType &&
			   tsbSessionManager &&
			   tsbSessionManager->GetTsbReader((AampMediaType)type) &&
			   tsbSessionManager->GetTsbReader((AampMediaType)type)->IsEos();
	};

	cachedFragment->position = dlInfo->pts;
	if (ISCONFIGSET(eAAMPConfig_EnablePTSReStamp))
	{
		// apply pts offset to position for restamped pts
		cachedFragment->position += dlInfo->ptsOffset.inSeconds();
		AAMPLOG_INFO("Type[%s] position after restamp = %fs", name, cachedFragment->position);
	}
	cachedFragment->duration = dlInfo->fragmentDurationSec;
	cachedFragment->discontinuity = dlInfo->isDiscontinuity;
	segDLFailCount = 0;
	// Update the last downloaded position for buffered duration calculation
	lastDownloadedPosition.store(dlInfo->absolutePosition + dlInfo->fragmentDurationSec);
	AAMPLOG_DEBUG("[%s] lastDownloadedPosition %lfs fragmentTime %lfs",
				 GetMediaTypeName(dlInfo->mediaType),
				 lastDownloadedPosition.load(),
				 dlInfo->absolutePosition);
	// Snapshot the underflow state BEFORE calling NotifyVideoFragmentToUnderflowMonitor.
	// That call may invoke SetBufferingState(false), which clears mBufUnderFlowStatus and
	// resumes the GStreamer pipeline.  Shortly afterwards GStreamer may fire a buffering(0)
	// event on another thread, re-setting mSinkPaused=true.  The TSB discard check below
	// (isPipelinePaused && !GetBufUnderFlowStatus()) would then incorrectly throw away this
	// fragment — the one that just ended the underflow — leaving the inject loop starved and
	// the player in a permanent stall.  Carrying the pre-notify flag forward ensures we
	// always inject the fragment that triggered underflow recovery.
	const bool wasUnderFlowActive = aamp->GetBufUnderFlowStatus();
	if ((eTRACK_VIDEO == type) && (!dlInfo->isInitSegment))
	{
		// reset count on video fragment success
		context->mRampDownCount = 0;
		// Notify the underflow monitor — re-arms the drain deadline.
		// Paused-state gating to 0.0f is handled inside
		// NotifyVideoFragmentToUnderflowMonitor under its mutex.
		context->NotifyVideoFragmentToUnderflowMonitor(
			dlInfo->absolutePosition + dlInfo->fragmentDurationSec,
			aamp->rate);
		// Notify the latency monitor so it can wake its worker early on
		// danger-buffer onset rather than waiting for the next scheduled poll.
		{
			const double bufferMs = aamp->GetMinAVBufferedDurationSecs() * 1000.0;
			if (bufferMs >= 0.0)
			{
				context->NotifyBufferLevelToLatencyMonitor(bufferMs);
			}
		}
	}

	if(tsbSessionManager && cachedFragment->fragment.size())
	{
		std::shared_ptr<CachedFragment> fragmentToTsbSessionMgr = std::make_shared<CachedFragment>();
		fragmentToTsbSessionMgr->Copy(*cachedFragment);
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
				auto fragmentForChunkCache = std::make_shared<CachedFragment>(*fragmentToTsbSessionMgr);
				CacheTsbFragment(std::move(fragmentForChunkCache));
			}
			SetLocalTSBInjection(false);
			// If all of the active media contexts are no longer injecting from TSB, update the AAMP flag
			aamp->UpdateLocalAAMPTsbInjection();
		}
		else if (fragmentToTsbSessionMgr->initFragment && !IsLocalTSBInjection() && !aamp->mSinkPaused.load())
		{
			// In chunk mode, media segments are added to the chunk cache in the SSL callback, but init segments are added here
			if (aamp->GetLLDashChunkMode())
			{
				auto fragmentForChunkCache = std::make_shared<CachedFragment>(*fragmentToTsbSessionMgr);
				CacheTsbFragment(std::move(fragmentForChunkCache));
			}
		}
		tsbSessionManager->EnqueueWrite(std::move(dlInfo->url), std::move(fragmentToTsbSessionMgr), context->GetPeriod()->GetId());
	}
	// Added the duplicate conditional statements, to log only for localAAMPTSB cases.
	else if (tsbSessionManager && cachedFragment->fragment.size() == 0)
	{
		AAMPLOG_WARN("Type[%d] Empty cachedFragment ignored!! fragmentUrl %s fragmentTime %f discontinuity %d scale %u duration %f", type, dlInfo->url.c_str(), dlInfo->pts, dlInfo->isDiscontinuity, dlInfo->timeScale, dlInfo->fragmentDurationSec);
	}
	else if (aamp->GetLLDashChunkMode() && dlInfo->isInitSegment)
	{
		std::shared_ptr<CachedFragment> fragmentToTsbSessionMgr = std::make_shared<CachedFragment>();
		fragmentToTsbSessionMgr->Copy(*cachedFragment);
		if (fragmentToTsbSessionMgr->initFragment)
		{
			fragmentToTsbSessionMgr->profileIndex = GetContext()->profileIdxForBandwidthNotification;
			GetContext()->UpdateStreamInfoBitrateData(fragmentToTsbSessionMgr->profileIndex, fragmentToTsbSessionMgr->cacheFragStreamInfo);
		}
		fragmentToTsbSessionMgr->cacheFragStreamInfo.bandwidthBitsPerSecond = fragmentDescriptor.Bandwidth;
		CacheTsbFragment(std::move(fragmentToTsbSessionMgr));
	}

	// If playing back from local TSB, or pending playing back from local TSB as paused, but not paused due to underflow.
	// Use wasUnderFlowActive (captured before the underflow-monitor notify above) to guard against a race where
	// GStreamer's buffering(0) message re-sets mSinkPaused=true after SetBufferingState(false) has already
	// cleared mBufUnderFlowStatus — which would otherwise cause this recovery fragment to be discarded.
	bool isPipelinePaused = aamp->mSinkPaused.load();
	if (tsbSessionManager &&
		(IsLocalTSBInjection() || (isPipelinePaused && !aamp->GetBufUnderFlowStatus() && !wasUnderFlowActive)))
	{
		AAMPLOG_TRACE("[%s] cachedFragment %p ptr %p not injecting IsLocalTSBInjection %d, aamp->mSinkPaused %d, aamp->GetBufUnderFlowStatus() %d",
			name, cachedFragment, cachedFragment->fragment.data(), IsLocalTSBInjection(), isPipelinePaused, aamp->GetBufUnderFlowStatus());
		aamp_utils::ClearAndRelease(cachedFragment->fragment);
		auto timeBasedBufferManager = GetTimeBasedBufferManager();
		if(timeBasedBufferManager)
		{
			timeBasedBufferManager->ConsumeBuffer(cachedFragment->duration);
		}
	}
	else
	{
		// Both SLD and LLD consume the time-based buffer counter.
		// SLD also caches the fragment for the inject thread first (see below).
		auto consumeBuffer = [this]()
		{
			auto timeBasedBufferManager = GetTimeBasedBufferManager();
			if (timeBasedBufferManager)
			{
				timeBasedBufferManager->ConsumeBuffer(mStagingFragment.duration);
			}
		};

		if (!aamp->GetLLDashChunkMode())
		{
			// Non-LLD DASH (SLD, AAMP TSB write-phase): signal the inject thread
			// before UpdateTSAfterFetchStats fires NotifyFragmentCachingComplete.
			CacheStagingFragmentForInjection();
			if (aamp->IsLocalAAMPTsb())
			{
				consumeBuffer();
			}
		}
		else
		{
			// LLD DASH: media data already injected via CacheFragmentChunk callbacks.
			// Only the buffer counter needs consuming; staging data is discarded.
			consumeBuffer();
		}
		// Update fetch statistics after the inject thread has been signalled,
		// so that any NotifyFragmentCachingComplete fired here arrives after
		// the inject thread already has data to forward to GStreamer.
		UpdateTSAfterFetchStats(&mStagingFragment, dlInfo->isInitSegment);
		mStagingFragment.Clear();
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

	if (nullptr == mActiveDownloadInfo || nullptr == dlInfo || !aamp->DownloadsAreEnabled() || abort)
	{
		AAMPLOG_WARN("OnFragmentDownloadFailed: mActiveDownloadInfo or dlInfo is NULL");
		return;
	}

	// Get staging fragment populated by CacheFragment
	CachedFragment *cachedFragment = &mStagingFragment;
	mActiveDownloadInfo = nullptr;
	AAMPLOG_INFO("fragment fetch failed - Free cachedFragment for %d", cachedFragment->type);
	aamp_utils::ClearAndRelease(cachedFragment->fragment);
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
					if (context->IsCurrentProfileLowest())
					{
						AAMPLOG_WARN("%s StreamAbstractionAAMP_MPD::Already at the lowest profile, skipping segment at pos:%lf dur:%lf disc:%d", name, dlInfo->pts, dlInfo->fragmentDurationSec, dlInfo->isDiscontinuity);
					}
					else
					{
						AAMPLOG_WARN("%s StreamAbstractionAAMP_MPD::Rampdown not applied for error:%d; skipping segment at pos:%lf dur:%lf disc:%d", name, httpErrorCode, dlInfo->pts, dlInfo->fragmentDurationSec, dlInfo->isDiscontinuity);
					}
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
			auto timeBasedBufferManager = GetTimeBasedBufferManager();
			if(timeBasedBufferManager)
			{
				// Consume the buffer for the segment duration as segment is skipped
				timeBasedBufferManager->ConsumeBuffer(dlInfo->fragmentDurationSec);
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

	URIInfo uriInfo;
	if (dlInfo->uriList.size() > 0)
	{
		// Asses the current bandwidth and get the appropriate URIInfo from the map with resolved URLs
		if (dlInfo->uriList.find(fragmentDescriptor.Bandwidth) != dlInfo->uriList.end())
		{
			uriInfo = dlInfo->uriList[fragmentDescriptor.Bandwidth];
		}
		if (uriInfo.url.empty() && dlInfo->uriList.size() > 0)
		{
			// If the fragment URL is not found in the map, then use the first URL in the map
			AAMPLOG_WARN("Fragment URL not found in the map, using the first URL in the map");
			uriInfo = dlInfo->uriList.begin()->second;
		}
	}

	// Handle change in bandwidth for segmentBase streams, so need to load new range
	if((dlInfo->bandwidth != fragmentDescriptor.Bandwidth) && !IDX.empty() && uriInfo.range.empty())
	{
		std::lock_guard<std::mutex> idxLock(mIdxMutex);
		// If the bandwidth is different, then set the range
		if (dlInfo->bandwidth > 0)
		{
			// mIdxBaseOffset is the byte position of segment 0 in the file for the
			// current IDX profile, captured when IDX was loaded in the FetcherLoop.
			// Using it here avoids the previous bug of starting from offset 1,
			// which landed inside the moov+SIDX prefix and fetched wrong byte ranges.
			dlInfo->fragmentOffset = mIdxBaseOffset;
			unsigned int referenced_size = 0;
			float fragmentDuration = 0.0f;
			AAMPLOG_DEBUG("current fragmentIndex = %d", dlInfo->fragmentIndex);
			// Find the offset of previous fragment in new representation
			for (int i = 0; i < dlInfo->fragmentIndex; i++)
			{
				if (ParseSegmentIndexBox(IDX.data(),
										 IDX.size(),
										 i,
										 &referenced_size,
										 &fragmentDuration,
										 NULL))
				{
					dlInfo->fragmentOffset += referenced_size;
				}
			}
			if (ParseSegmentIndexBox(IDX.data(),
									 IDX.size(),
									 dlInfo->fragmentIndex,
									 &referenced_size,
									 &fragmentDuration,
									 NULL))
			{
				char range[MAX_RANGE_STRING_CHARS];
				snprintf(range, sizeof(range), "%" PRIu64 "-%" PRIu64 "", dlInfo->fragmentOffset, dlInfo->fragmentOffset + referenced_size - 1);
				AAMPLOG_INFO("%s [%s]", GetMediaTypeName(dlInfo->mediaType), range);
				uriInfo.range = range;
				dlInfo->fragmentDurationSec = fragmentDuration;
			}
		}
		if(!uriInfo.range.empty())
		{
			// If the range is not empty, then set the range
			dlInfo->range = uriInfo.range;
		}
	}

	if (uriInfo.url.empty())
	{
		AAMPLOG_WARN("Fragment URL is empty");
		retval = false;
	}
	else
	{
		dlInfo->url = uriInfo.url;
	}

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

		if(lastDownloadedPosition > 0)
		{
			// Reset the absolute position to the last injected position for profile change
			AAMPLOG_TRACE("Setting absolute position to last injected position: %lf", lastDownloadedPosition.load());
			dlInfo->absolutePosition = lastDownloadedPosition.load();
		}
	}

	AAMPLOG_DEBUG("[%s] DownloadFragment from position:%lf url:%s;%s", name, dlInfo->absolutePosition, dlInfo->url.c_str(), dlInfo->range.c_str());

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
		auto DownloadsEnabled = [this]()
		{
			return aamp->DownloadsAreEnabled() && !abort;
		};
		// In low-latency mode, wait for needData/enoughData signals before
		// downloading the next fragment. Skip this wait when injecting from
		// the local AAMP TSB.
		auto WaitForLowLatencyDashDownloads = [this, DownloadsEnabled]()
		{
			return DownloadsEnabled() &&
				   !aamp->IsLocalAAMPTsbInjection() &&
				   aamp->GetLLDashServiceData()->lowLatencyMode &&
				   !aamp->TrackDownloadsAreEnabled(mediaType);
		};
		// Wait for a free cache slot before starting the download.
		// IsFragmentCacheFull() checks the unified fragment chunk cache usage, so
		// this wait throttles downloads until shared cache capacity is available.
		// Skip the wait when playing from local TSB
		if (IsFragmentCacheFull() && !aamp->IsLocalAAMPTsbInjection())
		{
			while (DownloadsEnabled() && !WaitForFreeFragmentAvailable(MAX_WAIT_TIMEOUT_MS))
			{
				AAMPLOG_TRACE("Waiting for free fragment");
			}
		}
		while (WaitForLowLatencyDashDownloads())
		{
			// Avoid tight loop for low latency mode when track downloads are disabled
			aamp->interruptibleMsSleep(100);
			AAMPLOG_TRACE("Waiting for track downloads to be enabled in low latency mode");
		}
		if (DownloadsEnabled())
		{
			retval = CacheFragment(dlInfo->url, dlInfo->curlInstance, dlInfo->pts, dlInfo->fragmentDurationSec, dlInfo->range.c_str(), dlInfo->isInitSegment, dlInfo->isDiscontinuity, dlInfo->isPlayingAd, dlInfo->timeScale);
		}
	}

	return retval;
}
