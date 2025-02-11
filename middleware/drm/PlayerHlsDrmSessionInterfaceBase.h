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

#ifndef _PLAYER_HLS_DRM_SESSION_INTERFACE_BASE_H
#define _PLAYER_HLS_DRM_SESSION_INTERFACE_BASE_H

/**
 * @file PlayerHlsDrmSessionManagerBase.h
 * @brief Operations for HLS DRM, base class
 */


#include "HlsDrmBase.h"
#include "DrmSession.h"
#include <functional>
#include "DrmHelper.h"

enum DrmMediaType
{ // renamed from "MediaType" to avoid namespace collision with OpenCDM definition
	eDRM_MEDIATYPE_VIDEO,               /**< Type video */
	eDRM_MEDIATYPE_AUDIO,               /**< Type audio */
	eDRM_MEDIATYPE_SUBTITLE,            /**< Type subtitle */
	eDRM_MEDIATYPE_AUX_AUDIO,           /**< Type auxiliary audio */
	eDRM_MEDIATYPE_MANIFEST,            /**< Type manifest */
	eDRM_MEDIATYPE_LICENCE,             /**< Type license */
	eDRM_MEDIATYPE_IFRAME,              /**< Type iframe */
	eDRM_MEDIATYPE_INIT_VIDEO,          /**< Type video init fragment */
	eDRM_MEDIATYPE_INIT_AUDIO,          /**< Type audio init fragment */
	eDRM_MEDIATYPE_INIT_SUBTITLE,       /**< Type subtitle init fragment */
	eDRM_MEDIATYPE_INIT_AUX_AUDIO,      /**< Type auxiliary audio init fragment */
	eDRM_MEDIATYPE_PLAYLIST_VIDEO,      /**< Type video playlist */
	eDRM_MEDIATYPE_PLAYLIST_AUDIO,      /**< Type audio playlist */
	eDRM_MEDIATYPE_PLAYLIST_SUBTITLE,	/**< Type subtitle playlist */
	eDRM_MEDIATYPE_PLAYLIST_AUX_AUDIO,	/**< Type auxiliary audio playlist */
	eDRM_MEDIATYPE_PLAYLIST_IFRAME,     /**< Type Iframe playlist */
	eDRM_MEDIATYPE_INIT_IFRAME,         /**< Type IFRAME init fragment */
	eDRM_MEDIATYPE_DSM_CC,              /**< Type digital storage media command and control (DSM-CC) */
	eDRM_MEDIATYPE_IMAGE,	        	/**< Type image for thumbnail playlist */
	eDRM_MEDIATYPE_DEFAULT              /**< Type unknown */
};

using GetHlsDrmSessionCallback = std::function<void(std::shared_ptr <HlsDrmBase>&bridge, std::shared_ptr<DrmHelper> &drmHelper ,  DrmSession* &session , int streamType)>;

/**
 * @class PlayerHlsDrmSessionInterfaceBase
 * @brief DRM Session Interface Base class for HLS stream operations
 */

class PlayerHlsDrmSessionInterfaceBase
{
protected:
	DrmSession* mDrmSession;
public:
	/**
	 * @fn isDrmSupported
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @return true if a DRM support available, false otherwise
	 */
	virtual bool isDrmSupported(const struct DrmInfo& drmInfo) const {return false;}

	/**
	 * @fn createSession
	 * @param drmInfo DrmInfo built by the HLS manifest parser
	 * @param streamType streamType
	 * @return true if a DRM support available, false otherwise
	 */
	virtual std::shared_ptr<HlsDrmBase> createSession( const struct DrmInfo& drmInfo, int  streamType){return nullptr;}
   
   	/** ProfileUpdate callback for initiating the curl init  from application  */
    GetHlsDrmSessionCallback GetHlsDrmSessionCb;

	/**
	 *@brief Registers GetAccessKey callback from application
	 */
	void RegisterGetHlsDrmSessionCb(const GetHlsDrmSessionCallback Callback){
			GetHlsDrmSessionCb = Callback;
	};
};

#endif //_PLAYER_HLS_DRM_SESSION_INTERFACE_BASE_H
