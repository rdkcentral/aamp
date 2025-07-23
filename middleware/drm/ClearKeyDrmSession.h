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
 * @file ClearKeyDrmSession.h
 * @brief Header file for ClearKeySession
 */

#ifndef ClearKeySession_h
#define ClearKeySession_h

#include "DrmSession.h"
#include "openssl/evp.h"

#include <memory>
#include <mutex>
#include <cjson/cJSON.h>

using namespace std;

/**
 * @class ClearKeySession
 * @brief Open CDM DRM session
 */
class ClearKeySession : public DrmSession
{

private:
	std::mutex decryptMutex;

	KeyState m_eKeyState;
	string m_sessionID;
	unsigned char* m_keyStr;
	size_t m_keyLen;
	unsigned char* m_keyId;
	size_t m_keyIdLen;
	/**
	 * @fn initDRMSession
	 */
	void initDRMSession();
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	EVP_CIPHER_CTX *mOpensslCtx;
#else
	EVP_CIPHER_CTX mOpensslCtx;
#endif
public:

	/**
	 * @fn ClearKeySession
	 */
	ClearKeySession();

	/**
	 * @fn ~ClearKeySession
	 */
	~ClearKeySession();
	/**     
     	 * @brief Copy constructor disabled
     	 *
     	 */
	ClearKeySession(const ClearKeySession&) = delete;
	/**
 	 * @brief assignment operator disabled
	 *
 	 */
	ClearKeySession& operator=(const ClearKeySession&) = delete;

	/**
	 * @fn generateDRMSession
	 * @param f_pbInitData pointer to initdata
	 * @param f_cbInitData init data size
	 */
	void generateDRMSession(const uint8_t *f_pbInitData,
			uint32_t f_cbInitData, std::string &customData);

	/**
	 * @fn GenerateKeyRequest
	 * @param destinationURL : gets updated with license server url
	 * @param timeout: max timeout untill which to wait for cdm key generation.
	 * @retval Pointer to DrmData containing license request, NULL if failure.
	 */
	DrmData * generateKeyRequest(string& destinationURL, uint32_t timeout);

	/**
	 * @fn DRMProcessKey
	 * @param key : License key from license server.
	 * @param timeout: max timeout untill which to wait for cdm processing.
	 * @retval DRM_SUCCESS(1) if no errors encountered
	 */
	int processDRMKey(DrmData* key, uint32_t timeout);

	/**
	 * @fn setKeyId
	 * @param keyId Clear key ID
	 * @param keyIDLen key length
	 */
	void setKeyId(const char* keyId, int32_t keyIDLen);

	/**
	 * @fn decrypt
	 * @param f_pbIV : Initialization vector.
	 * @param f_cbIV : Initialization vector length.
	 * @param payloadData : Data to decrypt.
	 * @param payloadDataSize : Size of data.
	 * @param ppOpaqueData : pointer to opaque buffer in case of SVP.
	 * @retval Returns 0 on success.
	 */
	int decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV,
			const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData);

	//If OCDM_ADAPTOR is in use below decrypt function will be invoked from plugin
	/**
	 * @fn decrypt
	 * @param keyIDBuffer : keyID Buffer.
	 * @param ivBuffer : Initialization vector buffer.
	 * @param buffer : Data to decrypt.
	 * @param subSampleCount : subSampleCount in buffer
	 * @param subSamplesBuffer : sub Samples Buffer.
	 * @retval Returns 0 on success.
	 */

	int decrypt(GstBuffer* keyIDBuffer, GstBuffer* ivBuffer, GstBuffer* buffer, unsigned subSampleCount,
                               GstBuffer* subSamplesBuffer, GstCaps* caps);

	/**
	 * @fn getState
	 * @retval KeyState
	 */
	KeyState getState();

	/**
	 * @fn clearDecryptContext
	 */
	void clearDecryptContext();
};

#endif

