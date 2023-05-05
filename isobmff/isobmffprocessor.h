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
* @file isobmffprocessor.h
* @brief Header file for ISO Base Media File Format Fragment Processor
*/

#ifndef __ISOBMFFPROCESSOR_H__
#define __ISOBMFFPROCESSOR_H__

#include "isobmffbuffer.h"
#include "mediaprocessor.h"
#include "priv_aamp.h"
#include <pthread.h>

/**
 * @enum IsoBmffProcessorType
 * @brief ISOBMFF Processor types
 */
enum IsoBmffProcessorType
{
	eBMFFPROCESSOR_TYPE_VIDEO = 0,
	eBMFFPROCESSOR_TYPE_AUDIO = 1
};

/**
 * @struct InitRestampSegment
 * @brief structure to hold init details of fragment
 */
typedef struct
{
	MediaType type;
	char *segment;
	size_t size;
	double position;
	double duration;
}stInitRestampSegment;

/**
 * @enum timeScaleChangeStateType
 * @brief Time Scale change type
 */
enum timeScaleChangeStateType
{
	eBMFFPROCESSOR_INIT_TIMESCALE,	 					/* Indicates no upscale or downscale requried keep injecting in current timescale */
	eBMFFPROCESSOR_CONTINUE_TIMESCALE, 					/* Indicates to push Init buffer on same time scale */
	eBMFFPROCESSOR_CONTINUE_WITH_ABR_CHANGED_TIMESCALE,	/* Indicates abr changed with new timescale	*/
	eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE,				/* Upscale or downscale based on new timescale(changes when discontinuity detected) */
	eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE, 	/* Handling curl 28 error for fragment when trasiting from ad->to->content/vice versa */
	eBMFFPROCESSOR_TIMESCALE_COMPLETE					/* push regular fragments on current timescale */
};

/**
 * @class IsoBmffProcessor
 * @brief Class for ISO BMFF Fragment Processor
 */
class IsoBmffProcessor : public MediaProcessor
{

public:
	/**
	 * @fn IsoBmffProcessor
	 *
	 * @param[in] aamp - PrivateInstanceAAMP pointer
	 * @param[in] trackType - track type (A/V)
	 * @param[in] peerBmffProcessor - peer instance of IsoBmffProcessor
	 */
	IsoBmffProcessor(class PrivateInstanceAAMP *aamp, AampLogManager *logObj=NULL, IsoBmffProcessorType trackType = eBMFFPROCESSOR_TYPE_VIDEO, IsoBmffProcessor* peerBmffProcessor = NULL);

	/**
	 * @fn ~IsoBmffProcessor
	 */
	~IsoBmffProcessor();

	IsoBmffProcessor(const IsoBmffProcessor&) = delete;
	IsoBmffProcessor& operator=(const IsoBmffProcessor&) = delete;

	/**
	 * @brief Enable or disable throttle
	 *
	 * @param[in] enable - throttle enable/disable
	 * @return void
	 */
	void setThrottleEnable(bool enable) override { };

	/**
	 * @brief Set frame rate for trickmode
	 *
	 * @param[in] frameRate - rate per second
	 * @return void
	 */
	void setFrameRateForTM (int frameRate) override { };

	/**
	 * @fn sendSegment
	 *
	 * @param[in] segment - fragment buffer pointer
	 * @param[in] size - fragment buffer size
	 * @param[in] position - position of fragment
	 * @param[in] duration - duration of fragment
	 * @param[in] discontinuous - true if discontinuous fragment
	 * @param[in] processor - Function to use for processing the fragments (only used by HLS/TS)
	 * @param[out] ptsError - flag indicates if any PTS error occurred
	 * @return true if fragment was sent, false otherwise
	 */
	bool sendSegment(char *segment, size_t& size, double position, double duration, bool discontinuous, process_fcn_t processor, bool &ptsError) override;

	/**
	 * @fn abort
	 *
	 * @return void
	 */
	void abort() override;

	/**
	 * @fn reset
	 *
	 * @return void
	 */
	void reset() override;

	/**
	 * @fn setRate
	 *
	 * @param[in] rate - playback rate
	 * @param[in] mode - playback mode
	 * @return void
	 */
	void setRate(double rate, PlayMode mode) override;

private:

	/**
	 * @brief Set peer instance of IsoBmffProcessor
	 *
	 * @param[in] processor - peer instance
	 * @return void
	 */
	void setPeerProcessor(IsoBmffProcessor *processor) { peerProcessor = processor; }

	/**
	 * @fn setBasePTS
	 *
	 * @param[in] pts - base PTS value
	 * @param[in] tScale - TimeScale value
	 * @return void
	 */
	void setBasePTS(uint64_t pts, uint32_t tScale);

	/**
	 * @fn resetRestampVariables
	 *
	 * @return void
	 */
	void resetRestampVariables();

	/**
	 * @fn setRestampBasePTS
	 *
	 * @param[in] pts - base PTS value after re-stamping
	 * @return void
	 */
	void setRestampBasePTS(uint64_t pts);

	/**
	 * @fn setTuneTimePTS
	 *
	 * @param[in] segment - fragment buffer pointer
	 * @param[in] size - fragment buffer size
	 * @param[in] position - position of fragment
	 * @param[in] duration - duration of fragment
	 * @param[in] discontinuous - true if discontinuous fragment
	 * @param[out] ptsError - flag indicates if any PTS error occurred
	 * @return false if base was set, true otherwise
	 */
	bool setTuneTimePTS(char *segment, size_t& size, double position, double duration, bool discontinuous, bool &ptsError);

	/**
	 * @fn restampPTSAndSendSegment
	 *
	 * @param[in] segment - fragment buffer pointer
	 * @param[in] size - fragment buffer size
	 * @param[in] position - position of fragment
	 * @param[in] duration - duration of fragment
	 * @param[in] isDiscontinuity - true if discontinuity fragment
	 * @return void
	 */
	void restampPTSAndSendSegment(char *segment, size_t& size, double position, double duration,bool isDiscontinuity);

	/**
	 * @fn cacheInitBufferForRestampingPTS
	 *
	 * @param[in] segment - fragment buffer pointer
	 * @param[in] size - fragment buffer size
	 * @param[in] tScale - timeScale of fragment
	 * @param[in] position - position of fragment
	 * @param[in] isAbrChangedTimeScale - indicates is timescale changed due to abr
	 * @return void
	 */
	void cacheInitBufferForRestampingPTS(char *segment, size_t& size,uint32_t tScale,double position,bool isAbrChangedTimeScale=false);

	/**
	 * @fn handleSkipFragments
	 *
	 * @param[in] diffDuration - difference between current position and previous position
	 * @return void
	 */
	uint64_t handleSkipFragments(float diffDuration);

	/**
	 * @fn pushInitAndSetRestampPTSAsBasePTS
	 *
	 * @param[in] pts - base PTS value after re-stamping
	 * @return true if init push is success, false otherwise
	 */
	bool pushInitAndSetRestampPTSAsBasePTS(uint64_t pts);

	/**
	 * @fn scaleToNewTimeScale
	 *
	 * @param[in] pts - base PTS value after re-stamping
	 * @return true if init push is success, false otherwise
	 */
	bool scaleToNewTimeScale(uint64_t pts);

	/**
	 * @fn continueInjectionInSameTimeScale
	 *
	 * @param[in] pts - base PTS value after re-stamping
	 * @return true if init push is success, false otherwise
	 */
	bool continueInjectionInSameTimeScale(uint64_t pts);
	
	/**
	 * @fn waitForVideoPTS
	 *
	 * @return void
	 */
	void waitForVideoPTS();

	/**
	 * @fn cacheRestampInitSegment
	 *
	 * @param[in] type - media type
	 * @param[in] segment - fragment buffer pointer
	 * @param[in] size - fragment buffer size
	 * @param[in] pos - fragment position
	 * @param[in] duartion - duartion of the position
	 * @return void
	 */
	void cacheRestampInitSegment(MediaType type,char *segment,size_t size,double pos,double duration);

	/**
	 * @fn pushRestampInitSegment
	 *
	 * @return void
	 */
	void pushRestampInitSegment();

	/**
	 * @fn clearRestampInitSegment
	 *
	 * @return void
	 */
	void clearRestampInitSegment();

	/**
	 * @fn cacheInitSegment
	 *
	 * @param[in] segment - buffer pointer
	 * @param[in] size - buffer size
	 * @return void
	 */
	void cacheInitSegment(char *segment, size_t size);

	/**
	 * @fn pushInitSegment
	 *
	 * @param[in] position - position value
	 * @return void
	 */
	void pushInitSegment(double position);

	/**
	 * @fn clearInitSegment
	 *
	 * @return void
	 */
	void clearInitSegment();

	PrivateInstanceAAMP *p_aamp;
	timeScaleChangeStateType timeScaleChangeState;
	ContentType contentType;

	uint32_t timeScale;
	uint32_t currTimeScale;

	double sumOfTrackDurationFromISOBuffer;
	double startPos;
	double prevPosition;
	double prevDuration;
	double maxDurationFromManifest;
	double playRate;

	uint64_t basePTS;
	uint64_t sumPTS;
	uint64_t prevPTS;
	uint64_t maxTrackDurationFromISOBufferInTS;

	IsoBmffProcessor *peerProcessor;
	IsoBmffProcessorType type;

	bool isRestampConfigEnabled;
	bool processPTSComplete;
	bool initSegmentProcessComplete;
	bool scalingOfPTSComplete;
	bool abortAll;

	std::vector<AampGrowableBuffer *> initSegment;
	std::vector<stInitRestampSegment *> resetPTSInitSegment;

	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
	AampLogManager *mLogObj;
};

#endif /* __ISOBMFFPROCESSOR_H__ */

