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
 * @file MockDrmBridge.h
 * @brief Google Mock for IDrmBridge used in AampRialtoPlayer unit tests.
 */

#pragma once

#include <gmock/gmock.h>
#include "IDrmBridge.h"

/**
 * @class MockDrmBridge
 * @brief Google Mock implementation of IDrmBridge.
 */
class MockDrmBridge : public IDrmBridge
{
public:
	MOCK_METHOD(int32_t, createSession,
		(const char *systemId, const void *initData, size_t len, AampMediaType type),
		(override));

	MOCK_METHOD(void, clearSessions, (), (override));
};

/// Global mock instance delegated to by FakeAampDrmBridge.
/// Tests that exercise DRM behaviour point this at their local MockDrmBridge
/// in SetUp() and reset it to nullptr in TearDown().
extern MockDrmBridge *g_mockDrmBridge;
