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
#ifndef AAMP_MOCK_TSB_SESSION_MANAGER_H
#define AAMP_MOCK_TSB_SESSION_MANAGER_H

#include <gmock/gmock.h>
#include "AampTSBSessionManager.h"

class MockTSBSessionManager : public AampTSBSessionManager
{
public:
	MockTSBSessionManager(PrivateInstanceAAMP *aamp) : AampTSBSessionManager(aamp) { }
	MOCK_METHOD(void, Init, ());
	MOCK_METHOD(std::shared_ptr<AampTsbReader>, GetTsbReader, (AampMediaType mediaType));
	MOCK_METHOD(bool, StartAdReservation, (const std::string &, uint64_t, AampTime));
	MOCK_METHOD(bool, EndAdReservation, (const std::string &, uint64_t, AampTime));
	MOCK_METHOD(bool, StartAdPlacement, (const std::string &, uint32_t, AampTime, double, uint32_t));
	MOCK_METHOD(bool, EndAdPlacement, (const std::string &, uint32_t, AampTime, double, uint32_t));
	MOCK_METHOD(bool, EndAdPlacementWithError, (const std::string &, uint32_t, AampTime, double, uint32_t));
	MOCK_METHOD(void, ShiftFutureAdEvents, ());
	MOCK_METHOD(void, Flush, ());
	MOCK_METHOD(bool, PushNextTsbFragment, (MediaStreamContext*, uint32_t));
};

extern MockTSBSessionManager *g_mockTSBSessionManager;

#endif /* AAMP_MOCK_TSB_SESSION_MANAGER_H */
