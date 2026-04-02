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
 * @brief Stub implementation of AampDrmBridge for unit tests.
 *
 * Provides link-time resolution of AampDrmBridge symbols so that
 * AampRialtoPlayer can construct the object in the test binary without
 * pulling in the full DrmSessionManager / OCDM / priv_aamp dependency
 * chain.  All methods are no-ops; individual tests replace m_drmBridge
 * via SetDrmBridgeForTesting() with a MockDrmBridge before exercising
 * DRM behaviour, so this stub is never called by any test case.
 */

#include "AampDrmBridge.h"

AampDrmBridge::AampDrmBridge(PrivateInstanceAAMP * /*aamp*/)
	: m_aamp(nullptr)
{}

int32_t AampDrmBridge::createSession(
	const char    * /*systemId*/,
	const void    * /*initData*/,
	size_t          /*len*/,
	AampMediaType   /*type*/)
{
	return -1;
}

void AampDrmBridge::clearSessions()
{}
