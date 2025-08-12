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
 * @file DrmSession.h
 * @brief Header file for DrmSession
 */


#ifndef DrmSession_h
#define DrmSession_h
#include <string>
#include <stdint.h>
#include <vector>
#include <gst/gst.h>
#include "DrmUtils.h"
#include "ContentSecurityManagerSession.h"

using namespace std;

#define PLAYREADY_KEY_SYSTEM_STRING "com.microsoft.playready"
#define WIDEVINE_KEY_SYSTEM_STRING "com.widevine.alpha"
#define CLEAR_KEY_SYSTEM_STRING "org.w3.clearkey"
#define VERIMATRIX_KEY_SYSTEM_STRING "com.verimatrix.ott"

#define HDCP_COMPLIANCE_CHECK_FAILURE 4327
#define HDCP_OUTPUT_PROTECTION_FAILURE 4427
/**
 * @enum KeyState 
 * @brief DRM session states
 */
typedef	enum
{
	KEY_INIT = 0,			/**< Has been initialized */
	KEY_PENDING = 1,		/**< Has a key message pending to be processed */
	KEY_READY = 2,			/**< Has a usable key */
	KEY_ERROR = 3,			/**< Has an error */
	KEY_CLOSED = 4,			/**< Has been closed */
	KEY_ERROR_EMPTY_SESSION_ID = 5	/**< Has Empty DRM session id */
	
} KeyState;

/**
 * @class DrmSession
 * @brief Base class for DRM sessions
 */
class DrmSession
{
protected:
	std::string m_keySystem;
	bool m_OutputProtectionEnabled;
	ContentSecurityManagerSession mContentSecurityManagerSession;
public:
	/**
	 * @brief Create drm session with given init data
	 * @param f_pbInitData : pointer to initdata
	 * @param f_cbInitData : init data size
	 */
	virtual void generateDRMSession(const uint8_t *f_pbInitData,uint32_t f_cbInitData, std::string &customData ) = 0;

	/**
	 * @brief Generate key request from DRM session
	 *        Caller function should free the returned memory.
	 * @param destinationURL : gets updated with license server url
	 * @param timeout: max timeout untill which to wait for cdm key generation.
	 * @retval Pointer to DrmData containing license request.
	 */
	virtual DrmData* generateKeyRequest(string& destinationURL, uint32_t timeout) = 0;

	/**
	 * @brief Updates the received key to DRM session
	 * @param key : License key from license server.
	 * @param timeout: max timeout untill which to wait for cdm key processing.
	 * @retval returns status of update request
	 */
	virtual int processDRMKey(DrmData* key, uint32_t timeout) = 0;

	/**
	 * @fn decrypt
	 * @param keyIDBuffer : Key ID.
	 * @param ivBuffer : Initialization vector.
	 * @param buffer : Data to decrypt.
	 * @param subSampleCount : Number of subsamples.
	 * @param subSamplesBuffer : Subsamples buffer.
	 * @param caps : Caps of the media that is currently being decrypted
	 * @retval Returns status of decrypt request.
	 */
        virtual int decrypt(GstBuffer* keyIDBuffer, GstBuffer* ivBuffer, GstBuffer* buffer, unsigned subSampleCount, GstBuffer* subSamplesBuffer, GstCaps* caps = NULL);

	/**
	 * @fn decrypt
	 * @param f_pbIV : Initialization vector.
	 * @param f_cbIV : Initialization vector length.
	 * @param payloadData : Data to decrypt.
	 * @param payloadDataSize : Size of data.
	 * @param ppOpaqueData : pointer to opaque buffer in case of SVP.
	 * @retval Returns status of decrypt request.
	 */
	virtual int decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV, const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData);

	/**
	 * @brief Get the current state of DRM Session.
	 * @retval KeyState
	 */
	virtual KeyState getState() = 0;

	/**
	 * @brief Waits for the current state of DRM Session to match required.. Timeout is that from the helper.
	 * Only used by OCDM Adapter for now
	 * @param state the KeyState to achieve
	 * @param timeout how long to wait in mSecs
	 * @return true if obtained, false otherwise
	 */
	virtual bool waitForState(KeyState state, const uint32_t timeout) { return true; }

	/**
	 * @brief Clear the current session context
	 *        So that new init data can be bound.
	 */
	virtual void clearDecryptContext() = 0;

	/**
	 * @fn DrmSession
	 * @param keySystem : DRM key system uuid
	 */
	DrmSession(const string &keySystem);
	/**     
     	 * @brief Copy constructor disabled
     	 *
     	 */
	DrmSession(const DrmSession&) = delete;
	/**
 	 * @brief assignment operator disabled
 	 *
 	 */
	DrmSession& operator=(const DrmSession&) = delete;
	/**
	 * @fn ~DrmSession
	 */
	virtual ~DrmSession();

	/**
	 * @fn getKeySystem
	 * @retval DRM system uuid
	 */
	string getKeySystem();

	/**
	 * @brief Set the OutputProtection for DRM Session
	 * @param bValue : Enable/Disable flag
	 * @retval void
	 */
	void setOutputProtection(bool bValue) { m_OutputProtectionEnabled = bValue;}
#if defined(USE_OPENCDM_ADAPTER)
	virtual void setKeyId(const std::vector<uint8_t>& keyId) {};
#endif
	void setSecManagerSession(ContentSecurityManagerSession session){mContentSecurityManagerSession=session;}
	ContentSecurityManagerSession getSecManagerSession() const { return mContentSecurityManagerSession;}
};
#endif
