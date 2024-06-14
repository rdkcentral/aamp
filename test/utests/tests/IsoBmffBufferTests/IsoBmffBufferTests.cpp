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

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

class IsoBmffBufferTests : public ::testing::Test
{
	protected:
		IsoBmffBuffer *mIsoBmffBuffer = nullptr;
		void SetUp() override
		{
			mLogObj = new AampLogManager();
			mIsoBmffBuffer = new IsoBmffBuffer();
		}

		void TearDown() override
		{
			delete mIsoBmffBuffer;
			mIsoBmffBuffer = nullptr;
			delete mLogObj;
			mLogObj=nullptr;
			delete gpGlobalConfig;
			gpGlobalConfig = nullptr;
		}
	public:

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
