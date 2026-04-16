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
#ifndef AAMP_MOCK_PLAYER_CC_MANAGER_H
#define AAMP_MOCK_PLAYER_CC_MANAGER_H

#include <gmock/gmock.h>
#include "PlayerCCManager.h"

class MockPlayerCCManager
{
public:
	MOCK_METHOD(int, Init, (void *handle));
	MOCK_METHOD(void, RestoreCC, (bool shouldRestoreCC));
	MOCK_METHOD(void, Release, (int iID));
	MOCK_METHOD(bool, IsOOBCCRenderingSupported, ());
	MOCK_METHOD(int, SetStatus, (bool enable));
	MOCK_METHOD(int, SetStyle, (const std::string &options));
	MOCK_METHOD(int, SetTrack, (const std::string &track, const CCFormat format));
	MOCK_METHOD(void, SetTrickplayStatus, (bool enable));
	MOCK_METHOD(void, SetParentalControlStatus, (bool locked));
	MOCK_METHOD(void, StartRendering, ());
	MOCK_METHOD(void, StopRendering, ());
	MOCK_METHOD(int, SetDigitalChannel, (unsigned int id));
	MOCK_METHOD(int, SetAnalogChannel, (unsigned int id));

	MOCK_METHOD(bool, CheckCCHandle, (), (const));

	// Virtual methods with default implementations
	MOCK_METHOD(int, GetId, ());
	MOCK_METHOD(void, updateLastTextTracks, (const std::vector<CCTrackInfo> &newTextTracks));

	// Non-virtual methods for additional testing flexibility
	MOCK_METHOD(void, EnsureInitialized, ());
	MOCK_METHOD(void, EnsureHALInitialized, ());
	MOCK_METHOD(void, EnsureRendererCommsInitialized, ());
	MOCK_METHOD(int, Initialize, (void *handle));
};

extern std::shared_ptr<MockPlayerCCManager> g_mockPlayerCCManager;

#endif /* AAMP_MOCK_PLAYER_CC_MANAGER_H */
