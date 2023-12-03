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
#include "priv_aamp.h"
#include "AampLogManager.h"
#include "isobmff/isobmffprocessor.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

class IsoBmffProcessorTests : public ::testing::Test
{
	protected:
		IsoBmffBuffer *mIsoBmffBuffer = nullptr;
		IsoBmffProcessor *mIsoBmffProcessor = nullptr;
		IsoBmffProcessor *mAudIsoBmffProcessor = nullptr;
		PrivateInstanceAAMP *mPrivateInstanceAAMP{};
		void SetUp() override
		{
			mLogObj = new AampLogManager();
			mIsoBmffBuffer = new IsoBmffBuffer();
			mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
			g_mockAampConfig = new MockAampConfig();
			EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp)).WillRepeatedly(Return(true));
			mAudIsoBmffProcessor = new IsoBmffProcessor(mPrivateInstanceAAMP, mLogObj, eBMFFPROCESSOR_TYPE_AUDIO);
			mIsoBmffProcessor = new IsoBmffProcessor(mPrivateInstanceAAMP, mLogObj, eBMFFPROCESSOR_TYPE_VIDEO, static_cast<IsoBmffProcessor*> (mAudIsoBmffProcessor));
		}

		void TearDown() override
		{
			delete mIsoBmffBuffer;
			mIsoBmffBuffer = nullptr;
			delete mIsoBmffProcessor;
			mIsoBmffProcessor = nullptr;
			delete mAudIsoBmffProcessor;
			mAudIsoBmffProcessor = nullptr;
			delete mLogObj;
			mLogObj=nullptr;
			delete gpGlobalConfig;
			gpGlobalConfig = nullptr;
			delete mPrivateInstanceAAMP;
			mPrivateInstanceAAMP = nullptr;
			delete g_mockAampConfig;
			g_mockAampConfig = nullptr;
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

TEST_F(IsoBmffProcessorTests, initSegmentTests)
{
	const int TS = 24000, TID = 1;
	uint32_t timeScale = 0, track_id = 0;
	std::string file_path = std::string(TESTS_DIR) + "/" + "initSegmentTests/v1_init.mp4";
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

TEST_F(IsoBmffProcessorTests, mp4SegmentTests)
{
	unsigned int segmentlen = 142845, baseMediaDecodeTime = 0, mdatLen = 142141, mdatCnt = 1, sampleDuration = 1001 * 48;
	uint64_t fPts = 0, mCount = 0, durationFromFragment = 0, pts = 0;
	uint8_t *mdat;
	size_t mSize, index = -1;
	bool isInit, bParse;
	std::string file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/v1_1.mp4";
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

//Processing of Init followed by 2 continuous video fragments
TEST_F(IsoBmffProcessorTests, ptsTests)
{
	std::string file_path = std::string(TESTS_DIR) + "/" + "initSegmentTests/v1_init.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vInitSeg, vSeg;
	size_t size;
	AampGrowableBuffer buffer("IsoBmffProcessorTests-ptsTests");

	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}
	double position = 0, duration = 0;
	bool discontinuous = false, ptsError = false;
	uint64_t basePts = 0, vCurrTS = 24000, rslt = 0, restampedPTS = 0, vDuration = 48048;
	duration = (double) vDuration / (double)vCurrTS;

	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/v1_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}
		
	rslt = ceil((position) * vCurrTS);
	mIsoBmffProcessor->sendSegment(&buffer, duration, 0, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	file_path = std::string(TESTS_DIR) + "/" + "ptsTests/v1_2.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}
	position += duration;
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, discontinuous, false, 
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	rslt = ceil((position) * vCurrTS);
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt); //Both fragments are of same duration and timescale
}

//Init A/V followed by Fragment A/V
//eBMFFPROCESSOR_INIT_TIMESCALE, eBMFFPROCESSOR_CONTINUE_TIMESCALE
TEST_F(IsoBmffProcessorTests, timeScaleTests_1)
{
	std::string file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_1/a1_init.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> aInitSeg, vInitSeg, aSeg, vSeg;
	size_t size;
	AampGrowableBuffer buffer("IsoBmffProcessorTests-timeScaleTests_1");

	if (!result.first.empty()) {
		aInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(aInitSeg.data(),size);
	}

	double vPosition = 0, aPosition = 0;
	bool discontinuous = false, ptsError = false;
	uint64_t basePts = 0, vCurrTS = 24000, aCurrTS = 48000, rslt = 0, restampedPTS = 0, vDuration = 48048, aDuration = 95232;
	double vSegDuration = 0, aSegDuration = 0;

	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	file_path = std::string(TESTS_DIR) + "/" + "initSegmentTests/v1_init.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}

	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	//eBMFFPROCESSOR_INIT_TIMESCALE
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);
	file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/v1_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}

	vSegDuration = (double)vDuration / (double)vCurrTS;
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);
	vPosition += vSegDuration;
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_1/a1_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		aSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(aSeg.data(),size);
	}
	aSegDuration = (double)aDuration / (double)aCurrTS;
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, aSegDuration, discontinuous,false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mAudIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((aPosition) * aCurrTS);
	restampedPTS = mAudIsoBmffProcessor->getSumPTS() - aDuration;
	EXPECT_EQ(restampedPTS, rslt);
	aPosition += aSegDuration;

	//eBMFFPROCESSOR_CONTINUE_TIMESCALE
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_1/v2_init.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}

	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, discontinuous,true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_CONTINUE_TIMESCALE);
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_1/v2_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, discontinuous,false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
}


//eBMFFPROCESSOR_CONTINUE_WITH_ABR_CHANGED_TIMESCALE - Case when no discontinuity but with different Timescale, BasePTS will not change after abr scale.
TEST_F(IsoBmffProcessorTests, timeScaleTests_2)
{
	std::string file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_2/ts1_init.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vInitSeg, aSeg, vSeg;
	size_t size;
	AampGrowableBuffer buffer("IsoBmffProcessorTests-timeScaleTests_2");

	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}
	uint64_t rslt, restampedPTS = 0, oldTS = 25000, basePts = 1733808848333, vDuration = 112000, vDurationAfterABR = 224000;
	double position = basePts / (double) oldTS, vSegDuration = vDuration / (double) oldTS;
	bool discontinuous = false, ptsError = false;

	
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous,true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_2/ts1_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * oldTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);
	//eBMFFPROCESSOR_CONTINUE_WITH_ABR_CHANGED_TIMESCALE
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_2/ts2_init.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}
	position += vSegDuration;

			mIsoBmffProcessor->sendSegment(&buffer, position, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);
	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_CONTINUE_WITH_ABR_CHANGED_TIMESCALE);

	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_2/ts2_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}

	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);

	uint64_t newTS = mIsoBmffProcessor->getCurrentTimeScale();
	rslt = ceil((position) * newTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDurationAfterABR;
	EXPECT_EQ(restampedPTS, rslt);
}

//eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE - BasePTS will change after discontinuity
TEST_F(IsoBmffProcessorTests, timeScaleTests_3)
{
	std::string file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_3/ad_init.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vInitSeg, aSeg, vSeg;
	size_t size;
	AampGrowableBuffer buffer("IsoBmffProcessorTests-timeScaleTests_3");

	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}
	bool discontinuous = false, ptsError = false;
	uint64_t basePts = 0, vDuration = 60060, vDurationAfterABR = 48048, currTS = 30000, rslt = 0, restampedPTS = 0;
	double position = 0, vSegDuration = (double)vDuration / (double)currTS;

	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_3/ad.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);
	//eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_3/ts_disc_init.mp4";
	discontinuous = true;
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}
	position += vSegDuration;
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE);

	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_3/ts_disc.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}

	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);
	uint64_t newTS = mIsoBmffProcessor->getCurrentTimeScale();
	rslt = ceil((position) * newTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDurationAfterABR;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), rslt);
	EXPECT_NE(mIsoBmffProcessor->getBasePTS(), basePts);
}


//eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE - Discontinuity (ad->content) and again rampup/down due to curl errors, hence 2 inits will be pushed back to back, BasePTS will be updated
TEST_F(IsoBmffProcessorTests, timeScaleTests_4)
{
	std::string file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_3/ad_init.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vInitSeg, aSeg, vSeg;
	size_t size;
	AampGrowableBuffer buffer("IsoBmffProcessorTests-timeScaleTests_4");

	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}
	bool discontinuous, ptsError = false;
	uint64_t basePts = 0, vDuration = 60060, vDurationAfterABR = 48048, currTS = 30000, rslt = 0, restampedPTS = 0;
	double position = 0, vSegDuration = (double)vDuration / (double)currTS;

	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_3/ad.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}

	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_4/ad_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}

	position += vSegDuration;
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);

	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_3/ts_disc_init.mp4";
	discontinuous = true;
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}
	position += vSegDuration;
	mIsoBmffProcessor->sendSegment(&buffer, position, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE);



	//eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_4/ts_disc_init_abr.mp4";
	discontinuous = false;
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}

	mIsoBmffProcessor->sendSegment(&buffer, position, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE);

	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_4/ts_disc_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}

	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);
	uint64_t newTS = mIsoBmffProcessor->getCurrentTimeScale();
	rslt = ceil((position) * newTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDurationAfterABR;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), rslt);
	EXPECT_NE(mIsoBmffProcessor->getBasePTS(), basePts);
}

//Difference in manifest duration vs buffer duration. Player should process the buffer one.
TEST_F(IsoBmffProcessorTests, ptsTests_2)
{
	std::string file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_3/ad_init.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vInitSeg, aSeg, vSeg;
	size_t size;
	AampGrowableBuffer buffer("IsoBmffProcessorTests-ptsTests_2");

	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}
	bool discontinuous, ptsError = false;
	uint64_t basePts = 0, vDuration = 60060, vDurationAfterABR = 48048, currTS = 30000, rslt = 0, restampedPTS = 0;
	double position = 0, vSegDuration = (double)vDuration / (double)currTS;

	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_3/ad.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}

	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_4/ad_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}

	position += vSegDuration;
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration-1, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
}


//Before disc, video ends at x, audio ends at x-1, after disc, both should be in sync and resume from x
TEST_F(IsoBmffProcessorTests, ptsTests_3)
{
	std::string file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_1/a1_init.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> aInitSeg, vInitSeg, aSeg, vSeg;
	size_t size;
	AampGrowableBuffer buffer("IsoBmffProcessorTests-ptsTests_3");

	if (!result.first.empty()) {
		aInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(aInitSeg.data(),size);
	}


	double vPosition = 0, aPosition = 0;
	bool discontinuous = false, ptsError = false;
	uint64_t basePts = 0, vCurrTS = 24000, aCurrTS = 48000, rslt = 0, restampedPTS = 0, vDuration = 48048,  aDuration = 95232, aNewDuration = 96256;
	double vSegDuration = 0, aSegDuration = 0;

	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	file_path = std::string(TESTS_DIR) + "/" + "initSegmentTests/v1_init.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}

	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/v1_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}

	vSegDuration = (double)vDuration / (double)vCurrTS;
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_1/a1_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		aSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(aSeg.data(),size);
	}
	aSegDuration = (double)aDuration / (double)aCurrTS;
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, aSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mAudIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((aPosition) * aCurrTS);
	restampedPTS = mAudIsoBmffProcessor->getSumPTS() - aDuration;
	EXPECT_EQ(restampedPTS, rslt);
	aPosition += aSegDuration;

	file_path = std::string(TESTS_DIR) + "/" + "ptsTests/v1_2.mp4";

	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}
	vPosition += vSegDuration;
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	file_path = std::string(TESTS_DIR) + "/" + "ptsTests_3/new_vid_init.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}
	discontinuous = true;
	vPosition += vSegDuration;

	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	file_path = std::string(TESTS_DIR) + "/" + "ptsTests_3/new_aud_init.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		aInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(aInitSeg.data(),size);
	}
	discontinuous = true;
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	discontinuous = false;
	file_path = std::string(TESTS_DIR) + "/" + "ptsTests_3/new_vid.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}
	vSegDuration = (double)vDuration / (double)vCurrTS;
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), rslt);

	aSegDuration = (double)aDuration / (double)aCurrTS;
	file_path = std::string(TESTS_DIR) + "/" + "ptsTests_3/new_aud.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		aSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(aSeg.data(),size);
	}
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, aSegDuration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	rslt = ceil((aPosition) * aCurrTS);
	restampedPTS = mAudIsoBmffProcessor->getSumPTS() - aNewDuration;
	EXPECT_NE(restampedPTS, rslt); //Sync the audio PTS with the video pts.
	EXPECT_EQ(mAudIsoBmffProcessor->getBasePTS(), restampedPTS);
}


//Dup video fragments
TEST_F(IsoBmffProcessorTests, ptsTests_4)
{
	std::string file_path = std::string(TESTS_DIR) + "/" + "initSegmentTests/v1_init.mp4";
	auto result = readFile(file_path.c_str());
	std::vector<uint8_t> vInitSeg, vSeg;
	size_t size;
	AampGrowableBuffer buffer("IsoBmffProcessorTests-ptsTests_4");

	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}
	double position = 0, duration = 0;
	bool discontinuous = false, ptsError = false;
	uint64_t basePts = 0, vCurrTS = 24000, rslt = 0, restampedPTS = 0, vDuration = 48048;
	duration = (double) vDuration / (double)vCurrTS;

	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	file_path = std::string(TESTS_DIR) + "/" + "mp4SegmentTests/v1_1.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}
	rslt = ceil((position) * vCurrTS);
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	file_path = std::string(TESTS_DIR) + "/" + "ptsTests/v1_2.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}
	position += duration;
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, discontinuous, false,   
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	rslt = ceil((position) * vCurrTS);
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt); //Both fragments are of same duration and timescale
	file_path = std::string(TESTS_DIR) + "/" + "timeScaleTests_1/v2_init.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vInitSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vInitSeg.data(),size);
	}

	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	file_path = std::string(TESTS_DIR) + "/" + "ptsTests_4/v2_2.mp4";
	result = readFile(file_path.c_str());
	if (!result.first.empty()) {
		vSeg = result.first;
		size = static_cast<size_t>(result.second);
		buffer.AppendBytes(vSeg.data(),size);
	}
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, discontinuous, false,
			[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
			{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
			},
			ptsError
			);

	buffer.Free();
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt); // Restamped PTS will not update on dup fragment
}
