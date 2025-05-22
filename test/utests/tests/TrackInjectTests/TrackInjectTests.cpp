
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
#include <gtest/gtest.h>
#include "MediaStreamContext.h"
#include "fragmentcollector_mpd.h"
#include "AampMemoryUtils.h"
#include "isobmff/isobmffbuffer.h"
#include "AampCacheHandler.h"
#include "../priv_aamp.h"
#include "AampDRMLicPreFetcherInterface.h"
// #include "AampConfig.h"
#include "MockAampConfig.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockMediaStreamContext.h"
#include "MockIsoBmffBuffer.h"

// #include "fragmentcollector_mpd.h"
#include "isobmff/isobmffprocessor.h"
#include "StreamAbstractionAAMP.h"
#include "AampLLDASHData.h"

using namespace testing;

AampConfig *gpGlobalConfig{nullptr};

class MediaTrackTest : public MediaTrack
{
public:
	std::string playlistURL;
	// StreamAbstractionAAMP* ctx; // Might need to define a dummy StreamAbstractionAAMP to for GetContext()
	MediaTrackTest(TrackType type, PrivateInstanceAAMP *aamp, const char *name) : MediaTrack(type, aamp, name)
	{
		playlistURL = "http://host/asset/low/manifest.mpd";
	}

	void ProcessPlaylist(AampGrowableBuffer &newPlaylist, int http_error)
	{
	}

	std::string &GetPlaylistUrl()
	{
		return playlistURL;
	}

	void SetEffectivePlaylistUrl(std::string url)
	{
		playlistURL = url;
	}

	std::string &GetEffectivePlaylistUrl()
	{
		return playlistURL;
	}

	long long GetLastPlaylistDownloadTime()
	{
		return 0;
	}

	long GetMinUpdateDuration()
	{
		return 2;
	}

	int GetDefaultDurationBetweenPlaylistUpdates()
	{
		return 0;
	}

	void SetLastPlaylistDownloadTime(long long time)
	{
		return;
	}
	void ABRProfileChanged(void) {}
	void updateSkipPoint(double position, double duration) {}

	void setDiscontinuityState(bool isDiscontinuity) {}

	void abortWaitForVideoPTS() {}

	double GetBufferedDuration(void) { return 0.0; }

	class StreamAbstractionAAMP *GetContext()
	{
		return aamp->mpStreamAbstractionAAMP;
	}

	void InjectFragmentInternal(CachedFragment *cachedFragment, bool &fragmentDiscarded, bool isDiscontinuity = false)
	{
		AAMPLOG_WARN("Type[%d] cachedFragment->position: %f cachedFragment->duration: %f cachedFragment->initFragment: %d",
					 type, cachedFragment->position, cachedFragment->duration, cachedFragment->initFragment);
		g_mockPrivateInstanceAAMP->SendStreamTransfer((AampMediaType)type, &cachedFragment->fragment, cachedFragment->position,
													  cachedFragment->position, cachedFragment->duration, 0.0, cachedFragment->initFragment, cachedFragment->discontinuity);
	}

	void fillCachedFragment(bool isInit, bool isDisc, bool isLLD)
	{
		unsigned char data[] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
		int fragmentIdxToFetch = 0;
		// int fragmentIdxToFetch = 0;
		CachedFragment *cachFragment = nullptr;
		if (isLLD)
		{
			cachFragment = &this->mCachedFragmentChunks[fragmentIdxToFetch];
			cachFragment->fragment.Clear();
		}
		else
		{
			// cachFragment = GetFetchBuffer(true);
			this->mCachedFragment = new CachedFragment[3];
			cachFragment = &this->mCachedFragment[fragmentIdxToFetch];
		}
		cachFragment->initFragment = isInit;
		cachFragment->discontinuity = isDisc;
		cachFragment->type = isInit ? eMEDIATYPE_INIT_VIDEO : eMEDIATYPE_VIDEO;
		cachFragment->fragment.AppendBytes(data, sizeof(data));
		if (isLLD)
		{
			UpdateTSAfterChunkFetch();
		}
		else
		{
			UpdateTSAfterFetch(false);
		}
	}
};

class TrackInjectTests : public testing::Test
{
public:
	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	MediaTrackTest *mMediaTrack;
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
			{eAAMPConfig_AudioOnlyPlayback, false},
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
			{eAAMPConfig_EnableABR, true},
			{eAAMPConfig_MPDDiscontinuityHandling, true},
			{eAAMPConfig_MPDDiscontinuityHandlingCdvr, true},
			{eAAMPConfig_ForceMultiPeriodDiscontinuity, false},
			{eAAMPConfig_SuppressDecode, false},
			{eAAMPConfig_InterruptHandling, false}};

	BoolConfigSettings mBoolConfigSettings;

	/** @brief Integer AAMP configuration settings. */
	const IntConfigSettings mDefaultIntConfigSettings =
		{
			{eAAMPConfig_ABRCacheLength, DEFAULT_ABR_CACHE_LENGTH},
			{eAAMPConfig_BufferHealthMonitorDelay, 1},
			{eAAMPConfig_BufferHealthMonitorInterval, 1},
			{eAAMPConfig_MaxABRNWBufferRampUp, AAMP_HIGH_BUFFER_BEFORE_RAMPUP},
			{eAAMPConfig_MinABRNWBufferRampDown, AAMP_LOW_BUFFER_BEFORE_RAMPDOWN},
			{eAAMPConfig_ABRNWConsistency, DEFAULT_ABR_NW_CONSISTENCY_CNT},
			{eAAMPConfig_RampDownLimit, -1},
			{eAAMPConfig_MaxFragmentCached, DEFAULT_CACHED_FRAGMENTS_PER_TRACK},
			{eAAMPConfig_PrePlayBufferCount, DEFAULT_PREBUFFER_COUNT},
			{eAAMPConfig_VODTrickPlayFPS, TRICKPLAY_VOD_PLAYBACK_FPS},
			{eAAMPConfig_ABRBufferCounter, DEFAULT_ABR_BUFFER_COUNTER},
			{eAAMPConfig_StallTimeoutMS, DEFAULT_STALL_DETECTION_TIMEOUT},
			{eAAMPConfig_MaxFragmentChunkCached, 20},
			{eAAMPConfig_DiscontinuityTimeout, 1}};

	IntConfigSettings mIntConfigSettings;

protected:
	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}
		g_mockIsoBmffBuffer = new MockIsoBmffBuffer();

		g_mockAampConfig = new NiceMock<MockAampConfig>();
		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();
		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		mBoolConfigSettings = mDefaultBoolConfigSettings;
		mIntConfigSettings = mDefaultIntConfigSettings;
	}

	void TearDown() override
	{
		delete g_mockMediaStreamContext;
		g_mockMediaStreamContext = nullptr;

		delete mMediaTrack;
		mMediaTrack = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockIsoBmffBuffer;
		g_mockIsoBmffBuffer = nullptr;
	}

public:
	void Initialize()
	{
		/* Setup configuration mock. */
		for (const auto &b : mBoolConfigSettings)
		{
			EXPECT_CALL(*g_mockAampConfig, IsConfigSet(b.first))
				.Times(AnyNumber())
				.WillRepeatedly(Return(b.second));
		}

		for (const auto &i : mIntConfigSettings)
		{
			EXPECT_CALL(*g_mockAampConfig, GetConfigValue(i.first))
				.Times(AnyNumber())
				.WillRepeatedly(Return(i.second));
		}

		mMediaTrack = new MediaTrackTest(eTRACK_VIDEO, mPrivateInstanceAAMP, "video");
		mMediaTrack->SetMonitorBufferDisabled(true);
		// mMediaTrack->SetMock(g_mockPrivateInstanceAAMP);
	}
};

TEST_F(TrackInjectTests, RunInjectLoopTestNonLLD)
{
	AampLLDashServiceData llDashData;
	llDashData.availabilityTimeOffset = 0.0;
	llDashData.lowLatencyMode = false;
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;

	this->mPrivateInstanceAAMP->SetLLDashServiceData(llDashData);
	// Initialize after mock has been setup
	Initialize();

	mMediaTrack->fillCachedFragment(false, false, llDashData.lowLatencyMode);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillOnce(Return(true))
		.WillOnce(Return(false));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(eMEDIATYPE_VIDEO, _, _, _, _, _, false, false));
	mMediaTrack->RunInjectLoop();
}

TEST_F(TrackInjectTests, RunInjectLoopTestNonLLDInit)
{
	AampLLDashServiceData llDashData;
	llDashData.availabilityTimeOffset = 0.0;
	llDashData.lowLatencyMode = false;
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;

	this->mPrivateInstanceAAMP->SetLLDashServiceData(llDashData);
	// Initialize after mock has been setup
	Initialize();

	mMediaTrack->fillCachedFragment(true, false, llDashData.lowLatencyMode);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillOnce(Return(true))
		.WillOnce(Return(false));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _, _, _, _, _, true, false));
	mMediaTrack->RunInjectLoop();
}

TEST_F(TrackInjectTests, RunInjectLoopTestLLD)
{
	AampLLDashServiceData llDashData;
	llDashData.availabilityTimeOffset = 2.0;
	llDashData.lowLatencyMode = true;
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	this->mPrivateInstanceAAMP->SetLLDashServiceData(llDashData);
	this->mPrivateInstanceAAMP->mpStreamAbstractionAAMP = new StreamAbstractionAAMP_MPD(this->mPrivateInstanceAAMP, 0, 1);
	this-> mPrivateInstanceAAMP->SetLLDashChunkMode(true);

	// Initialize after mock has been setup
	Initialize();

	mMediaTrack->fillCachedFragment(false, false, llDashData.lowLatencyMode);
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillOnce(Return(true))
		.WillOnce(Return(false));

	EXPECT_CALL(*g_mockIsoBmffBuffer, parseBuffer(_, _))
		.WillOnce(Return(true));

	char unParsedBuffer[] = "AAAAAAAAAAAAAAAAAA";
	int parsedBufferSize = 12, unParsedBufferSize = sizeof(unParsedBuffer);
	double pts = 10.0, duration = 0.48;
	EXPECT_CALL(*g_mockIsoBmffBuffer, ParseChunkData(_, _, _, _, _, _, _))
		.WillRepeatedly(DoAll(SetArgReferee<1>(unParsedBuffer),
							  SetArgReferee<3>(parsedBufferSize),
							  SetArgReferee<4>(unParsedBufferSize),
							  SetArgReferee<5>(pts),
							  SetArgReferee<6>(duration),
							  Return(true)));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetVidTimeScale())
		.WillRepeatedly(Return(1));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer((AampMediaType)eMEDIATYPE_VIDEO, _, pts, pts, duration, 0.0, false, false));
	mMediaTrack->RunInjectLoop();
}

TEST_F(TrackInjectTests, RunInjectLoopTestLLDInit)
{
	AampLLDashServiceData llDashData;
	llDashData.availabilityTimeOffset = 2.0;
	llDashData.lowLatencyMode = true;
	mPrivateInstanceAAMP->rate = AAMP_NORMAL_PLAY_RATE;
	this->mPrivateInstanceAAMP->SetLLDashServiceData(llDashData);
	this->mPrivateInstanceAAMP->mpStreamAbstractionAAMP = new StreamAbstractionAAMP_MPD(this->mPrivateInstanceAAMP, 0, 1);
	this->mPrivateInstanceAAMP->SetLLDashChunkMode(true);

	// Initialize after mock has been setup
	Initialize();

	mMediaTrack->fillCachedFragment(true, false, llDashData.lowLatencyMode);

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillOnce(Return(true))
		.WillOnce(Return(false));

	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendStreamTransfer(_, _, _, _, _, _, true, false));
	mMediaTrack->RunInjectLoop();
}
