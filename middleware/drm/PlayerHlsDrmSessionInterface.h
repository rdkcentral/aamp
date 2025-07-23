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

#ifndef _PLAYER_HLS_DRM_SESSION_INTERFACE_H
#define _PLAYER_HLS_DRM_SESSION_INTERFACE_H

/**
 * @file PlayerHlsDrmSessionInterface.h
 * @brief Interface for HLS DRM
 */


#include "PlayerHlsDrmSessionInterfaceBase.h"


/**
 * @class FakeHlsDrmSessionManager
 * @brief DRM Session manager for HLS stream operations
 */

class FakeHlsDrmSessionManager : public PlayerHlsDrmSessionInterfaceBase
{

public:
	/**
	 * @fn isDrmSupported
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return true if a DRM support available, false otherwise
	 */
	bool isDrmSupported(const struct DrmInfo& drmInfo) const override {return false;}

	/**
	 * @fn createSession
	 * @param drmInfo DrmInfo built by the HLS manifest parser
     * @param streamType streamType
	 * @return true if a DRM support available, false otherwise
	 */
	std::shared_ptr<HlsDrmBase> createSession( const struct DrmInfo& drmInfo, int  streamType) override {return nullptr;}
   
};

class PlayerHlsDrmSessionInterface
{

private:

    PlayerHlsDrmSessionInterfaceBase* m_pHlsDrmSessionManager;

	PlayerHlsDrmSessionInterface();

public:

    /**
	 * @fn getInstance 
	 * @return Player Hls Drm Session Interface instance
	 */
	static PlayerHlsDrmSessionInterface* getInstance();
	/**
	 * @fn isDrmSupported
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return true if a DRM support available, false otherwise
	 */
	bool isDrmSupported(const struct DrmInfo& drmInfo) const;

	/**
	 * @fn createSession
	 * @param drmInfo DrmInfo built by the HLS manifest parser
     * @param streamType streamType
	 * @return true if a DRM support available, false otherwise
	 */
	std::shared_ptr<HlsDrmBase> createSession( const struct DrmInfo& drmInfo, int  streamType);
   /** ProfileUpdate callback for initiating the curl init  from application  */
        
    /**
     *@brief Registers GetAccessKey callback from application
     */
    void RegisterGetHlsDrmSessionCb(const GetHlsDrmSessionCallback Callback);
};

#endif //_PLAYER_HLS_DRM_SESSION_INTERFACE_H
