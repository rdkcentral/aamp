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
#include <chrono>

//Google test dependencies
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// unit under test
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockIsoBmffBuffer.h"
#include "AampConfig.h"
#include "priv_aamp.h"
#include "AampLogManager.h"
#include "isobmff/isobmffprocessor.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::TypedEq;

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

class IsoBmffProcessorTests : public ::testing::Test
{
	protected:
		IsoBmffProcessor *mIsoBmffProcessor{};
		IsoBmffProcessor *mAudIsoBmffProcessor{};
		IsoBmffProcessor *mSubIsoBmffProcessor{};
		PrivateInstanceAAMP *mPrivateInstanceAAMP{};
		MediaProcessor::process_fcn_t mProcessorFn{};
		std::thread asyncTask;
		void SetUp() override
		{
			mLogObj = new AampLogManager();
			mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
			g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
			g_mockAampConfig = new MockAampConfig();
			g_mockIsoBmffBuffer = new MockIsoBmffBuffer();
			EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp)).WillRepeatedly(Return(true));
			EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseNewFetcherLoop)).WillRepeatedly(Return(true));
			EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_QtDemuxOverrideEnabled)).WillRepeatedly(Return(true));
			EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetMediaFormatTypeEnum()).WillRepeatedly(Return(eMEDIAFORMAT_DASH));
			EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_FragmentDownloadFailThreshold)).WillRepeatedly(Return(10));

			id3_callback_t id3Handler = nullptr;

			mAudIsoBmffProcessor = new IsoBmffProcessor(mPrivateInstanceAAMP, mLogObj, id3Handler, eBMFFPROCESSOR_TYPE_AUDIO);
			mSubIsoBmffProcessor = new IsoBmffProcessor(mPrivateInstanceAAMP, mLogObj, id3Handler, eBMFFPROCESSOR_TYPE_SUBTITILE);
			mIsoBmffProcessor = new IsoBmffProcessor(mPrivateInstanceAAMP, mLogObj, id3Handler, eBMFFPROCESSOR_TYPE_VIDEO, mAudIsoBmffProcessor, mSubIsoBmffProcessor);
		}

		void TearDown() override
		{
			delete mIsoBmffProcessor;
			mIsoBmffProcessor = nullptr;
			delete mAudIsoBmffProcessor;
			mAudIsoBmffProcessor = nullptr;
			delete mSubIsoBmffProcessor;
			mSubIsoBmffProcessor = nullptr;
			delete mLogObj;
			mLogObj=nullptr;
			delete gpGlobalConfig;
			gpGlobalConfig = nullptr;
			delete mPrivateInstanceAAMP;
			mPrivateInstanceAAMP = nullptr;
			delete g_mockPrivateInstanceAAMP;
			g_mockPrivateInstanceAAMP = nullptr;
			delete g_mockIsoBmffBuffer;
			g_mockIsoBmffBuffer = nullptr;
			delete g_mockAampConfig;
			g_mockAampConfig = nullptr;
		}
};


//Race condition between setTuneTimePTS and reset calls
TEST_F(IsoBmffProcessorTests, abortTests1)
{
	AampGrowableBuffer buffer("IsoBmffProcessorTests-abortTests1");
	bool ptsError = false;

	// Spawn thread to perform wait.
	std::thread t([this]{
		while(1){
			if (this->mIsoBmffProcessor->getBasePTS() == 10000) {
				this->mIsoBmffProcessor->abort();
				break;
			}
		}
	});
	mIsoBmffProcessor->setRate(AAMP_NORMAL_PLAY_RATE, PlayMode_normal);
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(1000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, true, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(10000), Return(true)));

	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, true, true, mProcessorFn, ptsError);

	t.join();
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendErrorEvent(_, _, _, _, _, _, _)).Times(0);
	buffer.Free();
}

//Race condition between setTuneTimePTS and reset calls
TEST_F(IsoBmffProcessorTests, abortTests2)
{
	AampGrowableBuffer buffer("IsoBmffProcessorTests-abortTests2");
	bool ptsError = false;

	mIsoBmffProcessor->setRate(AAMP_NORMAL_PLAY_RATE, PlayMode_normal);
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce([this](uint32_t timescale) { 
		timescale = 1000;
		// Call abort() in an async task to avoid mutex deadlock
		this->asyncTask = std::thread([this]{ this->mIsoBmffProcessor->abort(); });
		return true; 
	});
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, true, true, mProcessorFn, ptsError);

	// EXPECT_EQ(this->mIsoBmffProcessor->getInitSegmentCacheSize(), 0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendErrorEvent(_, _, _, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _, _, _, _, _, _)).Times(0);
	this->asyncTask.join();
	buffer.Free();
}


//Race condition between setTuneTimePTS and reset calls
TEST_F(IsoBmffProcessorTests, abortTests3)
{
	AampGrowableBuffer buffer("IsoBmffProcessorTests-abortTests3");
	bool ptsError = false;

	mIsoBmffProcessor->setRate(AAMP_NORMAL_PLAY_RATE, PlayMode_normal);
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(1000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, true, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly([this](uint64_t pts) {
		pts = 10000;
		// Call abort() (once) in an async task to avoid mutex deadlock
		if (!this->asyncTask.joinable())
			this->asyncTask = std::thread([this]{ this->mIsoBmffProcessor->abort(); });
		return true; 
	});

	(void)mIsoBmffProcessor->sendSegment(&buffer, 0, 0, true, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendErrorEvent(_, _, _, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _, _, _, _, _, _)).Times(0);
	this->asyncTask.join();
	buffer.Free();
}


//Processing of Init followed by 2 continuous video fragments
TEST_F(IsoBmffProcessorTests, ptsTests)
{
	AampGrowableBuffer buffer("IsoBmffProcessorTests-ptsTests");
	Box *box = (Box*)(0xdeadbeef);

	double position = 0, duration = 0;
	bool discontinuous = false, ptsError = false;
	uint64_t basePts = 0, vCurrTS = 24000, rslt = 0, restampedPTS = 0, vDuration = 48048;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(24000), Return(true)));

	mIsoBmffProcessor->sendSegment(&buffer, position, duration, discontinuous, true, mProcessorFn, ptsError);
		
	rslt = ceil((position) * vCurrTS);
	duration = (double) vDuration / (double)vCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(0), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	position += duration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(48048), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, discontinuous, false, mProcessorFn, ptsError);

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
	AampGrowableBuffer buffer("IsoBmffProcessorTests-timeScaleTests_1");
	Box *box = (Box*)(0xdeadbeef);

	double vPosition = 0, aPosition = 0;
	bool discontinuous = false, ptsError = false;
	uint64_t basePts = 0, vCurrTS = 24000, aCurrTS = 48000, rslt = 0, restampedPTS = 0, vDuration = 48048, aDuration = 95232;
	double vSegDuration = 0, aSegDuration = 0;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(aCurrTS), Return(true)));
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, 0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(vCurrTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, discontinuous, true, mProcessorFn, ptsError);

	//eBMFFPROCESSOR_INIT_TIMESCALE
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);

	vSegDuration = (double)vDuration / (double)vCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(0), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);
	vPosition += vSegDuration;

	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(0), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(aDuration));
	aSegDuration = (double)aDuration / (double)aCurrTS;
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, aSegDuration, discontinuous,false, mProcessorFn, ptsError);

	EXPECT_EQ(mAudIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((aPosition) * aCurrTS);
	restampedPTS = mAudIsoBmffProcessor->getSumPTS() - aDuration;
	EXPECT_EQ(restampedPTS, rslt);
	aPosition += aSegDuration;

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vCurrTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, discontinuous,true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_CONTINUE_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, discontinuous,false, mProcessorFn, ptsError);

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
	AampGrowableBuffer buffer("IsoBmffProcessorTests-timeScaleTests_2");
	Box *box = (Box*)(0xdeadbeef);

	uint64_t rslt, restampedPTS = 0, oldTS = 25000, basePts = 1733808848333, vDuration = 112000, vDurationAfterABR = 224000;
	double position = basePts / (double) oldTS, vSegDuration = vDuration / (double) oldTS;
	bool discontinuous = false, ptsError = false;

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(oldTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous,true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * oldTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);

	//eBMFFPROCESSOR_CONTINUE_WITH_ABR_CHANGED_TIMESCALE
	position += vSegDuration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(50000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, position, 0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_CONTINUE_WITH_ABR_CHANGED_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(3467617920666), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDurationAfterABR));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

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
	AampGrowableBuffer buffer("IsoBmffProcessorTests-timeScaleTests_3");
	Box *box = (Box*)(0xdeadbeef);

	bool discontinuous = false, ptsError = false;
	uint64_t basePts = 0, vDuration = 60060, vDurationAfterABR = 48048, currTS = 30000, rslt = 0, restampedPTS = 0;
	double position = 0, vSegDuration = (double)vDuration / (double)currTS;

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(currTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);

	//eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE
	discontinuous = true;
	position += vSegDuration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(24000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(0), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDurationAfterABR));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

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
	AampGrowableBuffer buffer("IsoBmffProcessorTests-timeScaleTests_4");
	Box *box = (Box*)(0xdeadbeef);

	bool discontinuous, ptsError = false;
	uint64_t basePts = 0, vDuration = 60060, vDurationAfterABR = 48048, currTS = 30000, rslt = 0, restampedPTS = 0;
	double position = 0, vSegDuration = (double)vDuration / (double)currTS;

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(currTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);

	position += vSegDuration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);

	discontinuous = true;
	position += vSegDuration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(24000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, position, 0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE);

	//eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE
	discontinuous = false;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(24000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, position, 0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(0), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDurationAfterABR));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

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
	AampGrowableBuffer buffer("IsoBmffProcessorTests-ptsTests_2");
	Box *box = (Box*)(0xdeadbeef);

	bool discontinuous, ptsError = false;
	uint64_t basePts = 0, vDuration = 60060, /*vDurationAfterABR = 48048,*/ currTS = 30000, rslt = 0, restampedPTS = 0;
	double position = 0, vSegDuration = (double)vDuration / (double)currTS;

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(currTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	position += vSegDuration;
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration-1, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
}


//Before disc, video ends at x, audio ends at x-1, after disc, both should be in sync and resume from x
TEST_F(IsoBmffProcessorTests, ptsTests_3)
{
	AampGrowableBuffer buffer("IsoBmffProcessorTests-ptsTests_3");
	Box *box = (Box*)(0xdeadbeef);

	double vPosition = 0, aPosition = 0;
	bool discontinuous = false, ptsError = false;
	uint64_t basePts = 0, vCurrTS = 24000, aCurrTS = 48000, rslt = 0, restampedPTS = 0, vDuration = 48048,  aDuration = 95232, aNewDuration = 96256;
	double vSegDuration = 0, aSegDuration = 0;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(aCurrTS), Return(true)));
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, 0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vCurrTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, discontinuous, true, mProcessorFn, ptsError);

	vSegDuration = (double)vDuration / (double)vCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	aSegDuration = (double)aDuration / (double)aCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(aDuration));
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, aSegDuration, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mAudIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((aPosition) * aCurrTS);
	restampedPTS = mAudIsoBmffProcessor->getSumPTS() - aDuration;
	EXPECT_EQ(restampedPTS, rslt);

	aPosition += aSegDuration;
	vPosition += vSegDuration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	discontinuous = true;
	vPosition += vSegDuration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(24000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, discontinuous, true, mProcessorFn, ptsError);

	discontinuous = true;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(48000), Return(true)));
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, 0, discontinuous, true, mProcessorFn, ptsError);

	discontinuous = false;
	vSegDuration = (double)vDuration / (double)vCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(240240), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, discontinuous, false, mProcessorFn, ptsError);

	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), rslt);

	aSegDuration = (double)aDuration / (double)aCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(481280), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(aNewDuration));
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, aSegDuration, discontinuous, false, mProcessorFn, ptsError);

	buffer.Free();
	rslt = ceil((aPosition) * aCurrTS);
	restampedPTS = mAudIsoBmffProcessor->getSumPTS() - aNewDuration;
	EXPECT_NE(restampedPTS, rslt); //Sync the audio PTS with the video pts.
	EXPECT_EQ(mAudIsoBmffProcessor->getBasePTS(), restampedPTS);
}


//Dup video fragments
TEST_F(IsoBmffProcessorTests, ptsTests_4)
{
	AampGrowableBuffer buffer("IsoBmffProcessorTests-ptsTests_4");
	Box *box = (Box*)(0xdeadbeef);

	double position = 0, duration = 0;
	bool discontinuous = false, ptsError = false;
	uint64_t basePts = 0, vCurrTS = 24000, rslt = 0, restampedPTS = 0, vDuration = 48048;
	duration = (double) vDuration / (double)vCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vCurrTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true, mProcessorFn, ptsError);

	rslt = ceil((position) * vCurrTS);
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, discontinuous, false, mProcessorFn, ptsError);

	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	position += duration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, discontinuous, false, mProcessorFn, ptsError); 

	rslt = ceil((position) * vCurrTS);
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt); //Both fragments are of same duration and timescale

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(24000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, discontinuous, false, mProcessorFn, ptsError);

	buffer.Free();
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt); // Restamped PTS will not update on dup fragment
}
