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
 * @file aampoutputprotection.h
 * @brief Output protection management for Aamp
 */

#ifndef aampoutputprotection_h
#define aampoutputprotection_h

#include <pthread.h>

#ifdef IARM_MGR
// IARM
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

#else
#include <stdint.h>
typedef int dsHdcpProtocolVersion_t;
#define dsHDCP_VERSION_MAX      30
#define dsHDCP_VERSION_2X       22
#define dsHDCP_VERSION_1X       14
#endif // IARM_MGR

#include <stdio.h>
#include <gst/gst.h>



#undef __in
#undef __out
#undef __reserved

#define UHD_WIDTH   3840
#define UHD_HEIGHT  2160

/**
 * @class ReferenceCount
 * @brief Provides reference based memory management
 */
class ReferenceCount
{

public:

    ReferenceCount() : m_refCount(0), m_refCountMutex() {
        pthread_mutex_init(&m_refCountMutex, NULL);
    }

    virtual ~ReferenceCount() {
        pthread_mutex_destroy(&m_refCountMutex);
    }


    uint32_t AddRef() const {
        pthread_mutex_lock(&m_refCountMutex);
        m_refCount++;
        pthread_mutex_unlock(&m_refCountMutex);
        return m_refCount;
    }


    void Release() const {
        pthread_mutex_lock(&m_refCountMutex);
        m_refCount--;
        pthread_mutex_unlock(&m_refCountMutex);

        if(m_refCount == 0) {
		delete (ReferenceCount *)this;
        }
    }

private:
    mutable pthread_mutex_t     m_refCountMutex;
    mutable int                 m_refCount;
};

/**
 * @class AampOutputProtection
 * @brief Class to enforce HDCP authentication
 */
class AampOutputProtection : public ReferenceCount
{

private:


    int                     m_sourceWidth;
    int                     m_sourceHeight;
    int                     m_displayWidth;
    int                     m_displayHeight;
    bool                    m_isHDCPEnabled;
    dsHdcpProtocolVersion_t m_hdcpCurrentProtocol;
    GstElement*             m_gstElement;

    /**
     * @fn SetHDMIStatus
     */
    void SetHDMIStatus();
    /**
     * @fn SetResolution
     * @param width display width
     * @param height display height
     */
    void SetResolution(int width, int height);

public:
    /**
     * @fn AampOutputProtection
     */
    AampOutputProtection();
    /**
     * @fn ~AampOutputProtection
     */
    virtual ~AampOutputProtection();
    /**     
     * @brief Copy constructor disabled
     *
     */
    AampOutputProtection(const AampOutputProtection&) = delete;
    /**
     * @brief assignment operator disabled
     *
     */
    AampOutputProtection& operator=(const AampOutputProtection&) = delete;


    
#ifdef IARM_MGR
    // IARM Callbacks
    /**
     * @fn HDMIEventHandler
     * @param owner Owner of the IARM mgr
     * @param eventId IARM Event ID
     * @param data HDMI data
     * @param len Length of the data
     */
    static void HDMIEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len); 
    /**
     * @fn ResolutionHandler
     * @param owner Owner of the IARM mgr
     * @param eventId IARM Event ID
     * @param data IARM data  
     * @param len Length of the data
     */
    static void ResolutionHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
#endif //IARM_MGR

    // State functions

    /**
     * @brief Check if HDCP is 2.2
     * @retval true if 2.2 false otherwise
     */
    bool isHDCPConnection2_2() { return m_hdcpCurrentProtocol == dsHDCP_VERSION_2X; }
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
    void setGstElement(GstElement *element) { m_gstElement = element;  }

    // Singleton for object creation
	
    /**
     * @fn GetAampOutputProcectionInstance
     * @retval AampOutputProtection object
     */	
    static AampOutputProtection * GetAampOutputProtectionInstance();
    /**
     * @fn IsAampOutputProcectionInstanceActive
     * @retval true or false
     */
    static bool IsAampOutputProtectionInstanceActive();
	
    /** 
    * @fn IsMS2V12Supported
    * @retval true or false
    */
    bool IsMS2V12Supported ();
};

#endif // aampoutputprotection_h
