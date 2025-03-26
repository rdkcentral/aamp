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
#include <assert.h>

#define FLOATING_POINT_EPSILON 0.1 // workaround for floating point math precision issues
#define DEFAULT_DURATION 0.0f

static const char *IsoBmffProcessorTypeName[] =
{
    "video", "audio", "subtitle", "metadata"
};

/**
 *  @brief IsoBmffProcessor constructor
 */
IsoBmffProcessor::IsoBmffProcessor(class PrivateInstanceAAMP *aamp, id3_callback_t id3_hdl, IsoBmffProcessorType trackType, IsoBmffProcessor* peerBmffProcessor, IsoBmffProcessor* peerSubProcessor)
	: p_aamp(aamp), type(trackType), peerProcessor(peerBmffProcessor), peerSubtitleProcessor(peerSubProcessor), basePTS(0),
	processPTSComplete(false), timeScale(0), initSegment(), resetPTSInitSegment(),
	playRate(1.0f), aborted(false), m_mutex(), m_cond(),initSegmentProcessComplete(false),
	isRestampConfigEnabled(false),
	sumPTS(0),prevPTS(UINT64_MAX),currTimeScale(0), startPos(DEFAULT_DURATION),
	prevPosition(-1), prevDuration(0.0), scalingOfPTSComplete(false),timeScaleChangeState(eBMFFPROCESSOR_INIT_TIMESCALE),
	mediaFormat(eMEDIAFORMAT_UNKNOWN), enabled(true), trackOffsetInSecs(DEFAULT_DURATION), peerListeners(),
	initSegmentTransferMutex(), skipMutex(), skipPointMap(),ptsDiscontinuity(false), nextPos(-1)
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
	mediaFormat = p_aamp->GetMediaFormatTypeEnum();

	// Sometimes AAMP pushes an encrypted init segment first to force decryptor plugin selection
	initSegment.reserve(3); //consider Subtitles as well
	// added check for eMEDIAFORMAT_HLS as HLS_MP4 will be updated only after the function is called.
	// eMEDIAFORMAT_HLS + ISOBMFF processor can be confirmed as HLS_MP4
	if (p_aamp->mConfig->IsConfigSet(eAAMPConfig_EnablePTSReStamp) && (eMEDIAFORMAT_HLS_MP4 == mediaFormat || eMEDIAFORMAT_HLS == mediaFormat))
	{
		isRestampConfigEnabled = true;
		AAMPLOG_WARN("IsoBmffProcessor:: %s mediaFormat=%d old PTS RE-STAMP ENABLED", IsoBmffProcessorTypeName[type],mediaFormat);
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
}

/**
 *  @brief Process and send ISOBMFF fragment
 */
bool IsoBmffProcessor::sendSegment(AampGrowableBuffer* pBuffer,double position,double duration, double fragmentPTSoffset, bool discontinuous,
									bool isInit, process_fcn_t processor, bool &ptsError)
{
	AAMPLOG_INFO("IsoBmffProcessor %s sending segment at pos:%f dur:%f fragmentPTSoffset: %.3f", IsoBmffProcessorTypeName[type], position, duration, fragmentPTSoffset);
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
			AAMPLOG_INFO("IsoBmffProcessor %s Restamping PTS", IsoBmffProcessorTypeName[type]);
			restampPTSAndSendSegment(pBuffer,position,duration,discontinuous,isInit);
		}
		else
		{
			p_aamp->ProcessID3Metadata(pBuffer->GetPtr(), pBuffer->GetLen(), (AampMediaType)type);
			sendStream(pBuffer, position, duration, fragmentPTSoffset, discontinuous, isInit);
		}
	}
	return true;
}
/**
 *  @brief Update PTS and send pts for flush subtitle
 */
void IsoBmffProcessor::resetPTSOnSubtitleSwitch(AampGrowableBuffer *pBuffer, double position)
{
	IsoBmffBuffer buffer;
	if(isRestampConfigEnabled && (playRate == AAMP_NORMAL_PLAY_RATE))
	{
		double pos = 0;
		float diffDuration = abs((position - prevPosition) - prevDuration);
		pos = ((double)sumPTS / (double)currTimeScale);
		AAMPLOG_WARN("IsoBmffProcessor %s position=%lf prevPos=%lf posnow=%lf SumPTS %" PRIu64 " ", IsoBmffProcessorTypeName[type],position,prevPosition,pos,sumPTS);

		uint64_t skippedPTS = 0;

		if(diffDuration > 0.0f)
		{
			AAMPLOG_INFO("IsoBmffProcessor %s fragments skipped due to Network/Other error skippedDuration = %.12f TS=%u",IsoBmffProcessorTypeName[type], diffDuration, currTimeScale);
			skippedPTS = diffDuration * currTimeScale ;
		}
		else
		{
			AAMPLOG_INFO("%s fragments not skipped because skippedDuration = %.12f",IsoBmffProcessorTypeName[type], diffDuration);
		}

		sumPTS -= skippedPTS;
		pos = ((double)sumPTS / (double)currTimeScale);
		p_aamp->FlushTrack((AampMediaType)type,pos);
		startPos = pos;
		prevPosition = position;
		AAMPLOG_WARN("IsoBmffProcessor %s Updated SumPTS %" PRIu64 "  TS: %u and start pos %f", IsoBmffProcessorTypeName[type],sumPTS, currTimeScale, startPos);
	}
	else
	{
		buffer.setBuffer((uint8_t *)pBuffer->GetPtr(), pBuffer->GetLen());
		buffer.parseBuffer();
		uint64_t currentPTS = 0;
		if(buffer.getFirstPTS(currentPTS))
		{
			double pos = (double)currentPTS / (double)currTimeScale;
			p_aamp->FlushTrack((AampMediaType)type,pos);
			AAMPLOG_MIL("Curr PTS %" PRIu64 " TS: %u",currentPTS,currTimeScale);
		}
	}
}

/**
 *  @brief Update PTS and send pts for flush audio
 */
void IsoBmffProcessor::resetPTSOnAudioSwitch(AampGrowableBuffer *pBuffer, double position)
{
	IsoBmffBuffer buffer;
	if(isRestampConfigEnabled && (playRate == AAMP_NORMAL_PLAY_RATE))
	{
		double pos = 0;
		float diffDuration = abs((position - prevPosition) - prevDuration);
		pos = ((double)sumPTS / (double)currTimeScale);
		AAMPLOG_WARN("IsoBmffProcessor %s position=%lf prevPos=%lf posnow=%lf SumPTS %" PRIu64 " ", IsoBmffProcessorTypeName[type],position,prevPosition,pos,sumPTS);

		uint64_t skippedPTS = 0;

		if(diffDuration > 0.0f)
		{
			AAMPLOG_INFO("IsoBmffProcessor %s fragments skipped due to Network/Other error skippedDuration = %.12f TS=%u",IsoBmffProcessorTypeName[type], diffDuration, currTimeScale);
			skippedPTS = diffDuration * currTimeScale ;
		}
		else
		{
			AAMPLOG_INFO("%s fragments not skipped because skippedDuration = %.12f",IsoBmffProcessorTypeName[type], diffDuration);
		}

		sumPTS -= skippedPTS;
		pos = ((double)sumPTS / (double)currTimeScale);
		p_aamp->FlushTrack((AampMediaType)type,pos);
		startPos = pos;
		prevPosition = position;
		AAMPLOG_WARN("IsoBmffProcessor %s Updated SumPTS %" PRIu64 "  TS: %u and start pos %f", IsoBmffProcessorTypeName[type],sumPTS, currTimeScale, startPos);
	}
	else
	{
		buffer.setBuffer((uint8_t *)pBuffer->GetPtr(), pBuffer->GetLen());
		buffer.parseBuffer();
		uint64_t currentPTS = 0;

		if(buffer.getFirstPTS(currentPTS))
		{
			double pos = (double)currentPTS / (double)currTimeScale;
			p_aamp->FlushTrack((AampMediaType)type,pos);
			AAMPLOG_MIL("Curr PTS %" PRIu64 " TS: %u",currentPTS,currTimeScale);
		}
	}
}

/**
 *  @brief Process and set tune time PTS
 */
bool IsoBmffProcessor::setTuneTimePTS(AampGrowableBuffer *fragBuffer, double position, double duration, bool discontinuous, bool isInit)
{
	bool ret = true;

	AAMPLOG_INFO("IsoBmffProcessor:: %s sending segment at pos:%f dur:%f aborted:%d", IsoBmffProcessorTypeName[type], position, duration, aborted);

	std::unique_lock<std::mutex> lock(m_mutex);
	ret = !aborted;  // check the module is active

	// Logic for Audio & Subtitle Track
	if (type == eBMFFPROCESSOR_TYPE_AUDIO || type == eBMFFPROCESSOR_TYPE_SUBTITLE)
	{
		if (ret && !processPTSComplete)
		{
			IsoBmffBuffer buffer;
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
				if (!processPTSComplete)
				{
					AAMPLOG_INFO("IsoBmffProcessor %s Going into wait for PTS processing to complete",  IsoBmffProcessorTypeName[type]);
					m_cond.wait(lock);
				}
				if (aborted) // check there wasn't an abort during the wait
				{
					AAMPLOG_INFO("IsoBmffProcessor %s aborting PTS processing",  IsoBmffProcessorTypeName[type]);
					ret = false;
				}
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
					sendStream(fragBuffer,pos,duration, 0.0, discontinuous,isInit);
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
		IsoBmffBuffer buffer;
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
				bool sendError = false;

				basePTS = fPts;
				processPTSComplete = true;
				AAMPLOG_WARN("IsoBmffProcessor %s Base PTS (%" PRIu64 ") set", IsoBmffProcessorTypeName[type], basePTS);

				if (timeScale == 0)
				{
					if (!aborted && initSegment.empty())
					{
						sendError = true;
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
					// For post processing, release mutex
					// If AAMP override hack is enabled for this platform, then we need to pass the basePTS value to
					// PrivateInstanceAAMP since PTS will be restamped in qtdemux. This ensures proper pts value is sent in progress event.
					lock.unlock();
					p_aamp->NotifyFirstVideoPTS(basePTS, timeScale);
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
					lock.lock();
					if (!aborted)
					{
						pushInitSegment(pos);
						initSegmentProcessComplete = true;
					}
				}

				if (sendError)
				{
					AAMPLOG_WARN("IsoBmffProcessor %s Init segment missing during PTS processing!",  IsoBmffProcessorTypeName[type]);
					p_aamp->SendErrorEvent(AAMP_TUNE_MP4_INIT_FRAGMENT_MISSING);
				}
			}
			else
			{
				AAMPLOG_WARN("IsoBmffProcessor %s Failed to process pts from buffer at pos:%f and dur:%f", IsoBmffProcessorTypeName[type], position, duration);
			}

		}
	}

	ret = ret && !aborted;
	return (ret);
}

/**
 *  @brief send stream based on media format
 */
void IsoBmffProcessor::sendStream(AampGrowableBuffer *pBuffer, double position, double duration, double fragmentPTSoffset, bool discontinuous, bool isInit)
{
	if(mediaFormat == eMEDIAFORMAT_DASH)
	{
		p_aamp->SendStreamTransfer((AampMediaType)type, pBuffer,position, position, duration, fragmentPTSoffset, isInit, discontinuous);
	}
	else
	{
		p_aamp->SendStreamCopy((AampMediaType)type, pBuffer->GetPtr(), pBuffer->GetLen(), position, position, duration);
	}
}

/**
 *  @brief restamp PTS and send segment to GST
 */
void IsoBmffProcessor::restampPTSAndSendSegment(AampGrowableBuffer *pBuffer,double position, double duration,bool isDiscontinuity,bool isInit)
{
	uint32_t tScale = 0;
	bool ret = true;
	IsoBmffBuffer buffer;
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
			AAMPLOG_INFO("IsoBmffProcessor %s  general init freshTS = %u isDiscontinuity = %d",IsoBmffProcessorTypeName[type],tScale,isDiscontinuity);

			if(sumPTS == 0 && timeScaleChangeState == eBMFFPROCESSOR_INIT_TIMESCALE)
			{
				AAMPLOG_WARN("IsoBmffProcessor %s  its First Init Time video already pushed push audio now timeScaleChangeState=%d",
								IsoBmffProcessorTypeName[type], timeScaleChangeState );

				currTimeScale = timeScale;
				p_aamp->ProcessID3Metadata(pBuffer->GetPtr(), pBuffer->GetLen(), (AampMediaType)type);
				sendStream(pBuffer,position,duration, 0.0, isDiscontinuity, isInit);
			}
			/*check is current time scale same. If same then save the init fragment*/
			else if ( currTimeScale == tScale && sumPTS != 0 && isDiscontinuity == false )
			{
				if( timeScaleChangeState == eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE)
				{
					clearRestampInitSegment();
					cacheRestampInitSegment((AampMediaType)type, pBuffer->GetPtr(), pBuffer->GetLen(), position, duration,isDiscontinuity);
					/*
					Here, eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE state indicates
					already init fragment for  ad<->to<->content is cached,
					however next fragment also init fragment due to ramping
					down of profile(curl 28 error). Due to this audio pts is
					not going to be in sync with video PTS and it will be  waiting
					for ever for video pts leading to video only playback and then stall
					Handling this case will slove mentioned issue
					*/
					AAMPLOG_INFO("IsoBmffProcessor %s  wait for main init push to complete ts-changeState: %d",IsoBmffProcessorTypeName[type],timeScaleChangeState);
					timeScaleChangeState = eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE;

				}
				else if(timeScaleChangeState == eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE )
				{
					cacheRestampInitSegment((AampMediaType)type, pBuffer->GetPtr(), pBuffer->GetLen(), position, duration,isDiscontinuity);
					AAMPLOG_INFO("IsoBmffProcessor %s  wait for main init push to complete ts-changeState: %d",IsoBmffProcessorTypeName[type], timeScaleChangeState);
				}
				else
				{
					clearRestampInitSegment();
					cacheRestampInitSegment((AampMediaType)type, pBuffer->GetPtr(), pBuffer->GetLen(), position, duration,isDiscontinuity);
					timeScaleChangeState = eBMFFPROCESSOR_CONTINUE_TIMESCALE; //Init fragment need to be pushed in same time scale
					AAMPLOG_INFO("IsoBmffProcessor %s  continue in same time scale ts-changeState: %d",IsoBmffProcessorTypeName[type], timeScaleChangeState);
				}
				AAMPLOG_WARN("IsoBmffProcessor %s  general init pos: %f dur: %f basePTS: %" PRIu64 " sumPTS: %" PRIu64 " oldTS: %u newTS: %u ts-changeState: %d",
								IsoBmffProcessorTypeName[type], position, duration, basePTS, sumPTS, currTimeScale, tScale,timeScaleChangeState );
			}
			else if( currTimeScale != tScale && sumPTS != 0 && isDiscontinuity == false )
			{
				AAMPLOG_INFO("IsoBmffProcessor %s  general init ABR changed with timescale oldTS = %u newTS = %u isDiscontinuity = %d",
								IsoBmffProcessorTypeName[type], currTimeScale, tScale, isDiscontinuity);

				clearRestampInitSegment();
				cacheRestampInitSegment((AampMediaType)type,pBuffer->GetPtr(), pBuffer->GetLen(),position,duration,isDiscontinuity);
				timeScaleChangeState = eBMFFPROCESSOR_CONTINUE_WITH_ABR_CHANGED_TIMESCALE; //init fragment need to be pushed in different timescale
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
		/*
		Get the exact duration from the box.Here we can't rely on the duration
		from the manifest fragment,since there is always around 200 nano to 500ms
		difference is seen between manifest duration and ISOBMFF duration.
		The ISOBMFF is having exact duration which matches the PTS value when
		added with total PTS value. The use of manifest	duration for Restamping
		will lead to PTS ERROR eventually causes position messed up in GST and
		lead to playback stop/retune
		*/

		size_t index = 0;
		uint64_t durationFromFragment =0;
		Box *pBox =  buffer.getBox(Box::MOOF, index);
		if (NULL != pBox)
		{
			buffer.getSampleDuration(pBox,durationFromFragment);
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

		/*Step 2. Get current PTS */
		uint64_t currentPTS = 0;
		if (buffer.getFirstPTS(currentPTS))
		{
			AAMPLOG_TRACE("IsoBmffProcessor %s currentPTS= %" PRIu64 " ts = %u", IsoBmffProcessorTypeName[type], currentPTS, currTimeScale);
		}
		else
		{
			AAMPLOG_ERR("IsoBmffProcessor %s Failed to process pts from buffer at pos = %f and dur = %f", IsoBmffProcessorTypeName[type], position, duration);
		}

		AAMPLOG_INFO("IsoBmffProcessor %s Before restamp: dur = %f prevPos = %f currentPos = %f currTS = %u currentPTS = %" PRIu64 " basePTS=%" PRIu64 ""
						"sumPTS = %" PRIu64 " PrevPTS = %" PRIu64 " TSChangeState = %d",	IsoBmffProcessorTypeName[type], duration, prevPosition,
						position, currTimeScale, currentPTS, basePTS, sumPTS, prevPTS, timeScaleChangeState);

		/* Step 3.Handle Skipped Fragments if Any BEFORE TIMESCALE*/
			if( false ==  skipPointMap.empty() )
			{
				AAMPLOG_WARN("IsoBmffProcessor %s skipping BEFORE_NEW_TIMESCALE currPos:%lf prevPos:%lf ", IsoBmffProcessorTypeName[type],  position, prevPosition);
				handleSkipFragments(position,eBMFFPROCESSOR_SKIP_BEFORE_NEW_TIMESCALE);
			}

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
			/* Step 5.Handle Skipped Fragments if Any AFTER TIMESCALE*/
			if( false ==  skipPointMap.empty() )
			{
				AAMPLOG_WARN("IsoBmffProcessor %s skipping  AFTER_NEW_TIMESCALE currPos:%lf prevPos:%lf ",
								IsoBmffProcessorTypeName[type], position, prevPosition);
				handleSkipFragments(position,eBMFFPROCESSOR_SKIP_AFTER_NEW_TIMESCALE);
			}

			//Step 6.Now time to restamp the PTS
			buffer.restampPTS(sumPTS,currentPTS,(uint8_t *)(pBuffer->GetPtr()),(uint32_t)(pBuffer->GetLen()));
			double newPos = ((double)sumPTS / (double) currTimeScale);
			prevPTS = currentPTS;

			sumPTS +=durationFromFragment;

			AAMPLOG_INFO("IsoBmffProcessor %s fragment restamp complete durationFromFragment = %" PRIu64 " currentPTS = %" PRIu64 ""
							" restampedPTS = %" PRIu64 " sumPTS = %" PRIu64 " position = %.02lf newPos = %0.2lf", IsoBmffProcessorTypeName[type], durationFromFragment, currentPTS,
							sumPTS-durationFromFragment, sumPTS, position, newPos);

			p_aamp->ProcessID3Metadata(pBuffer->GetPtr(), pBuffer->GetLen(), (AampMediaType)type);
			sendStream(pBuffer, newPos, duration, 0.0, isDiscontinuity, isInit);
		}
		prevPosition = position;
		prevDuration = duration;
		if( -1 == nextPos || 0.0f == position || position > nextPos )
			nextPos = position + duration;
		else
			nextPos += duration;
		AAMPLOG_INFO("IsoBmffProcessor %s after restamp nextPos: %lf, prevPosition: %lf, prevDuration: %lf",IsoBmffProcessorTypeName[type],nextPos,prevPosition, prevDuration );
	}
}

/**
 *  @brief handle skip fragments
 */
uint64_t IsoBmffProcessor::handleSkipFragments( double skipPosition , skipTimeType skipType )
{
	uint64_t skippedPTS = 0;
	std::lock_guard<std::mutex> lock(skipMutex);
	skipPosToDurationTypeMap::iterator posToDurMapIter;
	double tempPos = -1;
	bool isSkipPTS = false;
	double sumOfSkipDuration = 0.0f;
	AAMPLOG_WARN("IsoBmffProcessor %s [in] type: %d skipListSize: %zu  skipPosition: %lf discontinuity: %d",IsoBmffProcessorTypeName[type], skipType, skipPointMap.size(),skipPosition,ptsDiscontinuity );

	auto it = skipPointMap.find(skipType);
	if( it != skipPointMap.end() )
	{
		stSkipType st = it->second;
		for ( auto Iter  = st.skipPosToDurMap.begin(); Iter != st.skipPosToDurMap.end();Iter++)
		{
			AAMPLOG_WARN(" IsoBmffProcessor %s skipPos : %lf skipDur: %lf ",IsoBmffProcessorTypeName[type],Iter->first, Iter->second );
		}

		posToDurMapIter = st.skipPosToDurMap.find(skipPosition);
		if( posToDurMapIter != st.skipPosToDurMap.end())
		{
			tempPos =  posToDurMapIter->first;
		}
		AAMPLOG_WARN("IsoBmffProcessor %s skipDur: %lf skipPointPos: %lf spBeforeDisc: %lf nextPos: %lf tempPos %lf", IsoBmffProcessorTypeName[type],
						st.sumOfSkipDuration, st.skipPointPosition, st.skipPosBeforeDiscontinuity, nextPos, tempPos);

		if( ( ( abs(st.skipPointPosition - skipPosition) < FLOATING_POINT_EPSILON ) ||
			( abs(st.skipPointPosition - nextPos) < FLOATING_POINT_EPSILON ) ) &&
				 tempPos != skipPosition  )
		{
			isSkipPTS = true;
			sumOfSkipDuration = st.sumOfSkipDuration;
			st.sumOfSkipDuration = DEFAULT_DURATION;
			AAMPLOG_WARN("IsoBmffProcessor %s :mark for skipPTS for skipPosition: %lf durationToSkip: %lf remaining duration to skip: %lf %zu", IsoBmffProcessorTypeName[type],
									skipPosition,sumOfSkipDuration, st.sumOfSkipDuration, st.skipPosToDurMap.size() );
		}
		else if(tempPos == skipPosition)
		{
			AAMPLOG_WARN("IsoBmffProcessor %s tempPos: %lf skipPosition: %lf skipDuration:  %lf st.sumOfSkipDuration: %lf", IsoBmffProcessorTypeName[type], tempPos, skipPosition,
							posToDurMapIter->second,  st.sumOfSkipDuration);

			st.sumOfSkipDuration = st.sumOfSkipDuration - posToDurMapIter->second;

			if( st.sumOfSkipDuration < FLOATING_POINT_EPSILON )
			{
				AAMPLOG_WARN("IsoBmffProcessor %s :Ignore PTS Skip for skipPosition: %lf skipDuration: %lf remainingDur to Skip: %lf %zu", IsoBmffProcessorTypeName[type],
								skipPosition,posToDurMapIter->second, st.sumOfSkipDuration, st.skipPosToDurMap.size() );
			}
			else
			{
				st.skipPosToDurMap.erase(posToDurMapIter);
				AAMPLOG_WARN("IsoBmffProcessor %s :Ignore PTS Skip for skipPosition: %lf skipDuration: %lf remaining duration to skip: %lf %zu", IsoBmffProcessorTypeName[type],
								skipPosition,posToDurMapIter->second, st.sumOfSkipDuration, st.skipPosToDurMap.size() );
				/*
					Last fragment skipped, however it got recovered due to manifest update.
					However the fragments before the last one were marked for skip.Need to skip the PTS for non-recovered fragments
					Example : fragment 2,3,4 marked for skip. but 4 recovered. now don't skip pts for 4 however skip pts for  2 and 3.
				*/
				for( posToDurMapIter = st.skipPosToDurMap.begin(); posToDurMapIter != st.skipPosToDurMap.end(); )
				{
					if( posToDurMapIter->first < skipPosition )
					{
						sumOfSkipDuration = sumOfSkipDuration + posToDurMapIter->second;
						st.sumOfSkipDuration = st.sumOfSkipDuration - posToDurMapIter->second;
						posToDurMapIter = st.skipPosToDurMap.erase(posToDurMapIter);
						isSkipPTS = true;
					}
					else
					{
						posToDurMapIter++;
					}
				}
				if(isSkipPTS )
					AAMPLOG_WARN("IsoBmffProcessor %s :mark for skipPTS for skipPosition: %lf durationToSkip: %lf remaining duration to skip: %lf %zu", IsoBmffProcessorTypeName[type],
									skipPosition,sumOfSkipDuration, st.sumOfSkipDuration, st.skipPosToDurMap.size() );
			}
		}
		else
		{
			if(eBMFFPROCESSOR_SKIP_BEFORE_NEW_TIMESCALE == skipType  && -1 == tempPos &&  true == ptsDiscontinuity )
			{
				posToDurMapIter = st.skipPosToDurMap.find(st.skipPosBeforeDiscontinuity );
				if( posToDurMapIter != st.skipPosToDurMap.end())
				{
					tempPos =  posToDurMapIter->first;
					isSkipPTS = true;
					sumOfSkipDuration = st.sumOfSkipDuration;
					st.sumOfSkipDuration = DEFAULT_DURATION;
					AAMPLOG_WARN(" IsoBmffProcessor %s skipPTS is true: skipPos : %lf skipDur: %lf discontinuity: %d ",IsoBmffProcessorTypeName[type],tempPos, posToDurMapIter->second,ptsDiscontinuity );
				}
			}
			else
				AAMPLOG_INFO("IsoBmffProcessor %s nothing to skip at position %lf for type %d", IsoBmffProcessorTypeName[type], skipPosition, skipType );
		}

		if( st.sumOfSkipDuration < FLOATING_POINT_EPSILON )
		{
			st.skipPosToDurMap.clear();
			skipPointMap.erase(it);
		}
		else
		{
			skipPointMap[skipType] = st;
		}
		if( true == isSkipPTS )
		{
			skippedPTS = sumOfSkipDuration * (double)currTimeScale;
			if(skippedPTS > 0 )
			{
				AAMPLOG_WARN("IsoBmffProcessor %s [in] Skipping Fragments due to Network/other error type: %d skipPointPos: %lf skipPosition: %lf skipDur: %lf sumPTS:%" PRIu64 " ",
								IsoBmffProcessorTypeName[type], skipType, st.skipPointPosition, skipPosition,sumOfSkipDuration, sumPTS);
				sumPTS += skippedPTS;
			}
			AAMPLOG_WARN("IsoBmffProcessor %s skippedPTS:%" PRIu64 " sumPTS:%" PRIu64 " ", IsoBmffProcessorTypeName[type], skippedPTS, sumPTS );
		}
	}
	else
	{
		AAMPLOG_INFO("IsoBmffProcessor %s skiptype %d not present for skipPosition:%lf", IsoBmffProcessorTypeName[type], skipType, skipPosition);
	}
	if(true == skipPointMap.empty() ) //empty
			nextPos = skipPosition;

	return skippedPTS;
}

/**
 *  @brief cache init buffer for restamping before pushing next playable fragment
 */
void IsoBmffProcessor::cacheInitBufferForRestampingPTS(char *segment, size_t size,uint32_t tScale,double position,bool isAbrChangedTimeScale )
{
	AAMPLOG_INFO("IsoBmffProcessor %s  before push init for discontinuity TS: isAbrChangedTimeScale=%d startPos=%f newTS=%u currTS=%u basePTS=%" PRIu64 " sumPTS=%" PRIu64 " ",
						IsoBmffProcessorTypeName[type], isAbrChangedTimeScale,  startPos, tScale, currTimeScale, basePTS, sumPTS);
	sumPTS = ceil((sumPTS/(double)currTimeScale)*tScale);
	startPos = (sumPTS/(double)tScale);
	currTimeScale = tScale;

	if( isAbrChangedTimeScale == false )
	{
		//pts is not available in Init fragment, so we need to wait for first fragment to get the PTS
		timeScaleChangeState = eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE;
		cacheInitSegment(segment, size);
	}
	else
	{
		AAMPLOG_INFO("IsoBmffProcessor %s  abr changed with new timescale", IsoBmffProcessorTypeName[type]);
	}
	AAMPLOG_INFO("IsoBmffProcessor %s discontinuity TS: isAbrChangedTimeScale=%d startPos=%f newTS=%u currTS=%u basePTS=%" PRIu64 " sumPTS=%" PRIu64 " ",
						IsoBmffProcessorTypeName[type], isAbrChangedTimeScale, startPos, tScale, currTimeScale, basePTS, sumPTS);
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
		This need to be fixed in processplaylist changes which came as part of parallel playlist */
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
	if( prevPTS != pts) /*PrevPTS is not equal to current pts indicates it is fresh fragment or ABR changed*/
	{
		AAMPLOG_INFO("IsoBmffProcessor %s pushing Init Fragment prevPTS: %" PRIu64 " currentPTS: %" PRIu64 " sumPTS: %" PRIu64 " sumInitSegments: %zu",
						IsoBmffProcessorTypeName[type], prevPTS, pts, sumPTS, resetPTSInitSegment.size());
		pushRestampInitSegment();
	}
	else  /*Duplicate fragment discard init as well as duplicate fragment*/
	{
		AAMPLOG_WARN("IsoBmffProcessor %s duplicate fragment. discard init and current fragment. prevPTS = %" PRIu64 " currentPTS = %" PRIu64 " sumPTS = %" PRIu64 " ",
						IsoBmffProcessorTypeName[type], prevPTS, pts, sumPTS);
		ret = false;
	}
	clearRestampInitSegment();
	setDiscontinuityState(false);
	return ret;
}

/**
 *  @brief scale the first fragment to new time scale
 */
bool IsoBmffProcessor::scaleToNewTimeScale(uint64_t pts)
{
	bool ret=true;
	AAMPLOG_INFO("IsoBmffProcessor %s  Before push init when startPos=%f pts = %" PRIu64 " sumPTS = %" PRIu64 " basePTS = %" PRIu64 " ", IsoBmffProcessorTypeName[type],startPos,pts,sumPTS,basePTS);
	if( type == eBMFFPROCESSOR_TYPE_AUDIO || type == eBMFFPROCESSOR_TYPE_SUBTITLE)
	{
		if(peerProcessor)
   	 	{
			AAMPLOG_WARN("peerStartPos=%f peerSumPTS=%" PRIu64 "", peerProcessor->startPos, peerProcessor->sumPTS);
		}
		waitForVideoPTS();  //wait for video init to arrive
		/*
		BasePTS is derived from video timescale. There might be chances audio timescale is different.
		always good to sync the basePTS for audio timescale as well to avoid surprises
		*/
		//Now video and audio pts is in sync. push it
		if(peerProcessor)
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
			startPos = (sumPTS/(double)currTimeScale) + trackOffsetInSecs; // startpos can change when fragments skipped

			AAMPLOG_INFO("peerStartPos=%f peerSumPTS=%" PRIu64 "", peerProcessor->startPos, peerProcessor->sumPTS);
		}
		AAMPLOG_WARN("IsoBmffProcessor %s  startPos = %f sumPTS = %" PRIu64 " trackOffsetInSecs = %lf",
						IsoBmffProcessorTypeName[type], startPos, sumPTS, trackOffsetInSecs);


		pushInitSegment(startPos);
		basePTS = sumPTS;
		resetRestampVariables(); //reset the audio track variables
	}
	else if( type == eBMFFPROCESSOR_TYPE_VIDEO )
	{
		if(peerProcessor)
		{
			AAMPLOG_INFO("peerStartPos=%f peerSumPTS=%" PRIu64 "", peerProcessor->startPos, peerProcessor->sumPTS);
		}
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
		resetRestampVariables(); //reset the video track variables
	}
	AAMPLOG_INFO("IsoBmffProcessor %s  After push init when startPos=%f sumPTS=%" PRIu64 " basePTS=%" PRIu64 " ",
	IsoBmffProcessorTypeName[type],startPos,sumPTS,basePTS);
	if(peerProcessor)
	{
		AAMPLOG_INFO("peerStartPos=%f peerSumPTS=%" PRIu64 "", peerProcessor->startPos, peerProcessor->sumPTS);
	}
	setDiscontinuityState(false);
	return ret;
}

void IsoBmffProcessor::setDiscontinuityState(bool isDiscontinuity)
{
	std::lock_guard<std::mutex> lock(skipMutex);
	ptsDiscontinuity = isDiscontinuity;
}

/**
 *  @brief Update total skip duration
 */
void IsoBmffProcessor::updateSkipPoint(double skipPoint, double skipDuration )
{
	AAMPLOG_INFO("IsoBmffProcessor %s skipPoint: %lf skipDur: %lf timescaleChangeState: %d isDiscontinuity: %d size: %zu" ,
				IsoBmffProcessorTypeName[type], skipPoint, skipDuration,timeScaleChangeState,ptsDiscontinuity, skipPointMap.size());

	std::lock_guard<std::mutex> lock(skipMutex);
	stSkipType st={0.0,0.0,0.0};

	if(false == ptsDiscontinuity )
	{
		AAMPLOG_INFO("IsoBmffProcessor %s ptsDiscontinuity %d" ,IsoBmffProcessorTypeName[type],ptsDiscontinuity);

		auto it = skipPointMap.find(eBMFFPROCESSOR_SKIP_BEFORE_NEW_TIMESCALE);
		if( it != skipPointMap.end() )
		{
			st = it->second;
			AAMPLOG_INFO("IsoBmffProcessor %s found SKIP_BEFORE_NEW_TIMESCALE",IsoBmffProcessorTypeName[type]);
			if( st.skipPosToDurMap.find(abs(skipPoint - skipDuration)) != st.skipPosToDurMap.end() )
			{
				AAMPLOG_WARN("IsoBmffProcessor %s fragment skipPos: %lf with duration: %lf is already marked for skip!!!",IsoBmffProcessorTypeName[type], abs(skipPoint - skipDuration),skipDuration);
			}
			else
			{
				AAMPLOG_WARN("IsoBmffProcessor %s fragment skipPos: %lf with duration: %lf is marked for skip",IsoBmffProcessorTypeName[type], abs(skipPoint - skipDuration),skipDuration);
				st.sumOfSkipDuration += skipDuration;
				st.skipPointPosition = skipPoint;
				st.skipPosBeforeDiscontinuity = abs(skipPoint - skipDuration);
				st.skipPosToDurMap[st.skipPosBeforeDiscontinuity] = skipDuration;
				nextPos += skipDuration;
			}
		}
		else
		{
			AAMPLOG_INFO("IsoBmffProcessor %s not found SKIP_BEFORE_NEW_TIMESCALE adding now",IsoBmffProcessorTypeName[type]);
			st.sumOfSkipDuration = skipDuration;
			st.skipPointPosition = skipPoint;
			st.skipPosBeforeDiscontinuity = abs(skipPoint - skipDuration);
			st.skipPosToDurMap[st.skipPosBeforeDiscontinuity] = skipDuration;
			nextPos += skipDuration;
			AAMPLOG_WARN("IsoBmffProcessor %s fragment skipPos: %lf with duration: %lf is marked for skip",IsoBmffProcessorTypeName[type], abs(skipPoint - skipDuration),skipDuration);
		}
			skipPointMap[eBMFFPROCESSOR_SKIP_BEFORE_NEW_TIMESCALE] = st;
			AAMPLOG_WARN("IsoBmffProcessor %s SKIP_BEFORE_NEW_TIMESCALE skipPoint: %lf skippedPos: %lf sum duration to skip: %lf nextPos: %lf skipPointBeforeDisc: %lf isDiscontinuity: %d posDurMapSize: %zu",
							IsoBmffProcessorTypeName[type], skipPoint, abs(skipPoint - skipDuration), st.sumOfSkipDuration,	nextPos, st.skipPosBeforeDiscontinuity, ptsDiscontinuity,st.skipPosToDurMap.size() );
	}
	else
	{
		bool isUpdateReq = true;
		AAMPLOG_INFO("IsoBmffProcessor %s ptsDiscontinuity %d" ,IsoBmffProcessorTypeName[type],ptsDiscontinuity);
		auto it = skipPointMap.find(eBMFFPROCESSOR_SKIP_AFTER_NEW_TIMESCALE);
		if( it != skipPointMap.end() )
		{
			st = it->second;
			AAMPLOG_INFO("IsoBmffProcessor %s found SKIP_AFTER_NEW_TIMESCALE",IsoBmffProcessorTypeName[type]);
			if( st.skipPosToDurMap.find(abs(skipPoint - skipDuration)) != st.skipPosToDurMap.end() )
			{
				AAMPLOG_WARN("IsoBmffProcessor %s fragment skipPos: %lf with duration: %lf is already marked for skip!!!",IsoBmffProcessorTypeName[type], abs(skipPoint - skipDuration),skipDuration);
				isUpdateReq = false;
			}
			else
			{
				AAMPLOG_WARN("IsoBmffProcessor %s fragment skipPos: %lf with duration: %lf is marked for skip",IsoBmffProcessorTypeName[type], abs(skipPoint - skipDuration),skipDuration);
				st.sumOfSkipDuration += skipDuration;
				st.skipPointPosition = skipPoint;
				st.skipPosToDurMap[abs(skipPoint - skipDuration)] = skipDuration;
				nextPos += skipDuration;
			}
		}
		else
		{
			AAMPLOG_INFO("IsoBmffProcessor %s not found SKIP_AFTER_NEW_TIMESCALE adding now",IsoBmffProcessorTypeName[type]);
			st.sumOfSkipDuration = skipDuration;
			st.skipPointPosition = skipPoint;
			st.skipPosToDurMap[abs(skipPoint - skipDuration)] = skipDuration;
			nextPos += skipDuration;
			AAMPLOG_WARN("IsoBmffProcessor %s fragment skipPos: %lf with duration: %lf is marked for skip",IsoBmffProcessorTypeName[type], abs(skipPoint - skipDuration),skipDuration);
		}
		skipPointMap[eBMFFPROCESSOR_SKIP_AFTER_NEW_TIMESCALE] = st;

		AAMPLOG_WARN("IsoBmffProcessor %s SKIP_AFTER_NEW_TIMESCALE skipPoint: %lf skippedPos: %lf sum duration to skip : %lf nextPos: %lf skipPointBeforeDisc: %lf isDiscontinuity: %d posDurMapSize: %zu",
						IsoBmffProcessorTypeName[type], skipPoint, abs(skipPoint - skipDuration), st.sumOfSkipDuration,	nextPos, st.skipPosBeforeDiscontinuity, ptsDiscontinuity,st.skipPosToDurMap.size() );
		it = skipPointMap.find(eBMFFPROCESSOR_SKIP_BEFORE_NEW_TIMESCALE);
		if( it != skipPointMap.end() && isUpdateReq )
		{
			it->second.skipPointPosition = skipPoint;
			AAMPLOG_WARN("IsoBmffProcessor %s updated skiPoint for SKIP_BEFORE_NEW_TIMESCALE skipPoint: %lf" ,IsoBmffProcessorTypeName[type], skipPoint );
		}
	}
}

/**
 *  @brief wait for video PTS to arrive
 */
void IsoBmffProcessor::waitForVideoPTS()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	if( !scalingOfPTSComplete)
	{
		AAMPLOG_WARN("IsoBmffProcessor %s going in wait untill video PTS is ready", IsoBmffProcessorTypeName[type]);
		m_cond.wait(lock);
	}
	AAMPLOG_WARN("IsoBmffProcessor %s Wait complete", IsoBmffProcessorTypeName[type]);
}

/**
 *  @brief abort wait for video PTS to arrive
 */
void IsoBmffProcessor::abortWaitForVideoPTS()
{
	std::unique_lock<std::mutex> lock(m_mutex);
	AAMPLOG_WARN("IsoBmffProcessor %s unblocking PTS restamp", IsoBmffProcessorTypeName[type]);
	m_cond.notify_one();
	AAMPLOG_WARN("IsoBmffProcessor %s unblock complete", IsoBmffProcessorTypeName[type]);
}

/**
 * @fn reset
 *
 * @return void
 */
void IsoBmffProcessor::reset()
{
	AAMPLOG_MIL(" %s IsoBmffProcessor::reset() called ", IsoBmffProcessorTypeName[type]);
	{
		std::lock_guard<std::mutex> guard(m_mutex);
		aborted = false;
	}
	// reset variables that might have been set due to race conditions
	resetInternal();
}

/**
 *  @brief Abort all operations
 */
void IsoBmffProcessor::abort()
{
	AAMPLOG_WARN(" %s IsoBmffProcessor::abort() called ", IsoBmffProcessorTypeName[type]);
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		aborted = true;
		m_cond.notify_one();
	}
	resetInternal();
}

/**
 *  @brief reset all restamp variables (internal function without mtx lock)
 */
void IsoBmffProcessor::internalResetRestampVariables()
{
	clearInitSegment();
	clearRestampInitSegment();
	scalingOfPTSComplete=false;
	prevPTS = UINT64_MAX;
	AAMPLOG_INFO("IsoBmffProcessor %s scalingOfPTSComplete = %d basePTS = %" PRIu64 " ",IsoBmffProcessorTypeName[type], scalingOfPTSComplete, basePTS);
}

/**
 *  @brief reset all restamp variables
 */
void IsoBmffProcessor::resetRestampVariables()
{
	std::lock_guard<std::mutex> guard(m_mutex);
	internalResetRestampVariables();
}

/**
 *  @brief Reset all variables
 */
void IsoBmffProcessor::resetInternal()
{
	std::lock_guard<std::mutex> guard(m_mutex);
	internalResetRestampVariables();
	basePTS = 0;
	timeScale = 0;
	processPTSComplete = false;
	initSegmentProcessComplete = false;
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
	std::lock_guard<std::mutex> guard(m_mutex);
	basePTS = pts;
	timeScale = tScale;
	processPTSComplete = true;
	m_cond.notify_one();
}

std::pair<uint64_t, bool> IsoBmffProcessor::GetBasePTS()
{
	std::pair<uint64_t, bool> ret{0, false};
	std::lock_guard<std::mutex> guard(m_mutex);
	if (processPTSComplete)
	{
		ret.first = basePTS;
		ret.second = true;
	}
	return ret;
}

/**
* @brief Function to abort wait for injecting the segment
*/
void IsoBmffProcessor::abortInjectionWait()
{
	AAMPLOG_WARN("[%s] Aborting wait for injection", IsoBmffProcessorTypeName[type]);
	std::lock_guard<std::mutex> guard(m_mutex);
	processPTSComplete = true;
	m_cond.notify_one();
}

/**
 *  @brief Set restamped PTS
 */
void IsoBmffProcessor::setRestampBasePTS(uint64_t pts)
{
	AAMPLOG_WARN("[%s] Base PTS (%" PRIu64 ") ",  IsoBmffProcessorTypeName[type], pts);
	std::lock_guard<std::mutex> guard(m_mutex);
	basePTS = pts;
	scalingOfPTSComplete = true;
	m_cond.notify_one();
}

/**
 *  @brief Cache restamped init fragment internally
 */
void IsoBmffProcessor::cacheRestampInitSegment(AampMediaType type,char *segment,size_t size,double pos,double duration,bool isDiscontinuity)
{
	std::lock_guard<std::mutex> lock(initSegmentTransferMutex);
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
	std::lock_guard<std::mutex> lock(initSegmentTransferMutex);
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
	std::lock_guard<std::mutex> lock(initSegmentTransferMutex);
	if (resetPTSInitSegment.size() > 0)
	{
		for (auto it = resetPTSInitSegment.begin(); it != resetPTSInitSegment.end();)
		{
			stInitRestampSegment *Pst = *it;
			sendStream(Pst->buffer, Pst->position, Pst->duration, 0.0, Pst->isDiscontinuity, true);
			SAFE_DELETE(Pst->buffer);
			SAFE_DELETE(Pst);
			it = resetPTSInitSegment.erase(it);
		}
	}
	else
	{
		AAMPLOG_WARN("No init segment cached for injection");
	}
}

/**
 *  @brief Push init fragment cached earlier
 */
void IsoBmffProcessor::pushInitSegment(double position)
{
	// Push init segment now, duration = 0
	AAMPLOG_WARN("IsoBmffProcessor:: [%s] Push init fragment", IsoBmffProcessorTypeName[type]);
	std::lock_guard<std::mutex> lock(initSegmentTransferMutex);
	if (initSegment.size() > 0)
	{
		for (auto it = initSegment.begin(); it != initSegment.end();)
		{
			AampGrowableBuffer *buf = *it;
			p_aamp->SendStreamTransfer((AampMediaType)type, buf, position, position, 0, 0.0, true);
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
	std::lock_guard<std::mutex> lock(initSegmentTransferMutex);
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
	std::lock_guard<std::mutex> lock(initSegmentTransferMutex);
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
		if( type == eBMFFPROCESSOR_TYPE_VIDEO )
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
	// basePTS signaling will not happen anymore as video pts processing is complete
	initSegmentProcessComplete = true;
	// We need to get the sumPTS from video to start restamping subtitles
	// Hence setting timeScale changed state to complete
	timeScaleChangeState = eBMFFPROCESSOR_TIMESCALE_COMPLETE;
}
