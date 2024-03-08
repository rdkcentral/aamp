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

OpenCDMError opencdm_session_decrypt(struct OpenCDMSession* session,
    uint8_t encrypted[],
    const uint32_t encryptedLength,
    const EncryptionScheme encScheme,
    const EncryptionPattern pattern,
    const uint8_t* IV, uint16_t IVLength,
    const uint8_t* keyId, const uint16_t keyIdLength,
    uint32_t initWithLast15)
{
    if (g_mockopencdm != nullptr)
    {
	    return g_mockopencdm->opencdm_session_decrypt(session, encrypted, encryptedLength, encScheme, pattern, IV, IVLength, keyId, keyIdLength, initWithLast15);    
    }
    return ERROR_NONE;
}
