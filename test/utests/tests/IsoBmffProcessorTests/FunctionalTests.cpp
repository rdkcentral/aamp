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
			EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_UseNewFetcherLoop)).WillRepeatedly(Return(false));
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

	bool ret = mIsoBmffProcessor->sendSegment(&buffer, 0, 0, true, true, mProcessorFn, ptsError);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendErrorEvent(_, _, _, _, _, _, _)).Times(0);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _, _, _, _, _, _)).Times(0);
	this->asyncTask.join();
	buffer.Free();
}

