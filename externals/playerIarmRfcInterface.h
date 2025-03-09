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
 * @file playerIarmRfcInterface.h
 * @brief Output protection management for Player
 */

#ifndef playerIarmRfcInterface_h
#define playerIarmRfcInterface_h

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <memory>

#include "playerIARMInterfaceBase.h"


#undef __in
#undef __out
#undef __reserved


//used for FakePlayerIarmInterface only, mimics dsmgr params
#define PLAYER_dsHDCP_VERSION_MAX 30
#define PLAYER_dsHDCP_VERSION_2X 22
#define PLAYER_dsHDCP_VERSION_1X 14
typedef int playerDsHdcpProtocolVersion_t;

class FakePlayerIarmInterface : public PlayerIarmInterfaceBase
{
        playerDsHdcpProtocolVersion_t m_hdcpCurrentProtocol;
    public:
        FakePlayerIarmInterface(){}

        /**
         * @fn IARMInit
         * @brief Initialize IARM
         * @param[in] processName string of the name of the process initializing IARM
         */
        static void IARMInit(const char* processName){}

        /**
         * @fn IARMRegisterDsMgrEventHandler
         * @brief Register Display Settings Mgr event handlers
         */
        void IARMRegisterDsMgrEventHandler() override{}

        /**
         * @fn IARMRemoveDsMgrEventHandler
         * @brief Remove Display Settings Mgr event handlers
         */
        void IARMRemoveDsMgrEventHandler() override{}

        /**
         * @fn GetDisplayResolution
         * @brief Get current resolution's display width and height
         * @param[out] width width of current resolution
         * @param[out] height height of current resolution
         */
        void GetDisplayResolution(int &width, int &height) override{}

        /**
         * @fn SetHDMIStatus
         * @brief Checks Display Settings and sets HDMI parameters like video output resolution, HDCP protocol
         */
        void SetHDMIStatus() override{
            m_hdcpCurrentProtocol = PLAYER_dsHDCP_VERSION_1X;
            m_isHDCPEnabled = true;
        }

        /**
         * @fn IsLiveLatencyCorrectionSupported
         * @brief Checks if Live Latency Correction is supported
         * @return True if supported. False if not.
         */
        bool IsLiveLatencyCorrectionSupported() override {return false;}

        /**
         * @fn IsActiveStreamingInterfaceWifi
         * @brief Checks if current active interface is wifi and also sets up NET_SRV_MGR event to handles active interface change
         * @return True if current active is wifi. False if not.
         */
        static bool IsActiveStreamingInterfaceWifi(){return false;}
        
        /**
         * @fn GetTR181Config
         * @brief Gets appropriate TR181 Config
         * @param[in] paramName String of name of the parameter to be retrieved
         * @param[out] iConfigLen Length of config retrieved
         * @return Parameter config retrieved
         */
        char * GetTR181Config(const char * paramName, size_t & iConfigLen) override{return nullptr;}
        
        /**
         * @fn isHDCPConnection2_2
         * @brief Is current HDCP protocol 2_2
         * @return True if current HDCP protocol is 2_2. False, if not.
         */
        bool isHDCPConnection2_2() override{ return m_hdcpCurrentProtocol == PLAYER_dsHDCP_VERSION_2X; }

        /**
         * @fn GetActiveInterface
         * @brief Is current active interface wifi? 
         * @return True if wifi. False, if not.
         */
        bool GetActiveInterface()override{return false;}
        
        ~FakePlayerIarmInterface(){}
};

/**
 * @class PlayerIarmRfcInterface
 * @brief Class to enforce HDCP authentication
 */
class PlayerIarmRfcInterface
{

private:


    PlayerIarmInterfaceBase* m_pIarmInterface;

    static std::shared_ptr<PlayerIarmRfcInterface> s_pPlayerOP;

    
public:

    /**
     * @fn PlayerIarmRfcInterface
     */
    PlayerIarmRfcInterface();
    /**
     * @fn ~PlayerIarmRfcInterface
     */
    virtual ~PlayerIarmRfcInterface();
    /**     
     * @brief Copy constructor disabled
     *
     */
    PlayerIarmRfcInterface(const PlayerIarmRfcInterface&) = delete;
    /**
     * @brief assignment operator disabled
     *
     */
    PlayerIarmRfcInterface& operator=(const PlayerIarmRfcInterface&) = delete;
    
    /**
     * @brief Routine to check ActiveStreamingInterface
     *
     */
	static bool IsActiveStreamingInterfaceWifi(void);
	 
	 

    char * GetTR181PlayerConfig(const char * paramName, size_t & iConfigLen);    

    // State functions

    /**
     * @brief Check if HDCP is 2.2
     * @retval true if 2.2 false otherwise
     */
    bool isHDCPConnection2_2() { return m_pIarmInterface->isHDCPConnection2_2(); }
    /** 
     * @fn IsSourceUHD
     * @retval true, if source is UHD, otherwise false
     */
    bool IsSourceUHD();

    /**
     * @fn GetDisplayResolution
     * @param[out] width : Display Width
     * @param[out] height : Display height
     */
    void GetDisplayResolution(int &width, int &height);

    /**
     * @brief Set GstElement
     * @param element Gst element to set
     */
    void setGstElement(GstElement *element) { m_pIarmInterface->setGstElement(element);  }

    // Singleton for object creation
	
    /**
     * @fn GetPlayerIarmRfcInterfaceInstance
     * @retval PlayerIarmRfcInterface object
     */	
    static std::shared_ptr<PlayerIarmRfcInterface> GetPlayerIarmRfcInterfaceInstance();
    /**
     * @fn IsPlayerIarmRfcInterfaceInstanceActive
     * @retval true or false
     */
    static bool IsPlayerIarmRfcInterfaceInstanceActive();
	
    /** 
    * @fn IsLiveLatencyCorrectionSupported
    * @retval true or false
    */
    bool IsLiveLatencyCorrectionSupported();

    /**
     * @fn GetActiveInterface
     * @brief Is current active interface wifi? 
     * @return True if wifi. False, if not.
     */
    bool GetActiveInterface();

    /**
     * @fn IARMInit
     * @brief Initialize IARM
     * @param[in] processName string of the name of the process initializing IARM
     */
    static void IARMInit(const char* processName);

    /**
     * @fn IsConfigWifiCurlHeader
     * @brief Routine to find if IARM is supported in platform
     */
    bool IsConfigWifiCurlHeader();

};

#endif // playerIarmRfcInterface_h
