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
#include <condition_variable>
#include <mutex>

/**
 * @enum IsoBmffProcessorType
 * @brief ISOBMFF Processor types
 */
enum IsoBmffProcessorType
{
	eBMFFPROCESSOR_TYPE_VIDEO = 0,
	eBMFFPROCESSOR_TYPE_AUDIO = 1,
	eBMFFPROCESSOR_TYPE_SUBTITLE = 2,
	eBMFFPROCESSOR_TYPE_METADATA = 17

};

/**
 * @struct InitRestampSegment
 * @brief structure to hold init details of fragment
 */
typedef struct
{
	AampMediaType type;
	AampGrowableBuffer *buffer;
	double position;
	double duration;
	bool isDiscontinuity;
}stInitRestampSegment;

/**
 * @enum timeScaleChangeStateType
 * @brief Time Scale change type
 */
enum timeScaleChangeStateType
{
	eBMFFPROCESSOR_INIT_TIMESCALE,	 					/* Indicates no upscale or downscale required keep injecting in current timescale */
	eBMFFPROCESSOR_CONTINUE_TIMESCALE, 					/* Indicates to push Init buffer on same time scale */
	eBMFFPROCESSOR_CONTINUE_WITH_ABR_CHANGED_TIMESCALE,	/* Indicates abr changed with new timescale	*/
	eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE,				/* Upscale or downscale based on new timescale(changes when discontinuity detected) */
	eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE, 	/* Handling curl 28 error for fragment when transitioning from ad->to->content/vice versa */
	eBMFFPROCESSOR_TIMESCALE_COMPLETE					/* push regular fragments on current timescale */
};

/**
 * @enum skipTimeType
 * @brief skip fragment type
 */
enum skipTimeType
{
	eBMFFPROCESSOR_SKIP_NONE,
	eBMFFPROCESSOR_SKIP_BEFORE_NEW_TIMESCALE,
	eBMFFPROCESSOR_SKIP_AFTER_NEW_TIMESCALE,
};

typedef std::map<double, double>skipPosToDurationTypeMap;
/**
 * @struct SkipType
 * @brief structure to hold skip position details
 */
typedef struct
{
	double sumOfSkipDuration;
	double skipPointPosition;
	double skipPosBeforeDiscontinuity;
	skipPosToDurationTypeMap skipPosToDurMap;
}stSkipType;

/**
 * @brief mapping to type and skip position
 */
typedef std::map<skipTimeType,stSkipType> skipTypeMap;

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
	// IsoBmffProcessor(class PrivateInstanceAAMP *aamp, IsoBmffProcessorType trackType = eBMFFPROCESSOR_TYPE_VIDEO, IsoBmffProcessor* peerBmffProcessor = NULL, MediaProcessor* peerSubProcessor = NULL);
	IsoBmffProcessor(class PrivateInstanceAAMP *aamp, id3_callback_t id3_hdl, IsoBmffProcessorType trackType = eBMFFPROCESSOR_TYPE_VIDEO,
		IsoBmffProcessor* peerBmffProcessor = NULL, IsoBmffProcessor* peerSubProcessor = NULL);

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
 	 * @brief Reset PTS on subtitleSwitch
	 *
	 * @param[in] reset - true/false
	 * @return void
	 */
	void resetPTSOnSubtitleSwitch(AampGrowableBuffer *pBuffer, double position) override;

	/**
	 * @brief Reset PTS on audioSwitch
	 *
	 * @param[in] reset - true/false
	 * @return void
	 */
	void resetPTSOnAudioSwitch(AampGrowableBuffer *pBuffer, double position) override;

	double getFirstPts( AampGrowableBuffer* pBuffer ) override
	{
		return 0;
	}

	void setPtsOffset( double ptsOffset ) override
	{
	}

	/**
	 * @fn sendSegment
	 *
	 * @param[in] pBuffer - Pointer to the AampGrowableBuffer
	 * @param[in] position - position of fragment
	 * @param[in] duration - duration of fragment
	 * @param[in] fragmentPTSoffset - offset PTS value
	 * @param[in] discontinuous - true if discontinuous fragment
	 * @param[in] isInit - flag for buffer type (init, data)
	 * @param[in] processor - Function to use for processing the fragments (only used by HLS/TS)
	 * @param[out] ptsError - flag indicates if any PTS error occurred
	 * @return true if fragment was sent, false otherwise
	 */
	bool sendSegment(AampGrowableBuffer* pBuffer, double position, double duration, double fragmentPTSoffset, bool discontinuous,
						bool isInit,process_fcn_t processor, bool &ptsError) override;

	/**
	 * @fn updateSkipPoint
	 *
	 * @param[in] skipPoint - indicates at what position fragments to be skipped
	 * @param[in] skipDuration - duration of fragments to be skipped
	 * @return void
	 */
	void updateSkipPoint(double skipPoint, double skipDuration ) override;

	/**
	 * @fn setDiscontinuityState
	 *
	 * @param[in] isDiscontinuity - true if discontinuity false otherwise
	 * @return void
	 */
	void setDiscontinuityState(bool isDiscontinuity) override;

	/**
	 * @fn abortWaitForVideoPTS
	 * @return void
	 */
	void abortWaitForVideoPTS() override;

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

	/**
	* @brief Function to abort wait for injecting the segment
	*/
	void abortInjectionWait() override;

	uint64_t getBasePTS()
	{
		return basePTS;
	}

	uint64_t getSumPTS()
	{
		return sumPTS;
	}

	timeScaleChangeStateType getTimeScaleChangeState()
	{
		return timeScaleChangeState;
	}

	uint32_t getTimeScale()
	{
		return timeScale;
	}

	uint32_t getCurrentTimeScale()
	{
		return currTimeScale;
	}


	std::pair<uint64_t, bool> GetBasePTS();

	/**
	* @brief Function to enable/disable the processor
	* @param[in] enable true to enable, false otherwise
	*/
	void enable(bool enable) override { enabled = enable; }

	/**
	* @brief Function to set a track offset for restamping
	* @param[in] offset offset value in seconds
	*/
	void setTrackOffset(double offset) override { trackOffsetInSecs = offset; }

	/**
	 * @brief Set peer subtitle instance of IsoBmffProcessor
	 *
	 * @param[in] processor - peer instance
	 */
	void setPeerSubtitleProcessor(IsoBmffProcessor *processor);

	/**
	* @brief Function to add peer listener to a media processor
	* These listeners will be notified when the basePTS processing is complete
	* @param[in] processor processor instance
	*/
	void addPeerListener(MediaProcessor *processor);

	/**
	* @brief Initialize the processor to advance to restamp phase directly
	*/
	void initProcessorForRestamp();

private:

	/**
	 * @fn sendStream
	 *
	 * @param[in] pBuffer - Pointer to the AampGrowableBuffer
	 * @param[in] position - position of fragment
	 * @param[in] duration - duration of fragment
	 * @param[in] fragmentPTSoffset - offset PTS value
	 * @param[in] discontinuous - true if discontinuous fragment
	 * @param[in] isInit - flag for buffer type (init, data)
	 * @return void
	 */
	void sendStream(AampGrowableBuffer *pBuffer,double position, double duration, double fragmentPTSoffset, bool discontinuous, bool isInit);

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
	 * @fn internalResetRestampVariables
	 *
	 * @return void
	 */
	void internalResetRestampVariables();

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
	 * @param[in] pBuffer - Pointer to the AampGrowableBuffer
	 * @param[in] position - position of fragment
	 * @param[in] duration - duration of fragment
	 * @param[in] discontinuous - true if discontinuous fragment
	 * @param[in] isInit - flag for buffer type (init, data)
	 * @return false if base was set, true otherwise
	 */
	bool setTuneTimePTS(AampGrowableBuffer *pBuffer, double position, double duration, bool discontinuous, bool isInit);

	/**
	 * @fn restampPTSAndSendSegment
	 *
	 * @param[in] pBuffer - Pointer to the AampGrowableBuffer
	 * @param[in] position - position of fragment
	 * @param[in] duration - duration of fragment
	 * @param[in] isDiscontinuity - true if discontinuity fragment
	 * @param[in] isInit - flag for buffer type (init, data)
	 * @return void
	 */
	void restampPTSAndSendSegment(AampGrowableBuffer *pBuffer, double position, double duration,bool isDiscontinuity,bool isInit);

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
	void cacheInitBufferForRestampingPTS(char *segment, size_t size,uint32_t tScale,double position,bool isAbrChangedTimeScale=false);

	/**
	 * @fn handleSkipFragments
	 *
	 * @param[in] diffDuration - difference between current position and previous position
	 * @return void
	 */
	uint64_t handleSkipFragments(double position = 0.0f, skipTimeType skipType = eBMFFPROCESSOR_SKIP_NONE  );

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
	 * @param[in] duration - duration of the position
	 * @return void
	 */
	void cacheRestampInitSegment(AampMediaType type,char *segment,size_t size,double pos,double duration,bool isDiscontinuity);

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

	/**
	 * @fn resetInternal
	 *
	 * @return void
	 */
	void resetInternal();

	PrivateInstanceAAMP *p_aamp;
	timeScaleChangeStateType timeScaleChangeState;
	MediaFormat mediaFormat;

	uint32_t timeScale;
	uint32_t currTimeScale;

	double startPos;
	double prevPosition;
	double prevDuration;
	double playRate;
	double trackOffsetInSecs;
	double nextPos;

	uint64_t basePTS;
	uint64_t sumPTS;
	uint64_t prevPTS;

	IsoBmffProcessor *peerProcessor;
	IsoBmffProcessor *peerSubtitleProcessor;
	IsoBmffProcessorType type;


	bool isRestampConfigEnabled;
	bool processPTSComplete;
	bool initSegmentProcessComplete;
	bool scalingOfPTSComplete;
	bool aborted; // flag to indicate if the module is active
	bool enabled;
	bool ptsDiscontinuity;

	std::vector<AampGrowableBuffer *> initSegment;
	std::vector<stInitRestampSegment *> resetPTSInitSegment;
	std::vector<MediaProcessor *> peerListeners;
	std::mutex initSegmentTransferMutex;
	std::mutex skipMutex;
	skipTypeMap skipPointMap;

	std::mutex m_mutex;
	std::condition_variable m_cond;
};

#endif /* __ISOBMFFPROCESSOR_H__ */
