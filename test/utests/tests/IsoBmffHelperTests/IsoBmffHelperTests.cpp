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

#include "MockIsoBmffBuffer.h"
#include "AampLogManager.h"
#include "isobmff/isobmffhelper.h"

using ::testing::_;
using ::testing::Return;



class IsoBmffHelperTests : public ::testing::Test
{
	protected:
		std::shared_ptr<IsoBmffHelper> helper;

		void SetUp() override
		{
			g_mockIsoBmffBuffer = new MockIsoBmffBuffer();
			helper = std::make_shared<IsoBmffHelper>();
		}

		void TearDown() override
		{
			delete g_mockIsoBmffBuffer;
			g_mockIsoBmffBuffer = nullptr;
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
	AampGrowableBuffer buffer("IsoBmffHelperTests-restampPts");
	uint8_t bufferContent[] = ("IsoBmff buffer content");
	// Set the pointer and length in the AampGrowableBuffer fake
	buffer.AppendBytes(bufferContent, sizeof(bufferContent));
	int64_t ptsOffset{123};
    std::string url("Dummy");
	const char* trackName = "video";
	uint32_t timeScale = 48000;
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(bufferContent, sizeof(bufferContent)));
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
	AampGrowableBuffer buffer("IsoBmffHelperTests-restampPts");
	uint8_t bufferContent[] = ("IsoBmff buffer content");
	// Set the pointer and length in the AampGrowableBuffer fake
	buffer.AppendBytes(bufferContent, sizeof(bufferContent));
	int64_t ptsOffset{123};
    std::string url("Dummy");
	const char* trackName = "video";
	uint32_t timeScale = 48000;
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(bufferContent, sizeof(bufferContent)));
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
	AampGrowableBuffer buffer{"IsoBmffHelperTests-setPtsAndDuration"};
	uint8_t bufferContent[]{"IsoBmff buffer content"};
	// Set the pointer and length in the AampGrowableBuffer fake
	buffer.AppendBytes(bufferContent, sizeof(bufferContent));
	uint64_t pts{123};
	uint64_t duration{1};
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(bufferContent, sizeof(bufferContent)));
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
	AampGrowableBuffer buffer{"IsoBmffHelperTests-setPtsAndDuration"};
	uint8_t bufferContent[]{"IsoBmff buffer content"};
	// Set the pointer and length in the AampGrowableBuffer fake
	buffer.AppendBytes(bufferContent, sizeof(bufferContent));
	uint64_t pts{123};
	uint64_t duration{1};
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(bufferContent, sizeof(bufferContent)));
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
	AampGrowableBuffer buffer{"IsoBmffHelperTests-setTimescale"};
	uint8_t bufferContent[]{"IsoBmff buffer content"};
	// Set the pointer and length in the AampGrowableBuffer fake
	buffer.AppendBytes(bufferContent, sizeof(bufferContent));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(bufferContent, sizeof(bufferContent)));
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
	AampGrowableBuffer buffer{"IsoBmffHelperTests-setTimescale"};
	uint8_t bufferContent[]{"IsoBmff buffer content"};
	// Set the pointer and length in the AampGrowableBuffer fake
	buffer.AppendBytes(bufferContent, sizeof(bufferContent));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(bufferContent, sizeof(bufferContent)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, setTrickmodeTimescale(1000)).WillOnce(Return(false));
	EXPECT_FALSE(helper->SetTimescale(buffer, 1000));
}
