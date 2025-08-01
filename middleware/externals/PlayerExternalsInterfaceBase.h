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
 * @file PlayerExternalsInterfaceBase.h
 * @brief Base class for player interface with IARM
 */

#ifndef PLAYER_IARM_INTERFACE_BASE_H
#define PLAYER_IARM_INTERFACE_BASE_H


#include <stddef.h>
#include <string.h>

#include <gst/gst.h>

#include "PlayerLogManager.h"

#define UHD_WIDTH   3840
#define UHD_HEIGHT  2160

//base class for iarm interface
class PlayerExternalsInterfaceBase
{
    protected:
        bool m_isHDCPEnabled;
        int m_displayWidth;
        int m_displayHeight;

        int m_sourceWidth;
        int m_sourceHeight;


        GstElement* m_gstElement;


    public:

        PlayerExternalsInterfaceBase():m_sourceWidth(0),m_sourceHeight(0),m_gstElement(nullptr){}

        /**
        * @fn IsSourceUHD
        * @brief Finds out if source is of UHD resolution
        * @return True if UHD. False if not UHD.
        */
        bool IsSourceUHD()
	{
            bool retVal = false;

        //    DEBUG_FUNC;
            static gint     sourceHeight    = 0;
            static gint     sourceWidth     = 0;

            if(m_gstElement == NULL) {
                // Value not set, since there is no
                // decoder yet the output size can not
                // be determined
                return false;
            }

            g_object_get(m_gstElement, "video_height", &sourceHeight, NULL);
            g_object_get(m_gstElement, "video_width", &sourceWidth, NULL);

            if(sourceWidth != m_sourceWidth || sourceHeight != m_sourceHeight) {
                MW_LOG_WARN("viddec (%p) --> says width %d, height %d", m_gstElement, sourceWidth, sourceHeight);
                m_sourceWidth   = sourceWidth;
                m_sourceHeight  = sourceHeight;
            }
            if(sourceWidth != 0 && sourceHeight != 0 &&
            (sourceWidth >= UHD_WIDTH || sourceHeight >= UHD_HEIGHT) ) {
                // Source Material is UHD
                retVal = true;
            }
            return retVal;
        }

	/**
         * @fn setGstElement
         * @brief Set Video decoder Gst Element for UHD identification
         */
        void setGstElement(GstElement *element) { m_gstElement = element;  }

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
        virtual void IARMRegisterDsMgrEventHandler(){}

        /**
         * @fn IARMRemoveDsMgrEventHandler
         * @brief Remove Display Settings Mgr event handlers
         */
        virtual void IARMRemoveDsMgrEventHandler(){}

        /**
         * @fn GetDisplayResolution
         * @brief Get current resolution's display width and height
         * @param[out] width width of current resolution
         * @param[out] height height of current resolution
         */
        virtual void GetDisplayResolution(int &width, int &height){}

        /**
         * @fn SetHDMIStatus
         * @brief Checks Display Settings and sets HDMI parameters like video output resolution, HDCP protocol
         */
        virtual void SetHDMIStatus(){}
	
	/**
         * @fn IsActiveStreamingInterfaceWifi
         * @brief Checks if current active interface is wifi and also sets up NET_SRV_MGR event to handles active interface change
         * @return True if current active is wifi. False if not.
         */
        bool IsActiveStreamingInterfaceWifi(){return false;}

        /**
         * @fn GetTR181Config
         * @brief Gets appropriate TR181 Config
         * @param[in] paramName String of name of the parameter to be retrieved
         * @param[out] iConfigLen Length of config retrieved
         * @return Parameter config retrieved
         */
        virtual char * GetTR181Config(const char * paramName, size_t & iConfigLen){return nullptr;}

        /**
         * @fn isHDCPConnection2_2
         * @brief Is current HDCP protocol 2_2
         * @return True if current HDCP protocol is 2_2. False, if not.
         */
        virtual bool isHDCPConnection2_2() { return false;}

        /**
         * @fn GetActiveInterface
         * @brief Is current active interface wifi?
         * @return True if wifi. False, if not.
         */
        virtual bool GetActiveInterface(){return false;}

        virtual ~PlayerExternalsInterfaceBase(){}

};

#endif
