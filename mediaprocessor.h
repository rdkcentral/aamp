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
* @file mediaprocessor.h
* @brief Header file for base class of media container processor
*/

#ifndef __MEDIA_PROCESSOR_H__
#define __MEDIA_PROCESSOR_H__

#include "AampMediaType.h"
#include "AampSegmentInfo.hpp"
#include "AampGrowableBuffer.h"

#include <stddef.h>
#include <functional>
#include <memory>
#include <vector>


/**
 * @enum _PlayMode
 * @brief Defines the parameters required for Recording Playback
 */
typedef enum _PlayMode
{
	PlayMode_normal,		/**< Playing a recording in normal mode */
	PlayMode_retimestamp_IPB,	/**< Playing with I-Frame, P-Frame and B-Frame */
	PlayMode_retimestamp_IandP,	/**< Playing with I-Frame and P-Frame */
	PlayMode_retimestamp_Ionly,	/**< Playing a recording with I-Frame only */
	PlayMode_reverse_GOP,		/**< Playing a recording with rewind mode */
} PlayMode;

/**
 * @class MediaProcessor
 * @brief Base Class for Media Container Processor
 */
class MediaProcessor
{
public:

	/// @brief Function to use for processing the fragments
	using process_fcn_t = std::function<void (AampMediaType, SegmentInfo_t, std::vector<uint8_t>)>;

	/**
	 * @brief MediaProcessor constructor
	 */
	MediaProcessor()
	{

	}

	/**
	 * @brief MediaProcessor destructor
	 */
	virtual ~MediaProcessor()
	{

	}

	MediaProcessor(const MediaProcessor&) = delete;
	MediaProcessor& operator=(const MediaProcessor&) = delete;

	/**
	 * @brief given TS media segment (not yet injected), extract and report first PTS
	 */
	virtual double getFirstPts( AampGrowableBuffer* pBuffer ) = 0;

	/**
	 * @brief optionally specify new pts offset to apply for subsequently injected TS media segments
	 */
	virtual void setPtsOffset( double ptsOffset ) = 0;

	/**
	 * @fn sendSegment
	 *
	 * @param[in] pBuffer - Pointer to the AampGrowableBuffer
	 * @param[in] position - position of fragment
	 * @param[in] duration - duration of fragment
	 * @param[in] fragmentPTSoffset - offset PTS of fragment
	 * @param[in] discontinuous - true if discontinuous fragment
	 * @param[in] isInit - flag for buffer type (init, data)
	 * @param[in] processor - Function to use for processing the fragments (only used by HLS/TS)
	 * @param[out] ptsError - flag indicates if any PTS error occurred
	 * @return true if fragment was sent, false otherwise
	 */
	virtual bool sendSegment(AampGrowableBuffer* pBuffer,double position,double duration, double fragmentPTSoffset, bool discontinuous,
								bool isInit, process_fcn_t processor, bool &ptsError) = 0;

	/**
	 * @brief Set playback rate
	 *
	 * @param[in] rate - playback rate
	 * @param[in] mode - playback mode
	 * @return void
	 */
	virtual void setRate(double rate, PlayMode mode) = 0;

	/**
	 * @brief Enable or disable throttle
	 *
	 * @param[in] enable - throttle enable/disable
	 * @return void
	 */
	virtual void setThrottleEnable(bool enable) = 0;

	/**
	 * @brief Set frame rate for trickmode
	 *
	 * @param[in] frameRate - rate per second
	 * @return void
	 */
	virtual void setFrameRateForTM (int frameRate) = 0;

        /**
          * @brief Reset PTS on subtitleSwitch
          *
          * @param[in] pBuffer - Pointer to the AampGrowableBuffer
          * @param[in] position - position of fragment
          * @return void
          */

	virtual void resetPTSOnSubtitleSwitch(AampGrowableBuffer *pBuffer, double position) {};
        /**
          * @brief Reset PTS on audioSwitch
          *
          * @param[in] pBuffer - Pointer to the AampGrowableBuffer
          * @param[in] position - position of fragment
          * @return void
          */

	virtual void resetPTSOnAudioSwitch(AampGrowableBuffer *pBuffer, double position) {};
	/**
	 * @brief Abort all operations
	 *
	 * @return void
	 */
	virtual void abort() = 0;

	/**
	 * @brief Reset all variables
	 *
	 * @return void
	 */
	virtual void reset() = 0;

	/**
	* @fn Change Muxed Audio Track
	* @param[in] AudioTrackIndex
	*/
	virtual void ChangeMuxedAudioTrack(unsigned char index){};

	/**
	* @brief Function to set the group-ID
	* @param[in] string - id
	*/
	virtual void SetAudioGroupId(std::string& id){};

	/**
	* @brief Function to set a offsetflag. if the value is false, no need to apply offset while doing pts restamping
	* @param[in] bool - true/false
	*/
	virtual void setApplyOffsetFlag(bool enable){};

	/**
	* @brief Function to abort wait for injecting the segment
	*/
	virtual void abortInjectionWait() = 0;

	/**
	* @brief Function to enable/disable the processor
	* @param[in] enable true to enable, false otherwise
	*/
	virtual void enable(bool enable) = 0;

	/**
	* @brief Function to set a track offset for restamping
	* @param[in] offset offset value in seconds
	*/
	virtual void setTrackOffset(double offset) = 0;

	/**
	* @brief Function to set skipped fragment duration and skip point position
	* @param[in] skipPoint - skip point position in seconds
	* @param[in] skipDuration- duration in seconds to be skipped
	*/
	virtual void updateSkipPoint(double skipPoint, double skipDuration ) {}

	/**
	* @brief Function to set discontinuity
	*/
	virtual void setDiscontinuityState(bool isDiscontinuity) {}

	/**
	* @brief Function to abort wait for videoPTS arrival
	*/
	virtual void abortWaitForVideoPTS() {}
};
#endif /* __MEDIA_PROCESSOR_H__ */
