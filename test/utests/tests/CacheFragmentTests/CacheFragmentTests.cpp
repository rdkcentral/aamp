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
#include "AampCacheHandler.h"
#include "../priv_aamp.h"
#include "isobmff/isobmffbuffer.h"
#include "AampConfig.h"
#include "AampTSBSessionManager.h"
#include "MockAampConfig.h"
#include "MockIsoBmffBuffer.h"
#include "StreamAbstractionAAMP.h"
#include "AampDownloadInfo.hpp"
#include "MockPrivateInstanceAAMP.h"
#include "MockStreamAbstractionAAMP_MPD.h"
#include "MockTSBSessionManager.h"
#include "MockTSBReader.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::SetArgReferee;
using ::testing::AtLeast;
using ::testing::DoAll;

AampConfig *gpGlobalConfig{nullptr};
struct TestParams
{
	bool lowlatency;                   // true for low-latency mode, false for standard mode
	bool chunk;                        // true for chunk mode, false for standard mode
	bool tsb;                          // true if AAMP TSB is enabled, false otherwise
	bool eos;                          // true if simulating EOS, false otherwise; EOS tests inject from the TSB, the rest from live
	bool paused;                       // true if pipeline is paused, false otherwise
	bool underflow;                    // true if underflow occurred, false otherwise
	bool init;                         // true if init fragment, false if media fragment
	float rate;                        // play rate
	int expectedFragmentChunksCached;  // expected number of fragments added to chunk cache
	int expectedFragmentCached;        // expected number of fragments added to regular cache
};

// Test cases
TestParams testCases[] =
{
	{.lowlatency = true, .chunk = true, .tsb = false, .eos = false, .paused = false, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = true, .tsb = false, .eos = false, .paused = false, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = false, .tsb = false, .eos = false, .paused = false, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 1},
	{.lowlatency = true, .chunk = false, .tsb = false, .eos = false, .paused = false, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 1},
	{.lowlatency = false, .chunk = false, .tsb = false, .eos = false, .paused = false, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 1},
	{.lowlatency = false, .chunk = false, .tsb = false, .eos = false, .paused = false, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 1},

	// Test with AAMP TSB enabled, chunk cache is used for non-chunked fragments
	{.lowlatency = true, .chunk = true, .tsb = true, .eos = false, .paused = false, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = true, .tsb = true, .eos = false, .paused = false, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = false, .tsb = true, .eos = false, .paused = false, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = false, .tsb = true, .eos = false, .paused = false, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = false, .chunk = false, .tsb = true, .eos = false, .paused = false, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = false, .chunk = false, .tsb = true, .eos = false, .paused = false, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},

	// Test EOS with AAMP TSB enabled
	{.lowlatency = true, .chunk = true, .tsb = true, .eos = true, .paused = false, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = true, .tsb = true, .eos = true, .paused = false, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = false, .tsb = true, .eos = true, .paused = false, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = false, .tsb = true, .eos = true, .paused = false, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = false, .chunk = false, .tsb = true, .eos = true, .paused = false, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = false, .chunk = false, .tsb = true, .eos = true, .paused = false, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},

	// Test with pipeline paused
	{.lowlatency = true, .chunk = true, .tsb = false, .eos = false, .paused = true, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = true, .tsb = false, .eos = false, .paused = true, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = false, .tsb = false, .eos = false, .paused = true, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 1},
	{.lowlatency = true, .chunk = false, .tsb = false, .eos = false, .paused = true, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 1},
	{.lowlatency = false, .chunk = false, .tsb = false, .eos = false, .paused = true, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 1},
	{.lowlatency = false, .chunk = false, .tsb = false, .eos = false, .paused = true, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 1},

	// Test with AAMP TSB enabled and pipeline paused
	{.lowlatency = true, .chunk = true, .tsb = true, .eos = false, .paused = true, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = true, .tsb = true, .eos = false, .paused = true, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = false, .tsb = true, .eos = false, .paused = true, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 0},
	{.lowlatency = true, .chunk = false, .tsb = true, .eos = false, .paused = true, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 0},
	{.lowlatency = false, .chunk = false, .tsb = true, .eos = false, .paused = true, .underflow = false, .init = true, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 0},
	{.lowlatency = false, .chunk = false, .tsb = true, .eos = false, .paused = true, .underflow = false, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 0, .expectedFragmentCached = 0},

	// Test with pipeline paused and underflow
	{.lowlatency = false, .chunk = false, .tsb = true, .eos = false, .paused = true, .underflow = true, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},
	{.lowlatency = false, .chunk = false, .tsb = true, .eos = true, .paused = true, .underflow = true, .init = false, .rate = AAMP_NORMAL_PLAY_RATE, .expectedFragmentChunksCached = 1, .expectedFragmentCached = 0},

	// Test with rate != AAMP_NORMAL_PLAY_RATE
	{.lowlatency = true, .chunk = true, .tsb = true, .eos = false, .paused = true, .underflow = false, .init = true, .rate = (AAMP_NORMAL_PLAY_RATE*2), .expectedFragmentChunksCached = 0, .expectedFragmentCached = 0}
};


class MediaStreamContextTest : public ::testing::TestWithParam<TestParams> 
{
	public:
		PrivateInstanceAAMP *mPrivateInstanceAAMP;
		StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
		MediaStreamContext *mMediaStreamContext;
		AampTSBSessionManager *mTsbSessionManager;
		IPeriod *mPeriod;
		std::shared_ptr<AampTsbReader> mTsbReader;

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
			{eAAMPConfig_DisableAC4, false},
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
			{eAAMPConfig_EnableIFrameTrackExtract, false},
			{eAAMPConfig_useRialtoSink, false},
			{eAAMPConfig_UseMp4Demux, false},
			{eAAMPConfig_EnableABR, true},
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
			{eAAMPConfig_MaxDownloadBuffer, DEFAULT_MAX_DOWNLOAD_BUFFER},
			{eAAMPConfig_MaxFragmentChunkCached,DEFAULT_CACHED_FRAGMENT_CHUNKS_PER_TRACK}
		};

		IntConfigSettings mIntConfigSettings;
		void SetUp() override
		{
			mPeriod = nullptr;
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
			mTsbSessionManager = new AampTSBSessionManager(mPrivateInstanceAAMP);
			g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
			g_mockStreamAbstractionAAMP_MPD = new NiceMock<MockStreamAbstractionAAMP_MPD>(mPrivateInstanceAAMP, 0, 0);
			g_mockTSBSessionManager = new NiceMock<MockTSBSessionManager>(mPrivateInstanceAAMP);
			mTsbReader = std::make_shared<AampTsbReader>(mPrivateInstanceAAMP, nullptr, eMEDIATYPE_VIDEO, "sessionId");
			g_mockTSBReader = std::make_shared<MockTSBReader>();
			g_mockIsoBmffBuffer = new NiceMock<MockIsoBmffBuffer>();
		}

		void TearDown() override
		{
			delete g_mockIsoBmffBuffer;
			g_mockIsoBmffBuffer = nullptr;

			g_mockTSBReader.reset();

			delete g_mockTSBSessionManager;
			g_mockTSBSessionManager = nullptr;

			delete g_mockStreamAbstractionAAMP_MPD;
			g_mockStreamAbstractionAAMP_MPD = nullptr;

			if (mPeriod)
			{
				delete mPeriod;
				mPeriod = nullptr;
			}

			delete g_mockPrivateInstanceAAMP;
			g_mockPrivateInstanceAAMP = nullptr;

			delete mTsbSessionManager;
			mTsbSessionManager  =  nullptr;

			delete mMediaStreamContext;
			mMediaStreamContext = nullptr;

			delete mStreamAbstractionAAMP_MPD;
			mStreamAbstractionAAMP_MPD = nullptr;

			delete mPrivateInstanceAAMP;
			mPrivateInstanceAAMP = nullptr;

			delete g_mockAampConfig;
			g_mockAampConfig = nullptr;
		}

		// Create a set a dummy period due to the following parameter passed to EnqueueWrite(): context->GetPeriod()->GetId()
		void CreateAndSetDummyPeriod()
		{
			class DummyPeriod : public IPeriod
			{
			public:
				DummyPeriod() = default;
				virtual ~DummyPeriod() = default;

				// Implement all pure virtuals from IPeriod
				const std::string& GetId() const override { return mId; }
				const std::vector<IAdaptationSet*>& GetAdaptationSets() const override { return mAdaptationSets; }
				const std::string& GetStart() const override { return mStart; }
				const std::string& GetDuration() const override { return mDuration; }
				bool GetBitstreamSwitching() const override { return mBitstreamSwitching; }
				const std::vector<IBaseUrl *>& GetBaseURLs() const override { static std::vector<IBaseUrl *> vec; return vec; }
				ISegmentBase* GetSegmentBase() const override { return nullptr; }
				ISegmentList* GetSegmentList() const override { return nullptr; }
				ISegmentTemplate* GetSegmentTemplate() const override { return nullptr; }
				const std::vector<ISubset *>& GetSubsets() const override { static std::vector<ISubset *> vec; return vec; }
				const std::vector<IEventStream *>& GetEventStreams() const override { static std::vector<IEventStream *> vec; return vec; }
				const std::string& GetXlinkHref() const override { static std::string val; return val; }
				const std::vector<dash::xml::INode*> GetAdditionalSubNodes() const override { static std::vector<dash::xml::INode*> vec; return vec; }
				const std::string& GetXlinkActuate() const override { static std::string val; return val; }

				const std::map<std::string, std::string, std::less<std::string>, std::allocator<std::pair<const std::string, std::string>>> GetRawAttributes() const override
				{
					static std::map<std::string, std::string, std::less<std::string>, std::allocator<std::pair<const std::string, std::string>>> rawAttributes;
                    return rawAttributes;
                }

			private:
				std::string mId {"dummyPeriodId"};
				std::vector<IAdaptationSet*> mAdaptationSets;
				std::string mStart{"0.0"};
				std::string mDuration{"0.0"};
				bool mBitstreamSwitching{false};
			};
			mPeriod = new DummyPeriod();
		}

		void Initialize(bool lowlatency, bool chunk, bool tsb, bool eos, bool paused, bool underflow, bool init, float rate)
		{
			unsigned char data[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
			AampLLDashServiceData llDashData;
			llDashData.availabilityTimeOffset = 1.2;
			llDashData.lowLatencyMode = lowlatency;
			mPrivateInstanceAAMP->rate = rate;
			mPrivateInstanceAAMP->SetLLDashServiceData(llDashData);
			mPrivateInstanceAAMP->SetLocalAAMPTsb(tsb);
			mPrivateInstanceAAMP->mSinkPaused = paused;
			mPrivateInstanceAAMP->SetBufUnderFlowStatus(underflow);
			mMediaStreamContext = new MediaStreamContext(eTRACK_VIDEO, mStreamAbstractionAAMP_MPD, mPrivateInstanceAAMP, "SAMPLETEXT");
			mMediaStreamContext->mTempFragment.assign(std::begin(data), std::end(data));
			// The tests simulating EOS inject from the TSB, the rest of the tests inject from live
			mMediaStreamContext->SetLocalTSBInjection(eos);

			AampTSBSessionManager *tsbSessionManager = nullptr;
			if (tsb)
			{
				tsbSessionManager = mTsbSessionManager;
				CreateAndSetDummyPeriod();
				EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetPeriod()).WillOnce(Return(mPeriod));
			}
			if (eos)
			{
				mStreamAbstractionAAMP_MPD->mTuneType = eTUNETYPE_SEEKTOLIVE;
				EXPECT_CALL(*g_mockTSBSessionManager, GetTsbReader(_)).WillRepeatedly(Return(mTsbReader));
				EXPECT_CALL(*g_mockTSBReader, IsEos()).WillRepeatedly(Return(true));
				if (!paused || underflow)
				{
					EXPECT_CALL(*g_mockPrivateInstanceAAMP, UpdateLocalAAMPTsbInjection());
				}
			}
			EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBSessionManager()).WillOnce(Return(tsbSessionManager));
			EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetFile(_, _, _, _, _, _, _, _, _, _, _, _, _, _)).WillOnce(Return(true));
			EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled()).WillRepeatedly(Return(true));
			EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetLLDashChunkMode()).WillRepeatedly(Return(chunk));
			if(init)
			{
				EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _)).WillRepeatedly(Return(true));
				EXPECT_CALL(*g_mockIsoBmffBuffer, isInitSegment()).WillRepeatedly(Return(true));
				EXPECT_CALL(*g_mockIsoBmffBuffer, getTimeScale(_)).WillOnce(DoAll(SetArgReferee<0>(90000), Return(true)));
				EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetVidTimeScale(_)).Times(AtLeast(1));
			}
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
	AAMPLOG_INFO("Test with lowlatency: %d, chunk: %d, AAMP TSB: %d, eos: %d, paused: %d, underflow: %d, init: %d, rate: %f, expectedFragmentChunksCached: %d, expectedFragmentCached: %d",
		testParam.lowlatency,
		testParam.chunk,
		testParam.tsb,
		testParam.eos,
		testParam.paused,
		testParam.underflow,
		testParam.init,
		testParam.rate,
		testParam.expectedFragmentChunksCached,
		testParam.expectedFragmentCached);
	Initialize(testParam.lowlatency, testParam.chunk, testParam.tsb, testParam.eos, testParam.paused, testParam.underflow, testParam.init, testParam.rate);
	URIInfo uriInfo;
	uriInfo.url = "remoteUrl";
	URLBitrateMap urlList = { { 0, uriInfo } };
	mMediaStreamContext->mActiveDownloadInfo = std::make_shared<DownloadInfo>(eMEDIATYPE_VIDEO, eCURLINSTANCE_VIDEO, 10, 2, "", -1, 0, testParam.init, false, false, false, 0.0, 0, 1, 0, 0, urlList);
	bool retResult = mMediaStreamContext->CacheFragment("remoteUrl", 0, 10, 0, NULL, testParam.init, false, false, 0);
	if(retResult)
	{
		mMediaStreamContext->OnFragmentDownloadSuccess(mMediaStreamContext->mActiveDownloadInfo);
	}
	else
	{
		mMediaStreamContext->OnFragmentDownloadFailed(mMediaStreamContext->mActiveDownloadInfo);
	}

	if (testParam.eos && (!testParam.paused || testParam.underflow))
	{
		// Check that TSB injection flag is cleared after TSB Reader EoS if the
		// pipeline is not paused, or if paused due to underflow
		EXPECT_EQ(mMediaStreamContext->IsLocalTSBInjection(), false);
	}
	else
	{
		// Check that TSB injection flag is not modified in any other case (initially set to eos)
		EXPECT_EQ(mMediaStreamContext->IsLocalTSBInjection(), testParam.eos);
	}

	// Check expected results for fragment chunks cached and fragments cached
	EXPECT_EQ(mMediaStreamContext->numberOfFragmentChunksCached, testParam.expectedFragmentChunksCached);
	EXPECT_EQ(mMediaStreamContext->numberOfFragmentsCached, testParam.expectedFragmentCached);
}

INSTANTIATE_TEST_SUITE_P(
		MediaStreamContextTestSuite,
		MediaStreamContextTest,
		::testing::ValuesIn(testCases)
		);

// ---------------------------------------------------------------------------
// Underflow-recovery race: wasUnderFlowActive guard
// ---------------------------------------------------------------------------
// Declare the side-effect hook defined in FakeStreamAbstractionAamp.cpp.
extern std::function<void()> g_notifyVideoFragmentSideEffect;

/**
 * @brief Regression test for the wasUnderFlowActive guard in OnFragmentDownloadSuccess.
 *
 * Scenario (replicates the real race):
 *  1. Underflow is active (mBufUnderFlowStatus = true) when the snapshot is taken
 *     — wasUnderFlowActive is captured as true.
 *  2. NotifyVideoFragmentToUnderflowMonitor fires SetBufferingState(false), clearing
 *     mBufUnderFlowStatus to false and unpausing the pipeline.
 *  3. GStreamer's buffering(0) message fires on another thread, re-setting
 *     mSinkPaused = true before the discard check runs.
 *  4. At the discard check:
 *       isPipelinePaused         = true   (mSinkPaused re-set by buffering(0))
 *       GetBufUnderFlowStatus()  = false  (cleared by SetBufferingState(false))
 *       wasUnderFlowActive       = true   (captured before the notify)
 *
 * OLD behaviour (no guard):  condition = isPipelinePaused && !underflow
 *                           = true && true  → fragment DISCARDED  (bug)
 * NEW behaviour (with guard): condition = isPipelinePaused && !underflow && !wasUnderFlowActive
 *                           = true && true && false → fragment INJECTED (correct)
 *
 * The side-effect hook in NotifyVideoFragmentToUnderflowMonitor simulates steps 2–3
 * by clearing mBufUnderFlowStatus and setting mSinkPaused atomically within the
 * notify so the discard check sees the post-race state.
 */
TEST_F(MediaStreamContextTest, UnderflowRecoveryRace_FragmentInjectedWhenWasUnderFlowActive)
{
	// Arrange: TSB enabled (discard path only executes when tsbSessionManager != null),
	// pipeline initially NOT paused, underflow IS active.
	const bool lowlatency = false;
	const bool chunk      = false;
	const bool tsb        = true;
	const bool eos        = false;
	const bool paused     = false; // Will be set to true by the side effect below
	const bool underflow  = true;  // Active at snapshot → wasUnderFlowActive = true
	const bool init       = false;
	const float rate      = AAMP_NORMAL_PLAY_RATE;
	Initialize(lowlatency, chunk, tsb, eos, paused, underflow, init, rate);

	// Register the side effect that fires inside NotifyVideoFragmentToUnderflowMonitor.
	// This simulates:
	//  a) SetBufferingState(false) clearing mBufUnderFlowStatus
	//  b) GStreamer's buffering(0) re-setting mSinkPaused
	g_notifyVideoFragmentSideEffect = [this]()
	{
		mPrivateInstanceAAMP->ResetBufUnderFlowStatus(); // clears to false
		mPrivateInstanceAAMP->mSinkPaused.store(true);   // GStreamer buffering(0) race
	};

	URIInfo uriInfo;
	uriInfo.url = "remoteUrl";
	URLBitrateMap urlList = { { 0, uriInfo } };
	mMediaStreamContext->mActiveDownloadInfo = std::make_shared<DownloadInfo>(
		eMEDIATYPE_VIDEO, eCURLINSTANCE_VIDEO,
		/*absolutePosition=*/10.0, /*fragmentDurationSec=*/2.0,
		"", -1, 0,
		/*isInitSegment=*/false, /*isDiscontinuity=*/false,
		/*isPlayingAd=*/false, /*failoverContentSegment=*/false,
		/*pts=*/0.0, /*fragmentNumber=*/0, /*timeScale=*/1,
		/*bandwidth=*/0, /*ptsOffset=*/0, urlList);

	bool retResult = mMediaStreamContext->CacheFragment("remoteUrl", 0, 10, 0, nullptr, init, false, false, 0);
	if (retResult)
	{
		mMediaStreamContext->OnFragmentDownloadSuccess(mMediaStreamContext->mActiveDownloadInfo);
	}

	// Clear the side effect so it doesn't bleed into other tests.
	g_notifyVideoFragmentSideEffect = nullptr;

	// Assert: the recovery fragment must have been enqueued into the TSB chunk
	// cache (numberOfFragmentChunksCached == 1), not discarded.
	// Before the wasUnderFlowActive guard this would be 0 (fragment discarded).
	EXPECT_EQ(mMediaStreamContext->numberOfFragmentChunksCached, 1)
		<< "Recovery fragment must be injected when wasUnderFlowActive==true, "
		   "even if mSinkPaused is re-set by a GStreamer buffering(0) race.";
	EXPECT_EQ(mMediaStreamContext->numberOfFragmentsCached, 0);
}
