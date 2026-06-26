/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2023 RDK Management
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
#include <gmock/gmock.h>
#include <string_view>

#include "MockIsoBmffBuffer.h"
#include "isobmff/IsoBmffLog.h"
#include "isobmff/isobmffhelper.h"

using ::testing::_;
using ::testing::Return;
using namespace std::literals;

static std::vector<std::pair<IsoBmff::LogLevel, std::string>> gCapturedLogs;

static IsoBmff::Logger MakeTestLogger()
{
	return {
		[](IsoBmff::LogLevel level, std::string&& msg) {
			gCapturedLogs.emplace_back(level, std::move(msg));
		},
		IsoBmff::LogLevel::TRACE
	};
}

class IsoBmffHelperTests : public ::testing::Test
{
	protected:
		IsoBmff::Logger mLogger;
		std::shared_ptr<IsoBmffHelper> helper;

		void SetUp() override
		{
			gCapturedLogs.clear();
			mLogger = MakeTestLogger();
			g_mockIsoBmffBuffer = std::make_shared<MockIsoBmffBuffer>();
			helper = std::make_shared<IsoBmffHelper>(mLogger);
		}

		void TearDown() override
		{
			g_mockIsoBmffBuffer.reset();
			helper.reset();
		}
};


/**
 * @brief Test the PTS restamp function (positive case)
 *        Verify that the expected IsoBmffBuffer methods are called when
 *        RestampPts() function is called.
 */
TEST_F(IsoBmffHelperTests, restampPtsTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	int64_t ptsOffset{123};
    std::string url("Dummy");
	const char* trackName = "video";
	uint32_t timeScale = 48000;
	// Check that setBuffer receives the actual buffer pointer
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, restampPts(ptsOffset));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSegmentDuration());
	EXPECT_TRUE(helper->RestampPts(buffer, ptsOffset,url, trackName, timeScale));
}

/**
 * @brief Test the PTS restamp function (negative case)
 *        Verify that IsoBmffBuffer::restampPts() is not called if
 *        IsoBmffBuffer::parseBuffer() fails, when RestampPts() function
 *        is called.
 */
TEST_F(IsoBmffHelperTests, restampPtsNegativeTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	int64_t ptsOffset{123};
    std::string url("Dummy");
	const char* trackName = "video";
	uint32_t timeScale = 48000;
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, restampPts(_)).Times(0);
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSegmentDuration()).Times(0);
	EXPECT_FALSE(helper->RestampPts(buffer, ptsOffset, url, trackName, timeScale));
}

/**
 * @brief Test the set PTS and duration function (positive case)
 *        Verify that the expected IsoBmffBuffer methods are called when
 *        SetPtsAndDuration() function is called.
 */
TEST_F(IsoBmffHelperTests, setPtsAndDurationTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	uint64_t pts{123};
	uint64_t duration{1};
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setPtsAndDuration(pts, duration));
	EXPECT_TRUE(helper->SetPtsAndDuration(buffer, pts, duration));
}

/**
 * @brief Test the set PTS and duration function (positive case)
 *        Verify that IsoBmffBuffer::setPtsAndDuration() is not called if
 *        IsoBmffBuffer::parseBuffer() fails, when SetPtsAndDuration() function
 *        is called.
 */
TEST_F(IsoBmffHelperTests, setPtsAndDurationNegativeTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	uint64_t pts{123};
	uint64_t duration{1};
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setPtsAndDuration(_, _)).Times(0);
	EXPECT_FALSE(helper->SetPtsAndDuration(buffer, pts, duration));
}

/**
 * @brief Test the set timescale function
 *        Verify that IsoBmffBuffer::SetTimescale() is called
 */
TEST_F(IsoBmffHelperTests, setTimescaleTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setTrickmodeTimescale(1000)).WillOnce(Return(true));
	EXPECT_TRUE(helper->SetTimescale(buffer, 1000));
}

/**
 * @brief Test the set timescale function (negative case)
 *        Verify that SetTimescale returns false if
 *        IsoBmffBuffer::setTrickmodeTimescale() fails
 */

TEST_F(IsoBmffHelperTests, setTimescaleTestNegativeTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setTrickmodeTimescale(1000)).WillOnce(Return(false));
	EXPECT_FALSE(helper->SetTimescale(buffer, 1000));
}

/**
 * @brief Test the ClearMediaHeaderDuration function
 *        Verify that IsoBmffBuffer::clearMediaHeaderDuration() is called
 */
TEST_F(IsoBmffHelperTests, clearMediaHeaderDurationTest)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setMediaHeaderDuration(0)).WillOnce(Return(true));
	EXPECT_TRUE(helper->ClearMediaHeaderDuration(buffer));
}

/**
 * @brief Test the ClearMediaHeaderDuration function (negative case)
 *        Verify that ClearMediaHeaderDuration returns false if
 *        IsoBmffBuffer::isInitSegment() fails
 */
TEST_F(IsoBmffHelperTests, clearMediaHeaderDurationNegativeTest_1)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(false));
	EXPECT_FALSE(helper->ClearMediaHeaderDuration(buffer));
}

/**
 * @brief Test the ClearMediaHeaderDuration function (negative case)
 *        Verify that ClearMediaHeaderDuration returns false if
 *        IsoBmffBuffer::clearMediaHeaderDuration() fails
 */
TEST_F(IsoBmffHelperTests, clearMediaHeaderDurationNegativeTest_2)
{
	static constexpr auto BUFFER = "IsoBmff buffer content"sv;
	std::vector<uint8_t> buffer(BUFFER.begin(), BUFFER.end());
	auto expectedPtr = buffer.data();
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(expectedPtr, buffer.size()));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setMediaHeaderDuration(0)).WillOnce(Return(false));
	EXPECT_FALSE(helper->ClearMediaHeaderDuration(buffer));
}