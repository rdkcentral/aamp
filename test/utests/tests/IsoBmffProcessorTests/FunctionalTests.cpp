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
using ::testing::AnyNumber;

AampConfig *gpGlobalConfig{nullptr};

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
			mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
			g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
			g_mockAampConfig = new MockAampConfig();
			g_mockIsoBmffBuffer = new MockIsoBmffBuffer();
			EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp)).WillRepeatedly(Return(true));
			EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetMediaFormatTypeEnum()).WillRepeatedly(Return(eMEDIAFORMAT_HLS_MP4));
			EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_FragmentDownloadFailThreshold)).WillRepeatedly(Return(10));
			EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_,_)).WillRepeatedly(Return(true));
			EXPECT_CALL(*g_mockIsoBmffBuffer, setBuffer(_,_)).Times(AnyNumber());

			id3_callback_t id3Handler = nullptr;

			mAudIsoBmffProcessor = new IsoBmffProcessor(mPrivateInstanceAAMP, id3Handler, eBMFFPROCESSOR_TYPE_AUDIO);
			mSubIsoBmffProcessor = new IsoBmffProcessor(mPrivateInstanceAAMP, id3Handler, eBMFFPROCESSOR_TYPE_SUBTITLE);
			mIsoBmffProcessor = new IsoBmffProcessor(mPrivateInstanceAAMP, id3Handler, eBMFFPROCESSOR_TYPE_VIDEO, mAudIsoBmffProcessor, mSubIsoBmffProcessor);
		}

		void TearDown() override
		{
			delete mIsoBmffProcessor;
			mIsoBmffProcessor = nullptr;
			delete mAudIsoBmffProcessor;
			mAudIsoBmffProcessor = nullptr;
			delete mSubIsoBmffProcessor;
			mSubIsoBmffProcessor = nullptr;
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
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, true, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(10000), Return(true)));

	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, false, false, mProcessorFn, ptsError);

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

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendErrorEvent(_, _, _, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _, _, _, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamCopy(_, _, _, _, _, _)).Times(0);

	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, true, true, mProcessorFn, ptsError);

	this->asyncTask.join();
	buffer.Free();
}


//Scenario where audio and subtitle processors are waiting for videoPTS and abort gets called
TEST_F(IsoBmffProcessorTests, abortTests3)
{
	AampGrowableBuffer buffer("IsoBmffProcessorTests-abortTests3");
	bool ptsError = false;

	mAudIsoBmffProcessor->setRate(AAMP_NORMAL_PLAY_RATE, PlayMode_normal);
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(1000), Return(true)));
	mAudIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, true, true, mProcessorFn, ptsError);

	// Simulate scenario for fragment, since this will go into a cond_wait, start an asyncTask to call abort()
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly([this]() {
		// Call abort() (once) in an async task to avoid mutex deadlock
		if (!this->asyncTask.joinable())
			this->asyncTask = std::thread([this]{ this->mAudIsoBmffProcessor->abort(); });
		return false;
	});

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendErrorEvent(_, _, _, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _, _, _, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamCopy(_, _, _, _, _, _)).Times(0);

	(void)mAudIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, false, false, mProcessorFn, ptsError);

	this->asyncTask.join();
	buffer.Free();
}

//Race condition between InjectorLoop and reset calls
//Call sendSegment after an abort was called
TEST_F(IsoBmffProcessorTests, abortTests4)
{
	AampGrowableBuffer buffer("IsoBmffProcessorTests-abortTests4");
	bool ptsError = false;

	mIsoBmffProcessor->setRate(AAMP_NORMAL_PLAY_RATE, PlayMode_normal);
	// Not expecting any calls to isInitSegment and getFirstPTS as its aborted at the beginning
	// EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(false));
	// EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillOnce(DoAll(SetArgReferee<0>(10000), Return(true)));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendErrorEvent(_, _, _, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _, _, _, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamCopy(_, _, _, _, _, _)).Times(0);

	// Call sendSegment after an abort was called
	(void)mIsoBmffProcessor->abort();
	(void)mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, true, true, mProcessorFn, ptsError);

	buffer.Free();
}

//Race condition between InjectorLoop and reset calls
//Call sendSegment after an abort and reset was called
TEST_F(IsoBmffProcessorTests, abortTests5)
{
	AampGrowableBuffer buffer("IsoBmffProcessorTests-abortTests5");
	bool ptsError = false;
	Box *box = (Box*)(0xdeadbeef);

	mIsoBmffProcessor->setRate(AAMP_NORMAL_PLAY_RATE, PlayMode_normal);
	// Call abort and reset
	(void)mIsoBmffProcessor->abort();
	(void)mIsoBmffProcessor->reset();

	// Expecting segments in order. 1. initfragment, 2. video fragment
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(1000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, true, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(10000), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(20000));

	//Called twice for init and fragment
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _, _, _, _, _, _, _)).Times(1);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamCopy(_, _, _, _, _, _)).WillOnce(Return(true));

	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, false, false, mProcessorFn, ptsError);

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

	mIsoBmffProcessor->sendSegment(&buffer, position, duration, 0.0, discontinuous, true, mProcessorFn, ptsError);

	rslt = ceil((position) * vCurrTS);
	duration = (double) vDuration / (double)vCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(0), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, 0.0, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	position += duration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(48048), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, 0.0, discontinuous, false, mProcessorFn, ptsError);

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
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(vCurrTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	//eBMFFPROCESSOR_INIT_TIMESCALE
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);

	vSegDuration = (double)vDuration / (double)vCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(0), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError);

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
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, aSegDuration, 0.0, discontinuous,false, mProcessorFn, ptsError);

	EXPECT_EQ(mAudIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((aPosition) * aCurrTS);
	restampedPTS = mAudIsoBmffProcessor->getSumPTS() - aDuration;
	EXPECT_EQ(restampedPTS, rslt);
	aPosition += aSegDuration;

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vCurrTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, 0.0, discontinuous,true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_CONTINUE_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, 0.0, discontinuous,false, mProcessorFn, ptsError);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
}

//eBMFFPROCESSOR_INIT_TIMESCALE, eBMFFPROCESSOR_CONTINUE_TIMESCALE
TEST_F(IsoBmffProcessorTests, timeScaleTests_2) {
	AampGrowableBuffer buffer("IsoBmffProcessorTests-timeScaleTests_2");
	Box *box = (Box*)(0xdeadbeef);

	double vPosition = 0, aPosition = 0;
	bool discontinuous = false, ptsError = false;
	uint64_t basePts = 0, vCurrTS = 24000, aCurrTS = 48000, rslt = 0, restampedPTS = 0, vDuration = 48048, aDuration = 95232;
	double vSegDuration = 0, aSegDuration = 0;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(aCurrTS), Return(true)));
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(vCurrTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	//eBMFFPROCESSOR_INIT_TIMESCALE
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);

	vSegDuration = (double)vDuration / (double)vCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(0), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError);

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
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, aSegDuration, 0.0, discontinuous,false, mProcessorFn, ptsError);

	EXPECT_EQ(mAudIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((aPosition) * aCurrTS);
	restampedPTS = mAudIsoBmffProcessor->getSumPTS() - aDuration;
	EXPECT_EQ(restampedPTS, rslt);
	aPosition += aSegDuration;

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vCurrTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, 0.0, discontinuous,true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_CONTINUE_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, 0.0, discontinuous,false, mProcessorFn, ptsError);

	buffer.Free();
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
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
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, discontinuous, true, mProcessorFn, ptsError); //fragmentPTSoffset = 0.0

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError); //fragmentPTSoffset = 0.0

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
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, discontinuous, true, mProcessorFn, ptsError); //fragmentPTSoffset = 0.0

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(0), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDurationAfterABR));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError); //fragmentPTSoffset = 0.0

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
TEST_F(IsoBmffProcessorTests, timeScaleTests_4) {
	AampGrowableBuffer buffer("IsoBmffProcessorTests-timeScaleTests_4");
	Box *box = (Box*)(0xdeadbeef);

	bool discontinuous, ptsError = false;
	uint64_t basePts = 0, vDuration = 60060, vDurationAfterABR = 48048, currTS = 30000, rslt = 0, restampedPTS = 0;
	double position = 0, vSegDuration = (double)vDuration / (double)currTS;

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillOnce(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(currTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, discontinuous, true, mProcessorFn, ptsError); //fragmentPTSoffset = 0.0

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_INIT_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError); //fragmentPTSoffset = 0.0

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);

	position += vSegDuration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_TIMESCALE_COMPLETE);

	discontinuous = true;
	position += vSegDuration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(24000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, position, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_SCALE_TO_NEW_TIMESCALE);

	//eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE
	discontinuous = false;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(24000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, position, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getTimeScaleChangeState(), eBMFFPROCESSOR_AFTER_ABR_SCALE_TO_NEW_TIMESCALE);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(0), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDurationAfterABR));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError);

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
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((position) * currTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	position += vSegDuration;
	mIsoBmffProcessor->sendSegment(&buffer, position, vSegDuration-1, 0.0, discontinuous, false, mProcessorFn, ptsError);

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
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vCurrTS), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	vSegDuration = (double)vDuration / (double)vCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	aSegDuration = (double)aDuration / (double)aCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(aDuration));
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, aSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mAudIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((aPosition) * aCurrTS);
	restampedPTS = mAudIsoBmffProcessor->getSumPTS() - aDuration;
	EXPECT_EQ(restampedPTS, rslt);

	aPosition += aSegDuration;
	vPosition += vSegDuration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError);

	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	discontinuous = true;
	vPosition += vSegDuration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(24000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	discontinuous = true;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(48000), Return(true)));
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	discontinuous = false;
	vSegDuration = (double)vDuration / (double)vCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(240240), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, vPosition, vSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError);

	rslt = ceil((vPosition) * vCurrTS);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), rslt);

	aSegDuration = (double)aDuration / (double)aCurrTS;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(481280), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(aNewDuration));
	mAudIsoBmffProcessor->sendSegment(&buffer, aPosition, aSegDuration, 0.0, discontinuous, false, mProcessorFn, ptsError);

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
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	rslt = ceil((position) * vCurrTS);
	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(basePts), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, 0.0, discontinuous, false, mProcessorFn, ptsError);

	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt);

	position += duration;
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, 0.0, discontinuous, false, mProcessorFn, ptsError);

	rslt = ceil((position) * vCurrTS);
	EXPECT_EQ(mIsoBmffProcessor->getBasePTS(), basePts);
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt); //Both fragments are of same duration and timescale

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillRepeatedly(DoAll(SetArgReferee<0>(24000), Return(true)));
	mIsoBmffProcessor->sendSegment(&buffer, 0, 0, 0.0, discontinuous, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getFirstPTS(_)).WillRepeatedly(DoAll(SetArgReferee<0>(vDuration), Return(true)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getBox(_, TypedEq<size_t&>(0))).WillOnce(DoAll(SetArgReferee<1>(0), Return(box)));
	EXPECT_CALL(*g_mockIsoBmffBuffer, getSampleDuration(_,_)).WillOnce(SetArgReferee<1>(vDuration));
	mIsoBmffProcessor->sendSegment(&buffer, position, duration, 0.0, discontinuous, false, mProcessorFn, ptsError);

	buffer.Free();
	restampedPTS = mIsoBmffProcessor->getSumPTS() - vDuration;
	EXPECT_EQ(restampedPTS, rslt); // Restamped PTS will not update on dup fragment
}
