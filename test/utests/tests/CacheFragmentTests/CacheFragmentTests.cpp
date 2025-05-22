/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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

#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MediaStreamContext.h"
#include "fragmentcollector_mpd.h"
#include "AampMemoryUtils.h"
#include "AampCacheHandler.h"
#include "../priv_aamp.h"
#include "isobmff/isobmffbuffer.h"
#include "AampConfig.h"
#include "MockAampConfig.h"
#include "StreamAbstractionAAMP.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::SetArgReferee;
using ::testing::AtLeast;

AampConfig *gpGlobalConfig{nullptr};
struct TestParams
{
	bool lowlatencyMode;
	bool chunkMode;
	bool initFragment;
	int expectedFragmentChunksCached;
	int expectedFragmentCached;
};

// Test cases 
TestParams testCases[] = {
	{true, true, false, 0, 1},  // Low-latency, chunk mode, non-init fragment
	{true, true, true, 1, 0},   // Low-latency, chunk mode, init fragment
	{true, false, true, 0, 0},  // Low-latency, non-chunk mode, init fragment
	{true, false, false, 0, 1}, // Low-latency, non-chunk mode, non-init fragment
	{false, false, true, 0, 0}, // Non-low-latency, non-chunk mode, init fragment
	{false, false, false, 0, 1} // Non-low-latency, non-chunk mode, non-init fragment
};


class MediaStreamContextTest : public ::testing::TestWithParam<TestParams> 
{
	public:
		PrivateInstanceAAMP *mPrivateInstanceAAMP;
		StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
		MediaStreamContext *mMediaStreamContext;

	protected:
		using BoolConfigSettings = std::map<AAMPConfigSettingBool, bool>;
		using IntConfigSettings = std::map<AAMPConfigSettingInt, int>;

		/** @brief Boolean AAMP configuration settings. */
		const BoolConfigSettings mDefaultBoolConfigSettings =
		{
			{eAAMPConfig_EnableMediaProcessor, true},
			{eAAMPConfig_EnableCMCD, false},
			{eAAMPConfig_BulkTimedMetaReport, false},
			{eAAMPConfig_BulkTimedMetaReportLive, false},
			{eAAMPConfig_EnableSCTE35PresentationTime, false},
			{eAAMPConfig_EnableClientDai, false},
			{eAAMPConfig_MatchBaseUrl, false},
			{eAAMPConfig_UseAbsoluteTimeline, false},
			{eAAMPConfig_DisableAC4, true},
			{eAAMPConfig_LimitResolution, false},
			{eAAMPConfig_Disable4K, false},
			{eAAMPConfig_PersistHighNetworkBandwidth, false},
			{eAAMPConfig_PersistLowNetworkBandwidth, false},
			{eAAMPConfig_MidFragmentSeek, false},
			{eAAMPConfig_PropagateURIParam, true},
			{eAAMPConfig_DashParallelFragDownload, false},
			{eAAMPConfig_DisableATMOS, false},
			{eAAMPConfig_DisableEC3, false},
			{eAAMPConfig_DisableAC3, false},
			{eAAMPConfig_EnableLowLatencyDash, false},
			{eAAMPConfig_EnableIgnoreEosSmallFragment, false},
			{eAAMPConfig_EnablePTSReStamp, false},
			{eAAMPConfig_LocalTSBEnabled, false},
			{eAAMPConfig_EnableIFrameTrackExtract, false}
		};

		BoolConfigSettings mBoolConfigSettings;

		/** @brief Integer AAMP configuration settings. */
		const IntConfigSettings mDefaultIntConfigSettings =
		{
			{eAAMPConfig_ABRCacheLength, DEFAULT_ABR_CACHE_LENGTH},
			{eAAMPConfig_MaxABRNWBufferRampUp, AAMP_HIGH_BUFFER_BEFORE_RAMPUP},
			{eAAMPConfig_MinABRNWBufferRampDown, AAMP_LOW_BUFFER_BEFORE_RAMPDOWN},
			{eAAMPConfig_ABRNWConsistency, DEFAULT_ABR_NW_CONSISTENCY_CNT},
			{eAAMPConfig_RampDownLimit, -1},
			{eAAMPConfig_MaxFragmentCached, DEFAULT_CACHED_FRAGMENTS_PER_TRACK},
			{eAAMPConfig_PrePlayBufferCount, DEFAULT_PREBUFFER_COUNT},
			{eAAMPConfig_VODTrickPlayFPS, TRICKPLAY_VOD_PLAYBACK_FPS},
			{eAAMPConfig_ABRBufferCounter,DEFAULT_ABR_BUFFER_COUNTER},
			{eAAMPConfig_MaxFragmentChunkCached,DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK}
		};

		IntConfigSettings mIntConfigSettings;
		void SetUp() override
		{
			if (gpGlobalConfig == nullptr)
			{
				gpGlobalConfig = new AampConfig();
			}
			g_mockAampConfig = new MockAampConfig();
			mBoolConfigSettings = mDefaultBoolConfigSettings;
			mIntConfigSettings = mDefaultIntConfigSettings;
			for (const auto & b : mBoolConfigSettings)
			{
				EXPECT_CALL(*g_mockAampConfig, IsConfigSet(b.first))
					.Times(testing::AnyNumber())
					.WillRepeatedly(Return(b.second));
			}

			for (const auto & i : mIntConfigSettings)
			{
				EXPECT_CALL(*g_mockAampConfig, GetConfigValue(i.first))
					.Times(testing::AnyNumber())
					.WillRepeatedly(Return(i.second));
			}
			mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
			mStreamAbstractionAAMP_MPD = new StreamAbstractionAAMP_MPD(mPrivateInstanceAAMP, 123.45, 1);
		}

		void TearDown() override
		{
			delete mMediaStreamContext;
			mMediaStreamContext = nullptr;
                  
			delete mStreamAbstractionAAMP_MPD;
			mStreamAbstractionAAMP_MPD = nullptr;
                  
			delete mPrivateInstanceAAMP;
			mPrivateInstanceAAMP = nullptr;

			delete g_mockAampConfig;
			g_mockAampConfig = nullptr;
		}

		void Initialize(bool lowlatencyMode, bool chunkMode)
		{
			unsigned char data[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
			AampLLDashServiceData llDashData;
			llDashData.availabilityTimeOffset = 1.2;
			llDashData.lowLatencyMode = lowlatencyMode;
			mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
			mPrivateInstanceAAMP->SetLLDashServiceData(llDashData);
			mMediaStreamContext = new MediaStreamContext(eTRACK_VIDEO, mStreamAbstractionAAMP_MPD, mPrivateInstanceAAMP, "SAMPLETEXT");
			mMediaStreamContext->mTempFragment->AppendBytes(data, 12);
			mPrivateInstanceAAMP->SetLLDashChunkMode(chunkMode);
		}
};

/**
 * @brief Tests the fragment caching behavior of MediaStreamContext.
 * This test verifies that the expected number of fragment chunks are cached and TotalFragmentFetched
 * based on the provided test parameters.
 */
TEST_P(MediaStreamContextTest, CacheFragment)
{
	TestParams testParam = GetParam();
	Initialize(testParam.lowlatencyMode, testParam.chunkMode);

	bool retResult = mMediaStreamContext->CacheFragment("remoteUrl", 0, 10, 0, NULL, testParam.initFragment, false, false, 0, 0, false);

	// Check expected results for fragment chunks cached and fragments cached
	EXPECT_EQ(mMediaStreamContext->numberOfFragmentChunksCached, testParam.expectedFragmentChunksCached);
	EXPECT_EQ(mMediaStreamContext->GetTotalFragmentsFetched(), testParam.expectedFragmentCached);
}

INSTANTIATE_TEST_SUITE_P(
		MediaStreamContextTestSuite,
		MediaStreamContextTest,
		::testing::ValuesIn(testCases)
		);

