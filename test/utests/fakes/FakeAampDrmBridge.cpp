/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
 * @file FakeAampDrmBridge.cpp
 * @brief Fake implementation of AampDrmBridge for L1 tests.
 *
 * Methods delegate to g_mockDrmBridge when non-null, allowing test cases to
 * place EXPECT_CALL expectations on a MockDrmBridge without any injection
 * seam in the production AampRialtoPlayer constructor.
 * g_mockDrmBridge is declared in MockDrmBridge.h.
 */

#include "AampDrmBridge.h"
#include "MockDrmBridge.h"

/// Definition of the global declared in MockDrmBridge.h.
MockDrmBridge *g_mockDrmBridge = nullptr;

AampDrmBridge::AampDrmBridge(PrivateInstanceAAMP * /*aamp*/)
	: m_aamp(nullptr)
{}

int32_t AampDrmBridge::createSession(
	const char    *systemId,
	const void    *initData,
	size_t         len,
	AampMediaType  type)
{
	if (g_mockDrmBridge)
	{
		return g_mockDrmBridge->createSession(systemId, initData, len, type);
	}
	return -1;
}

void AampDrmBridge::clearSessions()
{
	if (g_mockDrmBridge)
	{
		g_mockDrmBridge->clearSessions();
	}
}
