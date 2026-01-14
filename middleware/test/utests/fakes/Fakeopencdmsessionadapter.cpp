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
#include "DrmData.h"
#include "DrmSession.h"
#include "MockOpenCdmSessionAdapter.h"

MockOpenCdmSessionAdapter *g_mockOpenCdmSessionAdapter = nullptr;
std::vector<uint8_t> g_mockKeyId{1,2,3,4,5,6,7,8,9,0,1,2,3,4};

OCDMSessionAdapter::OCDMSessionAdapter(std::shared_ptr<DrmHelper> drmHelper, DrmCallbacks *callbacks) :
    DrmSession("ocdmkeysystem"), m_keyId{g_mockKeyId}, m_drmHelper{drmHelper}
{
}

OCDMSessionAdapter::~OCDMSessionAdapter()
{}
bool OCDMSessionAdapter::verifyOutputProtection()
{
    bool ret_val = true;
    if (g_mockOpenCdmSessionAdapter != nullptr)
    {
        ret_val = g_mockOpenCdmSessionAdapter->verifyOutputProtection();
    }
    return ret_val;
}

void OCDMSessionAdapter::generateDRMSession(const uint8_t *f_pbInitData, uint32_t f_cbInitData, std::string &customData)
{

}

DrmData * OCDMSessionAdapter::generateKeyRequest(string& destinationURL, uint32_t timeout)
{
    return nullptr;
}

int OCDMSessionAdapter::processDRMKey(DrmData* key, uint32_t timeout)
{
    return 0;
}

KeyState OCDMSessionAdapter::getState()
{
    return KEY_INIT;
}
void OCDMSessionAdapter::clearDecryptContext()
{
}
bool OCDMSessionAdapter::waitForState(KeyState state, const uint32_t timeout)
{
    return true;
}
