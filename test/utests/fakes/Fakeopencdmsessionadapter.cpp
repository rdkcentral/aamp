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

#include "opencdmsessionadapter.h"
#include "AampDrmData.h"
#include "AampDrmSession.h"
#include "MockOpenCdmSessionAdapter.h"

MockOpenCdmSessionAdapter *g_mockOpenCdmSessionAdapter = nullptr;
std::vector<uint8_t> g_mockKeyId{1,2,3,4,5,6,7,8,9,0,1,2,3,4};

AAMPOCDMSessionAdapter::AAMPOCDMSessionAdapter(AampLogManager *logObj, std::shared_ptr<AampDrmHelper> drmHelper, AampDrmCallbacks *callbacks) :
    AampDrmSession(logObj, "ocdmkeysystem"), m_keyId{g_mockKeyId}, m_drmHelper{drmHelper}
{
}

AAMPOCDMSessionAdapter::~AAMPOCDMSessionAdapter()
{}
bool AAMPOCDMSessionAdapter::verifyOutputProtection()
{
    bool ret_val = true;
    if (g_mockOpenCdmSessionAdapter != nullptr)
    {
        ret_val = g_mockOpenCdmSessionAdapter->verifyOutputProtection();
    }
    return ret_val;
}

void AAMPOCDMSessionAdapter::generateAampDRMSession(const uint8_t *f_pbInitData, uint32_t f_cbInitData, std::string &customData) 
{
}

DrmData * AAMPOCDMSessionAdapter::aampGenerateKeyRequest(string& destinationURL, uint32_t timeout) 
{
    return nullptr;
}

int AAMPOCDMSessionAdapter::aampDRMProcessKey(DrmData* key, uint32_t timeout) 
{
    return 0;
}

KeyState AAMPOCDMSessionAdapter::getState() 
{
    return KEY_INIT;
}
void AAMPOCDMSessionAdapter::clearDecryptContext() 
{
}
bool AAMPOCDMSessionAdapter::waitForState(KeyState state, const uint32_t timeout)
{
    return true;
}

