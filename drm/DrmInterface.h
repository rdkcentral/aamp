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

#ifndef _DRM_INTERFACE_H_
#define _DRM_INTERFACE_H_

/**
 * @file Drm_interface.h
 * @brief Header file for interface between player and  middleware DRM  
 */

#include <stddef.h>
#include <memory>
#include <condition_variable>
#include <priv_aamp.h>
#include <AampCurlDefine.h>
#include <AampUtils.h>
#ifdef AAMP_VANILLA_AES_SUPPORT
#include "Aes.h"
#endif
#include "DrmSession.h"
#include "DrmHelper.h"
#include "PlayerHlsDrmSessionInterface.h"
/**
 * @class DrmInterface
 */
class DrmInterface 
{
public:
	DrmInterface() = delete;
	/**
	 * @fn GetInstance
	 */
	static std::shared_ptr<DrmInterface> GetInstance(PrivateInstanceAAMP* aamp);

	/** 
	 * @fn DrmInterface - constructor
	 */
	DrmInterface(PrivateInstanceAAMP* aamp);
	/**
	 * @fn ~drmInterface - Destructor
	 */

	~DrmInterface();
#ifdef AAMP_VANILLA_AES_SUPPORT
	/*
	 * @fn - register callbacks between aes and aamp
	 */
	void RegisterAesInterfaceCb( std::shared_ptr<HlsDrmBase> instance);
#endif
	/*
	 * @fn - register callbacks between HlsOcdmBridge and aamp
	 */
	void RegisterHlsInterfaceCb(PlayerHlsDrmSessionInterface* instance);

	/** 
	 * variable - stores the drmInterface instance
	 */	
	static std::shared_ptr<DrmInterface> mInstance;

	/** 
	 * @fn TerminateCurlInstance - Terminate the curl instances 
	 * */	
	void TerminateCurlInstance(int mCurlInstance);

	/*
	 * @fn ProfileUpdateDrmDecryptInit -Update init profiling 
	 */
	void ProfileUpdateDrmDecrypt(bool type, int bucketType);

	/**
	 * @fn GetCurlInit 
	 */
	void GetCurlInit(int &curlInstance);

	/**
	 * @fn -NotifyDrmError Notify the DRM errors
	 */
	void NotifyDrmError(int drmFailure);
	/**
	 * Storing aamp instance */
	PrivateInstanceAAMP* mpAamp;
	/**Storing AampGrowableBuffer */
	AampGrowableBuffer mAesKeyBuf;

	/** 
	 * @fn GetAccessKey 
	 */
	void GetAccessKey(std::string &keyURI,
			std::string& tempEffectiveUrl, int& http_error, double&
			downloadTime, unsigned int curlInstance, bool
			&KeyAcquisitionStatus, int &failureReason,   char**
			ptr);
	/*
	 * @fn getHlsDrmSession 
	 */
	void  getHlsDrmSession(std::shared_ptr <HlsDrmBase>&bridge, std::shared_ptr<DrmHelper> &drmHelper,DrmSession* &session , int streamType);

	/*
	 * @fn enumeration update mapping wrt aamp enumeration 
	 */
	ProfilerBucketType MapDrmToProfilerBucket(DrmProfilerBucketType drmType);

	/*
	 *@brief Updates the PrivateInstanceAAMP instance.
	 */
	void UpdateAamp(PrivateInstanceAAMP* aamp);

};

#endif // _DRM_INTERFACE_H_
