/*
 * If not stated otherwise in this file or this component's LICENSE file the
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
#include <gmock/gmock.h>
#include <cstring>

#include "isobmff/isobmffbox.h"
#include "AampConfig.h"

#include "testData/IsoBMFFTestData.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::WithParamInterface;

using ConstBuffer = std::pair<const uint8_t*, size_t>;

const size_t SIZEOF_TAG{4};

AampConfig *gpGlobalConfig{nullptr};

class IsoBmffBoxTests : public ::testing::Test
{
	protected:
		void SetUp() override
		{
			buffer = (uint8_t *)malloc(bufferSize);
			memset(buffer, 0xff, bufferSize);
		}

		void TearDown() override
		{
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

TEST_F(IsoBmffBoxTests, tfhdDefaultSampleDurationTests)
{
	memcpy(buffer, tfhdDefaultSampleDurationPresent, sizeof(tfhdDefaultSampleDurationPresent));
	auto ptr{buffer};
	auto tfhdSize = READ_U32(ptr);
	EXPECT_TRUE((IS_TYPE(ptr, Box::TFHD)));
	ptr += SIZEOF_TAG;
	auto tfhd{TfhdBox::constructTfhdBox(tfhdSize, ptr)};

	uint64_t durationOld{512}; // Old default sample duration embedded in the test data
	uint64_t durationNew{1};

	EXPECT_TRUE(tfhd->defaultSampleDurationPresent());
	EXPECT_EQ(tfhd->getDefaultSampleDuration(), durationOld);
	tfhd->setDefaultSampleDuration(durationNew);
	EXPECT_EQ(tfhd->getDefaultSampleDuration(), durationNew);
	delete tfhd;

	// Check that the underlying buffer was also updated, by creating a new box for the same buffer
	ptr = buffer;
	tfhdSize = READ_U32(ptr);
	ptr += SIZEOF_TAG;
	tfhd = TfhdBox::constructTfhdBox(tfhdSize, ptr);
	EXPECT_EQ(tfhd->getDefaultSampleDuration(), durationNew);
	delete tfhd;

	memcpy(buffer, tfhdDefaultSampleDurationAbsent, sizeof(tfhdDefaultSampleDurationAbsent));
	ptr = buffer;
	tfhdSize = READ_U32(ptr);
	EXPECT_TRUE((IS_TYPE(ptr, Box::TFHD)));
	ptr += SIZEOF_TAG;
	tfhd = TfhdBox::constructTfhdBox(tfhdSize, ptr);

	EXPECT_FALSE(tfhd->defaultSampleDurationPresent());
	EXPECT_EQ(tfhd->getDefaultSampleDuration(), 0);
	delete tfhd;
}

TEST_F(IsoBmffBoxTests, rewriteAsSkipTest)
{
	memcpy(buffer, exampleMdatBox, sizeof(exampleMdatBox));
	auto size{sizeof(exampleMdatBox)};
	auto testBox = Box::constructBox(buffer, (uint32_t)size, true, -1);
	EXPECT_STREQ(testBox->getType(), Box::MDAT);
	EXPECT_EQ(testBox->getSize(), size);
	testBox->rewriteAsSkipBox();
	EXPECT_STREQ(testBox->getType(), Box::SKIP);
	EXPECT_EQ(testBox->getSize(), size);
	EXPECT_EQ(buffer[4], 's');
	EXPECT_EQ(buffer[5], 'k');
	EXPECT_EQ(buffer[6], 'i');
	EXPECT_EQ(buffer[7], 'p');

	delete testBox;
}

class IsoBmffTfdtBoxVersionTests : public IsoBmffBoxTests,
								   public testing::WithParamInterface<ConstBuffer>
{
};

TEST_P(IsoBmffTfdtBoxVersionTests, tfdtVersionTests)
{
	ConstBuffer testData{GetParam()};
	memcpy(buffer, testData.first, testData.second);
	auto ptr{buffer};
	auto tfdtSize = READ_U32(ptr);
	EXPECT_TRUE((IS_TYPE(ptr, Box::TFDT)));
	ptr += SIZEOF_TAG;
	auto tfdt{TfdtBox::constructTfdtBox(tfdtSize, ptr)};

	uint64_t mdtOld{1254400}; // Old base media decode time embedded in the testData
	uint64_t mdtNew{123};

	EXPECT_EQ(tfdt->getBaseMDT(), mdtOld);
	tfdt->setBaseMDT(mdtNew);
	EXPECT_EQ(tfdt->getBaseMDT(), mdtNew);
	delete tfdt;

	// Check that the underlying buffer was also updated, by creating a new box for the same buffer
	ptr = buffer;
	tfdtSize = READ_U32(ptr);
	ptr += SIZEOF_TAG;
	tfdt = TfdtBox::constructTfdtBox(tfdtSize, ptr);
	EXPECT_EQ(tfdt->getBaseMDT(), mdtNew);
	delete tfdt;
}

INSTANTIATE_TEST_SUITE_P(IsoBmffBoxTests, IsoBmffTfdtBoxVersionTests,
						 ::testing::Values(ConstBuffer(tfdtDataV0, sizeof(tfdtDataV0)),
										   ConstBuffer(tfdtDataV1, sizeof(tfdtDataV1))));
