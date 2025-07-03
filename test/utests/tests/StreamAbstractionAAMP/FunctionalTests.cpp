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
#include <chrono>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "MediaStreamContext.h"
#include "MockAampConfig.h"
#include "MockAampUtils.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockStreamAbstractionAAMP.h"

using ::testing::_;
using ::testing::An;
using ::testing::SetArgReferee;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::WithArgs;
using ::testing::WithoutArgs;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;

AampConfig *gpGlobalConfig{nullptr};


class StreamAbstractionAAMP_Test : public ::testing::Test
{
protected:

	class TestableStreamAbstractionAAMP : public StreamAbstractionAAMP
	{
	public:
		// Constructor to pass parameters to the base class constructor
		TestableStreamAbstractionAAMP(PrivateInstanceAAMP* aamp)
			: StreamAbstractionAAMP(aamp),
			mMockAudioTrack(nullptr),
			mMockVideoTrack(nullptr)
		{
		}

		~TestableStreamAbstractionAAMP()
		{
			if (mMockAudioTrack)
			{
				delete mMockAudioTrack;
				mMockAudioTrack = nullptr;
			}
			if (mMockVideoTrack)
			{
				delete mMockVideoTrack;
				mMockVideoTrack = nullptr;
			}
		}

		MockMediaTrack *mMockAudioTrack;
		MockMediaTrack *mMockVideoTrack;

		virtual AAMPStatusType Init(TuneType tuneType){return eAAMPSTATUS_OK;}
		virtual void Start(){}
		virtual void Stop(bool clearChannelData){}
		virtual void GetStreamFormat(StreamOutputFormat &primaryOutputFormat, StreamOutputFormat &audioOutputFormat, StreamOutputFormat &auxAudioOutputFormat, StreamOutputFormat &subtitleOutputFormat){}

		virtual MediaTrack* GetMediaTrack(TrackType type)
		{
			if (type == eTRACK_AUDIO)
				return mMockAudioTrack;
			else
				return mMockVideoTrack;
		}

		void testSetTrackState(MediaTrackDiscontinuityState state)
		{
			mTrackState = state;
		}
	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP *mStreamAbstractionAAMP;
	AampConfig *mConfig;

	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}
		g_mockAampConfig = new NiceMock<MockAampConfig>();

		if (g_mockPrivateInstanceAAMP == nullptr)
		{
			g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(mConfig);
		mStreamAbstractionAAMP = new TestableStreamAbstractionAAMP(mPrivateInstanceAAMP);

		// For initialisation of mediatrack
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
			.Times(AnyNumber())
			.WillRepeatedly(Return(0));
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
			.Times(AnyNumber())
			.WillRepeatedly(Return(0));

		mStreamAbstractionAAMP->mMockAudioTrack = new MockMediaTrack(eTRACK_AUDIO, mPrivateInstanceAAMP, "audio");
		mStreamAbstractionAAMP->mMockVideoTrack = new MockMediaTrack(eTRACK_VIDEO, mPrivateInstanceAAMP, "video");

		mStreamAbstractionAAMP->mMockAudioTrack->fragmentDurationSeconds = 1.92;
		mStreamAbstractionAAMP->mMockVideoTrack->fragmentDurationSeconds = 1.92;
	}

	void TearDown() override
	{
		delete mStreamAbstractionAAMP;
		mStreamAbstractionAAMP = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;
	}
};

// Check that WaitForVideoTrackCatchup() waits till injected buffers match
TEST_F(StreamAbstractionAAMP_Test, WaitFor_VideoTrackCatchup_wait)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillRepeatedly(Return(true));

	// Check aamp loops till video catches up
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, GetTotalInjectedDuration())
		.Times(4)
		.WillRepeatedly(Return(2));

	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetTotalInjectedDuration())
		.Times(4)
		.WillOnce(Return(0))
		.WillOnce(Return(0))
		.WillOnce(Return(0))
		.WillOnce(Return(2));

	mStreamAbstractionAAMP->WaitForVideoTrackCatchup();
}

// Check that WaitForVideoTrackCatchup() does not wait if video is processing a discontinuity
TEST_F(StreamAbstractionAAMP_Test, WaitFor_VideoTrackCatchup_discontinuity)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillRepeatedly(Return(true));
		
	// Set the flag that indicates processing discontinuity
	mStreamAbstractionAAMP->testSetTrackState(eDISCONTINUITY_IN_VIDEO);

	// Check aamp does not loop till video catches up
	EXPECT_CALL(*mStreamAbstractionAAMP->mMockAudioTrack, GetTotalInjectedDuration())
		.Times(1)
		.WillOnce(Return(2));

	EXPECT_CALL(*mStreamAbstractionAAMP->mMockVideoTrack, GetTotalInjectedDuration())
		.Times(1)
		.WillOnce(Return(0));

	mStreamAbstractionAAMP->WaitForVideoTrackCatchup();
}

