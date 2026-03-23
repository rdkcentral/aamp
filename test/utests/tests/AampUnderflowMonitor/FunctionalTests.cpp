/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2026 RDK Management
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
#include <atomic>
#include <thread>
#include <chrono>
#include <stdexcept>

#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampUnderflowMonitor.h"
#include "StreamAbstractionAAMP.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMediaTrack.h"

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::Return;

static AampConfig *gUnderflowConfig = nullptr;

class AampUnderflowMonitor_FunctionalTests : public ::testing::Test
{
protected:
	class TestStreamAbstraction : public StreamAbstractionAAMP
	{
	public:
		explicit TestStreamAbstraction(PrivateInstanceAAMP* aamp)
			: StreamAbstractionAAMP(aamp), videoTrack(nullptr)
		{
		}
		~TestStreamAbstraction() override
		{
			delete videoTrack;
			videoTrack = nullptr;
		}
		MockMediaTrack* videoTrack;

		AAMPStatusType Init(TuneType) override { return eAAMPSTATUS_OK; }
		void Start() override {}
		void Stop(bool) override {}
		void GetStreamFormat(StreamOutputFormat&, StreamOutputFormat&, StreamOutputFormat&) override {}
		MediaTrack* GetMediaTrack(TrackType t) override
		{
			if (t == eTRACK_VIDEO) return videoTrack;
			return nullptr;
		}
	};

	PrivateInstanceAAMP* aamp = nullptr;
	TestStreamAbstraction* stream = nullptr;
	AampUnderflowMonitor* monitor = nullptr;

	void SetUp() override
	{
		if (!gUnderflowConfig) gUnderflowConfig = new AampConfig();
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		if (!g_mockPrivateInstanceAAMP) g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();

		aamp = new PrivateInstanceAAMP(gUnderflowConfig);
		stream = new TestStreamAbstraction(aamp);
		stream->videoTrack = new NiceMock<MockMediaTrack>(eTRACK_VIDEO, aamp, "video");
		monitor = new AampUnderflowMonitor(stream, aamp);

		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
			.Times(AnyNumber()).WillRepeatedly(Return(0));
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
			.Times(AnyNumber()).WillRepeatedly(Return(0));

		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowDetectThresholdSec))
			.Times(AnyNumber()).WillRepeatedly(Return(1.0));
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowResumeThresholdSec))
			.Times(AnyNumber()).WillRepeatedly(Return(2.0));
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowLowBufferSec))
			.Times(AnyNumber()).WillRepeatedly(Return(1.0));
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowHighBufferSec))
			.Times(AnyNumber()).WillRepeatedly(Return(10.0));
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowLowBufferPollMs))
			.Times(AnyNumber()).WillRepeatedly(Return(10));
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowMediumBufferPollMs))
			.Times(AnyNumber()).WillRepeatedly(Return(10));
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowHighBufferPollMs))
			.Times(AnyNumber()).WillRepeatedly(Return(10));
	}

	void TearDown() override
	{
		delete monitor; monitor = nullptr;
		delete stream; stream = nullptr;
		delete aamp; aamp = nullptr;
		delete g_mockPrivateInstanceAAMP; g_mockPrivateInstanceAAMP = nullptr;
		delete g_mockAampConfig; g_mockAampConfig = nullptr;
		delete gUnderflowConfig; gUnderflowConfig = nullptr;
	}
};

TEST_F(AampUnderflowMonitor_FunctionalTests, ThrowsOnNullPointers)
{
	EXPECT_THROW(AampUnderflowMonitor(nullptr, nullptr), std::invalid_argument);
}

TEST_F(AampUnderflowMonitor_FunctionalTests, StartStopTransitionsRunningState)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.Times(AnyNumber())
		.WillRepeatedly(Return(eSTATE_PLAYING));

	monitor->Start();
	EXPECT_TRUE(monitor->IsRunning());

	monitor->Stop();
	EXPECT_FALSE(monitor->IsRunning());
}

TEST_F(AampUnderflowMonitor_FunctionalTests, StartTwiceIsIdempotent)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.Times(AnyNumber())
		.WillRepeatedly(Return(eSTATE_PLAYING));

	monitor->Start();
	EXPECT_TRUE(monitor->IsRunning());
	EXPECT_NO_THROW(monitor->Start());

	monitor->Stop();
	EXPECT_FALSE(monitor->IsRunning());
}

TEST_F(AampUnderflowMonitor_FunctionalTests, RunDetectsUnderflowByBufferThreshold)
{
	aamp->rate = AAMP_NORMAL_PLAY_RATE;

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.Times(AnyNumber())
		.WillRepeatedly(Return(eSTATE_PLAYING));
	EXPECT_CALL(*stream->videoTrack, GetBufferedDuration())
		.Times(AnyNumber())
		.WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsSinkCacheEmpty(eMEDIATYPE_VIDEO))
		.WillRepeatedly(Return(false));

	std::atomic<bool> bufferingStarted{false};
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetBufferingState(true))
		.Times(AtLeast(1))
		.WillOnce(::testing::Invoke([&](bool){ bufferingStarted.store(true); }));

	monitor->Start();
	auto start = std::chrono::steady_clock::now();
	while (!bufferingStarted.load() && (std::chrono::steady_clock::now() - start) < std::chrono::milliseconds(1000))
	{
		std::this_thread::yield();
	}
	monitor->Stop();
	EXPECT_TRUE(bufferingStarted.load());
}

TEST_F(AampUnderflowMonitor_FunctionalTests, RunEndsUnderflowDuringTrickplayWhenResumeThresholdMet)
{
	aamp->SetBufUnderFlowStatus(true);
	aamp->mSinkPaused.store(true);
	aamp->rate = 2.0f;

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowResumeThresholdSec))
		.Times(AnyNumber()).WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.Times(AnyNumber())
		.WillRepeatedly(Return(eSTATE_PLAYING));
	EXPECT_CALL(*stream->videoTrack, GetBufferedDuration())
		.Times(AnyNumber())
		.WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsSinkCacheEmpty(eMEDIATYPE_VIDEO))
		.Times(AtLeast(1))
		.WillRepeatedly(Return(false));

	std::atomic<bool> bufferingEnded{false};
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetBufferingState(false))
		.Times(AtLeast(1))
		.WillOnce(::testing::Invoke([&](bool){ bufferingEnded.store(true); }));

	monitor->Start();
	auto start = std::chrono::steady_clock::now();
	while (!bufferingEnded.load() && (std::chrono::steady_clock::now() - start) < std::chrono::milliseconds(1000))
	{
		std::this_thread::yield();
	}
	monitor->Stop();
	EXPECT_TRUE(bufferingEnded.load());
}

TEST_F(AampUnderflowMonitor_FunctionalTests, RunKeepsPollingWhenPlayerStoppedUntilExternalStop)
{
	// Monitor does not self-exit on eSTATE_STOPPED — it relies entirely on
	// Stop() being called externally (via privaamp stop or destructor).
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.Times(AnyNumber())
		.WillRepeatedly(Return(eSTATE_STOPPED));

	monitor->Start();
	EXPECT_TRUE(monitor->IsRunning());
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	EXPECT_TRUE(monitor->IsRunning());

	monitor->Stop();
	EXPECT_FALSE(monitor->IsRunning());
}
