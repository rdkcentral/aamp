/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
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
* @file isobmffprocessor.cpp
* @brief Source file for ISO Base Media File Format Segment Processor
*/

#include "isobmffprocessor.h"
#include "StreamAbstractionAAMP.h"

#include <pthread.h>
#include <assert.h>


#define FLOATING_POINT_EPSILON 0.1 // workaround for floating point math precision issues
#define	TIMESTAMP_OFFSET	60	//(0.0005) History of diffrent HLS shows the diffrence between manifest duartion PTS and isobmff buffer duartion in terms of PTS
#define INIT_SYNC_OFFSET	1.0f

static const char *IsoBmffProcessorTypeName[] =
{
    "video", "audio", "subtitle", "metadata"
};

/**
 *  @brief IsoBmffProcessor constructor
 */
IsoBmffProcessor::IsoBmffProcessor(class PrivateInstanceAAMP *aamp, AampLogManager *logObj, 
	id3_callback_t id3_hdl, IsoBmffProcessorType trackType, IsoBmffProcessor* peerBmffProcessor, IsoBmffProcessor* peerSubProcessor)
	: p_aamp(aamp), type(trackType), peerProcessor(peerBmffProcessor), peerSubtitleProcessor(peerSubProcessor), basePTS(0),
	processPTSComplete(false), timeScale(0), initSegment(), resetPTSInitSegment(),
	playRate(1.0f), abortAll(false), m_mutex(), m_cond(),initSegmentProcessComplete(false),
	isRestampConfigEnabled(false),
	mLogObj(logObj),
	sumPTS(0),prevPTS(0),currTimeScale(0),sumOfTrackDurationFromISOBuffer(0.0f),startPos(0.0f),
	prevPosition(-1),maxDurationFromManifest(-1),scalingOfPTSComplete(false),timeScaleChangeState(eBMFFPROCESSOR_INIT_TIMESCALE),
	prevDuration(0.0),maxTrackDurationFromISOBufferInTS(0),mediaFormat(eMEDIAFORMAT_UNKNOWN), enabled(true), trackOffsetInSecs(0.0), peerListeners()
{
	AAMPLOG_WARN("IsoBmffProcessor:: Created IsoBmffProcessor(%p) for type:%d and peerProcessor(%p)", this, type, peerBmffProcessor);
	if (peerProcessor)
	{
		peerProcessor->setPeerProcessor(this);
	}
	if (peerSubtitleProcessor)
	{
		peerSubtitleProcessor->setPeerProcessor(this);
	}
	pthread_mutex_init(&m_mutex, NULL);
	pthread_cond_init(&m_cond, NULL);
	mediaFormat = p_aamp->GetMediaFormatTypeEnum();

	// Sometimes AAMP pushes an encrypted init segment first to force decryptor plugin selection
	initSegment.reserve(3); //consider Subtitles as well
	if(p_aamp->mConfig->IsConfigSet(eAAMPConfig_EnablePTSReStamp))
	{
		isRestampConfigEnabled = true;
		AAMPLOG_WARN("IsoBmffProcessor:: %s mediaFormat = %d PTS RE-STAMP ENABLED", IsoBmffProcessorTypeName[type],mediaFormat);
	}
	else
	{
		isRestampConfigEnabled = false;
		AAMPLOG_WARN("IsoBmffProcessor:: %s mediaFormat=%d PTS RE-STAMP DISABLED", IsoBmffProcessorTypeName[type],mediaFormat);
	}
}

/**
 *  @brief IsoBmffProcessor destructor
 */
IsoBmffProcessor::~IsoBmffProcessor()
{
	AAMPLOG_DEBUG("IsoBmffProcessor %s instance (%p) getting destroyed", IsoBmffProcessorTypeName[type], this);
	clearInitSegment();
	clearRestampInitSegment();
	pthread_mutex_destroy(&m_mutex);
	pthread_cond_destroy(&m_cond);
}

/**
 *  @brief Process and send ISOBMFF fragment
 */
bool IsoBmffProcessor::sendSegment(AampGrowableBuffer* pBuffer,double position,double duration, bool discontinuous,
									bool isInit, process_fcn_t processor, bool &ptsError)
{
	AAMPLOG_INFO("IsoBmffProcessor:: %s sending segment at pos:%f dur:%f", IsoBmffProcessorTypeName[type], position, duration);
	bool ret = true;
	ptsError = false;
	if (!initSegmentProcessComplete)
	{
		ret = setTuneTimePTS(pBuffer,position,duration,discontinuous,isInit);
	}
	if (ret)
	{
		if(isRestampConfigEnabled && (playRate == AAMP_NORMAL_PLAY_RATE))
		{
			restampPTSAndSendSegment(pBuffer,position,duration,discontinuous,isInit);
		}
		else
		{
			p_aamp->ProcessID3Metadata(pBuffer->GetPtr(), pBuffer->GetLen(), (MediaType)type);
			sendStream(pBuffer,position,duration,discontinuous,isInit);
		}
	}
	return true;
}

/**
 *  @brief Process and set tune time PTS
 */
bool IsoBmffProcessor::setTuneTimePTS(AampGrowableBuffer *fragBuffer, double position, double duration, bool discontinuous, bool isInit)
{
	bool ret = true;

	AAMPLOG_INFO("IsoBmffProcessor:: %s sending segment at pos:%f dur:%f", IsoBmffProcessorTypeName[type], position, duration);

	// Logic for Audio & Subtitle Track
	if (type == eBMFFPROCESSOR_TYPE_AUDIO || type == eBMFFPROCESSOR_TYPE_SUBTITILE)
	{
		if (!processPTSComplete)
		{
			IsoBmffBuffer buffer(mLogObj);
			buffer.setBuffer((uint8_t *)fragBuffer->GetPtr(), fragBuffer->GetLen());
			buffer.parseBuffer();

			if (buffer.isInitSegment())
			{
				uint32_t tScale = 0;
				if (buffer.getTimeScale(tScale))
				{
					currTimeScale = tScale;
					AAMPLOG_INFO("IsoBmffProcessor %s TimeScale %u (%u)", IsoBmffProcessorTypeName[type], currTimeScale,currTimeScale);
				}

				AAMPLOG_INFO("IsoBmffProcessor %s caching init fragment %u (%u)", IsoBmffProcessorTypeName[type], currTimeScale,currTimeScale);
				cacheInitSegment(fragBuffer->GetPtr(), fragBuffer->GetLen());
				ret = false;
			}
			else
			{
				// Wait for video to parse PTS
				pthread_mutex_lock(&m_mutex);
				if (!processPTSComplete)
				{
					AAMPLOG_INFO("IsoBmffProcessor %s Going into wait for PTS processing to complete",  IsoBmffProcessorTypeName[type]);
					pthread_cond_wait(&m_cond, &m_mutex);
				}
				if (abortAll)
				{
					ret = false;
				}
				pthread_mutex_unlock(&m_mutex);
			}
		}
		if (ret && !initSegmentProcessComplete)
		{
			if (processPTSComplete)
			{
				double pos = ((double)basePTS / (double)timeScale);
				if (!initSegment.empty())
				{
					pushInitSegment(pos);
				}
				else
				{
					// We have no cached init fragment, maybe audio download was delayed very much
					// Push this fragment with calculated PTS
					currTimeScale = timeScale;
					sendStream(fragBuffer,pos,duration,discontinuous,isInit);
					ret = false;
				}
				initSegmentProcessComplete = true;
			}
		}
	}

	// Logic for Video Track
	// For trickplay, restamping is done in qtdemux. We can avoid
	// pts parsing logic
	if (ret && !processPTSComplete && playRate == AAMP_NORMAL_PLAY_RATE)
	{
		// We need to parse PTS from first buffer
		IsoBmffBuffer buffer(mLogObj);
		buffer.setBuffer((uint8_t *)fragBuffer->GetPtr(), fragBuffer->GetLen());
		buffer.parseBuffer();

		if (buffer.isInitSegment())
		{
			uint32_t tScale = 0;
			if (buffer.getTimeScale(tScale))
			{
				timeScale = tScale;
				currTimeScale = tScale;
			}
			AAMPLOG_INFO("IsoBmffProcessor %s TimeScale (%u) (%u) ", IsoBmffProcessorTypeName[type], currTimeScale,timeScale);
			cacheInitSegment(fragBuffer->GetPtr(), fragBuffer->GetLen());
			ret = false;
		}
		else
		{
			// Init segment was parsed and stored previously. Find the base PTS now
			uint64_t fPts = 0;
			if (buffer.getFirstPTS(fPts))
			{
				pthread_mutex_lock(&m_mutex);
				basePTS = fPts;
				processPTSComplete = true;
				pthread_mutex_unlock(&m_mutex);
				AAMPLOG_WARN("IsoBmffProcessor %s Base PTS (%" PRIu64 ") set", IsoBmffProcessorTypeName[type], basePTS);
			}
			else
			{
				AAMPLOG_WARN("IsoBmffProcessor %s Failed to process pts from buffer at pos:%f and dur:%f", IsoBmffProcessorTypeName[type], position, duration);
			}

			pthread_mutex_lock(&m_mutex);
			if (abortAll)
			{
				ret = false;
			}
			pthread_mutex_unlock(&m_mutex);

			if (ret && processPTSComplete)
			{
				if (timeScale == 0)
				{
					if (initSegment.empty())
					{
						AAMPLOG_WARN("IsoBmffProcessor %s Init segment missing during PTS processing!",  IsoBmffProcessorTypeName[type]);
						p_aamp->SendErrorEvent(AAMP_TUNE_MP4_INIT_FRAGMENT_MISSING);
						ret = false;
					}
					else
					{
						AAMPLOG_WARN("IsoBmffProcessor %s MDHD/MVHD boxes are missing in init segment!",  IsoBmffProcessorTypeName[type]);
						uint32_t tScale = 0;
						if (buffer.getTimeScale(tScale))
						{
							timeScale = tScale;
							currTimeScale = tScale;
							AAMPLOG_INFO("IsoBmffProcessor %s TimeScale (%u) set",  IsoBmffProcessorTypeName[type], timeScale);
						}
						if (timeScale == 0)
						{
							AAMPLOG_ERR("IsoBmffProcessor %s TimeScale value missing in init segment and mp4 fragment, setting to a default of 1!",  IsoBmffProcessorTypeName[type]);
							timeScale = 1; // to avoid div-by-zero errors later. MDHD and MVHD are mandatory boxes, but lets relax for now
						}

					}
				}

				if (ret)
				{
					double pos = ((double)basePTS / (double)timeScale);
					// If AAMP override hack is enabled for this platform, then we need to pass the basePTS value to
					// PrivateInstanceAAMP since PTS will be restamped in qtdemux. This ensures proper pts value is sent in progress event.
#ifdef ENABLE_AAMP_QTDEMUX_OVERRIDE
					p_aamp->NotifyFirstVideoPTS(basePTS, timeScale);
					// Here, basePTS might not be based on a 90KHz clock, whereas gst videosink might be.
					// So PTS value sent via progress event might not be accurate.
					p_aamp->NotifyVideoBasePTS(basePTS, timeScale);
#endif
					if (type == eBMFFPROCESSOR_TYPE_VIDEO)
					{
						// Send flushing seek to gstreamer pipeline.
						// For new tune, this will not work, so send pts as fragment position
						p_aamp->FlushStreamSink(pos, playRate);
					}

					if (peerProcessor)
					{
						peerProcessor->setBasePTS(basePTS, timeScale);
					}
					if(peerSubtitleProcessor)
					{
						peerSubtitleProcessor->setBasePTS(basePTS, timeScale);
					}
					if (!peerListeners.empty())
					{
						for (auto peer : peerListeners)
						{
							peer->abortInjectionWait();
						}
					}
					// This is one-time operation
					peerListeners.clear();

					pushInitSegment(pos);
					initSegmentProcessComplete = true;
				}
			}
		}
	}
	return ret;
}

/**
 *  @brief send stream based on media format
 */
void IsoBmffProcessor::sendStream(AampGrowableBuffer *pBuffer,double position, double duration,bool discontinuous,bool isInit)
{
	if(mediaFormat == eMEDIAFORMAT_DASH)
	{
		p_aamp->SendStreamTransfer((MediaType)type, pBuffer,position, position, duration, isInit, discontinuous);
	}
	else
	{
		p_aamp->SendStreamCopy((MediaType)type, pBuffer->GetPtr(), pBuffer->GetLen(), position, position, duration);
	}
}

/**
 *  @brief restamp PTS and send segment to GST
 */
void IsoBmffProcessor::restampPTSAndSendSegment(AampGrowableBuffer *pBuffer,double position, double duration,bool isDiscontinuity,bool isInit)
{
	uint32_t tScale = 0;
	bool ret = true;
	IsoBmffBuffer buffer(mLogObj);
	buffer.setBuffer((uint8_t *)pBuffer->GetPtr(), pBuffer->GetLen());
	buffer.parseBuffer();

	/* Step 1: Check is it Init fragment */
	if (buffer.isInitSegment())
	{
		/*
		1. Get timescale
		2. Is it same timescale and sumPTS is already updated then cache the Init fragment,
			so that on next fragment player can check is it duplicate or not
		3. If not same timescale then either two possibilities main-content to discontinuity or vice-versa
		*/

		if (buffer.getTimeScale(tScale))
		{
			maxTrackDurationFromISOBufferInTS = 0;
			peerProcessor->maxTrackDurationFromISOBufferInTS = 0;
			maxDurationFromManifest = 0.0;
			peerProcessor->maxDurationFromManifest = 0.0;

			AAMPLOG_INFO("IsoBmffProcessor %s  general init freshTS = %u isDiscontinuity = %d",IsoBmffProcessorTypeName[type],tScale,isDiscontinuity);

			if(sumPTS == 0 && timeScaleChangeState == eBMFFPROCESSOR_INIT_TIMESCALE)
			{
				AAMPLOG_WARN("IsoBmffProcessor %s  its First Init Time video already pushed push audio now timeScaleChangeState=%d",
								IsoBmffProcessorTypeName[type], timeScaleChangeState );

				currTimeScale = timeScale;
				p_aamp->ProcessID3Metadata(pBuffer->GetPtr(), pBuffer->GetLen(), (MediaType)type);
				sendStream(pBuffer,position,duration,isDiscontinuity,isInit);
			}
			/*check is current time scale same. If same then save the init fragment*/
			else if ( currTimeScale == tScale && sumPTS != 0 && isDiscontinuity == false )
			{
				clearRestampInitSegment();
				cacheRestampInitSegment((MediaType)type, pBuffer->GetPtr(), pBuffer->GetLen(), position, duration,isDiscontinuity);
				if( timeScaleChangeState == eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE)
				{
					/*
					Here, eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE state indicates
					already init fragment for  ad<->to<->content is cached,
					however next fragment also init fragment due to ramping
					down of profile(curl 28 error). Due to this audio pts is
					not synced and its waiting for video to unblock audio
					injection by syncing pts. Due to this wait audio loss occurs
					and never recovers. Handling this case will slove mentioned issue
					*/
					timeScaleChangeState = eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE;

					AAMPLOG_INFO("IsoBmffProcessor %s  wait for main init push to complete",IsoBmffProcessorTypeName[type]);
				}
				else
				{
					timeScaleChangeState = eBMFFPROCESSOR_CONTINUE_TIMESCALE; //Init fragment need to be pushed in same time scale
				}
				AAMPLOG_INFO("IsoBmffProcessor %s  general init pos = %f dur = %f basePTS = %" PRIu64 " sumPTS = %" PRIu64 " oldTS = %u newTS = %u",
								IsoBmffProcessorTypeName[type], position, duration, basePTS, sumPTS, currTimeScale, tScale);
			}
			else if( currTimeScale != tScale && sumPTS != 0 && isDiscontinuity == false )
			{
				AAMPLOG_INFO("IsoBmffProcessor %s  general init ABR changed with timescale oldTS = %u newTS = %u isDiscontinuity = %d",
								IsoBmffProcessorTypeName[type], currTimeScale, tScale, isDiscontinuity);

				clearRestampInitSegment();
				cacheRestampInitSegment((MediaType)type,pBuffer->GetPtr(), pBuffer->GetLen(),position,duration,isDiscontinuity);
				timeScaleChangeState = eBMFFPROCESSOR_CONTINUE_WITH_ABR_CHANGED_TIMESCALE; //init fragment need to be pushed in diffrent timescale
				cacheInitBufferForRestampingPTS(pBuffer->GetPtr(), pBuffer->GetLen(),tScale,position,true); // timescale changed with abr scale the pts to continue push
			}
			else
			{
				//time scale is changed save the init buffer for new time scale*/
				cacheInitBufferForRestampingPTS(pBuffer->GetPtr(), pBuffer->GetLen(),tScale,position);
			}
		}
		AAMPLOG_WARN("IsoBmffProcessor %s timeScaleChangeState=%d",IsoBmffProcessorTypeName[type], timeScaleChangeState );
	}
	else
	{
		if(duration > maxDurationFromManifest)
		{
			maxDurationFromManifest = duration;
		}
		/*	
		Get the exact duration from the box.Here we can't rely on the duration
		from the manifest fragment,since there is always around 200 nano to 500ms
		difference is seen between manifest duration and ISOBMFF duration.
		The ISOBMFF is having exact duration which matches the PTS value when
		added with total PTS value. The use of manifest	duration for Restamping
		will lead to PTS ERROR eventually causes position messed up in GST and
		lead to playback stop/retune
		*/

		size_t index =-1;
		uint64_t durationFromFragment =0;
		Box *pBox =  buffer.getBox(Box::MOOF, index);
		if(index >= 0 && NULL != pBox)
		{
			buffer.getSampleDuration(pBox,durationFromFragment);
			if( maxTrackDurationFromISOBufferInTS == 0 || ( durationFromFragment > maxTrackDurationFromISOBufferInTS  ))
			{
				/*Copy the maximum duration which will be used when handling the skip fragment */
				maxTrackDurationFromISOBufferInTS = durationFromFragment;
			}
			AAMPLOG_TRACE("IsoBmffProcessor %s duration= %" PRIu64 " ", IsoBmffProcessorTypeName[type],durationFromFragment);
			if (durationFromFragment == 0)
			{
				// If we can't deduce duration from fragment, use manifest provided one
				durationFromFragment = (duration * (double)currTimeScale);
			}
		}
		else
		{
			AAMPLOG_ERR("IsoBmffProcessor %s Error index = %zu", IsoBmffProcessorTypeName[type],index);
		}

		/* Step 2.Handle Skipped Fragments if Any */
		float diffDuration = position-prevPosition;

		AAMPLOG_INFO("IsoBmffProcessor %s diffDuration = %.12f position = %.12f prevPosition = %.12f maxDurationFromManifest = %.12f ",
						IsoBmffProcessorTypeName[type],	diffDuration, position, prevPosition, maxDurationFromManifest);
		int fragmentDownloadFailThreshold = p_aamp->mConfig->GetConfigValue(eAAMPConfig_FragmentDownloadFailThreshold);
		float maxPossibleSkipFragmentDuration = fragmentDownloadFailThreshold * maxDurationFromManifest;
		//Added Floating point EPSILON in order to avoid handling skipPosition even diffPosition and MaxDuration are same 19.2
		if( prevPosition != -1 && diffDuration != duration  && diffDuration > (maxDurationFromManifest+FLOATING_POINT_EPSILON) && (diffDuration < maxPossibleSkipFragmentDuration))
		{
			uint64_t skippedPTS = handleSkipFragments(diffDuration);
			sumPTS += skippedPTS; //Update the sumPTS
			sumOfTrackDurationFromISOBuffer = sumOfTrackDurationFromISOBuffer + (skippedPTS/(double)currTimeScale);
			startPos = startPos + sumOfTrackDurationFromISOBuffer;
			sumOfTrackDurationFromISOBuffer = 0;
		}
		else
		{
			AAMPLOG_INFO("IsoBmffProcessor %s Avoiding handleSkipFragments maxPossibleSkipFragmentDuration = %.12f", IsoBmffProcessorTypeName[type], maxPossibleSkipFragmentDuration);
		}

		/*Step 3. Get current PTS */
		uint64_t currentPTS = 0;
		if (buffer.getFirstPTS(currentPTS))
		{
			AAMPLOG_TRACE("IsoBmffProcessor %s currentPTS= %" PRIu64 " ts = %u", IsoBmffProcessorTypeName[type], currentPTS, currTimeScale);
		}
		else
		{
			AAMPLOG_ERR("IsoBmffProcessor %s Failed to process pts from buffer at pos = %f and dur = %f", IsoBmffProcessorTypeName[type], position, duration);
		}
	
		AAMPLOG_INFO("IsoBmffProcessor %s Before restamp: dur = %f maxDur = %f prevPos = %f currenPos = %f currTS = %u currentPTS = %" PRIu64 " basePTS=%" PRIu64 ""
						"sumPTS = %" PRIu64 " PrevPTS = %" PRIu64 " TSChangeState = %d",	IsoBmffProcessorTypeName[type], duration, maxDurationFromManifest,
						prevPosition, position, currTimeScale, currentPTS, basePTS, sumPTS, prevPTS, timeScaleChangeState);

		/* 	
		Step 4: Check any pending Init need to be pushed and PTS need to be adjusted/updated for
		1. init time, copying the basePTS for both audio and video
		2. timescale change
		3. abr timescale change followed by discontinuity timescale
		4. duplicate init followed by duplicate fragment
		*/ 
		if(timeScaleChangeState != eBMFFPROCESSOR_TIMESCALE_COMPLETE)
		{
			ret = pushInitAndSetRestampPTSAsBasePTS(currentPTS);
		}
		if(false == ret)
		{
			AAMPLOG_WARN("IsoBmffProcessor %s duplicate fragment/not in sync fragment at init. discard init and current fragment. prevPTS = %" PRIu64 " currentPTS = %" PRIu64 " \
							sumPTS = %" PRIu64 " ",	IsoBmffProcessorTypeName[type], prevPTS, currentPTS, sumPTS);
		}
		else
		{
			//Step 5.Now time to restamp the PTS
			buffer.restampPTS(sumPTS,currentPTS,(uint8_t *)(pBuffer->GetPtr()),(uint32_t)(pBuffer->GetLen()));
			double newPos = ((double)sumPTS / (double) currTimeScale);
			prevPTS = currentPTS;

			sumPTS +=durationFromFragment;
			sumOfTrackDurationFromISOBuffer += ((double)durationFromFragment / (double)currTimeScale);

			AAMPLOG_INFO("IsoBmffProcessor %s fragment restamp complete maxTrackDurationFromISOBufferInTS = %" PRIu64 " sumOfTrackDurationFromISOBuffer = %lf durationFromFragment = %" PRIu64 " currentPTS = %" PRIu64 ""
							" restampedPTS = %" PRIu64 " sumPTS = %" PRIu64 " position = %.02lf newPos = %0.2lf", IsoBmffProcessorTypeName[type],maxTrackDurationFromISOBufferInTS, sumOfTrackDurationFromISOBuffer, durationFromFragment, currentPTS,
							sumPTS-durationFromFragment, sumPTS, position, newPos);

			p_aamp->ProcessID3Metadata(pBuffer->GetPtr(), pBuffer->GetLen(), (MediaType)type);
			sendStream(pBuffer,newPos,duration,isDiscontinuity,isInit);
		}
		prevPosition = position;
		prevDuration = duration;
	}
}

/**
 *  @brief handle skip fragments
 */
uint64_t IsoBmffProcessor::handleSkipFragments(float diffDuration)
{
	/* 	
	Calculate difference in duration. Will be difference of Previous Position and current Position
	Calculate Skipped Duration. Will be difference of  Step1 and previous duration.
	E.g. : 	prevPosition = 3, skippedDuration=2 currentPosition=8
			diffDuration =  currentPosition - prevPosition
			skippedDuration= diffDuration - prevDuration
	*/

	uint64_t skippedPTS = 0;
	float skippedDuration = diffDuration - prevDuration;
	skippedPTS = ceil(skippedDuration * (double)currTimeScale);
	uint64_t maxTrackDuartionFromManifestInTS = maxDurationFromManifest * currTimeScale;

	AAMPLOG_INFO("IsoBmffProcessor %s diffDuration = %lf skippedDuration = %.12f maxDurationFromManifestInTS = %" PRIu64 " maxTrackDurationFromISOBufferInTS =  %" PRIu64 " skippedPTS =  %" PRIu64 " ",
					IsoBmffProcessorTypeName[type], diffDuration, skippedDuration, maxTrackDuartionFromManifestInTS, maxTrackDurationFromISOBufferInTS, skippedPTS);
	if(skippedPTS > TIMESTAMP_OFFSET)
	{
		AAMPLOG_WARN("IsoBmffProcessor %s fragments skipped due to Network/Other error diffDuration = %lf skippedDuration = %lf maxDurationFromManifest = %lf",
						IsoBmffProcessorTypeName[type],diffDuration,skippedDuration,maxDurationFromManifest);

		//Adding the code to align PTS values with respect to isobmff buffer
		int timeStampOffset = (int)(maxTrackDurationFromISOBufferInTS - maxTrackDuartionFromManifestInTS);

		AAMPLOG_INFO("IsoBmffProcessor %s  maxTrackDuartionFromManifestInTS =  %" PRIu64 " maxTrackDurationFromISOBufferInTS = %" PRIu64 " skippedPTS =  %" PRIu64 " timeStampOffset = %d",
						IsoBmffProcessorTypeName[type], maxTrackDuartionFromManifestInTS, maxTrackDurationFromISOBufferInTS, skippedPTS, timeStampOffset);

		//TimeStampOffset zero indicates duartion from manifest and isobmff is aligned
		if( abs(timeStampOffset) > 0 &&  abs(timeStampOffset) <= TIMESTAMP_OFFSET )
		{
			AAMPLOG_INFO("IsoBmffProcessor %s  diff between manifest and isobmff duration is %d less than %d",
							IsoBmffProcessorTypeName[type], abs(timeStampOffset), TIMESTAMP_OFFSET);

			//Last fragments in some of discontinuity will have odd duartions from main duartion,so separting them below for more accuracy
			uint64_t PTSNotAlignedWithISOBMFFMaxDuartion = skippedPTS % maxTrackDuartionFromManifestInTS;

			//Now Obtain how much timeoffset is accurate/fragments align with isobmff duration
			uint64_t PTSAlignedWithISOBMFFMaxDuration =  skippedPTS - PTSNotAlignedWithISOBMFFMaxDuartion;

			//Calculate number of fragments skipped which is aligned with max duartion of isobmffbuffer
			uint16_t NumberOfAlignedFragmentsSkipped = PTSAlignedWithISOBMFFMaxDuration / maxTrackDuartionFromManifestInTS;

			//Adjust PTS with respect to isombff duration and the left over non-aligned PTS
			skippedPTS =  ( NumberOfAlignedFragmentsSkipped * maxTrackDurationFromISOBufferInTS) + PTSNotAlignedWithISOBMFFMaxDuartion;

			AAMPLOG_INFO("IsoBmffProcessor %s  PTSNotAlignedWithISOBMFFMaxDuartion = %" PRIu64 " PTSAlignedWithISOBMFFMaxDuration = %" PRIu64 " NumberOfAlignedFragmentsSkipped=%d skippedPTS= %" PRIu64 " ",
							IsoBmffProcessorTypeName[type], PTSNotAlignedWithISOBMFFMaxDuartion, PTSAlignedWithISOBMFFMaxDuration, NumberOfAlignedFragmentsSkipped, skippedPTS);
		}				 
	}
	else
	{
		skippedPTS = 0;
	}
	AAMPLOG_WARN("IsoBmffProcessor %s PTS adjusted for skipped fragments Network/Other error sumOfTrackDurationFromISOBuffer = %lf,sumPTS = %" PRIu64 " skippedPTS = %" PRIu64 " ",
						IsoBmffProcessorTypeName[type], sumOfTrackDurationFromISOBuffer, sumPTS, skippedPTS);
	return skippedPTS;
}

/**
 *  @brief cache init buffer for restamping before pushing next playable fragment
 */
void IsoBmffProcessor::cacheInitBufferForRestampingPTS(char *segment, size_t size,uint32_t tScale,double position,bool isAbrChangedTimeScale )
{
	currTimeScale = tScale; //Update current timescale
	startPos = startPos+sumOfTrackDurationFromISOBuffer; //Start position = start position from tune + total track duration played
	sumPTS = ceil(startPos*((double)currTimeScale)); //change the pts value to new start position based on new time scale
	sumOfTrackDurationFromISOBuffer = 0; //reset Track Duration

	if( isAbrChangedTimeScale == false )
	{
		//pts is not available in Init fragment, so we need to wait for first fragment to get the PTS
		timeScaleChangeState = eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE;
		cacheInitSegment(segment, size);
		AAMPLOG_INFO("IsoBmffProcessor %s  before push init for discontinuity TS: sumOfTrackDurationFromISOBuffer=%f startPos=%f newTS=%u currTS=%u basePTS=%" PRIu64 " sumPTS=%" PRIu64 " ",
						IsoBmffProcessorTypeName[type], sumOfTrackDurationFromISOBuffer, startPos, tScale, currTimeScale, basePTS, sumPTS);
	}
	else
	{
		AAMPLOG_INFO("IsoBmffProcessor %s  abr changed with new timescale", IsoBmffProcessorTypeName[type]);
	}
	AAMPLOG_WARN("IsoBmffProcessor %s  scalingOfPTSComplete = %d maxTrackDurationFromISOBufferInTS = %" PRIu64 " ",IsoBmffProcessorTypeName[type], scalingOfPTSComplete,maxTrackDurationFromISOBufferInTS);
}

/**
 *  @brief push init and set restamped PTS as base PTS
 */
bool IsoBmffProcessor::pushInitAndSetRestampPTSAsBasePTS(uint64_t pts)
{
	bool ret=true;
	AAMPLOG_INFO("IsoBmffProcessor %s startPos = %f sumPTS = %" PRIu64 " basePTS = %" PRIu64 " currentPTS = %" PRIu64 " currTS = %u TSProcess-State = %d",
				IsoBmffProcessorTypeName[type], startPos, sumPTS, basePTS, pts, currTimeScale, timeScaleChangeState);

	switch(timeScaleChangeState)
	{
		/* Indicates it is at the time of tune so copy the basepts*/
		case eBMFFPROCESSOR_INIT_TIMESCALE:
		{
			AAMPLOG_INFO("IsoBmffProcessor %s case: %d", IsoBmffProcessorTypeName[type], timeScaleChangeState);
			sumPTS = pts;
			startPos = sumPTS/((double)currTimeScale);
			AAMPLOG_INFO("IsoBmffProcessor %s eBMFFPROCESSOR_INIT_TIMESCALE: First Time startPos = %f sumPTS = %" PRIu64 " currTS = %u ",
									IsoBmffProcessorTypeName[type], startPos, sumPTS, currTimeScale);
		}
		break;

		/*Special case to avoid duplicate fragment followed by old init
		This need to be fixed in processplaylist changes which came as part of parllel playlist */
		case eBMFFPROCESSOR_CONTINUE_TIMESCALE:
		case eBMFFPROCESSOR_CONTINUE_WITH_ABR_CHANGED_TIMESCALE:
		{
			AAMPLOG_INFO("IsoBmffProcessor %s case: %d", IsoBmffProcessorTypeName[type], timeScaleChangeState);

			ret = continueInjectionInSameTimeScale(pts);
		}
		break;

		case eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE:
		case eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE:
		{
			AAMPLOG_INFO("IsoBmffProcessor %s case: %d", IsoBmffProcessorTypeName[type], timeScaleChangeState);

			ret = scaleToNewTimeScale(pts);
		}
		break;

		case eBMFFPROCESSOR_TIMESCALE_COMPLETE:
		{
			AAMPLOG_WARN("IsoBmffProcessor %s case:eBMFFPROCESSOR_TIMESCALE_COMPLETE must not be here", IsoBmffProcessorTypeName[type]);
		}
		break;

		default:
		AAMPLOG_WARN("IsoBmffProcessor %s case:default must not be here", IsoBmffProcessorTypeName[type]);
		break;
	}
	AAMPLOG_INFO("IsoBmffProcessor %s timeScaleChangeState=%d and eBMFFPROCESSOR_TIMESCALE_COMPLETE", IsoBmffProcessorTypeName[type], timeScaleChangeState);

	timeScaleChangeState = eBMFFPROCESSOR_TIMESCALE_COMPLETE;
	return ret;
}

/**
 * @brief 
 * continue injecting on same time sacle
 */
bool IsoBmffProcessor::continueInjectionInSameTimeScale(uint64_t pts)
{
	bool ret= true;
	if( prevPTS != pts) /*PrevPTS is not equal to current pts indcates it is fresh fragment or ABR changed*/
	{
		AAMPLOG_INFO("IsoBmffProcessor %s pushing Init Fragment prevPTS = %" PRIu64 " currentPTS = %" PRIu64 " sumPTS = %" PRIu64 "", 
						IsoBmffProcessorTypeName[type], prevPTS, pts, sumPTS);
		pushRestampInitSegment();
	}
	else  /*Duplicate fragment discard init as well as duplicate fragment*/
	{
		AAMPLOG_WARN("IsoBmffProcessor %s duplicate fragment. discard init and current fragment. prevPTS = %" PRIu64 " currentPTS = %" PRIu64 " sumPTS = %" PRIu64 " ",
						IsoBmffProcessorTypeName[type], prevPTS, pts, sumPTS);
		ret = false;
	}
	clearRestampInitSegment();
	return ret;
}

/**
 *  @brief scale the first fragment to new time scale
 */
bool IsoBmffProcessor::scaleToNewTimeScale(uint64_t pts)
{
	bool ret=true;
	if( type == eBMFFPROCESSOR_TYPE_AUDIO || type == eBMFFPROCESSOR_TYPE_SUBTITILE)
	{
		AAMPLOG_WARN("IsoBmffProcessor %s  Before push init when startPos=%f peerStartPos=%f pts = %" PRIu64 " sumPTS = %" PRIu64 " peerSumPTS = %" PRIu64 " \
		basePTS = %" PRIu64 " ", IsoBmffProcessorTypeName[type], startPos, peerProcessor->startPos, pts, sumPTS, peerProcessor->sumPTS, basePTS);

		waitForVideoPTS();  //wait for video init to arrive

		/*
		BasePTS is derived from video timescale. There might be chances audio timescale is diffrent.
		always good to sync the basePTS for audio timescale as well to avoid surprises 
		*/

		//Now video and audio pts is in sync. push it
		if( pts != 0 )
		{
			AAMPLOG_INFO("IsoBmffProcessor %s  startPos = %f PeerStartPos = %f trackOffsetInSecs = %lf",
							IsoBmffProcessorTypeName[type], startPos, peerProcessor->startPos, trackOffsetInSecs);
			if (sumPTS == 0)
			{
				sumPTS = ceil((basePTS/(double)peerProcessor->currTimeScale)*currTimeScale);  //Now we got the basePTS for audio update the same as starting PTS value for main content processing
			}
			else
			{
				sumPTS = ceil((basePTS/(double)peerProcessor->currTimeScale)*currTimeScale) + (trackOffsetInSecs * currTimeScale);  //Now we got the basePTS for audio update the same as starting PTS value for main content processing
			}
			startPos = peerProcessor->startPos + trackOffsetInSecs; // startpos will never change

			AAMPLOG_WARN("IsoBmffProcessor %s  startPos = %f PeerStartPos = %f sumPTS = %" PRIu64 " peersumPTS = %" PRIu64 " trackOffsetInSecs = %lf",
							IsoBmffProcessorTypeName[type], startPos, peerProcessor->startPos, sumPTS, peerProcessor->sumPTS, trackOffsetInSecs);
		}

		pushInitSegment(startPos);
		basePTS = sumPTS;
		resetRestampVariables(); //reset the audio track variables
		if (type != eBMFFPROCESSOR_TYPE_SUBTITILE)
			peerProcessor->resetRestampVariables(); //reset the video track variables
	}
	else if( type == eBMFFPROCESSOR_TYPE_VIDEO )
	{
		AAMPLOG_INFO("IsoBmffProcessor %s  Before push init when startPos=%f peerStartPos = %f pts = %" PRIu64 " sumPTS = %" PRIu64 " peerSumPTS = %" PRIu64 " basePTS = %" PRIu64 " ",
						IsoBmffProcessorTypeName[type],startPos,peerProcessor->startPos,pts,sumPTS,peerProcessor->sumPTS,basePTS);

		/*
		Push in order
		1. Main init(ad<->to<->content transition)
		2. abr changed init
		*/
		pushInitSegment(startPos);
		if(timeScaleChangeState == eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE )
		{
			ret = continueInjectionInSameTimeScale(pts);
		}
		basePTS = sumPTS;
		if (peerProcessor)
		{
			peerProcessor->setRestampBasePTS(sumPTS);
		}
		// peerSubtitleProcessor has to be enabled to signal the values. In DASH, some periods will not have subtitle track
		if (peerSubtitleProcessor && peerSubtitleProcessor->enabled)
		{
			peerSubtitleProcessor->setRestampBasePTS(sumPTS);
		}
	}
	AAMPLOG_INFO("IsoBmffProcessor %s  After push init when startPos=%f peerStartPos=%f sumPTS=%" PRIu64 " peerSumPTS=%" PRIu64 " basePTS=%" PRIu64 " ",
	IsoBmffProcessorTypeName[type],startPos,peerProcessor->startPos,sumPTS,peerProcessor->sumPTS,basePTS);
	return ret;
}

/**
 *  @brief wait for video PTS to arrive
 */
void IsoBmffProcessor::waitForVideoPTS()
{
	pthread_mutex_lock(&m_mutex);
	if( !scalingOfPTSComplete)
	{
		AAMPLOG_INFO("IsoBmffProcessor %s going in wait untill video PTS is ready", IsoBmffProcessorTypeName[type]);
		pthread_cond_wait(&m_cond, &m_mutex);
	}
	pthread_mutex_unlock(&m_mutex);
	AAMPLOG_INFO("IsoBmffProcessor %s Wait complete", IsoBmffProcessorTypeName[type]);
}

/**
 *  @brief Abort all operations
 */
void IsoBmffProcessor::abort()
{
	AAMPLOG_WARN(" %s IsoBmffProcessor::abort() called ", IsoBmffProcessorTypeName[type]);
	pthread_mutex_lock(&m_mutex);
	abortAll = true;
	pthread_cond_signal(&m_cond);
	pthread_mutex_unlock(&m_mutex);
	reset();
}

/**
 *  @brief reset all restamp variables
 */
void IsoBmffProcessor::resetRestampVariables()
{
	clearInitSegment();
	clearRestampInitSegment();
	pthread_mutex_lock(&m_mutex);
	scalingOfPTSComplete=false;
	prevPTS = 0;
	pthread_mutex_unlock(&m_mutex);
	AAMPLOG_INFO("IsoBmffProcessor %s scalingOfPTSComplete = %d basePTS = %" PRIu64 " ",IsoBmffProcessorTypeName[type], scalingOfPTSComplete, basePTS);
}

/**
 *  @brief Reset all variables
 */
void IsoBmffProcessor::reset()
{
	AAMPLOG_INFO("IsoBmffProcessor %s reset called",IsoBmffProcessorTypeName[type]);
	clearInitSegment();
	resetRestampVariables();
	pthread_mutex_lock(&m_mutex);
	basePTS = 0;
	timeScale = 0;
	processPTSComplete = false;
	abortAll = false;
	initSegmentProcessComplete = false;
	pthread_mutex_unlock(&m_mutex);
}

/**
 *  @brief Set playback rate
 */
void IsoBmffProcessor::setRate(double rate, PlayMode mode)
{
	playRate = rate;
}

/**
 *  @brief Set base PTS and TimeScale value
 */
void IsoBmffProcessor::setBasePTS(uint64_t pts, uint32_t tScale)
{
	AAMPLOG_WARN("[%s] Base PTS (%" PRIu64 ") and TimeScale (%u) set",  IsoBmffProcessorTypeName[type], pts, tScale);
	pthread_mutex_lock(&m_mutex);
	basePTS = pts;
	timeScale = tScale;
	processPTSComplete = true;
	pthread_cond_signal(&m_cond);
	pthread_mutex_unlock(&m_mutex);
}

std::pair<uint64_t, bool> IsoBmffProcessor::GetBasePTS()
{
	std::pair<uint64_t, bool> ret{0, false};
	pthread_mutex_lock(&m_mutex);
	if (processPTSComplete)
	{
		ret.first = basePTS;
		ret.second = true;
	}
	pthread_mutex_unlock(&m_mutex);
	return ret;
}

/**
* @brief Function to abort wait for injecting the segment
*/
void IsoBmffProcessor::abortInjectionWait()
{
	AAMPLOG_WARN("[%s] Aborting wait for injection", IsoBmffProcessorTypeName[type]);
	pthread_mutex_lock(&m_mutex);
	processPTSComplete = true;
	pthread_cond_signal(&m_cond);
	pthread_mutex_unlock(&m_mutex);
}

/**
 *  @brief Set restamped PTS
 */
void IsoBmffProcessor::setRestampBasePTS(uint64_t pts)
{
	AAMPLOG_WARN("[%s] Base PTS (%" PRIu64 ") ",  IsoBmffProcessorTypeName[type], pts);
	pthread_mutex_lock(&m_mutex);
	basePTS = pts;
	scalingOfPTSComplete = true;
	pthread_cond_signal(&m_cond);
	pthread_mutex_unlock(&m_mutex);
}

/**
 *  @brief Cache restamped init fragment internally
 */
void IsoBmffProcessor::cacheRestampInitSegment(MediaType type,char *segment,size_t size,double pos,double duration,bool isDiscontinuity)
{
	stInitRestampSegment *pSt = new stInitRestampSegment;
	memset(pSt,0,sizeof(stInitRestampSegment));
	pSt->buffer =  new AampGrowableBuffer("cached-restamp-init-segment");
	pSt->buffer->AppendBytes(segment, size);
	pSt->type = type;
	pSt->position = pos;
	pSt->duration = duration;
	pSt->isDiscontinuity = isDiscontinuity;
	resetPTSInitSegment.push_back(pSt);
}

/**
 *  @brief Cache init fragment internally
 */
void IsoBmffProcessor::cacheInitSegment(char *segment, size_t size)
{
	// Save init segment for later. Init segment will be pushed once basePTS is calculated
	AAMPLOG_INFO("IsoBmffProcessor::[%s] Caching init fragment", IsoBmffProcessorTypeName[type]);
	AampGrowableBuffer *buffer = new AampGrowableBuffer("cached-init-segment");
	buffer->AppendBytes(segment, size);
	initSegment.push_back(buffer);
}

/**
 *  @brief Push init fragment cached earlier
 */
void IsoBmffProcessor::pushRestampInitSegment()
{
	if (resetPTSInitSegment.size() > 0)
	{
		for (auto it = resetPTSInitSegment.begin(); it != resetPTSInitSegment.end();)
		{
			stInitRestampSegment *Pst = *it;
			sendStream(Pst->buffer, Pst->position, Pst->duration,Pst->isDiscontinuity,true);
			SAFE_DELETE(Pst->buffer);
			SAFE_DELETE(Pst);
			it = resetPTSInitSegment.erase(it);
		}
	}
}

/**
 *  @brief Push init fragment cached earlier
 */
void IsoBmffProcessor::pushInitSegment(double position)
{
	// Push init segment now, duration = 0
	AAMPLOG_WARN("IsoBmffProcessor:: [%s] Push init fragment", IsoBmffProcessorTypeName[type]);
	if (initSegment.size() > 0)
	{
		for (auto it = initSegment.begin(); it != initSegment.end();)
		{
			AampGrowableBuffer *buf = *it;
			p_aamp->SendStreamTransfer((MediaType)type, buf, position, position, 0, true);
			SAFE_DELETE(buf);
			it = initSegment.erase(it);
		}
	}
}

/**
 *  @brief Clear restamp init fragment cached earlier
 */
void IsoBmffProcessor::clearRestampInitSegment()
{
	if (resetPTSInitSegment.size() > 0)
	{
		for (auto it = resetPTSInitSegment.begin(); it != resetPTSInitSegment.end();)
		{
			stInitRestampSegment *Pst = *it;
			SAFE_DELETE(Pst->buffer);
			SAFE_DELETE(Pst);
			it = resetPTSInitSegment.erase(it);
		}
	}
}

/**
 *  @brief Clear init fragment cached earlier
 */
void IsoBmffProcessor::clearInitSegment()
{
	if (initSegment.size() > 0)
	{
		for (auto it = initSegment.begin(); it != initSegment.end();)
		{
			AampGrowableBuffer *buf = *it;
			SAFE_DELETE(buf);
			it = initSegment.erase(it);
		}
	}
}

/**
 * @brief Set peer subtitle instance of IsoBmffProcessor
 *
 * @param[in] processor - peer instance
 */
void IsoBmffProcessor::setPeerSubtitleProcessor(IsoBmffProcessor *processor)
{
	AAMPLOG_DEBUG("Set peerSubtitleProcessor(%p) for %s instance(%p)", processor, IsoBmffProcessorTypeName[type], this);
	peerSubtitleProcessor = processor;
	// Video is master for all other tracks. If video segment processing is completed,
	// then subtitle processor needs to be updated as well
	if (peerSubtitleProcessor)
	{
		peerSubtitleProcessor->setPeerProcessor(this);
		if ((type == eBMFFPROCESSOR_TYPE_VIDEO))
		{
			peerSubtitleProcessor->initProcessorForRestamp();
		}
	}
}

/**
* @brief Function to add peer listener to a media processor
* These listeners will be notified when the basePTS processing is complete
* @param[in] processor processor instance
*/
void IsoBmffProcessor::addPeerListener(MediaProcessor *processor)
{
	if (processor)
	{
		peerListeners.push_back(processor);
	}
}

/**
 * @brief Initialize the processor to advance to restamp phase directly
 */
void IsoBmffProcessor::initProcessorForRestamp()
{
	// basePTS signalling will not happen anymore as video pts processing is complete
	initSegmentProcessComplete = true;
	// We need to get the sumPTS from video to start restamping subtitles
	// Hence setting timeScale changed state to complete
	timeScaleChangeState = eBMFFPROCESSOR_TIMESCALE_COMPLETE;
}
