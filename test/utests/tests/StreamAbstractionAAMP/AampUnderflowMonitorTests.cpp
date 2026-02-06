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
#include "priv_aamp.h"
#include "AampConfig.h"
#include "AampUnderflowMonitor.h"
#include "StreamAbstractionAAMP.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMediaTrack.h"
#include <atomic>
#include <thread>
#include <chrono>

using ::testing::Return;
using ::testing::NiceMock;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InSequence;

extern MockAampConfig *g_mockAampConfig;
extern MockPrivateInstanceAAMP *g_mockPrivateInstanceAAMP;

// Local global config for tests in this TU
static AampConfig *gLocalConfig = nullptr;

class AampUnderflowMonitorTests : public ::testing::Test
{
    protected:
        class TestStreamAbstraction : public StreamAbstractionAAMP
        {
        public:
            explicit TestStreamAbstraction(PrivateInstanceAAMP* aamp)
                : StreamAbstractionAAMP(aamp)
                , videoTrack(nullptr)
                , audioTrack(nullptr)
            {
            }
			~TestStreamAbstraction() override = default;
			std::unique_ptr<MockMediaTrack> videoTrack;
			std::unique_ptr<MockMediaTrack> audioTrack;
            // Minimal overrides not relevant to test focus
            AAMPStatusType Init(TuneType) override { return eAAMPSTATUS_OK; }
            void Start() override {}
            void Stop(bool) override {}
            void GetStreamFormat(StreamOutputFormat&, StreamOutputFormat&, StreamOutputFormat&) override {}
			MediaTrack* GetMediaTrack(TrackType t) override
			{
				if (t == eTRACK_VIDEO) return videoTrack.get();
				if (t == eTRACK_AUDIO) return audioTrack.get();
				return nullptr;
			}
        };

        PrivateInstanceAAMP* aamp = nullptr;
        TestStreamAbstraction* stream = nullptr;

        void SetUp() override
        {
            if (!gLocalConfig) gLocalConfig = new AampConfig();
            // Hook the mock config so GETCONFIGVALUE/ISCONFIGSET macros route to mocks
            g_mockAampConfig = new NiceMock<MockAampConfig>();

            // Create aamp using the real fake implementation used in utests
            aamp = new PrivateInstanceAAMP(gLocalConfig);
            // Provide a mock proxy used by various calls
            if (!g_mockPrivateInstanceAAMP) g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();

            stream = new TestStreamAbstraction(aamp);

            // MediaTrack init expectations commonly present across tests
            EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentCached))
                .Times(AnyNumber()).WillRepeatedly(Return(0));
            EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_MaxFragmentChunkCached))
                .Times(AnyNumber()).WillRepeatedly(Return(0));

            // Underflow monitor configuration expectations used by AampUnderflowMonitor::Run
            EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowDetectThresholdSec))
                .Times(AnyNumber()).WillRepeatedly(Return(1.0));
            EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowResumeThresholdSec))
                .Times(AnyNumber()).WillRepeatedly(Return(2.0));
            EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowLowBufferSec))
                .Times(AnyNumber()).WillRepeatedly(Return(1.0));
            EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowHighBufferSec))
                .Times(AnyNumber()).WillRepeatedly(Return(10.0));
            EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowLowBufferPollMs))
                .Times(AnyNumber()).WillRepeatedly(Return(50));
            EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowMediumBufferPollMs))
                .Times(AnyNumber()).WillRepeatedly(Return(100));
            EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowHighBufferPollMs))
                .Times(AnyNumber()).WillRepeatedly(Return(150));
        }

        void TearDown() override
        {
            delete stream; stream = nullptr;
            delete aamp; aamp = nullptr;
            delete g_mockPrivateInstanceAAMP; g_mockPrivateInstanceAAMP = nullptr;
            delete g_mockAampConfig; g_mockAampConfig = nullptr;
        }
};

// 1) Monitor gated off by config -> no instance created
TEST_F(AampUnderflowMonitorTests, GatedOffByConfig)
{
	// Configure gating
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_EnableAampUnderflowMonitor))
		.WillOnce(Return(false));

	stream->StartUnderflowMonitor();
	// Expect monitor not running
	EXPECT_FALSE(stream->IsUnderflowMonitorRunning());
}
// 2) No video track -> early exit
TEST_F(AampUnderflowMonitorTests, NoVideoTrackSkipsStart)
{
	// Enable monitor feature
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_EnableAampUnderflowMonitor))
		.WillOnce(Return(true));

	// No videoTrack set
	stream->videoTrack.reset();

	stream->StartUnderflowMonitor();
	EXPECT_FALSE(stream->IsUnderflowMonitorRunning());
}

// 3) Happy path: video track present, monitor starts
TEST_F(AampUnderflowMonitorTests, StartsWhenEnabledAndVideoPresent)
{
	// Enable monitor feature
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_EnableAampUnderflowMonitor))
		.WillOnce(Return(true));

	// Provide video track
	stream->videoTrack = std::make_unique<NiceMock<MockMediaTrack>>(eTRACK_VIDEO, aamp, "video");

	// Drive thread state; allow any number of polls
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.Times(AnyNumber())
		.WillRepeatedly(Return(eSTATE_PLAYING));

	stream->StartUnderflowMonitor();
	EXPECT_TRUE(stream->IsUnderflowMonitorRunning());

	// Cleanup
	stream->StopUnderflowMonitor();
	EXPECT_FALSE(stream->IsUnderflowMonitorRunning());
}

// 4) Idempotent: starting again while already started should keep running without crash
TEST_F(AampUnderflowMonitorTests, StartTwiceKeepsRunning)
{
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_EnableAampUnderflowMonitor))
		.WillRepeatedly(Return(true));

	stream->videoTrack = std::make_unique<NiceMock<MockMediaTrack>>(eTRACK_VIDEO, aamp, "video");

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.Times(AnyNumber())
		.WillRepeatedly(Return(eSTATE_PLAYING));

	stream->StartUnderflowMonitor();
	EXPECT_TRUE(stream->IsUnderflowMonitorRunning());
	// Thread likely exited; ensure no crash on double-start

	// Call StartUnderflowMonitor again; implementation is idempotent and should not throw
	EXPECT_NO_THROW(stream->StartUnderflowMonitor());
	EXPECT_TRUE(stream->IsUnderflowMonitorRunning());

	stream->StopUnderflowMonitor();
	EXPECT_FALSE(stream->IsUnderflowMonitorRunning());
}

// 5) Run() enters detection path when buffer <= threshold and downloads enabled
TEST_F(AampUnderflowMonitorTests, Run_DetectsUnderflowByBufferThreshold)
{
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_EnableAampUnderflowMonitor))
		.WillOnce(Return(true));

	stream->videoTrack = std::make_unique<NiceMock<MockMediaTrack>>(eTRACK_VIDEO, aamp, "video");

	// Keep PLAYING; allow any number of polls
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.Times(AnyNumber())
		.WillRepeatedly(Return(eSTATE_PLAYING));
	// Buffered duration at/below threshold to trigger detection
	EXPECT_CALL(*stream->videoTrack, GetBufferedDuration())
		.Times(AnyNumber())
		.WillRepeatedly(Return(0.0));

	// Allow buffer check: normal rate, not seeking
	aamp->rate = AAMP_NORMAL_PLAY_RATE;

	// Downloads enabled and sink cache not empty
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, TrackDownloadsAreEnabled(eMEDIATYPE_VIDEO))
		.WillRepeatedly(Return(true));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsSinkCacheEmpty(eMEDIATYPE_VIDEO))
		.WillRepeatedly(Return(false));

	// Expect buffering to start when underflow detected
	std::mutex mtx;
	std::condition_variable cv;
	bool signaled = false;
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetBufferingState(true))
		.Times(AtLeast(1))
		.WillOnce(::testing::Invoke([&](bool){
			{
				std::lock_guard<std::mutex> lock(mtx);
				signaled = true;
			}
			cv.notify_one();
		}));

	stream->StartUnderflowMonitor();
	// Wait until buffering start is observed or timeout
	{
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait_for(lock, std::chrono::milliseconds(1000), [&]{ return signaled; });
	}
	stream->StopUnderflowMonitor();
	EXPECT_TRUE(signaled);
}

// 6) Run() suppresses buffer checks during trickplay and ends underflow when resume threshold met and cache not empty
TEST_F(AampUnderflowMonitorTests, Run_TrickplaySuppressesBufferCheckAndEndsUnderflow)
{
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_EnableAampUnderflowMonitor))
		.WillOnce(Return(true));

	stream->videoTrack = std::make_unique<NiceMock<MockMediaTrack>>(eTRACK_VIDEO, aamp, "video");

	// Underflow active and pipeline paused
	aamp->SetBufUnderFlowStatus(true);
	aamp->mSinkPaused.store(true);

	// Make allowBufferCheck false via trickplay rate
	aamp->rate = 2.0f;

	// Ensure detection condition is false: cache not empty
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, IsSinkCacheEmpty(eMEDIATYPE_VIDEO))
		.Times(AtLeast(1))
		.WillRepeatedly(Return(false));
	// Downloads enabled state doesn't matter here
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, TrackDownloadsAreEnabled(eMEDIATYPE_VIDEO))
		.Times(AnyNumber()).WillRepeatedly(Return(true));

	// Configure resume threshold at 0 so buffered(=0) meets it
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_UnderflowResumeThresholdSec))
		.Times(AnyNumber()).WillRepeatedly(Return(0.0));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetState())
		.Times(AnyNumber())
		.WillRepeatedly(Return(eSTATE_PLAYING));
	// Buffered duration via GetBufferedVideoDurationSec() uses fake impl returning 0.0

	// Expect buffering to end
	std::mutex mtx;
	std::condition_variable cv;
	bool signaled = false;
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetBufferingState(false))
		.Times(AtLeast(1))
		.WillOnce(::testing::Invoke([&](bool){
			{
				std::lock_guard<std::mutex> lock(mtx);
				signaled = true;
			}
			cv.notify_one();
		}));

	stream->StartUnderflowMonitor();
	// Wait until buffering end is observed or timeout
	{
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait_for(lock, std::chrono::milliseconds(1000), [&]{ return signaled; });
	}
	stream->StopUnderflowMonitor();
	EXPECT_TRUE(signaled);
}