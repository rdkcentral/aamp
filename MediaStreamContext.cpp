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
bool MediaStreamContext::CacheFragment(std::string fragmentUrl, unsigned int curlInstance, double position, double fragmentDurationS, const char *range, bool initSegment, bool discontinuity, bool playingAd, double pto, uint32_t scale, bool overWriteTrackId)
{
	bool ret = false;
	double posInAbsTimeline = ((double)fragmentTime);
	AAMPLOG_INFO("Type[%d] position(before restamp) %f discontinuity %d pto %f scale %u duration %f mPTSOffsetSec %f absTime %lf fragmentUrl %s", type, position, discontinuity, pto, scale, fragmentDurationS, GetContext()->mPTSOffset.inSeconds(), posInAbsTimeline, fragmentUrl.c_str());

	fragmentDurationSeconds = fragmentDurationS;
	ProfilerBucketType bucketType = aamp->GetProfilerBucketForMedia(mediaType, initSegment);
	CachedFragment* cachedFragment = GetFetchBuffer(true);
	BitsPerSecond bitrate = 0;
	double downloadTimeS = 0;
	AampMediaType actualType = (AampMediaType)(initSegment?(eMEDIATYPE_INIT_VIDEO+mediaType):mediaType); //Need to revisit the logic

	cachedFragment->type = actualType;
	cachedFragment->initFragment = initSegment;
	cachedFragment->timeScale = fragmentDescriptor.TimeScale;
	cachedFragment->uri = fragmentUrl; // For debug output
	cachedFragment->absPosition = posInAbsTimeline;
	/* The value of PTSOffsetSec in the context can get updated at the start of a period before
	 * the last segment from the previous period has been injected, hence we copy it
	 */
	cachedFragment->PTSOffsetSec = GetContext()->mPTSOffset.inSeconds();
	if(ISCONFIGSET(eAAMPConfig_EnablePTSReStamp))
	{
		// apply pts offset to position which ends up getting put into gst_buffer in sendHelper
		position += GetContext()->mPTSOffset.inSeconds();
		AAMPLOG_INFO("Type[%d] position after restamp = %fs", type, position);
	}
	AampTSBSessionManager *tsbSessionManager = aamp->GetTSBSessionManager();

	auto CheckEos = [this, &tsbSessionManager, &actualType]() {
		return IsLocalTSBInjection() &&
			AAMP_NORMAL_PLAY_RATE == aamp->rate &&
			!aamp->pipeline_paused &&
			eTUNETYPE_SEEKTOLIVE == context->mTuneType &&
			tsbSessionManager &&
			tsbSessionManager->GetTsbReader((AampMediaType)type) &&
			tsbSessionManager->GetTsbReader((AampMediaType)type)->IsEos();
	};

	if(initSegment && discontinuity )
	{
		setDiscontinuityState(true);
	}

	if(!initSegment && mDownloadedFragment.GetPtr() )
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
		if(initSegment)
		{
			ret = bReadfromcache = aamp->getAampCacheHandler()->RetrieveFromInitFragmentCache(fragmentUrl,&cachedFragment->fragment,effectiveUrl);
		}
		if(!bReadfromcache)
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
			if(actualType == eMEDIATYPE_VIDEO)
			{
				actualType = eMEDIATYPE_IFRAME;
			}
			else if(actualType == eMEDIATYPE_INIT_VIDEO)
			{
				actualType = eMEDIATYPE_INIT_IFRAME;
			}
		}
		else
		{
			if ((actualType == eMEDIATYPE_INIT_VIDEO || actualType == eMEDIATYPE_INIT_AUDIO || actualType == eMEDIATYPE_INIT_SUBTITLE) && ret) // Only if init fragment successful or available from cache
			{
				//To read track_id from the init fragments to check if there any mismatch.
				//A mismatch in track_id is not handled in the gstreamer version 1.10.4
				//But is handled in the latest version (1.18.5),
				//so upon upgrade to it or introduced a patch in qtdemux,
				//this portion can be reverted
				IsoBmffBuffer buffer;
				buffer.setBuffer((uint8_t *)cachedFragment->fragment.GetPtr(), cachedFragment->fragment.GetLen() );
				buffer.parseBuffer();
				uint32_t track_id = 0;
				buffer.getTrack_id(track_id);
				if(buffer.isInitSegment())
				{
					uint32_t timeScale = 0;
					buffer.getTimeScale(timeScale);
					if(actualType == eMEDIATYPE_INIT_VIDEO)
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
				if(actualType == eMEDIATYPE_INIT_VIDEO)
				{
					AAMPLOG_INFO("Video track_id read from init fragment: %d ", track_id);
					bool trackIdUpdated = false;
					if(aamp->mCurrentVideoTrackId != -1 && track_id != aamp->mCurrentVideoTrackId)
					{
						if(overWriteTrackId)
						{
							//Overwrite the track id of the init fragment with the existing track id since overWriteTrackId is true only while pushing the encrypted init fragment while clear content is being played
							buffer.parseBuffer(false, aamp->mCurrentVideoTrackId);
							AAMPLOG_WARN("Video track_id of the current track is overwritten as init fragment is pushing only for DRM purpose, track id: %d ", track_id);
							trackIdUpdated = true;
						}
						else
						{
							aamp->mIsTrackIdMismatch = true;
							AAMPLOG_WARN("TrackId mismatch detected for video, current track_id: %d, next period track_id: %d", aamp->mCurrentVideoTrackId, track_id);
						}
					}
					if(!trackIdUpdated)
					{
						aamp->mCurrentVideoTrackId = track_id;
					}
				}
				else if(actualType == eMEDIATYPE_INIT_AUDIO)
				{
					bool trackIdUpdated = false;
					AAMPLOG_INFO("Audio track_id read from init fragment: %d ", track_id);
					if(aamp->mCurrentAudioTrackId != -1 && track_id != aamp->mCurrentAudioTrackId)
					{
						if(overWriteTrackId)
						{
							buffer.parseBuffer(false, aamp->mCurrentAudioTrackId);
							AAMPLOG_WARN("Audio track_id of the current track is overwritten as init fragment is pushing only for DRM purpose, track id: %d ", track_id);
							trackIdUpdated = true;
						}
						else
						{
							aamp->mIsTrackIdMismatch = true;
							AAMPLOG_WARN("TrackId mismatch detected for audio, current track_id: %d, next period track_id: %d", aamp->mCurrentAudioTrackId, track_id);
						}
					}
					if(!trackIdUpdated)
					{
						aamp->mCurrentAudioTrackId = track_id;
					}
				}
				// Not overwriting for subtitles, as subtecmp4transform never read trackId from init fragments
			}
		}

		if(!bReadfromcache)
		{
			//update videoend info
			aamp->UpdateVideoEndMetrics( actualType, bitrate? bitrate : fragmentDescriptor.Bandwidth, (iFogError > 0 ? iFogError : httpErrorCode),effectiveUrl,fragmentDurationS, downloadTimeS);
		}
	}

	mCheckForRampdown = false;
	// Check for overWriteTrackId to avoid this logic for PushEncrypted init fragment use-case
	if(ret && (bitrate > 0 && bitrate != fragmentDescriptor.Bandwidth && !overWriteTrackId))
	{
		AAMPLOG_INFO("Bitrate changed from %u to %ld",fragmentDescriptor.Bandwidth, bitrate);
		fragmentDescriptor.Bandwidth = (uint32_t)bitrate;
		context->SetTsbBandwidth(bitrate);
		context->mUpdateReason = true;
		mDownloadedFragment.Replace(&cachedFragment->fragment);
		ret = false;
	}
	else if (!ret)
	{
		AAMPLOG_INFO("fragment fetch failed - Free cachedFragment for %d",actualType);
		cachedFragment->fragment.Free();
		if( aamp->DownloadsAreEnabled())
		{
			AAMPLOG_WARN("%sfragment fetch failed -- fragmentUrl %s", (initSegment)?"Init ":" ", fragmentUrl.c_str());
			if (mSkipSegmentOnError)
			{
				// Skip segment on error, and increase fail count
				if(httpErrorCode != 502)
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
				if(!playingAd)    //If playingAd, we are invalidating the current Ad in onAdEvent().
				{
					if (!initSegment)
					{
						if(type != eTRACK_SUBTITLE) // Avoid sending error for failure to download subtitle fragments
						{
							AAMPLOG_ERR("%s Not able to download fragments; reached failure threshold sending tune failed event",name);
							abortWaitForVideoPTS();
							aamp->SetFlushFdsNeededInCurlStore(true);
							aamp->SendDownloadErrorEvent(AAMP_TUNE_FRAGMENT_DOWNLOAD_FAILURE, httpErrorCode);
						}
					}
					else
					{
						// When rampdown limit is not specified, init segment will be ramped down, this will
						AAMPLOG_ERR("%s Not able to download init fragments; reached failure threshold sending tune failed event",name);
						abortWaitForVideoPTS();
						aamp->SetFlushFdsNeededInCurlStore(true);

						aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, httpErrorCode);
					}
				}
			}
			// Profile RampDown check and rampdown is needed only for Video . If audio fragment download fails
			// should continue with next fragment,no retry needed .
			else if ((eTRACK_VIDEO == type) && !(context->CheckForRampDownLimitReached()))
			{
				// Attempt rampdown
				if (context->CheckForRampDownProfile(httpErrorCode))
				{
					mCheckForRampdown = true;
					if (!initSegment)
					{
						// Rampdown attempt success, download same segment from lower profile.
						mSkipSegmentOnError = false;
					}
					AAMPLOG_WARN( "StreamAbstractionAAMP_MPD::Error while fetching fragment:%s, failedCount:%d. decrementing profile",
								 fragmentUrl.c_str(), segDLFailCount);
				}
				else
				{
					if(!playingAd && initSegment && httpErrorCode !=502 )
					{
						// Already at lowest profile, send error event for init fragment.
						AAMPLOG_ERR("Not able to download init fragments; reached failure threshold sending tune failed event");
						abortWaitForVideoPTS();
						aamp->SetFlushFdsNeededInCurlStore(true);
						aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, httpErrorCode);
					}
					else
					{
						AAMPLOG_WARN("%s StreamAbstractionAAMP_MPD::Already at the lowest profile, skipping segment at pos:%lf dur:%lf disc:%d",name,position,fragmentDurationS,discontinuity);
						if(!initSegment)
							updateSkipPoint(position+fragmentDurationS,fragmentDurationS );
						context->mRampDownCount = 0;
					}
				}
			}
			else if (AampLogManager::isLogworthyErrorCode(httpErrorCode))
			{
				AAMPLOG_ERR("StreamAbstractionAAMP_MPD::Error on fetching %s fragment. failedCount:%d",name, segDLFailCount);

				if (initSegment)
				{
					// For init fragment, rampdown limit is reached. Send error event.
					if (!playingAd && httpErrorCode != 502)
					{
						abortWaitForVideoPTS();
						aamp->SetFlushFdsNeededInCurlStore(true);
						aamp->SendDownloadErrorEvent(AAMP_TUNE_INIT_FRAGMENT_DOWNLOAD_FAILURE, httpErrorCode);
					}
				}
				else
				{
					updateSkipPoint(position + fragmentDurationS, fragmentDurationS);
				}
			}
		}
	}
	else
	{
		cachedFragment->position = position;
		cachedFragment->duration = fragmentDurationS;
		cachedFragment->discontinuity = discontinuity;
		segDLFailCount = 0;
		if ((eTRACK_VIDEO == type) && (!initSegment))
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
			tsbSessionManager->EnqueueWrite(fragmentUrl, fragmentToTsbSessionMgr, context->GetPeriod()->GetId());
		}
		// Added the duplicate conditional statements, to log only for localAAMPTSB cases.
		else if(tsbSessionManager && cachedFragment->fragment.GetLen() == 0)
		{
			AAMPLOG_WARN("Type[%d] Empty cachedFragment ignored!! fragmentUrl %s fragmentTime %f discontinuity %d pto %f  scale %u duration %f", type, fragmentUrl.c_str(), position, discontinuity, pto, scale, fragmentDurationS);
		}
		else if(aamp->GetLLDashChunkMode() && initSegment)
		{
			std::shared_ptr<CachedFragment> fragmentToTsbSessionMgr = std::make_shared<CachedFragment>();
			fragmentToTsbSessionMgr->Copy(cachedFragment, cachedFragment->fragment.GetLen());
			if(fragmentToTsbSessionMgr->initFragment)
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
		}
		else
		{
			// Update buffer index after fetch for injection
			UpdateTSAfterFetch(initSegment);

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
			}
		}

		ret = true;
	}
	return ret;
}

/**
 *  @brief Cache Fragment Chunk
 */
bool MediaStreamContext::CacheFragmentChunk(AampMediaType actualType, const char *ptr, size_t size, std::string remoteUrl, long long dnldStartTime)
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
		double posInAbsTimeline = ((double)fragmentTime);
		cachedFragment->absPosition =  posInAbsTimeline;
		cachedFragment->type = actualType;
		cachedFragment->downloadStartTime = dnldStartTime;
		cachedFragment->fragment.AppendBytes(ptr, size);
		cachedFragment->timeScale = fragmentDescriptor.TimeScale;
		cachedFragment->uri = remoteUrl;
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
