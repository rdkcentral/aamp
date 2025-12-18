/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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
 * @file FakeDrmSessionFactory.cpp
 * @brief Fake implementation for DrmSessionFactory with mock support
 */

#include "DrmSessionFactory.h"
#include "MockDrmSessionFactory.h"

// Global pointer to the mock DrmSessionFactory instance
MockDrmSessionFactory* g_MockDrmSessionFactory = nullptr;

/**
 * @brief Fake implementation of GetDrmSession
 * 
 * If g_MockDrmSessionFactory is set, delegates to the mock.
 * Otherwise returns nullptr.
 */
DrmSession* DrmSessionFactory::GetDrmSession(DrmHelperPtr drmHelper, DrmCallbacks* drmCallbacks)
{
	if (g_MockDrmSessionFactory)
	{
		return g_MockDrmSessionFactory->GetDrmSession(drmHelper, drmCallbacks);
	}
	return nullptr;
}
