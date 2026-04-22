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

TEST_F(HybridAbrTests, UpdateABRBitrateDataBasedOnCacheOutlierOdd)
{
	std::vector<long> tmpData = {100, 200, 300, 400, 500};
	long result = abrManager.UpdateABRBitrateDataBasedOnCacheOutlier(tmpData);
	// Median of {100, 200, 300, 400, 500} is 300.
	// Outlier diff is 1000000, so no outliers will be removed.
	// Average is (100+200+300+400+500)/5 = 300.
	ASSERT_EQ(result, 300);
}
