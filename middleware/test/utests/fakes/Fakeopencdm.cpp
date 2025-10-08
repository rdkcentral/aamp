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

#include "open_cdm.h"
#include "open_cdm_adapter.h"
#include "MockOpenCdm.h"

MockOpenCdm *g_mockopencdm = nullptr;

struct OpenCDMSystem* opencdm_create_system(const char keySystem[])
{
	OpenCDMSystem *system = nullptr;
	if (g_mockopencdm != nullptr)
	{
		system = g_mockopencdm->opencdm_create_system(keySystem);
	}
	return system;
}

OpenCDMError opencdm_destruct_system(struct OpenCDMSystem* system)
{
	OpenCDMError ret = ERROR_NONE;
	if (g_mockopencdm != nullptr)
	{
		ret = g_mockopencdm->opencdm_destruct_system(system);
	}
	return ret;
}

OpenCDMError opencdm_construct_session(struct OpenCDMSystem* system, const LicenseType licenseType,
	const char initDataType[], const uint8_t initData[], const uint16_t initDataLength,
	const uint8_t CDMData[], const uint16_t CDMDataLength, OpenCDMSessionCallbacks* callbacks, void* userData,
	struct OpenCDMSession** session)
{
	OpenCDMError ret = ERROR_NONE;
	if (g_mockopencdm != nullptr)
	{
		ret = g_mockopencdm->opencdm_construct_session(system, licenseType, initDataType, initData, initDataLength, CDMData, CDMDataLength, callbacks, userData, session);
	}
	return ret;
}

KeyStatus opencdm_session_status(const struct OpenCDMSession* session,
	const uint8_t keyId[], const uint8_t length)
{
	KeyStatus ret = KeyStatus::Usable;
	if (g_mockopencdm != nullptr)
	{
		ret = g_mockopencdm->opencdm_session_status(session, keyId, length);
	}
	return ret;
}

OpenCDMError opencdm_session_update(struct OpenCDMSession* session,
	const uint8_t keyMessage[],
	const uint16_t keyLength)
{
	OpenCDMError ret = ERROR_NONE;
	if (g_mockopencdm != nullptr)
	{
		ret = g_mockopencdm->opencdm_session_update(session, keyMessage, keyLength);
	}
	return ret;
}

OpenCDMError opencdm_session_close(struct OpenCDMSession* session)
{
	OpenCDMError ret = ERROR_NONE;
	if (g_mockopencdm != nullptr)
	{
		ret = g_mockopencdm->opencdm_session_close(session);
	}
	return ret;
}

OpenCDMError opencdm_destruct_session(struct OpenCDMSession* session)
{
	OpenCDMError ret = ERROR_NONE;
	if (g_mockopencdm != nullptr)
	{
		ret = g_mockopencdm->opencdm_destruct_session(session);
	}
	return ret;
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
