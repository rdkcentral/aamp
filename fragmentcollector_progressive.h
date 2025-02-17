/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
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
 * @file fragmentcollector_progressive.h
 * @brief Streamer for progressive mp3/mp4 playback
 */

#ifndef FRAGMENTCOLLECTOR_PROGRESSIVE_H_
#define FRAGMENTCOLLECTOR_PROGRESSIVE_H_

#include "StreamAbstractionAAMP.h"
#include <string>
#include <stdint.h>
using namespace std;

/**
 * @class StreamAbstractionAAMP_PROGRESSIVE
 * @brief Streamer for progressive mp3/mp4 playback
 */
class StreamAbstractionAAMP_PROGRESSIVE : public StreamAbstractionAAMP
{
public:
    /**
     * @fn StreamAbstractionAAMP_PROGRESSIVE
     * @param aamp pointer to PrivateInstanceAAMP object associated with player
     * @param seekpos Seek position
     * @param rate playback rate
     */
    StreamAbstractionAAMP_PROGRESSIVE(class PrivateInstanceAAMP *aamp,double seekpos, float rate);
    /**
     * @fn ~StreamAbstractionAAMP_PROGRESSIVE
     */
    ~StreamAbstractionAAMP_PROGRESSIVE();
    /**
     * @brief Copy constructor disabled
     *
     */
    StreamAbstractionAAMP_PROGRESSIVE(const StreamAbstractionAAMP_PROGRESSIVE&) = delete;
    /**
     * @brief assignment operator disabled
     *
     */
    StreamAbstractionAAMP_PROGRESSIVE& operator=(const StreamAbstractionAAMP_PROGRESSIVE&) = delete;
    double seekPosition;
	/**
	 *   @fn Start
	 *   @return void
	 */
    void Start() override;
    /**
     *   @fn Stop
     *   @return void
     */
    void Stop(bool clearChannelData) override;
    /**
     *   @fn Init
     *   @note   To be implemented by sub classes
     *   @param  tuneType to set type of object.
     *   @retval true on success
     *   @retval false on failure
     */
    AAMPStatusType Init(TuneType tuneType) override;
	/**
	 * @fn GetStreamFormat
	 * @param[out]  primaryOutputFormat - format of primary track
	 * @param[out]  audioOutputFormat - format of audio track
	 * @param[out]  auxOutputFormat - format of aux audio track
	 * @param[out]  subtitleOutputFormat - format of subtitle track
	 */
	void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxOutputFormat, StreamOutputFormat &subtitleOutputFormat) override;
    /**
     * @fn GetStreamPosition 
     *
     * @retval current position of stream.
     */
    double GetStreamPosition() override;
    /**
     *   @fn GetFirstPTS 
     *
     *   @retval PTS of first sample
     */
    double GetFirstPTS() override;
    
    /**
     *  @fn IsInitialCachingSupported
     *
     */
    bool IsInitialCachingSupported() override;
    /**
     * @fn GetMaxBitrate
     * @return long MAX video bitrates
     */
    BitsPerSecond GetMaxBitrate(void) override;
    /**
     * @fn FetcherLoop
     * @return void
     */
    void FetcherLoop();
    /**
     * @fn FragmentCollector
     * @retval void
     */
    void FragmentCollector();

private:
    void StreamFile( const char *uri, int *http_error );
    bool fragmentCollectorThreadStarted;
    std::thread fragmentCollectorThreadID;
};

#endif //FRAGMENTCOLLECTOR_PROGRESSIVE_H_
/**
 * @}
 */
 


