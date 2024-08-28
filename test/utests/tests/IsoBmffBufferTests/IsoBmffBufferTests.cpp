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

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string.h>

//Google test dependencies
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// unit under test
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "isobmff/isobmffbuffer.h"
#include "AampConfig.h"
#include "isobmff/isobmffbox.h"
#include "AampLogManager.h"

#include "testFiles/helperTestData.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

AampConfig *gpGlobalConfig{nullptr};

class IsoBmffBufferTests : public ::testing::Test
{
	protected:
		IsoBmffBuffer *mIsoBmffBuffer = nullptr;
		void SetUp() override
		{
			mIsoBmffBuffer = new IsoBmffBuffer();
		}

		void TearDown() override
		{
			delete mIsoBmffBuffer;
			mIsoBmffBuffer = nullptr;
			delete gpGlobalConfig;
			gpGlobalConfig = nullptr;
		}

		// Verify the PTS value in a buffer
		void VerifyPts(std::vector<uint64_t> expectedPts, uint8_t *buffer, size_t size)
		{
			bool bParse;
			uint64_t fPts = 0;
			uint64_t pts = 0;
			IsoBmffBuffer *isoBmffBuffer = new IsoBmffBuffer();
			isoBmffBuffer->setBuffer(buffer, size);
			bParse = isoBmffBuffer->parseBuffer();
			EXPECT_TRUE(bParse);
			isoBmffBuffer->getFirstPTS(fPts);
			EXPECT_EQ(fPts, expectedPts.front());
			size_t index = 0;
			for (auto expectedPtsIter = expectedPts.begin(); expectedPtsIter != expectedPts.end(); ++expectedPtsIter)
			{
				Box *pBox = isoBmffBuffer->getBox(Box::MOOF, index);
				EXPECT_NE(pBox, nullptr);
				isoBmffBuffer->getPts(pBox, pts);
				EXPECT_EQ(pts, *expectedPtsIter);
				// Increment the index to continue searching from the next box
				index++;
			}
			// There should be no more moof boxes left
			Box *pBox = isoBmffBuffer->getBox(Box::MOOF, index);
			EXPECT_EQ(pBox, nullptr);
			delete isoBmffBuffer;
		}
};

std::pair<std::vector<uint8_t>, std::streampos> readFile(const char* file_path) {
	std::ifstream file(file_path, std::ios::binary);

	if (!file.is_open()) {
		std::cout<< "IsoBmffProcessorTests :: The file cant be opened" <<std::endl;
		return {{}, 0};
	}

	file.seekg(0, std::ios::end);
	std::streampos file_size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> data(file_size);
	file.read(reinterpret_cast<char*>(data.data()), file_size);
	file.close();
	return {data, file_size};
}

TEST_F(IsoBmffBufferTests, initSegmentTests)
{
	const int TS = 12800, TID = 1;
	uint32_t timeScale = 0, track_id = 0;
	std::string file_path = std::string(TESTS_DIR) + "/" + "initSegmentTests/vInit.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vInitSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vInitSeg.data(), size);
	mIsoBmffBuffer->parseBuffer();
	bool isInit=mIsoBmffBuffer->isInitSegment();
	EXPECT_TRUE(isInit);
	mIsoBmffBuffer->getTimeScale(timeScale);
	EXPECT_EQ(timeScale,TS);
	mIsoBmffBuffer->getTrack_id(track_id);
	EXPECT_EQ(track_id, TID);
}

TEST_F(IsoBmffBufferTests, mp4SegmentTests)
{
	unsigned int baseMediaDecodeTime = 1254400, mdatLen = 55312, mdatCnt = 1, sampleDuration = 512 * 50;
	uint64_t fPts = 0, mCount = 0, durationFromFragment = 0, pts = 0;
	uint8_t *mdat;
	size_t mSize, index = 0;
	bool isInit, bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/vFragment.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	isInit=mIsoBmffBuffer->isInitSegment(); // Not an init segment
	EXPECT_FALSE(isInit);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	bParse = mIsoBmffBuffer->getMdatBoxSize(mSize);
	EXPECT_EQ(mSize, mdatLen);
	mdat = (uint8_t *)malloc(mSize);
	bParse = mIsoBmffBuffer->parseMdatBox(mdat, mSize);
	EXPECT_TRUE(bParse);
	size_t count = static_cast<size_t> (mCount);
	mIsoBmffBuffer->getMdatBoxCount(count);
	EXPECT_EQ(count,mdatCnt);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getSampleDuration(pBox,durationFromFragment);
	EXPECT_EQ(sampleDuration,durationFromFragment);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);
}

TEST_F(IsoBmffBufferTests, parseBufferTwiceTest)
{
	const int TS = 12800, TID = 1;
	uint32_t timeScale = 0, track_id = 0;
	std::string file_path = std::string(TESTS_DIR) + "/" + "initSegmentTests/vInit.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vInitSeg;
	std::streampos size;
	if (!result.first.empty())
	{
		vInitSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vInitSeg.data(), size);
	mIsoBmffBuffer->parseBuffer();
	std::vector<Box*> *boxes = mIsoBmffBuffer->getParsedBoxes();
	std::size_t numBoxesAfter1parse = boxes->size();
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	mIsoBmffBuffer->parseBuffer();
	std::size_t numBoxesAfter2parse = boxes->size();
	EXPECT_EQ(numBoxesAfter1parse, numBoxesAfter2parse);
}

/**
 * @brief Test PTS restamp with offset 0
 *        Test the PTS restamp method with an offset value of 0.
 */
TEST_F(IsoBmffBufferTests, ptsRestampOffset0Test)
{
	uint64_t baseMediaDecodeTime = 1254400;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/vFragment.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	int64_t ptsOffset{0};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime + ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {baseMediaDecodeTime + ptsOffset};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with offset 1
 *        Test the PTS restamp method with an offset value of 1.
 */
TEST_F(IsoBmffBufferTests, ptsRestampOffset1Test)
{
	uint64_t baseMediaDecodeTime = 1254400;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/vFragment.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	int64_t ptsOffset{1};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime + ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {baseMediaDecodeTime + ptsOffset};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with offset bigger than 1
 *        Test the PTS restamp method with an offset bigger than 1.
 */
TEST_F(IsoBmffBufferTests, ptsRestampOffsetManyTest)
{
	uint64_t baseMediaDecodeTime = 1254400;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/vFragment.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	int64_t ptsOffset{123456789};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime + ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {baseMediaDecodeTime + ptsOffset};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with negative offset
 *        Test the PTS restamp method with a negative offset.
 */
TEST_F(IsoBmffBufferTests, ptsRestampNegativeOffsetTest)
{
	uint64_t baseMediaDecodeTime = 1254400;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/vFragment.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	int64_t ptsOffset{-73};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime + ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {baseMediaDecodeTime + ptsOffset};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with multiple moof
 *        Test the PTS restamp method when there are more than one moof box
 *        (with more than one tfdt, with different baseMediaDecodeTime) in the
 *        ISO BMFF buffer.
 */
TEST_F(IsoBmffBufferTests, ptsRestampSeveralMoofTest)
{
	std::vector<uint64_t> baseMediaDecodeTime = {563200, 565760};
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/multiMoofDefaultDuration.m4s";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime.front());
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime.front());

	int64_t ptsOffset{123456789};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime.front() + ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that all the PTS values have been updated in the buffer
	std::vector<uint64_t> expectedPts;
	for (uint64_t mdt : baseMediaDecodeTime)
	{
		expectedPts.push_back(mdt + ptsOffset);
	}
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with tfdt version 0
 *        Test the PTS restamp method with a version 0 tfdt box.
 */
TEST_F(IsoBmffBufferTests, ptsRestampTfdtVersion0Test)
{
	uint64_t baseMediaDecodeTime = 49152;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/m1_video_3.m4s";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	int64_t ptsOffset{123456789};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime + ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS, fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {baseMediaDecodeTime + ptsOffset};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with restamped PTS 0
 *        Test the PTS restamp method when the restamped PTS is 0:
 *            baseMediaDecodeTime + offset = 0
 */
TEST_F(IsoBmffBufferTests, ptsRestampPts0Test)
{
	uint64_t baseMediaDecodeTime = 1254400;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/vFragment.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	// PTS restamped = baseMediaDecodeTime + offset = 0
	int64_t ptsOffset{-1 * static_cast<int64_t>(baseMediaDecodeTime)};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime + ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {baseMediaDecodeTime + ptsOffset};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with PTS underflow
 *        Test the PTS restamp method when there is underflow in the restamped
 *        PTS calculation (the offset is signed, but PTS is an unsigned value):
 *            baseMediaDecodeTime + offset < 0
 *        The restamped PTS is expected to 'wrap around' (have a very large value)
 */
TEST_F(IsoBmffBufferTests, ptsRestampPtsUnderflowTest)
{
	uint64_t baseMediaDecodeTime = 1254400;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/vFragment.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	// baseMediaDecodeTime + offset = -1, so PTS restamped = UINT64_MAX
	// in tfdt version 1, baseMediaDecodeTime is a 64 bit unsigned value
	int64_t ptsOffset{-1 * static_cast<int64_t>(baseMediaDecodeTime) - 1};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, UINT64_MAX);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {UINT64_MAX};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with tfdt version 0 and PTS underflow
 *        Test the PTS restamp method with a version 0 tfdt box, when there is
 *        underflow in the restamped PTS calculation (the offset is signed, but
 *        PTS is an unsigned 32-bit value):
 *            baseMediaDecodeTime + offset < 0
 *        The restamped PTS is expected to 'wrap around' (have a very large value)
 */
TEST_F(IsoBmffBufferTests, ptsRestampTfdtVersion0PtsUnderflowTest)
{
	uint64_t baseMediaDecodeTime = 49152;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/m1_video_3.m4s";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	// baseMediaDecodeTime + offset = -1, so PTS restamped = UINT32_MAX
	// in tfdt version 0, baseMediaDecodeTime is a 32 bit unsigned value
	int64_t ptsOffset{-1 * static_cast<int64_t>(baseMediaDecodeTime) - 1};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, UINT32_MAX);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {UINT32_MAX};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with restamped PTS max value
 *        Test the PTS restamp method when the restamped PTS is the maximum
 *        possible value:
 *            baseMediaDecodeTime + offset = UINT64_MAX
 */
TEST_F(IsoBmffBufferTests, ptsRestampPtsMaxTest)
{
	uint64_t baseMediaDecodeTime = 1254400;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/vFragment.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	// PTS restamped = baseMediaDecodeTime + offset = 0
	int64_t ptsOffset{static_cast<int64_t>(UINT64_MAX) - static_cast<int64_t>(baseMediaDecodeTime)};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, UINT64_MAX);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {UINT64_MAX};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with tfdt version 0 and restamped PTS max value
 *        Test the PTS restamp method with tfdt version 0, when the restamped
 *        PTS is the maximum possible value:
 *            baseMediaDecodeTime + offset = UINT32_MAX
 */
TEST_F(IsoBmffBufferTests, ptsRestampTfdtVersion0PtsMaxTest)
{
	uint64_t baseMediaDecodeTime = 49152;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/m1_video_3.m4s";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	// PTS restamped = baseMediaDecodeTime + offset = 0
	// in tfdt version 0, baseMediaDecodeTime is a 32 bit unsigned value
	int64_t ptsOffset{static_cast<int64_t>(UINT32_MAX) - static_cast<int64_t>(baseMediaDecodeTime)};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, UINT32_MAX);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {UINT32_MAX};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with restamped PTS overflow
 *        Test the PTS restamp method when there is overflow in the restamped
 *        PTS calculation:
 *            baseMediaDecodeTime + offset > UINT64_MAX
 */
TEST_F(IsoBmffBufferTests, ptsRestampPtsOverflowTest)
{
	uint64_t baseMediaDecodeTime = 1254400;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/vFragment.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	// PTS restamped = baseMediaDecodeTime + offset = 0
	int64_t ptsOffset{1 + static_cast<int64_t>(UINT64_MAX) - static_cast<int64_t>(baseMediaDecodeTime)};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, 0);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {0};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with tfdt version 0 and restamped PTS max value
 *        Test the PTS restamp method when the restamped PTS is the maximum
 *        possible value:
 *            baseMediaDecodeTime + offset > UINT32_MAX
 */
TEST_F(IsoBmffBufferTests, ptsRestampTfdtVersion0PtsOverflowTest)
{
	uint64_t baseMediaDecodeTime = 49152;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/m1_video_3.m4s";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	// PTS restamped = baseMediaDecodeTime + offset = 0
	// in tfdt version 0, baseMediaDecodeTime is a 32 bit unsigned value
	int64_t ptsOffset{1 + static_cast<int64_t>(UINT32_MAX) - static_cast<int64_t>(baseMediaDecodeTime)};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, 0);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {0};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with restamped PTS overflow
 *        Test the PTS restamp method when there is overflow in the restamped
 *        PTS calculation, with a different value from previous overflow test:
 *            baseMediaDecodeTime + offset > UINT64_MAX
 */
TEST_F(IsoBmffBufferTests, ptsRestampPtsOverflow2Test)
{
	uint64_t baseMediaDecodeTime = 1254400;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/vFragment.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	// PTS restamped = baseMediaDecodeTime + offset = 0
	uint64_t expectedValue {1973};
	int64_t ptsOffset{static_cast<int64_t>(expectedValue) + 1 + static_cast<int64_t>(UINT64_MAX) - static_cast<int64_t>(baseMediaDecodeTime)};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, expectedValue);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {expectedValue};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test PTS restamp with tfdt version 0 and restamped PTS max value
 *        Test the PTS restamp method when the restamped PTS is the maximum
 *        possible value, with a different value from previous overflow test:
 *            baseMediaDecodeTime + offset > UINT32_MAX
 */
TEST_F(IsoBmffBufferTests, ptsRestampTfdtVersion0PtsOverflow2Test)
{
	uint64_t baseMediaDecodeTime = 49152;
	uint64_t fPts = 0, pts = 0;
	size_t index = 0;
	bool bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/m1_video_3.m4s";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, baseMediaDecodeTime);
	Box *pBox =  mIsoBmffBuffer->getBox(Box::MOOF, index);
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTime);

	// PTS restamped = baseMediaDecodeTime + offset = 0
	// in tfdt version 0, baseMediaDecodeTime is a 32 bit unsigned value
	uint64_t expectedValue{1973};
	int64_t ptsOffset{static_cast<int64_t>(expectedValue) + 1 + static_cast<int64_t>(UINT32_MAX) - static_cast<int64_t>(baseMediaDecodeTime)};
	mIsoBmffBuffer->restampPts(ptsOffset);
	EXPECT_EQ(mIsoBmffBuffer->beforePTS,pts);
	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	mIsoBmffBuffer->getFirstPTS(fPts);
	EXPECT_EQ(fPts, expectedValue);
	EXPECT_EQ(mIsoBmffBuffer->afterPTS,fPts);
	// Verify that the PTS has been updated in the buffer
	std::vector<uint64_t> expectedPts {expectedValue};
	VerifyPts(expectedPts, vSeg.data(), size);
}

/**
 * @brief Test Get Segment Duration with no boxes (negative test)
 *        Test the getSegmentDuration method when called without having called parseBuffer.
 *        The returned duration should be 0, as no boxes have been created
 */
TEST_F(IsoBmffBufferTests, noBoxesGetSegmentDurationTest)
{
	EXPECT_EQ(mIsoBmffBuffer->getSegmentDuration(),0);
}


/**
 * @brief Test setting of PTS and duration, as done for trick mode restamping
 */
TEST_F(IsoBmffBufferTests, setPtsAndDurationTest)
{
	Box* pBox{nullptr};
	uint64_t defaultSampleDurationOld{512};
	// The tfhd default sample duration should be set to 1 second for trickmodes
	uint64_t defaultSampleDurationNew{1};
	// In the "mp4SegmentTests/vFragment.mp4" test data file, the trun lists 50 samples
	uint64_t sampleCount{50};
	uint64_t sampleDurationTotal{0};
	uint64_t baseMediaDecodeTimeOld{1254400};
	uint64_t baseMediaDecodeTimeNew{123};
	uint64_t pts{0};
	size_t index{0};
	bool bParse{false};
	std::string file_path{std::string{TESTS_DIR} + "/mp4SegmentTests/vFragment.mp4"};
	auto result{readFile(file_path.c_str())};
	std::vector<uint8_t> vSeg;
	std::streampos size;
	if (!result.first.empty()) {
		vSeg = result.first;
		size = result.second;
	}
	mIsoBmffBuffer->setBuffer(vSeg.data(), size);
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	pBox = mIsoBmffBuffer->getBox(Box::MOOF, index);

	// Verify old PTS
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTimeOld);

	// Verify old sample duration
	mIsoBmffBuffer->getSampleDuration(pBox, sampleDurationTotal);
	EXPECT_EQ(sampleDurationTotal, defaultSampleDurationOld * sampleCount);

	mIsoBmffBuffer->setPtsAndDuration(baseMediaDecodeTimeNew, defaultSampleDurationNew);

	// Verify new PTS in box following trick mode restamping
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTimeNew);

	// Verify new sample duration in box following trick mode restamping
	mIsoBmffBuffer->getSampleDuration(pBox, sampleDurationTotal);
	EXPECT_EQ(sampleDurationTotal, defaultSampleDurationNew * sampleCount);

	// Parse the ISO BMFF buffer again and check that the PTS has been updated
	// The boxes in the buffer must be destroyed before parseBuffer can be called a second time
	mIsoBmffBuffer->destroyBoxes();
	bParse = mIsoBmffBuffer->parseBuffer();
	EXPECT_TRUE(bParse);
	index = 0;
	pBox = mIsoBmffBuffer->getBox(Box::MOOF, index);

	// Verify new PTS in buffer following trick mode restamping
	mIsoBmffBuffer->getPts(pBox, pts);
	EXPECT_EQ(pts, baseMediaDecodeTimeNew);

	// Verify new sample duration in buffer following trick mode restamping
	mIsoBmffBuffer->getSampleDuration(pBox, sampleDurationTotal);
	EXPECT_EQ(sampleDurationTotal, defaultSampleDurationNew * sampleCount);
}

/**
 * @brief Test getChildBox
 */
TEST_F(IsoBmffBufferTests, getChildBoxTest)
{
	size_t index{0}, index1{0};
	Box *parent{nullptr}, *child{nullptr};

	mIsoBmffBuffer->setBuffer((uint8_t *)childBoxTestData, sizeof(childBoxTestData));
	mIsoBmffBuffer->parseBuffer();

	parent = mIsoBmffBuffer->getBox(Box::TRAF, index);
	EXPECT_NE(parent, nullptr);

	EXPECT_EQ(mIsoBmffBuffer->getChildBox(parent, Box::MDAT, index), nullptr);
	EXPECT_EQ(index, 0);

	child = mIsoBmffBuffer->getChildBox(parent, Box::TFHD, index);

	// Validate child type
	EXPECT_TRUE(IS_TYPE(child->getType(), Box::TFHD));
	++index;

	// Test for a non-present second box of the same type
	EXPECT_EQ(mIsoBmffBuffer->getChildBox(parent, Box::TFHD, index), nullptr);

	// Test for 2 boxes of the same type
	index = 0;
	child = mIsoBmffBuffer->getChildBox(parent, Box::TRUN, index);
	EXPECT_NE(child, nullptr);
	EXPECT_EQ(index, 1);
	index1 = index;
	++index;
	child = mIsoBmffBuffer->getChildBox(parent, Box::TRUN, index);
	EXPECT_NE(child, nullptr);
	EXPECT_TRUE(index > index1);
}

TEST_F(IsoBmffBufferTests, setTrickmodeTimescale)
{
	std::vector<uint8_t>localBuffer(setTimescaleTestData, std::end(setTimescaleTestData));

	mIsoBmffBuffer->setBuffer(localBuffer.data(), localBuffer.size());
	mIsoBmffBuffer->parseBuffer();

	// Find mdhd box
	size_t index{0};
	auto root{mIsoBmffBuffer->getBox(Box::MOOV, index)};
	EXPECT_NE(root, nullptr);
	index = 0;
	auto trak{mIsoBmffBuffer->getChildBox(root, Box::TRAK, index)};
	EXPECT_NE(trak, nullptr);
	index = 0;
	auto mdia{mIsoBmffBuffer->getChildBox(trak, Box::MDIA, index)};
	EXPECT_NE(mdia, nullptr);
	index = 0;
	auto mvhd{dynamic_cast<MvhdBox *>(mIsoBmffBuffer->getChildBox(root, Box::MVHD, index))};
	EXPECT_NE(mvhd, nullptr);
	index = 0;
	auto mdhd{dynamic_cast<MdhdBox *>(mIsoBmffBuffer->getChildBox(mdia, Box::MDHD, index))};
	EXPECT_NE(mdhd, nullptr);

	EXPECT_EQ(mdhd->getTimeScale(), 1000);
	EXPECT_EQ(mvhd->getTimeScale(), 1000);

	mIsoBmffBuffer->setTrickmodeTimescale(90000);
	EXPECT_EQ(mdhd->getTimeScale(), 90000);
	EXPECT_EQ(mvhd->getTimeScale(), 90000);
}


TEST_F(IsoBmffBufferTests, setTrickmodeTimescaleNegative)
{
	mIsoBmffBuffer->setBuffer((uint8_t *)childBoxTestData, sizeof(childBoxTestData));
	mIsoBmffBuffer->parseBuffer();

	// No MVHD or MDHD box
	EXPECT_EQ(mIsoBmffBuffer->setTrickmodeTimescale(100), false);
}

TEST_F(IsoBmffBufferTests, truncateMultipleTruns)
{
	std::vector<uint8_t>localBuffer(truncateTestStream, std::end(truncateTestStream));
	mIsoBmffBuffer->setBuffer(localBuffer.data(), localBuffer.size());
	mIsoBmffBuffer->parseBuffer();

	mIsoBmffBuffer->truncate();

	// Verify that the senc, saiz, tfhd & both trun boxes have only 1 sample
	// The real IsoBmffBox code is used for this test, so the box calls are not mocked.
	size_t index{0};

	auto moof{mIsoBmffBuffer->getBox(Box::MOOF, index)};
	EXPECT_NE(moof, nullptr);
	index = 0;
	auto traf{mIsoBmffBuffer->getChildBox(moof, Box::TRAF, index)};
	EXPECT_NE(traf, nullptr);
	index = 0;
	auto saiz{dynamic_cast<SaizBox *>(mIsoBmffBuffer->getChildBox(traf, Box::SAIZ, index))};
	EXPECT_NE(saiz, nullptr);
	index = 0;
	auto tfhd{dynamic_cast<TfhdBox *>(mIsoBmffBuffer->getChildBox(traf, Box::TFHD, index))};
	EXPECT_NE(tfhd, nullptr);
	index = 0;
	auto trun1{dynamic_cast<TrunBox *>(mIsoBmffBuffer->getChildBox(traf, Box::TRUN, index))};
	EXPECT_NE(trun1, nullptr);
	auto trun2{dynamic_cast<TrunBox *>(mIsoBmffBuffer->getChildBox(traf, Box::SKIP, ++index))};
	EXPECT_NE(trun2, nullptr);
}
