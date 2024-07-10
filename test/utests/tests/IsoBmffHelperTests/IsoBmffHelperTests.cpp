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

AampLogManager *mLogObj{nullptr};

class IsoBmffHelperTests : public ::testing::Test
{
	protected:
		void SetUp() override
		{
			mLogObj = new AampLogManager();
			g_mockIsoBmffBuffer = new MockIsoBmffBuffer();
		}

		void TearDown() override
		{
			delete mLogObj;
			mLogObj=nullptr;
			delete g_mockIsoBmffBuffer;
			g_mockIsoBmffBuffer = nullptr;
		}
};


/**
 * @brief Test the PTS restamp function (positive case)
 *        Verify that the expected IsoBmffBuffer methods are called when
 *        IsoBmffRestampPts() function is called.
 */
TEST_F(IsoBmffHelperTests, restampPtsTest)
{
	AampGrowableBuffer buffer("IsoBmffHelperTests-restampPts");
	uint8_t bufferContent[] = ("IsoBmff buffer content");
	// Set the pointer and length in the AampGrowableBuffer fake
	buffer.AppendBytes(bufferContent, sizeof(bufferContent));
	int64_t ptsOffset{123};

	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(bufferContent, sizeof(bufferContent)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, restampPts(ptsOffset));
	EXPECT_TRUE(IsoBmffRestampPts(buffer, ptsOffset));
}

/**
 * @brief Test the PTS restamp function (negative case)
 *        Verify that IsoBmffBuffer::restampPts() is not called if
 *        IsoBmffBuffer::parseBuffer() fails, when IsoBmffRestampPts() function
 *        is called.
 */
TEST_F(IsoBmffHelperTests, restampPtsNegativeTest)
{
	AampGrowableBuffer buffer("IsoBmffHelperTests-restampPts");
	uint8_t bufferContent[] = ("IsoBmff buffer content");
	// Set the pointer and length in the AampGrowableBuffer fake
	buffer.AppendBytes(bufferContent, sizeof(bufferContent));
	int64_t ptsOffset{123};

	EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(bufferContent, sizeof(bufferContent)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(false, -1)).WillOnce(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, restampPts(_)).Times(0);
	EXPECT_FALSE(IsoBmffRestampPts(buffer, ptsOffset));
}
