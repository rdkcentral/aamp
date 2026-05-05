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
// SpeedCache is provided by AampSpeedCache.h, included (or defined inline as
// a fallback) by HybridABRManager.h above.

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
 *        in HybridABRManager.
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

/**
 * @brief Bug #6: CheckAbrThresholdSize must multiply before dividing to
 *        avoid integer truncation when bufferlen < downloadTimeMs.
 */
TEST_F(HybridAbrTests, CheckAbrThresholdSize_SmallFragment_NoTruncation)
{
	// 500 bytes in 1000 ms → (500 * 8000) / 1000 = 4000 bps
	// Old code: (500 / 1000) * 8000 = 0 * 8000 = 0  (BUG)
	long result = abrManager.CheckAbrThresholdSize(
		500, 1000, 100000, 2000,
		HybridABRManager::eCURL_ABORT_REASON_NONE);
	EXPECT_EQ(result, 4000);
}

TEST_F(HybridAbrTests, CheckAbrThresholdSize_LargeFragment_Correct)
{
	// 125000 bytes in 1000 ms → (125000 * 8000) / 1000 = 1000000 bps
	// Use fragmentDurationMs=1500 so downloadTimeMs(1000) >= fragmentDurationMs/2(750)
	// to avoid the guard clause that resets downloadbps to currentProfilebps
	long result = abrManager.CheckAbrThresholdSize(
		125000, 1000, 2000000, 1500,
		HybridABRManager::eCURL_ABORT_REASON_NONE);
	EXPECT_EQ(result, 1000000);
}

/**
 * @brief Bug #17: CheckLLDashABRSpeedStoreSize must multiply before dividing
 *        to avoid integer truncation when total_dl_diff < time_diff.
 */
TEST_F(HybridAbrTests, CheckLLDashABRSpeedStoreSize_SmallChunk_NoTruncation)
{
	SpeedCache sc{};
	long bps = 0;
	// 500 bytes downloaded in 1000 ms diff → (500 * 8000) / 1000 = 4000 bps
	// Old code: (500 / 1000) * 8000 = 0  (BUG)
	abrManager.CheckLLDashABRSpeedStoreSize(&sc, bps, 2000, 500, 1000, 500);
	EXPECT_EQ(sc.speed_now, 4000);
}

TEST_F(HybridAbrTests, CheckLLDashABRSpeedStoreSize_LargeChunk_Correct)
{
	SpeedCache sc{};
	long bps = 0;
	// 10000 bytes in 500 ms → (10000 * 8000) / 500 = 160000 bps
	abrManager.CheckLLDashABRSpeedStoreSize(&sc, bps, 1000, 10000, 500, 10000);
	EXPECT_EQ(sc.speed_now, 160000);
}

/**
 * @brief Bug #11: FragmentfailureRampdown must skip iframe tracks when
 *        selecting a rampdown target, matching every other ABR function.
 */
TEST_F(HybridAbrTests, FragmentfailureRampdown_SkipsIframeTrack)
{
	eAAMPAbrConfig.abrMaxBuffer = 30;

	HybridABRManager mgr;
	mgr.ReadPlayerConfig(&eAAMPAbrConfig);

	ABRManager::ProfileInfo p{};
	// Profile 0: 500 kbps video
	p.isIframeTrack = false;
	p.bandwidthBitsPerSecond = 500000;
	p.width = 320; p.height = 240;
	mgr.addProfile(p);

	// Profile 1: 800 kbps iframe — should be skipped
	p.isIframeTrack = true;
	p.bandwidthBitsPerSecond = 800000;
	p.width = 640; p.height = 360;
	mgr.addProfile(p);

	// Profile 2: 1 Mbps video
	p.isIframeTrack = false;
	p.bandwidthBitsPerSecond = 1000000;
	p.width = 640; p.height = 360;
	mgr.addProfile(p);

	// Profile 3: 2 Mbps video (current)
	p.isIframeTrack = false;
	p.bandwidthBitsPerSecond = 2000000;
	p.width = 1280; p.height = 720;
	mgr.addProfile(p);

	// Buffer at 15/30 = 50%.  Sorted video BWs: 500k, 1M, 2M.
	// Profile percentages (relative to max 2M): 25%, 50%, 100%.
	// Iterating descending: 100% !< 50% skip, 50% !< 50% skip, 25% < 50% → 500k.
	// Without the fix the iframe 800k (40%) would also be in the list and could
	// be selected as the rampdown target.
	long result = mgr.FragmentfailureRampdown(15, 3);
	EXPECT_EQ(result, 500000);
}

TEST_F(HybridAbrTests, FragmentfailureRampdown_NoIframes_NormalBehavior)
{
	eAAMPAbrConfig.abrMaxBuffer = 30;

	HybridABRManager mgr;
	mgr.ReadPlayerConfig(&eAAMPAbrConfig);

	ABRManager::ProfileInfo p{};
	p.isIframeTrack = false;

	p.bandwidthBitsPerSecond = 500000;
	p.width = 320; p.height = 240;
	mgr.addProfile(p);

	p.bandwidthBitsPerSecond = 1000000;
	p.width = 640; p.height = 360;
	mgr.addProfile(p);

	p.bandwidthBitsPerSecond = 2000000;
	p.width = 1280; p.height = 720;
	mgr.addProfile(p);

	// Buffer 21/30 = 70%.  Profile percentages: 25%, 50%, 100%.
	// Descending: 100% !< 70%, 50% < 70% AND 1M < 2M → 1M.
	long result = mgr.FragmentfailureRampdown(21, 2);
	EXPECT_EQ(result, 1000000);
}

TEST_F(HybridAbrTests, FragmentfailureRampdown_FallbackLowestIsNotIframe)
{
	eAAMPAbrConfig.abrMaxBuffer = 30;

	HybridABRManager mgr;
	mgr.ReadPlayerConfig(&eAAMPAbrConfig);

	ABRManager::ProfileInfo p{};

	// Profile 0: 200 kbps iframe — lowest BW overall but should be excluded
	p.isIframeTrack = true;
	p.bandwidthBitsPerSecond = 200000;
	p.width = 160; p.height = 120;
	mgr.addProfile(p);

	// Profile 1: 500 kbps video — lowest video profile
	p.isIframeTrack = false;
	p.bandwidthBitsPerSecond = 500000;
	p.width = 320; p.height = 240;
	mgr.addProfile(p);

	// Profile 2: 2 Mbps video (current)
	p.isIframeTrack = false;
	p.bandwidthBitsPerSecond = 2000000;
	p.width = 1280; p.height = 720;
	mgr.addProfile(p);

	// Buffer 1/30 ≈ 3.3%.  Sorted video BWs: 500k, 2M.
	// Profile percentages: 25%, 100%.  Both ≥ 3.3%, but only 500k < currentbw.
	// 25% < 3.3% is false, so no profile matches → fallback to lowest.
	// Without the fix, fallback would return 200k (the iframe track).
	long result = mgr.FragmentfailureRampdown(1, 2);
	EXPECT_EQ(result, 500000);
}
