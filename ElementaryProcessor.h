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
* @file ElementaryProcessor.h
* @brief Header file for Elementary Fragment Processor
*/

#ifndef __ELEMENTARYPROCESSOR_H__
#define __ELEMENTARYPROCESSOR_H__

#include "mediaprocessor.h"
#include "priv_aamp.h"
#include <mutex>
#include <condition_variable>

/**
 * @class ElementaryProcessor
 * @brief Class for Elementary Fragment Processor
 */
class ElementaryProcessor : public MediaProcessor
{

public:
	/**
	 * @fn ElementaryProcessor
	 *
	 * @param[in] aamp - PrivateInstanceAAMP pointer
	 * @param[in] trackType - track type (A/V)
	 * @param[in] peerBmffProcessor - peer instance of ElementaryProcessor
	 */
	ElementaryProcessor(class PrivateInstanceAAMP *aamp);

	/**
	 * @fn ~ElementaryProcessor
	 */
	~ElementaryProcessor();

	ElementaryProcessor(const ElementaryProcessor&) = delete;
	ElementaryProcessor& operator=(const ElementaryProcessor&) = delete;

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
	 * @param[in] fragmentPTSoffset - PTS offset
	 * @param[in] discontinuous - true if discontinuous fragment
	 * @param[in] isInit - flag for buffer type (init, data)
	 * @param[in] processor - Function to use for processing the fragments (only used by HLS/TS)
	 * @param[out] ptsError - flag indicates if any PTS error occurred
	 * @return true if fragment was sent, false otherwise
	 */
	bool sendSegment(AampGrowableBuffer* pBuffer, double position, double duration, double fragmentPTSoffset, bool discontinuous,
						bool isInit, process_fcn_t processor, bool &ptsError) override;

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

	/**
	* @brief Function to enable/disable the processor
	* @param[in] enable true to enable, false otherwise
	*/
	void enable(bool enable) override { };

	/**
	* @brief Function to set a track offset for restamping
	* @param[in] offset offset value in seconds
	*/
	void setTrackOffset(double offset) override { };

	uint64_t getBasePTS()
	{
		return basePTS;
	}

private:

	/**
	 * @fn sendStream
	 *
	 * @param[in] pBuffer - Pointer to the AampGrowableBuffer
	 * @param[in] position - position of fragment
	 * @param[in] duration - duration of fragment
	 * @param[in] fragmentPTSoffset - PTS offset
	 * @param[in] discontinuous - true if discontinuous fragment
	 * @param[in] isInit - flag for buffer type (init, data)
	 * @return void
	 */
	void sendStream(AampGrowableBuffer *pBuffer,double position, double duration, double fragmentPTSoffset, bool discontinuous,bool isInit);

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
	bool setTuneTimePTS(char *segment, const size_t& size, double position, double duration, bool discontinuous, bool &ptsError);

	/**
	 * @fn setBasePTS
	 *
	 * @param[in] pts - base PTS value
	 * @param[in] tScale - TimeScale value
	 * @return void
	 */
    void setBasePTS(uint64_t pts){ basePTS = pts; }

	PrivateInstanceAAMP *p_aamp;
	ContentType contentType;
	MediaFormat mediaFormat;

	double playRate;
	uint64_t basePTS;

    bool processPTSComplete;
	bool abortAll;

	std::mutex accessMutex;
	std::condition_variable abortSignal;
};

#endif /* __ELEMENTARYPROCESSOR_H__ */
