/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include <cstdlib>
#include <iostream>
#include <string>
#include <string.h>

//include the google test dependencies
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "dash/utils/Utils.h"

class DashUtilsTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}
};


TEST(DashUtilsTests, isoDateTimeToEpochSeconds)
{
	double seconds;
	seconds = isoDateTimeToEpochSeconds(("1977-05-25T18:00:00.000Z"),0);
	EXPECT_DOUBLE_EQ(seconds, 233431200.0);
	seconds = isoDateTimeToEpochSeconds(("2023-05-25T18:00:00.000Z"),0);
	EXPECT_DOUBLE_EQ(seconds, 1685037600.0);
	seconds = isoDateTimeToEpochSeconds(("2023-05-25T19:00:00.000Z"),0);
	EXPECT_DOUBLE_EQ(seconds, 1685041200.0);
	seconds = isoDateTimeToEpochSeconds(("2023-02-25T20:00:00.000Z"),0);
	EXPECT_DOUBLE_EQ(seconds, 1677355200.0);
}

TEST(DashUtilsTests, isoDateTimeToEpochSecondsBlankParameter)
{
	double seconds;
	seconds = isoDateTimeToEpochSeconds((""),0);
	EXPECT_DOUBLE_EQ(seconds, 0);
}
