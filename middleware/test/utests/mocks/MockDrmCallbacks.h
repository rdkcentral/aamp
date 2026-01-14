/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#ifndef PLAYER_MOCK_DRM_CALLBACKS
#define PLAYER_MOCK_DRM_CALLBACKS

#include <gmock/gmock.h>
#include <gmock/gmock.h>
#include "MockDrmCallbacks.h"

class MockDrmCallbacks : public DrmCallbacks {
public:
    MOCK_METHOD(void, Individualization, (const std::string& payload), (override));
    MOCK_METHOD(void, LicenseRenewal, (DrmHelperPtr drmHelper, void* userData), (override));
};
#endif // PLAYER_MOCK_DRM_CALLBACKS