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

#ifndef AAMP_MOCK_AD_MANAGER_H
#define AAMP_MOCK_AD_MANAGER_H

#include <gmock/gmock.h>
#include "admanager_mpd.h"

class MockPrivateCDAIObjectMPD
{
public:
    MOCK_METHOD(bool, isAdBreakObjectExist, (const std::string &adBreakId));
    MOCK_METHOD(bool, WaitForNextAdResolved, (int timeoutMs));
    MOCK_METHOD(bool, WaitForNextAdResolved, (int timeoutMs, std::string periodId));
    MOCK_METHOD(int, CheckForAdStart, (const float &rate, bool init, const std::string &periodId, double offSet, std::string &breakId, double &adOffset));
    MOCK_METHOD(void, SetAlternateContents, (const std::string &adBreakId, const std::string &adId, const std::string &url));
};

extern MockPrivateCDAIObjectMPD *g_MockPrivateCDAIObjectMPD;

#endif /* AAMP_MOCK_AD_MANAGER_H */
