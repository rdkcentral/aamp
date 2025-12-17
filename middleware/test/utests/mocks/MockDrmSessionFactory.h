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

#ifndef GST_MOCK_DRM_SESSION_FACTORY_H
#define GST_MOCK_DRM_SESSION_FACTORY_H

#include <gmock/gmock.h>
#include "DrmSessionFactory.h"

/**
 * @class MockDrmSessionFactory
 * @brief Mock class for DrmSessionFactory to enable testing
 */
class MockDrmSessionFactory
{
public:
	MOCK_METHOD(DrmSession*, GetDrmSession, (DrmHelperPtr drmHelper, DrmCallbacks* drmCallbacks), ());
};

extern MockDrmSessionFactory *g_mockDrmSessionFactory;

#endif // GST_MOCK_DRM_SESSION_FACTORY_H
