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

#ifndef AAMP_MOCK_ABR_MANAGER_H
#define AAMP_MOCK_ABR_MANAGER_H

#include <gmock/gmock.h>
#include <memory>
#include "abr.h"

class MockABRManager
{
public:
	MOCK_METHOD(int, getProfileCount, ());
	MOCK_METHOD(int, getBestMatchedProfileIndexByBandWidth, (int bandwidth));
	MOCK_METHOD(int, getMaxBandwidthProfile, (const std::string& periodId));
	MOCK_METHOD(BitsPerSecond, getBandwidthOfProfile, (int profileIndex));
	MOCK_METHOD(void, clearProfiles, ());
	MOCK_METHOD(void, addProfile, (const ABRManager::ProfileInfo &profile));
	MOCK_METHOD(int, getRampedDownProfileIndex, (int currentProfileIndex, const std::string& periodId));
	MOCK_METHOD(int, getUserDataOfProfile, (int currentProfileIndex));
	MOCK_METHOD(void, setDefaultInitBitrate, (long defaultInitBitrate));
	MOCK_METHOD(void, updateProfile, ());
	MOCK_METHOD(int, getDesiredIframeProfile, ());
	MOCK_METHOD(int, getInitialProfileIndex, (bool chooseMediumProfile, const std::string& periodId));
	MOCK_METHOD(int, getLowestIframeProfile, ());
	MOCK_METHOD(int, getProfileIndexByBitrateRampUpOrDown, (int currentProfileIndex, BitsPerSecond currentBandwidth, BitsPerSecond networkBandwidth, int nwConsistencyCnt, const std::string& periodId));
	MOCK_METHOD(int, getRampedUpProfileIndex, (int currentProfileIndex, const std::string& periodId));
	MOCK_METHOD(void, CheckRampupFromSteadyState, (int currProfileIndex, int &newProfileIndex, BitsPerSecond nwBandwidth, double bufferValue, BitsPerSecond newBandwidth, ABRManager::BitrateChangeReason &mhBitrateReason, int &mMaxBufferCountCheck, const std::string& periodId));
	MOCK_METHOD(bool, isProfileIndexBitrateLowest, (int currentProfileIndex, const std::string& periodId));
};

extern std::shared_ptr<MockABRManager> g_mockABRManager;

#endif // AAMP_MOCK_ABR_MANAGER_H
