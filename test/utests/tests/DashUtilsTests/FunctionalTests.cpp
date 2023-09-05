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
