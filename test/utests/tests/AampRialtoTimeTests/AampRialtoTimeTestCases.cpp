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
 * @file AampRialtoTimeTestCases.cpp
 * @brief L1 tests for the AampRialtoTime.h conversions.
 *
 * Oracle: seconds convert to the nearest whole unit.  Values such as 1.001 s
 * are not exact in binary, so truncating instead of rounding loses one unit.
 */

#include <gtest/gtest.h>

#include "AampRialtoTime.h"

/**
 * @test AampRialtoTime_Constants_AreConsistent
 * @brief A second is 1000 ms of 1000000 ns each.
 */
TEST(AampRialtoTimeTest, AampRialtoTime_Constants_AreConsistent)
{
	EXPECT_EQ(kNsPerSecond, 1'000'000'000LL);
	EXPECT_EQ(kNsPerMs * 1000, kNsPerSecond);
}

/**
 * @test AampRialtoTime_SecondsToNs_RoundsToNearest
 * @brief 1.001 s is 1001000000 ns, not the truncated 1000999999.
 */
TEST(AampRialtoTimeTest, AampRialtoTime_SecondsToNs_RoundsToNearest)
{
	EXPECT_EQ(SecondsToNs(0.0), 0);
	EXPECT_EQ(SecondsToNs(1.001), 1'001'000'000LL);
	EXPECT_EQ(SecondsToNs(0.033), 33'000'000LL);
	EXPECT_EQ(SecondsToNs(-1.001), -1'001'000'000LL);
}

/**
 * @test AampRialtoTime_SecondsToMs_RoundsToNearest
 * @brief 0.251 s is 251 ms (truncation gives 250); halves round away from
 *        zero.
 */
TEST(AampRialtoTimeTest, AampRialtoTime_SecondsToMs_RoundsToNearest)
{
	EXPECT_EQ(SecondsToMs(0.251), 251);
	EXPECT_EQ(SecondsToMs(1.0625), 1063);
	EXPECT_EQ(SecondsToMs(0.0004), 0);
	EXPECT_EQ(SecondsToMs(-0.251), -251);
	EXPECT_EQ(SecondsToMs(-1.5), -1500);
}

/**
 * @test AampRialtoTime_SecondsToMs_LargePositions
 * @brief Wall-clock positions (about 1.8e9 s) convert exactly to ms.
 */
TEST(AampRialtoTimeTest, AampRialtoTime_SecondsToMs_LargePositions)
{
	EXPECT_EQ(SecondsToMs(1'792'610'521.125), 1'792'610'521'125LL);
}
