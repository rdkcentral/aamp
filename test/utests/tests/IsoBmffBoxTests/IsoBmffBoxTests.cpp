/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 Synamedia Ltd.
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
#include <cstring>

#include "isobmff/isobmffbox.h"
#include "AampConfig.h"

#include "testData/IsoBMFFTestData.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

const size_t SIZEOF_TAG{4};

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

class IsoBmffBoxTests : public ::testing::Test
{
	protected:
		void SetUp() override
		{
			mLogObj = new AampLogManager();
			buffer = (uint8_t *)malloc(bufferSize);
			memset(buffer, 0xff, bufferSize);
		}

		void TearDown() override
		{
			delete mLogObj;
			mLogObj=nullptr;
			free(buffer);
		}

		static const uint32_t bufferSize{0x400};
		uint8_t *buffer;

	public:

};

const uint32_t IsoBmffBoxTests::bufferSize;

TEST_F(IsoBmffBoxTests, skipTests)
{
	// Create a skip box
	auto size{512};
	auto name = new uint8_t[4]{'s', 'k', 'i', 'p'};

	auto skip = new SkipBox(size, buffer);

	// Check the size is correct
	auto ptr{buffer};
	auto value = READ_U32(ptr);
	EXPECT_EQ(value, size);

	// Check the tag is correct
	EXPECT_TRUE((IS_TYPE(ptr, name)));
	EXPECT_TRUE((IS_TYPE(ptr, Box::SKIP)));
}

TEST_F(IsoBmffBoxTests, mdatTests)
{
	// Test that the mdat box updates its internal length
	// and in the buffer
	auto name = new uint8_t[4]{'m', 'd', 'a', 't'};
	auto size{512};
	uint32_t newLength{0x200};

	// Copy the mdat test data in
	memcpy(buffer, mdatData, sizeof(mdatData));
	auto ptr{buffer};
	auto mdatSize = READ_U32(ptr);
	EXPECT_TRUE((IS_TYPE(ptr, Box::MDAT)));
	ptr += SIZEOF_TAG;

	// NB hard coded in test data
	EXPECT_EQ(mdatSize, bufferSize);

	auto mdat = MdatBox::constructMdatBox(mdatSize, ptr);
	mdat->truncate(newLength);

	ptr = buffer;
	auto truncatedLength = READ_U32(ptr);
	EXPECT_EQ(truncatedLength, newLength);
	EXPECT_EQ(mdat->getSize(), newLength);

	// MdatBox::truncate() does not insert a skip box
}

TEST_F(IsoBmffBoxTests, sencTests)
{
	memcpy(buffer, sencSingleSample, sizeof(sencSingleSample));
	auto ptr{buffer};
	auto sencSize = READ_U32(ptr);
	EXPECT_TRUE((IS_TYPE(ptr, Box::SENC)));
	ptr += SIZEOF_TAG;
	auto senc = SencBox::constructSencBox(sencSize, ptr);

	// First sample size is set external to the senc box and the box has no internal knowledge of it.
	senc->truncate(0);

	// First data set has only one sample, so truncate does nothing.

	// Need a data set with > 1 samples

	// Compare against a pre-generated buffer
}

TEST_F(IsoBmffBoxTests, saizTests)
{
	memcpy(buffer, saizSingleSample, sizeof(saizSingleSample));
	auto ptr{buffer};
	auto seizSize = READ_U32(ptr);
	EXPECT_TRUE((IS_TYPE(ptr, Box::SAIZ)));
	ptr += SIZEOF_TAG;
	auto saiz = SaizBox::constructSaizBox(seizSize, ptr);

	saiz->truncate();

	// First data set has only one sample, so truncate does nothing.

	// Need a data set with > 1 samples

	// Compare against a pre-generated buffer
}

TEST_F(IsoBmffBoxTests, trunTests)
{
	memcpy(buffer, trunSingleEntryTrackDefaults, sizeof(trunSingleEntryTrackDefaults));
	auto ptr{buffer};
	auto trunSize = READ_U32(ptr);
	EXPECT_TRUE((IS_TYPE(ptr, Box::TRUN)));
	ptr += SIZEOF_TAG;
	auto trun = TrunBox::constructTrunBox(trunSize, ptr);

	trun->truncate();

	// First data set has only one sample, so truncate does nothing.

	// Need a data set with > 1 samples

	// Compare against a pre-generated buffer

}