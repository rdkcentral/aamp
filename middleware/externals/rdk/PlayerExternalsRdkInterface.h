/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file PlayerExternalsRdkInterface.h
 * @brief player interface with IARM specific to RDK
 */

#ifndef PLAYER_IARM_RDK_INTERFACE_H
#define PLAYER_IARM_RDK_INTERFACE_H
#include "manager.hpp"
#include "host.hpp"
#include "videoResolution.hpp"
#include "videoOutputPort.hpp"
#include "videoOutputPortType.hpp"
#include <libIARM.h>
#include <libIBus.h>
#include "libIBusDaemon.h"
#include "dsMgr.h"
#include "dsDisplay.h"
#include <iarmUtil.h>
#include "audioOutputPort.hpp"
#include "dsAudio.h"

#include "PlayerExternalsInterfaceBase.h"

//class representing IARM interface in rdk
class PlayerExternalsRdkInterface : public PlayerExternalsInterfaceBase
{

       dsHdcpProtocolVersion_t m_hdcpCurrentProtocol;
    public:

        /**
         * @fn IARMInit
         * @brief Initialize IARM
         * @param[in] processName string of the name of the process initializing IARM
         */
        static void IARMInit(const char* processName);

        /**
         * @fn IARMRegisterDsMgrEventHandler
         * @brief Register Display Settings Mgr event handlers
         */
        void IARMRegisterDsMgrEventHandler() override;

        /**
         * @fn IARMRemoveDsMgrEventHandler
         * @brief Remove Display Settings Mgr event handlers
         */
        void IARMRemoveDsMgrEventHandler() override;

        /**
         * @fn GetDisplayResolution
         * @brief Get current resolution's display width and height
         * @param[out] width width of current resolution
         * @param[out] height height of current resolution
         */
        void GetDisplayResolution(int &width, int &height) override;

        /**
         * @fn SetHDMIStatus
         * @brief Checks Display Settings and sets HDMI parameters like video output resolution, HDCP protocol
         */
        void SetHDMIStatus() override;

        /**
         * @fn SetResolution
         * @brief Sets current resolution's width and height
         * @param[in] width width of current resolution
         * @param[in] height height of current resolution
         */
        void SetResolution(int width, int height);

        /**
         * @fn IsActiveStreamingInterfaceWifi
         * @brief Checks if current active interface is wifi and also sets up NET_SRV_MGR event to handles active interface change
         * @return True if current active is wifi. False if not.
         */
        static bool IsActiveStreamingInterfaceWifi();

        /**
         * @fn IsLiveLatencyCorrectionSupported
         * @brief Checks if Live Latency Correction is supported
         * @return True if supported. False if not.
         */
        bool IsLiveLatencyCorrectionSupported() override;

        // Singleton for object creation

        /**
         * @fn GetPlayerExternalsRdkInterfaceInstance
         * @retval PlayerExternalsRdkInterface object
         */
        static PlayerExternalsRdkInterface * GetPlayerExternalsRdkInterfaceInstance();

        /**
         * @fn GetTR181Config
         * @brief Gets appropriate TR181 Config
         * @param[in] paramName String of name of the parameter to be retrieved
         * @param[out] iConfigLen Length of config retrieved
         * @return Parameter config retrieved
         */
        char * GetTR181Config(const char * paramName, size_t & iConfigLen) override;

        /**
         * @fn isHDCPConnection2_2
         * @brief Is current HDCP protocol 2.2
         * @return True if current HDCP protocol is 2.2. False, if not.
         */
        bool isHDCPConnection2_2() override { return m_hdcpCurrentProtocol == dsHDCP_VERSION_2X; }

        /**
         * @fn GetActiveInterface
         * @brief Is current active interface wifi?
         * @return True if wifi. False, if not.
         */
        bool GetActiveInterface();

        ~PlayerExternalsRdkInterface(){}
};


#endif
