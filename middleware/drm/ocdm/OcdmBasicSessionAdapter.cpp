/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
 * @file OcdmBasicSessionAdapter.cpp
 * @brief Handles operations on OCDM session
 */

#include "OcdmBasicSessionAdapter.h"
#include "PlayerLogManager.h"

int OCDMBasicSessionAdapter::decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV, const uint8_t *payloadData,
										 uint32_t payloadDataSize, uint8_t **ppOpaqueData)
{
	if (!verifyOutputProtection())
	{
		return HDCP_COMPLIANCE_CHECK_FAILURE;
	}

	std::lock_guard<std::mutex> guard(decryptMutex);

	uint8_t *dataToSend = const_cast<uint8_t *>(payloadData);
	uint32_t sizeToSend = payloadDataSize;
	std::vector<uint8_t> vdata;

	if (m_drmHelper->getMemorySystem() != nullptr)
	{
		if (!m_drmHelper->getMemorySystem()->encode(payloadData, payloadDataSize, vdata))
		{
			MW_LOG_WARN("Failed to encode memory for transmission");
			return -1;
		}
		sizeToSend = (uint32_t)vdata.size();
		dataToSend = vdata.data();
	}

	EncryptionScheme encScheme = AesCtr_Cenc;
	EncryptionPattern pattern = {0};
	/* CID:313823 - Waiting while holding a lock, got detected due to usage of external API. It may be replaced if approach is redesigned in future */
	int retvalue = opencdm_session_decrypt(m_pOpenCDMSession,
										   dataToSend,
										   sizeToSend,
										   encScheme, pattern,
										   f_pbIV, f_cbIV,
										   m_keyId.data(), m_keyId.size(), 0 );
	if (retvalue != 0)
	{
		if (m_drmHelper->getMemorySystem() != nullptr)
		{
			m_drmHelper->getMemorySystem()->terminateEarly();
		}
		MW_LOG_INFO("decrypt returned : %d", retvalue);
	}
	else if (m_drmHelper->getMemorySystem() != nullptr)
	{
		if (!m_drmHelper->getMemorySystem()->decode(dataToSend, sizeToSend, const_cast<uint8_t*>(payloadData), payloadDataSize))
		{
			MW_LOG_WARN("Failed to decode memory for transmission");
			return -1;
		}
	}	
	return retvalue;
}
