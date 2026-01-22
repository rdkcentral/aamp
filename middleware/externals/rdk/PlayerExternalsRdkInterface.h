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
#include "dsMgr.h"
#include "dsDisplay.h"
#include "audioOutputPort.hpp"
#include "dsAudio.h"

#include <memory>
#include "PlayerExternalsInterfaceBase.h"

 /*
IARM Deprecation Note:
IARM is to be deprecated in favor of DeviceSettings and Firebolt Device API.
*/

/*
Deprecate HDCP support in PlayerExternalsRdkInterface when deprecating IARM
*/

/*
Remove the section between the comment section remove-start and remove-end when deprecating IARM
*/

/*
Replace the section between the comment section replace-start, replace-with and replace-end when deprecating IARM
*/

class DeviceInterfaceBase;

//class representing IARM interface in rdk
class PlayerExternalsRdkInterface : public PlayerExternalsInterfaceBase
#ifdef USE_DS_EVENT_SUPPORTED
	, public device::Host::IDisplayDeviceEvents
	, public device::Host::IVideoOutputPortEvents
#endif
{
        enum InitState{
            NOT_INITIALIZED,
            FIREBOLT,
            IARM
        };
    
        dsHdcpProtocolVersion_t m_hdcpCurrentProtocol = dsHDCP_VERSION_1X;

        //replace-start
        std::shared_ptr<DeviceInterfaceBase> m_pDeviceInterfaceBase = nullptr;
        //replace-with
        //std::shared_ptr<DeviceFireboltInterface> m_pDeviceInterfaceBase = nullptr;
        //replace-end

        //remove-start
        bool m_use_firebolt_sdk = false;

        InitState m_initialized = NOT_INITIALIZED;
        //remove-end

        bool mPowerEvt = false;

        /**< Callback function for fake tune operations */
        std::function<void()> m_doFakeTuneCallback = nullptr;

        PlayerExternalsRdkInterface();

    public:
#ifdef USE_DS_EVENT_SUPPORTED
		template <typename T>
		T* baseInterface()
		{
			static_assert(std::is_base_of<T, PlayerExternalsRdkInterface>::value, "base type mismatch");
			return static_cast<T*>(this);
		}
#endif
        void Initialize() override;

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

        // Singleton for object creation
	
        /**
         * @fn GetPlayerExternalsRdkInterfaceInstance
         * @retval PlayerExternalsRdkInterface object
         */	
        static std::shared_ptr<PlayerExternalsRdkInterface> GetPlayerExternalsRdkInterfaceInstance();

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

        void SetActiveInterface(bool isWifi);

        std::shared_ptr<DeviceInterfaceBase> GetDeviceInterface();

        void setHdcpProtocol(dsHdcpProtocolVersion_t t_protocol);

        void SetUseFireBoltSDK(bool t_use_firebolt_sdk) override;

	void SetPowerEvent(bool powerEvt) override;

        bool GetPowerEvent() override;

        /**
         * @brief Set callback function for fake tune operations
         * @param[in] t_doFakeTuneCallback Function to call when fake tune is triggered from power events
         */
        void SetDoFakeTuneCallBack(const std::function<void()>& t_doFakeTuneCallback) override;

        /**
         * @brief Get callback function for fake tune operations
         * @return Function pointer for fake tune callback
         */
        std::function<void()> GetDoFakeTuneCallBack() override;

        ~PlayerExternalsRdkInterface();

#ifdef USE_DS_EVENT_SUPPORTED
        void RegisterDsClientEventHandler();
        void RemoveDsClientEventHandlers();

        /* IVideoOutputPortEvents */
        void OnResolutionPreChange(const int width, const int height) override;
        void OnResolutionPostChange(const int width, const int height) override;
        void OnHDCPStatusChange(dsHdcpStatus_t hdcpStatus) override;

        /* IDisplayDeviceEvents */
        void OnDisplayHDMIHotPlug(dsDisplayEvent_t displayEvent) override;
#endif
};


#endif
