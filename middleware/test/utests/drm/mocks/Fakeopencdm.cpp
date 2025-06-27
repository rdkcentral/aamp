/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#include <string.h>

#include "open_cdm.h"
#include "open_cdm_adapter.h"
#include <gst/gst.h>

#include "Fakeopencdm.h"
#include "MockOpenCdm.h"
#include "PlayerLogManager.h"

MockOpenCdm* g_mockopencdm = nullptr;

typedef struct _MockOpenCdmInstanceData
{
	MockOpenCdmSessionInfo sessionInfo;
	MockOpenCdmCallbacks callbacks;
	void* mockUserData; // User data from the client of the mock
} MockOpenCdmInstanceData;

static MockOpenCdmInstanceData f_mockInstance;

/* BEGIN - methods to access mock functionality */
MockOpenCdmSessionInfo* MockOpenCdmGetSessionInfo()
{
	return &f_mockInstance.sessionInfo;
}

void MockOpenCdmSetCallbacks(MockOpenCdmCallbacks callbacks, void* mockUserData)
{
	f_mockInstance.callbacks = callbacks;
	f_mockInstance.mockUserData = mockUserData;
}

void MockOpenCdmReset(void)
{
	memset(&f_mockInstance, 0, sizeof(f_mockInstance));
}

/* END - methods to access mock functionality */

struct OpenCDMSystem* opencdm_create_system(const char keySystem[])
{
	if (g_mockopencdm != nullptr)
	{
		return g_mockopencdm->opencdm_create_system(keySystem);
	}
	return nullptr;
}

OpenCDMError opencdm_construct_session(struct OpenCDMSystem* system, const LicenseType licenseType,
									   const char initDataType[], const uint8_t initData[],
									   const uint16_t initDataLength, const uint8_t CDMData[],
									   const uint16_t CDMDataLength,
									   OpenCDMSessionCallbacks* callbacks, void* userData,
									   struct OpenCDMSession** session)
{
	OpenCDMError retValue = ERROR_NONE;
	*session = nullptr;
	MW_LOG_ERR("TRACE Enter opencdm_constructSession");
	if (g_mockopencdm != nullptr)
	{
		retValue = g_mockopencdm->opencdm_construct_session(
			system, licenseType, initDataType, initData, initDataLength, CDMData, CDMDataLength,
			callbacks, userData, session);
	}
#if 0
	MW_LOG_ERR("TRACE2");
	f_mockInstance.sessionInfo.session = *session;
	f_mockInstance.sessionInfo.callbacks = *callbacks;
	f_mockInstance.sessionInfo.userData = userData;
	MW_LOG_ERR("TRACE3");
	if (f_mockInstance.callbacks.constructSessionCallback)
	{
		f_mockInstance.callbacks.constructSessionCallback(&f_mockInstance.sessionInfo,
														  f_mockInstance.mockUserData);
	}
	MW_LOG_ERR("TRACE4");
#endif
	return retValue;
}

OpenCDMError opencdm_destruct_system(struct OpenCDMSystem* system)
{
	return ERROR_NONE;
}

KeyStatus opencdm_session_status(const struct OpenCDMSession* session, const uint8_t keyId[],
								 const uint8_t length)
{
	return Usable;
}

OpenCDMError opencdm_session_update(struct OpenCDMSession* session, const uint8_t keyMessage[],
									const uint16_t keyLength)
{
	OpenCDMError retValue = ERROR_NONE;

	if (g_mockopencdm != nullptr)
	{
		retValue = g_mockopencdm->opencdm_session_update(session, keyMessage, keyLength);
	}

	if (f_mockInstance.callbacks.sessionUpdateCallback)
	{
		f_mockInstance.callbacks.sessionUpdateCallback(&f_mockInstance.sessionInfo, keyMessage,
													   keyLength);
	}

	return retValue;
}

OpenCDMError opencdm_gstreamer_session_decrypt(struct OpenCDMSession* session, GstBuffer* buffer,
											   GstBuffer* subSample, const uint32_t subSampleCount,
											   GstBuffer* IV, GstBuffer* keyID,
											   uint32_t initWithLast15)
{
	return ERROR_NONE;
}

OpenCDMError opencdm_session_decrypt(struct OpenCDMSession* session, uint8_t encrypted[],
									 const uint32_t encryptedLength,
									 const EncryptionScheme encScheme,
									 const EncryptionPattern pattern, const uint8_t* IV,
									 uint16_t IVLength, const uint8_t* keyId,
									 const uint16_t keyIdLength, uint32_t initWithLast15)
{
	if (g_mockopencdm != nullptr)
	{
		return g_mockopencdm->opencdm_session_decrypt(session, encrypted, encryptedLength,
													  encScheme, pattern, IV, IVLength, keyId,
													  keyIdLength, initWithLast15);
	}
	return ERROR_NONE;
}

OpenCDMError opencdm_session_close(struct OpenCDMSession* session)
{
	return ERROR_NONE;
}

OpenCDMError opencdm_destruct_session(struct OpenCDMSession* session)
{
	return ERROR_NONE;
}
