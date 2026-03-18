/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
* @file ElementaryProcessor.cpp
* @brief Source file for Elementary Fragment Processor
*/

#include "ElementaryProcessor.h"
#include <assert.h>

ElementaryProcessor::ElementaryProcessor(class PrivateInstanceAAMP *aamp)
: p_aamp(aamp), basePTS(0), playRate(1.0f), abortAll(false), contentType(ContentType_UNKNOWN),
	processPTSComplete(false),mediaFormat(eMEDIAFORMAT_UNKNOWN)
{
    mediaFormat = p_aamp->GetMediaFormatTypeEnum();

	AAMPLOG_WARN("ElementaryProcessor:: Created ElementaryProcessor(%p) for mediaformat %d", this,mediaFormat);
}

/**
 *  @brief ElementaryProcessor destructor
 */
ElementaryProcessor::~ElementaryProcessor()
{
}

/**
 *  @brief Process and send Elementary fragment
 */
bool ElementaryProcessor::sendSegment(AampGrowableBuffer* pBuffer,double position,double duration, double fragmentPTSoffset, bool discontinuous,
											bool isInit,process_fcn_t processor, bool &ptsError)
{
	ptsError = false;
	bool ret = true;
	ret = setTuneTimePTS(pBuffer->GetPtr(), pBuffer->GetLen(), position, duration, discontinuous, ptsError);
	if (ret)
	{
		AAMPLOG_INFO("IsoBmffProcessor:: eMEDIATYPE_SUBTITLE sending segment at pos:%f dur:%f", position, duration);
		sendStream(pBuffer, position, duration, fragmentPTSoffset, discontinuous, isInit);
	}
	return true;
}

/**
 *  @brief send stream based on media format
 */
void ElementaryProcessor::sendStream(AampGrowableBuffer *pBuffer,double position, double duration, double fragmentPTSoffset,bool discontinuous,bool isInit)
{
	if(mediaFormat == eMEDIAFORMAT_DASH)
	{
		p_aamp->SendStreamTransfer((AampMediaType)eMEDIATYPE_SUBTITLE, pBuffer,position, position, duration, fragmentPTSoffset, isInit, discontinuous);
	}
	else
	{
		p_aamp->SendStreamCopy((AampMediaType)eMEDIATYPE_SUBTITLE, pBuffer->GetPtr(), pBuffer->GetLen(), position, position, duration);
	}
}

/**
 *  @brief Process and set tune time PTS
 */
bool ElementaryProcessor::setTuneTimePTS(char *segment, const size_t& size, double position, double duration, bool discontinuous, bool &ptsError)
{
	ptsError = false;
	bool ret = true;

	AAMPLOG_INFO("ElementaryProcessor:: sending segment at pos:%f dur:%f", position, duration);

	// Logic for Audio Track
	// Wait for video to parse PTS
	std::unique_lock<std::mutex> guard(accessMutex);

	if(!processPTSComplete)
	{
		AAMPLOG_INFO("ElementaryProcessor Going into wait for PTS processing to complete");
		// Or abort raised
		abortSignal.wait(guard);
	}

	if (abortAll)
	{
		ret = false;
	}

	return ret;
}

/**
* @brief Function to abort wait for injecting the segment
*/
void ElementaryProcessor::abortInjectionWait()
{
	{
		std::lock_guard<std::mutex> guard(accessMutex);
		processPTSComplete = true;
	}
	abortSignal.notify_one();
}

/**
 *  @brief Abort all operations
 */
void ElementaryProcessor::abort()
{
	AAMPLOG_WARN("ElementaryProcessor::abort() called ");
	{
		std::lock_guard<std::mutex> guard(accessMutex);
		abortAll = true;
	}
	abortSignal.notify_one();

	reset();
}

/**
 *  @brief Reset all variables
 */
void ElementaryProcessor::reset()
{
	AAMPLOG_INFO("ElementaryProcessor reset called");
	std::lock_guard<std::mutex> guard(accessMutex);
	basePTS = 0;
	processPTSComplete = false;
	abortAll = false;
}

/**
 *  @brief Set playback rate
 */
void ElementaryProcessor::setRate(double rate, PlayMode mode)
{
	playRate = rate;
}
