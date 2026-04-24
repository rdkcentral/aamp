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


#include <gtest/gtest.h>
#include "support/aampabr/HybridABRManager.h"

extern HybridABRManager::AampAbrConfig eAAMPAbrConfig;

class HybridAbrTests : public ::testing::Test
{
protected:
	HybridABRManager abrManager;
	void SetUp() override
	{
		eAAMPAbrConfig = HybridABRManager::AampAbrConfig();
		// Cache life is interpreted as milliseconds by the estimator.
		// Use a large value to avoid timing-related flakes in unit tests.
		eAAMPAbrConfig.abrCacheLife = 600000;
		eAAMPAbrConfig.abrCacheLength = 3;
		eAAMPAbrConfig.abrCacheOutlier = 1000000;
	}
};

TEST_F(HybridAbrTests, UpdateABRBitrateDataBasedOnCacheOutlierEven)
{
	std::vector<long> tmpData = {100, 200, 300, 400, 500, 600};
	long result = abrManager.UpdateABRBitrateDataBasedOnCacheOutlier(tmpData);
	// Median of {100, 200, 300, 400, 500, 600} is (300+400)/2 = 350.
	// Outlier diff is 1000000, so no outliers will be removed.
	// Average is (100+200+300+400+500+600)/6 = 350.
	ASSERT_EQ(result, 350);
}

/**
 * @brief Two HybridABRManager instances must have independent rampup loop
 *        counters. Verifies the per-instance mRampupFromSteadyStateLoop fix
 *        in HybridABRManager (VPAAMP-173).
 */
TEST_F(HybridAbrTests, CheckRampupFromSteadyState_LoopIsPerInstance)
{
	eAAMPAbrConfig.abrBufferCounter = 2;

	HybridABRManager mgr1;
	mgr1.ReadPlayerConfig(&eAAMPAbrConfig);

	HybridABRManager mgr2;
	mgr2.ReadPlayerConfig(&eAAMPAbrConfig);

	// Add two video profiles to each manager so rampup has a target
	ABRManager::ProfileInfo p{};
	p.isIframeTrack = false;
	p.bandwidthBitsPerSecond = 1000000;
	p.width = 640; p.height = 360;
	mgr1.addProfile(p);
	mgr2.addProfile(p);
	p.bandwidthBitsPerSecond = 2000000;
	p.width = 1280; p.height = 720;
	mgr1.addProfile(p);
	mgr2.addProfile(p);

	int currIdx = 0;
	int newIdx1 = currIdx;
	int newIdx2 = currIdx;
	long nwBw = 1800000;
	long newBw = 2000000;
	HybridABRManager::BitrateChangeReason reason1 = HybridABRManager::eAAMP_BITRATE_CHANGE_BY_ABR;
	HybridABRManager::BitrateChangeReason reason2 = HybridABRManager::eAAMP_BITRATE_CHANGE_BY_ABR;
	int maxBuf1 = 1;
	int maxBuf2 = 1;

	// Ramp up mgr1 three times to advance its loop counter
	for (int i = 0; i < 3; i++)
	{
		newIdx1 = currIdx;
		reason1 = HybridABRManager::eAAMP_BITRATE_CHANGE_BY_ABR;
		mgr1.CheckRampupFromSteadyState(
			currIdx, newIdx1, nwBw, 20.0, newBw, reason1, maxBuf1);
	}

	// Now ramp up mgr2 once — its loop should be independent
	mgr2.CheckRampupFromSteadyState(
		currIdx, newIdx2, nwBw, 20.0, newBw, reason2, maxBuf2);

	// mgr2's first rampup: loop goes 1->2, maxBuf = 2^2 = 4
	// If mRampupFromSteadyStateLoop were static, mgr2 would inherit mgr1's counter
	EXPECT_EQ(maxBuf2, 4);
	// mgr1 called 3 times: loop went 1->2->3->4, maxBuf = 2^4 = 16
	EXPECT_EQ(maxBuf1, 16);
}

TEST_F(HybridAbrTests, UpdateABRBitrateDataBasedOnCacheOutlierOdd)
{
	std::vector<long> tmpData = {100, 200, 300, 400, 500};
	long result = abrManager.UpdateABRBitrateDataBasedOnCacheOutlier(tmpData);
	// Median of {100, 200, 300, 400, 500} is 300.
	// Outlier diff is 1000000, so no outliers will be removed.
	// Average is (100+200+300+400+500)/5 = 300.
	ASSERT_EQ(result, 300);
}
