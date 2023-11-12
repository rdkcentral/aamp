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

#include "AampMediaType.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "tsprocessor.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"

using ::testing::_;
using ::testing::Return;
AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

const int tsPacketLength = 188;

class sendSegmentTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *mPrivateInstanceAAMP{};
	TSProcessor *mTSProcessor{};
	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}
		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		
		g_mockAampConfig = new MockAampConfig();

		mTSProcessor = new TSProcessor(mLogObj, mPrivateInstanceAAMP, eStreamOp_DEMUX_AUDIO);

		g_mockPrivateInstanceAAMP = new MockPrivateInstanceAAMP();
	}
		
	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete mTSProcessor;
		mTSProcessor = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;
	}
};

TEST_F(sendSegmentTests, FilterAudioCodecWithAC3Enabled)
{
	bool ignoreProfile = mTSProcessor->FilterAudioCodecBasedOnConfig(FORMAT_MPEGTS);
	EXPECT_FALSE(ignoreProfile);
}

TEST_F(sendSegmentTests, SetAudio1)
{
	std::string id = "group123";
	mTSProcessor->SetAudioGroupId(id);
}

TEST_F(sendSegmentTests, SetAudio2)
{
	std::string id = "   CDCACVDC    ";
	mTSProcessor->SetAudioGroupId(id);
}

TEST_F(sendSegmentTests, FlushTest)
{
	 mTSProcessor->flush();
}

TEST_F(sendSegmentTests, GetLanguageCodeTest)
{
	std::string lang = "fr";
	mTSProcessor->GetLanguageCode(lang);
	ASSERT_EQ(lang, "fr");
}

TEST_F(sendSegmentTests, FilterAudioCodecBasedOnConfig_ATMOSEnabled)
{
	bool result = mTSProcessor->FilterAudioCodecBasedOnConfig(FORMAT_AUDIO_ES_ATMOS);
	ASSERT_FALSE(result);
}

TEST_F(sendSegmentTests, SetThrottleEnableTest)
{
	mTSProcessor->setThrottleEnable(true);
	mTSProcessor->setThrottleEnable(false);
}

TEST_F(sendSegmentTests, FilterAudioCodecBasedOnConfig_ATMOSEnabled11)
{
	bool result = mTSProcessor->FilterAudioCodecBasedOnConfig(FORMAT_AUDIO_ES_AC3);
	ASSERT_FALSE(result);
}

TEST_F(sendSegmentTests, setFrameRateForTMTests)
{
   mTSProcessor->setFrameRateForTM(-12);
}

TEST_F(sendSegmentTests, FilterAudioCodecBasedOnConfig_ATMOSEnabled12)
{
	bool result = mTSProcessor->FilterAudioCodecBasedOnConfig(FORMAT_AUDIO_ES_EC3);
	ASSERT_FALSE(result);
}

TEST_F(sendSegmentTests, ResetTest)
{
	mTSProcessor->reset();
}

TEST_F(sendSegmentTests, SelectAudioIndexToPlay_NoAudioComponents)
{
	int selectedTrack = mTSProcessor->SelectAudioIndexToPlay();
	ASSERT_EQ(selectedTrack, -1);
}

TEST_F(sendSegmentTests, ChangeMuxedAudioTrackTest)
{
	mTSProcessor->ChangeMuxedAudioTrack(UCHAR_MAX);
}

TEST_F(sendSegmentTests, SetApplyOffsetFlagTrue)
{
	
	mTSProcessor->setApplyOffsetFlag(true);
}

TEST_F(sendSegmentTests, SendSegmentTest)
{
	size_t size = 100;
	char segment[100];
	double position = 0.0;
	double duration = 10.0;
	bool discontinuous = false;
	bool ptsError = true;
	bool result;
	result = mTSProcessor->sendSegment(segment, size, position, duration, discontinuous, nullptr, ptsError);
	ASSERT_FALSE(result);
}

TEST_F(sendSegmentTests, SetApplyOffsetFlagFalse)
{
	mTSProcessor->setApplyOffsetFlag(false);
}

TEST_F(sendSegmentTests, esMP3test)
{
	/* Following segment definition contains PAT and PMT details wherein the audio is MP3 i.e. FORMAT_AUDIO_ES_MP3*/
	unsigned char segment[tsPacketLength*2] =
	{
		0x47,0x40,0x00,0x10,0x00,0x00,0xb0,0x0d,0x00,0x01,0xc1,0x00,0x00,0x00,0x01,0xef, \
		0xff,0x36,0x90,0xe2,0x3d,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x47,0x4f,0xff,0x10, \
		0x00,0x02,0xb0,0x3c,0x00,0x01,0xc1,0x00,0x00,0xe1,0x00,0xf0,0x11,0x25,0x0f,0xff, \
		0xff,0x49,0x44,0x33,0x20,0xff,0x49,0x44,0x33,0x20,0x00,0x1f,0x00,0x01,0x1b,0xe1, \
		0x00,0xf0,0x00,0x03,0xe1,0x01,0xf0,0x00,0x15,0xe1,0x02,0xf0,0x0f,0x26,0x0d,0xff, \
		0xff,0x49,0x44,0x33,0x20,0xff,0x49,0x44,0x33,0x20,0x00,0x0f,0x6f,0x2d,0xc7,0x0d, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff, \
		0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
	};
	size_t size = sizeof(segment);
	double position = 0;
	double duration = 2.43;            /*Duration of the stream from which the segment data is extracted*/
	bool discontinuous = false;
	bool ptsError = false;

	/*A thread was waiting for base PTS, in order to get around it eAAMPConfig_AudioOnlyPlayback is configured true*/
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_AudioOnlyPlayback)).WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePublishingMuxedAudio)).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamFormat(_,FORMAT_AUDIO_ES_MP3, _));

	mTSProcessor->sendSegment((char*)segment, size, position, duration, discontinuous,
		[this](MediaType type, SegmentInfo_t info, std::vector<uint8_t> buf)
		{
			mPrivateInstanceAAMP->SendStreamCopy(type, buf.data(), buf.size(), info.pts_ms, info.dts_ms, info.duration);
		},
		ptsError
	);
}

TEST_F(sendSegmentTests, SetRateTest)
{
	double m_playRateNext;
	double rate = 1.5;
	PlayMode mode = PlayMode_normal;
	mTSProcessor->setRate(rate, mode);
}

TEST_F(sendSegmentTests, AbortTest)
{
	mTSProcessor->setThrottleEnable(false);
	mTSProcessor->setRate(2.22, PlayMode_reverse_GOP);
	mTSProcessor->abort();
}
