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
 * @file videoin_shim.h
 * @brief shim for dispatching UVE Video input playback
 */

#ifndef VIDEOIN_SHIM_H_
#define VIDEOIN_SHIM_H_

#include "StreamAbstractionAAMP.h"
#include <string>
#include <stdint.h>
#include "PlayerThunderInterface.h"
using namespace std;

/**
 * @class StreamAbstractionAAMP_VIDEOIN
 * @brief Fragment collector for MPEG DASH
 */
class StreamAbstractionAAMP_VIDEOIN : public StreamAbstractionAAMP
{
public:
    /**
     * @fn StreamAbstractionAAMP_VIDEOIN
     * @param aamp pointer to PrivateInstanceAAMP object associated with player
     * @param seekpos Seek position
     * @param rate playback rate
     */
    StreamAbstractionAAMP_VIDEOIN(const std::string name, PlayerThunderAccessPlugin callSign, class PrivateInstanceAAMP *aamp,double seekpos, float rate, const std::string type);
    /**
     * @fn ~StreamAbstractionAAMP_VIDEOIN
     */
    virtual ~StreamAbstractionAAMP_VIDEOIN();
    /**     
     * @brief Copy constructor disabled
     *
     */
    StreamAbstractionAAMP_VIDEOIN(const StreamAbstractionAAMP_VIDEOIN&) = delete;
    /**
     * @brief assignment operator disabled
     *
     */
    StreamAbstractionAAMP_VIDEOIN& operator=(const StreamAbstractionAAMP_VIDEOIN&) = delete;
    /**
     *   @fn Start
     */
    void Start() override;
    /**
     *   @fn Stop
     */ 
    void Stop(bool clearChannelData) override;
    /**
     * @fn SetVideoRectangle
     * 
     * @param[in] x,y - position coordinates of video rectangle
     * @param[in] w,h - width & height of video rectangle
     */
    void SetVideoRectangle(int x, int y, int w, int h) override;
   /**
     *   @fn Init
     *   @note   To be implemented by sub classes
     *   @param  tuneType to set type of object.
     *   @retval eAAMPSTATUS_OK
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
     *   @fn GetFirstPTS
     *
     *   @retval PTS of first sample
     */
    double GetFirstPTS() override;
    /**
     * @fn IsInitialCachingSupported
     * @return true if yes 
     */
    bool IsInitialCachingSupported() override;
    /**
     * @fn GetMaxBitrate
     * @return long MAX video bitrates
     */
    BitsPerSecond GetMaxBitrate(void) override;

    static std::mutex mEvtMutex;
protected:
    AAMPStatusType InitHelper(TuneType tuneType);
    /**
     *   @fn StartHelper
     */
    void StartHelper(int port);
    /**
     *   @fn StopHelper
     */
    void StopHelper() ;
    bool mTuned;

    
//private:
protected:
    PlayerThunderInterface thunderAccessObj;
    
    /**
     *  @fn OnInputStatusChanged
     *  @param strStatus string containing "status"
     *  @return void
     */
    void OnInputStatusChanged(std::string strStatus);
    /** 
     *  @fn OnSignalChanged
     *  @param strStatus string containing "status"
     *  @return void
     */
    void OnSignalChanged(std::string strStatus);
    std::string videoInputType;
    std::string mName; // Used for logging
    bool mIsInitialized;
};

#endif // VIDEOIN_SHIM_H_
/**
 * @}
 */

