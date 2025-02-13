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
 * @file compositein_shim.h
 * @brief shim for dispatching UVE Composite input playback
 */

#ifndef COMPOSITEIN_SHIM_H_
#define COMPOSITEIN_SHIM_H_

#include "videoin_shim.h"
#include <string>
#include <stdint.h>
using namespace std;

/**
 * @class StreamAbstractionAAMP_COMPOSITEIN
 * @brief Fragment collector for MPEG DASH
 */
class StreamAbstractionAAMP_COMPOSITEIN : public StreamAbstractionAAMP_VIDEOIN
{
public:
    /**
    *   @brief get StreamAbstractionAAMP_COMPOSITEIN instance
    */
    static StreamAbstractionAAMP_COMPOSITEIN * GetInstance(class PrivateInstanceAAMP *aamp,double seekpos, float rate);

    /**
    *  @brief Clear aamp of CompositeInInstance
    */
    static void ResetInstance();

    /**
     * @fn ~StreamAbstractionAAMP_COMPOSITEIN
     */
    ~StreamAbstractionAAMP_COMPOSITEIN();
    /**
     * @brief Copy constructor disabled
     *
     */
    StreamAbstractionAAMP_COMPOSITEIN(const StreamAbstractionAAMP_COMPOSITEIN&) = delete;
    /**
     * @brief assignment operator disabled
     *
     */
    StreamAbstractionAAMP_COMPOSITEIN& operator=(const StreamAbstractionAAMP_COMPOSITEIN&) = delete;
    /**
     *   @brief  Initialize a newly created object.
     *   @param  tuneType to set type of object.
     *   @retval eAAMPSTATUS_OK
     */
    AAMPStatusType Init(TuneType tuneType) override;
    /**
     *   @fn Start
     */
    void Start() override;
    /**
     * @fn Stop
     */
    void Stop(bool clearChannelData) override;

    private:
    /**
     * @fn StreamAbstractionAAMP_COMPOSITEIN
     * @param aamp pointer to PrivateInstanceAAMP object associated with player
     * @param seekpos Seek position
     * @param rate playback rate
     */
     StreamAbstractionAAMP_COMPOSITEIN(class PrivateInstanceAAMP *aamp,double seekpos, float rate);
     static StreamAbstractionAAMP_COMPOSITEIN* mCompositeinInstance;
};

#endif // COMPOSITEIN_SHIM_H_
/**
 * @}
 */

