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
														cachedFragment->duration, cachedFragment->PTSOffsetSec, isDiscontinuity, cachedFragment->initFragment, std::move(processor), ptsError);
	}
	else
	{
		aamp->ProcessID3Metadata(cachedFragment->fragment.GetVector(), (AampMediaType) type);
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

	fragmentDurationSeconds = fragmentDurationS;
	ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia(mediaType, initSegment);
	AampMediaType actualType = (AampMediaType)(initSegment ? (eMEDIATYPE_INIT_VIDEO + mediaType) : mediaType); // Need to revisit the logic

	// Capture download start time BEFORE download begins
	uint64_t downloadStartTime = aamp_GetCurrentTimeMS();

	// Prepare download or retrieve from cache
	BitsPerSecond bitrate = 0;
	double downloadTimeS = 0;
	std::string effectiveUrl;
	int iFogError = -1;
	int iCurrentRate = aamp->rate;
	bool bReadfromcache = false;
	
	// Check if reading from mDownloadedFragment (non-init segment fragment cache)
	if (!initSegment && mDownloadedFragment.capacity() != 0)
	{
		ret = true;
		// Use mDownloadedFragment for non-init segments
		mTempFragment->Replace(&mDownloadedFragment);
	}
	else
	{
		// Try init fragment cache first for init segments
		if (initSegment)
		{
			ret = bReadfromcache = aamp->getAampCacheHandler()->RetrieveFromInitFragmentCache(fragmentUrl, mTempFragment->GetVector(), effectiveUrl);
		}
		
		// If not in cache, download it
		if (!bReadfromcache)
		{
			AampMPDDownloader *dnldInstance = aamp->GetMPDDownloader();
			int maxInitDownloadTimeMS = 0;
			if ((aamp->IsLocalAAMPTsb()) && (dnldInstance))
			{
				maxInitDownloadTimeMS = aamp->mTsbDepthMs - (dnldInstance->GetPublishTime() - (fragmentTime * 1000));
				AAMPLOG_INFO("maxInitDownloadTimeMS %d, initSegment %d, mTsbDepthMs %d, GetPublishTime %llu(ms), fragmentTime %f(s) ",
					maxInitDownloadTimeMS, initSegment, aamp->mTsbDepthMs, (unsigned long long)dnldInstance->GetPublishTime(), fragmentTime);
			}

			ret = aamp->GetFile(fragmentUrl, actualType, mTempFragment.get(), effectiveUrl, &httpErrorCode, &downloadTimeS, range, curlInstance, true/*resetBuffer*/, &bitrate, &iFogError, fragmentDurationS, bucketType, maxInitDownloadTimeMS);
			if (initSegment && ret)
			{
				aamp->getAampCacheHandler()->InsertToInitFragCache(fragmentUrl, mTempFragment->GetVector(), effectiveUrl, actualType);
			}
		}
	}

	// Handle bitrate changes (ramp-down)
	mCheckForRampdown = false;
	if (ret && (bitrate > 0 && bitrate != fragmentDescriptor.Bandwidth))
	{
		AAMPLOG_INFO("Bitrate changed from %" BITSPERSECOND_FORMAT " to %" BITSPERSECOND_FORMAT "",
					 fragmentDescriptor.Bandwidth, bitrate);
		fragmentDescriptor.Bandwidth = (uint32_t)bitrate;
		context->SetTsbBandwidth(bitrate);
		context->mUpdateReason = true;
		mDownloadedFragment.Replace(mTempFragment.get());
		ret = false;
		return ret;
	}

	// Rate change handling for trick play
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

	// Update metrics if not from cache
	if (!bReadfromcache)
	{
		aamp->UpdateVideoEndMetrics(actualType, bitrate ? bitrate : fragmentDescriptor.Bandwidth, (iFogError > 0 ? iFogError : httpErrorCode), effectiveUrl, fragmentDurationS, downloadTimeS);
	}

	// ===== Unified Caching API: Build descriptor and delegate =====
	if (ret && mTempFragment->capacity() != 0)
	{
		FragmentCacheDescriptor desc;
		desc.downloadBuffer = mTempFragment.get();
		desc.url = fragmentUrl;
		desc.position = position;
		desc.duration = fragmentDurationS;
		desc.absolutePosition = mActiveDownloadInfo ? mActiveDownloadInfo->absolutePosition : 0.0;
		desc.timeScale = mActiveDownloadInfo ? mActiveDownloadInfo->timeScale : fragmentDescriptor.TimeScale;
		desc.ptsOffsetSec = mActiveDownloadInfo ? mActiveDownloadInfo->ptsOffset.inSeconds() : 0.0;
		desc.mediaType = actualType;
		desc.curlInstance = curlInstance;
		desc.range = range;
		desc.profileIndex = context->currentProfileIndex;
		desc.isInitSegment = initSegment;
		desc.isDiscontinuity = discontinuity;
		desc.playingAd = playingAd;
		desc.isChunkMode = false;
		desc.skipInitSegmentParsing = false;
		desc.downloadStartTime = downloadStartTime;

		ret = CacheFragmentData(desc);
		
		if (ret)
		{
			mTempFragment->Free();
		}
	}
	else if (!ret)
	{
		AAMPLOG_WARN("[%s] Fragment download/caching failed", name);
	}

	return ret;
}

/**
 *  @brief Cache Fragment Chunk
 */
bool MediaStreamContext::CacheFragmentChunk(AampMediaType actualType, const char *ptr, size_t size, std::string remoteUrl, uint64_t dnldStartTime)
{
	AAMPLOG_DEBUG("[%s] Chunk Buffer Length %zu Remote URL %s", name, size, remoteUrl.c_str());

	// ===== Unified Caching API: Build descriptor and delegate =====
	FragmentCacheDescriptor desc;
	desc.chunkPayload = ptr;
	desc.payloadSize = size;
	desc.url = remoteUrl;
	desc.mediaType = actualType;
	desc.downloadStartTime = dnldStartTime;
	desc.isChunkMode = true;
	desc.skipInitSegmentParsing = true;
	
	// Populate optional fields from context
	if (mActiveDownloadInfo)
	{
		desc.absolutePosition = mActiveDownloadInfo->absolutePosition;
		desc.timeScale = mActiveDownloadInfo->timeScale;
		desc.ptsOffsetSec = mActiveDownloadInfo->ptsOffset.inSeconds();
	}
	else
	{
		desc.absolutePosition = 0.0;
		desc.timeScale = fragmentDescriptor.TimeScale;
		desc.ptsOffsetSec = GetContext()->mPTSOffset.inSeconds();
	}

	return CacheFragmentData(desc);
}

/**
 *  @brief Unified fragment caching implementation
 *  @note Handles both full fragment downloads (fragment mode) and progressive chunks (chunk mode)
 *        Uses zero-copy semantics where possible (fragment mode moves ownership)
 */
bool MediaStreamContext::CacheFragmentData(const FragmentCacheDescriptor& desc)
{
	// =================================================================
	// Step 1: Slot Acquisition (mode-dependent)
	// =================================================================
	CachedFragment* cached = nullptr;
	
	if (desc.isChunkMode)
	{
		// Chunk mode: Wait for injection slot availability (rate-limiting)
		if (!WaitForCachedFragmentChunkInjected())
		{
			AAMPLOG_TRACE("[%s] WaitForCachedFragmentChunkInjected aborted", name);
			return false;
		}
		
		// Acquire chunk buffer slot
		cached = GetFetchChunkBuffer(true);
		if (!cached)
		{
			AAMPLOG_WARN("[%s] GetFetchChunkBuffer failed - no available slot", name);
			return false;
		}
	}
	else
	{
		// Fragment mode: Acquire fragment buffer slot
		// Note: WaitForFreeFragmentAvailable() called externally by fragment collector
		cached = GetFetchBuffer(true);
		if (!cached)
		{
			AAMPLOG_WARN("[%s] GetFetchBuffer failed - no available slot", name);
			return false;
		}
	}
	
	// =================================================================
	// Step 2: Zero-Copy Data Transfer (mode-dependent)
	// =================================================================
	if (desc.isChunkMode)
	{
		// Chunk mode: COPY (unavoidable - CURL buffer is temporary)
		// chunkPayload is transient callback data, must copy before return
		cached->fragment.AppendBytes(desc.chunkPayload, desc.payloadSize);
	}
	else
	{
		// Fragment mode: ZERO-COPY move (ownership transfer)
		// downloadBuffer's contents transferred via move semantics
		// After this, desc.downloadBuffer is empty (moved-from state)
		if (desc.downloadBuffer)
		{
			cached->fragment = std::move(*desc.downloadBuffer);
		}
		else
		{
			AAMPLOG_WARN("[%s] Fragment mode but downloadBuffer is null", name);
			return false;
		}
	}
	
	// =================================================================
	// Step 3: Populate Common Fields
	// =================================================================
	// URI (for debug logging)
	cached->uri = desc.url;
	
	// Type information
	cached->type = desc.mediaType;
	cached->profileIndex = desc.profileIndex;
	
	// Behavioral flags
	cached->initFragment = desc.isInitSegment;
	cached->discontinuity = desc.isDiscontinuity;
	
	// Timestamp (for download metrics)
	cached->downloadStartTime = desc.downloadStartTime;
	
	// NOTE: Timing fields (position, duration, absPosition, timeScale, PTSOffsetSec)
	// are NOT set here. They are populated by OnFragmentDownloadSuccess() which applies
	// PTS restamping and finalizes timing information. This maintains separation of
	// concerns: CacheFragmentData() handles buffer storage, OnFragmentDownloadSuccess()
	// handles timing calculation and TSB integration.
	
	// =================================================================
	// Step 4: Conditional Processing (mode-specific behaviors)
	// =================================================================
	
	// ---- Init segment timescale extraction (FRAGMENT MODE ONLY) ----
	if (!desc.isChunkMode && desc.isInitSegment && !desc.skipInitSegmentParsing)
	{
		// Parse init segment to extract track_id and timeScale
		IsoBmffBuffer buffer;
		buffer.setBuffer((uint8_t*)cached->fragment.GetPtr(), cached->fragment.size());
		buffer.parseBuffer();
		
		uint32_t track_id = 0;
		buffer.getTrack_id(track_id);
		
		if (buffer.isInitSegment())
		{
			uint32_t timeScale = 0;
			if (buffer.getTimeScale(timeScale))
			{
				// Store timeScale in context based on media type
				// Handle both normal init segments and trick-play I-frame init segments
				if (cached->type == eMEDIATYPE_INIT_VIDEO || cached->type == eMEDIATYPE_INIT_IFRAME)
				{
					AAMPLOG_INFO("Video TimeScale [%d]", timeScale);
					aamp->SetVidTimeScale(timeScale);
				}
				else if (cached->type == eMEDIATYPE_INIT_AUDIO)
				{
					AAMPLOG_INFO("Audio TimeScale [%d]", timeScale);
					aamp->SetAudTimeScale(timeScale);
				}
				else if (cached->type == eMEDIATYPE_INIT_SUBTITLE)
				{
					AAMPLOG_INFO("Subtitle TimeScale [%d]", timeScale);
					aamp->SetSubTimeScale(timeScale);
				}
			}
		}
	}
	
	// ---- Discontinuity state management (FRAGMENT MODE ONLY) ----
	if (!desc.isChunkMode && desc.isInitSegment && desc.isDiscontinuity)
	{
		setDiscontinuityState(true);
	}
	
	// ---- TSB (Time Shift Buffer) integration (MODE-DEPENDENT) ----
	// Note: Fragment mode may write to both fragment cache AND TSB chunk cache
	//       Chunk mode only writes to chunk cache (already done in slot acquisition)
	// This dual-cache copy for TSB+fragment mode is unavoidable due to separate
	// storage requirements for live playback vs time-shifted playback
	// TODO: Future optimization could use shared_ptr or refcounted buffers
	
	// =================================================================
	// Step 5: Update Metrics & Counters (mode-dependent)
	// =================================================================
	if (desc.isChunkMode)
	{
		// Chunk mode: Update chunk-specific counters
		UpdateTSAfterChunkFetch();
	}
	else
	{
		// Fragment mode: Update fragment-specific counters
		// Note: UpdateTSAfterFetch() called externally by fragment collector
		// Profiler updates also handled externally
	}
	
	AAMPLOG_TRACE("[%s] CacheFragmentData: %s mode, type=%d, pos=%.3f, dur=%.3f, size=%zu",
		name, desc.isChunkMode ? "chunk" : "fragment", 
		cached->type, cached->position, cached->duration, cached->fragment.size());
	
	return true;
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
		cachedFragment->fragment.clear();
		cachedFragment->Copy(fragment.get(), fragment->fragment.size());
		if(cachedFragment->fragment.GetPtr() && cachedFragment->fragment.size() > 0)
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
 * @fn OnFragmentDownloadSuccess
 * @brief Function called on fragment download success
 * @param[in] downloadInfo - download information
 */
void MediaStreamContext::OnFragmentDownloadSuccess(DownloadInfoPtr dlInfo)
{
	if (nullptr == mActiveDownloadInfo || nullptr == dlInfo || !aamp->DownloadsAreEnabled() || abort)
	{
		AAMPLOG_WARN("mActiveDownloadInfo or dlInfo is NULL or downloads are disabled");
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
	if (ISCONFIGSET(eAAMPConfig_EnablePTSReStamp))
	{
		// apply pts offset to position for restamped pts
		cachedFragment->position += dlInfo->ptsOffset.inSeconds();
		AAMPLOG_INFO("Type[%s] position after restamp = %fs", name, cachedFragment->position);
	}
	cachedFragment->duration = dlInfo->fragmentDurationSec;
	cachedFragment->absPosition = dlInfo->absolutePosition;
	cachedFragment->PTSOffsetSec = dlInfo->ptsOffset.inSeconds();
	if (dlInfo->timeScale > 0)
	{
		cachedFragment->timeScale = dlInfo->timeScale;
	}
	else
	{
		cachedFragment->timeScale = fragmentDescriptor.TimeScale;
	}
	cachedFragment->discontinuity = dlInfo->isDiscontinuity;
	segDLFailCount = 0;
	// Update the last downloaded position for buffered duration calculation
	lastDownloadedPosition.store(dlInfo->absolutePosition + dlInfo->fragmentDurationSec);
	AAMPLOG_DEBUG("[%s] lastDownloadedPosition %lfs fragmentTime %lfs",
				 GetMediaTypeName(dlInfo->mediaType),
				 lastDownloadedPosition.load(),
				 dlInfo->absolutePosition);
	if ((eTRACK_VIDEO == type) && (!dlInfo->isInitSegment))
	{
		// reset count on video fragment success
		context->mRampDownCount = 0;
	}

	if(tsbSessionManager && cachedFragment->fragment.size())
	{
		std::shared_ptr<CachedFragment> fragmentToTsbSessionMgr = std::make_shared<CachedFragment>();
		fragmentToTsbSessionMgr->Copy(cachedFragment, cachedFragment->fragment.size());
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
			// If all of the active media contexts are no longer injecting from TSB, update the AAMP flag
			aamp->UpdateLocalAAMPTsbInjection();
		}
		else if (fragmentToTsbSessionMgr->initFragment && !IsLocalTSBInjection() && !aamp->pipeline_paused)
		{
			// In chunk mode, media segments are added to the chunk cache in the SSL callback, but init segments are added here
			if (aamp->GetLLDashChunkMode())
			{
				CacheTsbFragment(fragmentToTsbSessionMgr);
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
		fragmentToTsbSessionMgr->Copy(cachedFragment, cachedFragment->fragment.size());
		if (fragmentToTsbSessionMgr->initFragment)
		{
			fragmentToTsbSessionMgr->profileIndex = GetContext()->profileIdxForBandwidthNotification;
			GetContext()->UpdateStreamInfoBitrateData(fragmentToTsbSessionMgr->profileIndex, fragmentToTsbSessionMgr->cacheFragStreamInfo);
		}
		fragmentToTsbSessionMgr->cacheFragStreamInfo.bandwidthBitsPerSecond = fragmentDescriptor.Bandwidth;
		CacheTsbFragment(std::move(fragmentToTsbSessionMgr));
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
			fragmentToCache->Copy(cachedFragment, cachedFragment->fragment.size());
			CacheTsbFragment(std::move(fragmentToCache));
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

	if (nullptr == mActiveDownloadInfo || nullptr == dlInfo || !aamp->DownloadsAreEnabled() || abort)
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
	if((dlInfo->bandwidth != fragmentDescriptor.Bandwidth) && IDX.capacity() != 0 && uriInfo.range.empty())
	{
		// If the bandwidth is different, then set the range
		if (dlInfo->bandwidth > 0)
		{
			dlInfo->fragmentOffset = 0;
			dlInfo->fragmentOffset++; // first byte following packed index
			if (IDX.capacity() != 0)
			{
				unsigned int firstOffset;
				ParseSegmentIndexBox(
										IDX.GetPtr(),
										IDX.size(),
										0,
										NULL,
										NULL,
										&firstOffset);
				dlInfo->fragmentOffset += firstOffset;
			}
			if (dlInfo->fragmentOffset != 0 && IDX.capacity() != 0)
			{
				unsigned int referenced_size;
				float fragmentDuration;
				AAMPLOG_DEBUG("current fragmentIndex = %d", dlInfo->fragmentIndex);
				//Find the offset of previous fragment in new representation
				for (int i = 0; i < dlInfo->fragmentIndex; i++)
				{
					if (ParseSegmentIndexBox(
												IDX.GetPtr(),
												IDX.size(),
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
										IDX.size(),
										dlInfo->fragmentIndex,
										&referenced_size,
										&fragmentDuration,
										NULL) )
			{
				char range[MAX_RANGE_STRING_CHARS];
				snprintf(range, sizeof(range), "%" PRIu64 "-%" PRIu64 "", dlInfo->fragmentOffset, dlInfo->fragmentOffset + referenced_size - 1);
				AAMPLOG_INFO("%s [%s]",GetMediaTypeName(dlInfo->mediaType), range);
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
		int maxCachedFragmentsPerTrack = GETCONFIGVALUE(eAAMPConfig_MaxFragmentCached); // Max cached fragments per track
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
		// Wait for free fragment only if the number of fragments cached is equal to the max cached fragments per track
		if (numberOfFragmentsCached == maxCachedFragmentsPerTrack)
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
