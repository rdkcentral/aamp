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

#ifndef AAMP_MOCK_INTERFACE_PLAYER_RDK_H
#define AAMP_MOCK_INTERFACE_PLAYER_RDK_H

#include <gmock/gmock.h>
#include <memory>

/**
 * @brief Standalone mock for the InterfacePlayerRDK seam used by AAMPGstPlayer.
 *
 * Only the methods that L1 tests need to observe or control are listed here.
 * FakeInterfacePlayerRDK.cpp delegates the matching real-class methods to
 * g_mockInterfacePlayerRDK when the pointer is non-null, following the same
 * pattern used by MockPrivateInstanceAAMP / FakePrivateInstanceAAMP.
 */
class MockInterfacePlayerRDK
{
public:
	/**
	 * @brief Mirrors InterfacePlayerRDK::CheckDiscontinuity.
	 *
	 * @param mediaType        Track type (int cast of AampMediaType).
	 * @param streamFormat     Current video format (int cast of MediaFormat).
	 * @param codecChange      Result of aamp->ReconfigureForElementaryStreamUpdate().
	 * @param unblockDiscProcess  Output: set true to trigger CompleteDiscontinuityDataDeliverForPTSRestamp.
	 * @param shouldHaltBuffering Output: set true to stop buffering (codec-change EOS path).
	 * @return true if the inject-loop should stop; false to keep injection running.
	 */
	MOCK_METHOD(bool, CheckDiscontinuity,
		(int mediaType, int streamFormat, bool codecChange,
		 bool& unblockDiscProcess, bool& shouldHaltBuffering));
};

extern std::shared_ptr<MockInterfacePlayerRDK> g_mockInterfacePlayerRDK;

#endif /* AAMP_MOCK_INTERFACE_PLAYER_RDK_H */
