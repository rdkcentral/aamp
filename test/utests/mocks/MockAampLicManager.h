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

#ifndef AAMP_MOCK_AAMP_LICENSE_MANAGER_H
#define AAMP_MOCK_AAMP_LICENSE_MANAGER_H

#include <gmock/gmock.h>
#include <memory>
#include "AampDRMLicManager.h"

class MockAampLicenseManager
{
public:
    MOCK_METHOD(void, setVideoWindowSize, (int width, int height));
    MOCK_METHOD(DrmSession*, createDrmSession, (std::shared_ptr<DrmHelper> drmHelper, DrmCallbacks* aampInstance,  DrmMetaDataEventPtr eventHandle, int streamTypeIn));
    MOCK_METHOD(void, setSessionMgrState, (SessionMgrState state));
    MOCK_METHOD(bool, queueContentProtection, (DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type, bool isVssPeriod));
    MOCK_METHOD(void, queueProtectionEvent, (DrmHelperPtr drmHelper, std::string periodId, uint32_t adapIdx, AampMediaType type));
    MOCK_METHOD(void, notifyCleanup, ());
};

extern std::shared_ptr<MockAampLicenseManager> g_mockAampLicenseManager;

#endif /* AAMP_MOCK_AAMP_LICENSE_MANAGER_H */
