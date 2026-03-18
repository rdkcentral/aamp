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
#include <gmock/gmock.h>

#include "priv_aamp.h"

#include "AampConfig.h"
#include "AampScheduler.h"
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP_MPD.h"
#include "MockTSBSessionManager.h"
#include "MockAampScheduler.h"

using ::testing::_;
using ::testing::WithParamInterface;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::Invoke;

class LocalTSBTests : public ::testing::Test
{
protected:
	PrivateInstanceAAMP *mPrivateInstanceAAMP{};

	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);

		g_mockAampConfig = new NiceMock<MockAampConfig>();

		g_mockTSBSessionManager = new MockTSBSessionManager(mPrivateInstanceAAMP);

        g_mockAampGstPlayer = new MockAAMPGstPlayer( mPrivateInstanceAAMP);
        g_mockStreamAbstractionAAMP_MPD = new NiceMock<MockStreamAbstractionAAMP_MPD>(mPrivateInstanceAAMP, 0, 0);

        //mPrivateInstanceAAMP->mStreamSink = g_mockAampGstPlayer; //TODO fix
    }

	void TearDown() override
	{
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete g_mockStreamAbstractionAAMP_MPD;
		g_mockStreamAbstractionAAMP_MPD = nullptr;

		delete g_mockTSBSessionManager;
		g_mockTSBSessionManager = nullptr;

		delete g_mockAampGstPlayer;
		g_mockAampGstPlayer = nullptr;

		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;
	}
};

TEST_F(LocalTSBTests, TimeOut_Config_Based_On_Network)
{
	float networkTimeout = 2.0;
	float manifestTimeout = CURL_FRAGMENT_DL_TIMEOUT;
	float playlistTimeout = 0;
	// By default fake will return AAMP_DEFAULT_SETTING
	// EXPECT_CALL(*g_mockAampConfig, GetConfigOwner(_)).WillRepeatedly(Return(AAMP_DEFAULT_SETTING));

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_PlaybackOffset)).WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_LiveOffset)).WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_LiveOffsetDriftCorrectionInterval)).WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_LiveOffset4K)).WillRepeatedly(Return(0.0));

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_NetworkTimeout)).WillRepeatedly(Return(networkTimeout));
	// Return the current value of the local variable
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_ManifestTimeout)).WillRepeatedly([&manifestTimeout] { return manifestTimeout; });
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_PlaylistTimeout)).WillRepeatedly([&playlistTimeout] { return playlistTimeout; });

	EXPECT_CALL(*g_mockAampConfig, SetConfigValue(eAAMPConfig_ManifestTimeout, networkTimeout)).WillOnce([&manifestTimeout, networkTimeout] { manifestTimeout = networkTimeout; });
	EXPECT_CALL(*g_mockAampConfig, SetConfigValue(eAAMPConfig_PlaylistTimeout, networkTimeout)).WillOnce([&playlistTimeout, networkTimeout] { playlistTimeout = networkTimeout; });


	const char *lldUrl = "http://localhost:80/test/manifest.mpd";
	mPrivateInstanceAAMP->Tune(lldUrl, false, "LINEAR_TV");

	EXPECT_EQ(mPrivateInstanceAAMP->mNetworkTimeoutMs, networkTimeout * 1000);
	EXPECT_EQ(mPrivateInstanceAAMP->mManifestTimeoutMs, networkTimeout * 1000);
	EXPECT_EQ(mPrivateInstanceAAMP->mPlaylistTimeoutMs, networkTimeout * 1000);
}

TEST_F(LocalTSBTests, Chunked_With_LLD_And_Config_On)
{
	// Chunked key word should trigger TSBSessionManager creation
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_LocalTSBEnabled)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp)).WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingInt>(_))).WillRepeatedly(Return(0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingFloat>(_))).WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingString>(_)))
		.WillRepeatedly(Return(""));

	EXPECT_CALL(*g_mockTSBSessionManager, Init()).Times(1);
	const char *chunkedUrl = "http://localhost:80/low/manifest.mpd";

	// For low latency stream case
	AampLLDashServiceData llData;
	llData.lowLatencyMode = true;
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this, &llData] {
					this->mPrivateInstanceAAMP->SetLLDashServiceData(llData);
					return eAAMPSTATUS_OK;
				});
	mPrivateInstanceAAMP->Tune(chunkedUrl, true, "LINEAR_TV");
	EXPECT_TRUE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
	// IsLocalAAMPTsbInjection() is true when Local AAMP TSB is enabled and playing from TSB (not live)
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsbInjection());
}

TEST_F(LocalTSBTests, Config_On_And_PtsReStamp_Off)
{
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_LocalTSBEnabled)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp)).WillRepeatedly(Return(false));

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingInt>(_))).WillRepeatedly(Return(0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingFloat>(_))).WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingString>(_)))
		.WillRepeatedly(Return(""));

	EXPECT_CALL(*g_mockTSBSessionManager, Init()).Times(0);
	const char *url = "http://localhost:80/manifest.mpd";

	mPrivateInstanceAAMP->Tune(url, true, "LINEAR_TV");
	// IsLocalAAMPTsb() is true if PTS Restamp is enabled and local TSB is enabled in configuration.
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
	// IsLocalAAMPTsbInjection() is true when Local AAMP TSB is enabled and playing from TSB (not live)
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsbInjection());
}

TEST_F(LocalTSBTests, SLD_And_Config_On)
{
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_LocalTSBEnabled)).WillOnce(Return(true));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnablePTSReStamp)).WillRepeatedly(Return(true));

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingInt>(_))).WillRepeatedly(Return(0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingFloat>(_))).WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingString>(_)))
		.WillRepeatedly(Return(""));

	EXPECT_CALL(*g_mockTSBSessionManager, Init()).Times(1);
	const char *url = "http://localhost:80/manifest.mpd";

	// For standard latency stream case
	AampLLDashServiceData llData;
	llData.lowLatencyMode = false;
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this, &llData] {
					this->mPrivateInstanceAAMP->SetLLDashServiceData(llData);
					return eAAMPSTATUS_OK;
				});
	mPrivateInstanceAAMP->Tune(url, true, "LINEAR_TV");
	EXPECT_TRUE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
	// IsLocalAAMPTsbInjection() is true when Local AAMP TSB is enabled and playing from TSB (not live)
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsbInjection());
}

TEST_F(LocalTSBTests, VOD_And_Config_On)
{
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingInt>(_))).WillRepeatedly(Return(0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingFloat>(_))).WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingString>(_)))
		.WillRepeatedly(Return(""));

	const char *url = "http://localhost:80/manifest.mpd";
	mPrivateInstanceAAMP->Tune(url, true, "VOD");
	// IsLocalAAMPTsb() is false for VOD content
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsbInjection());
}

TEST_F(LocalTSBTests, Chunked_With_LLD_And_Config_Off)
{
	// All configs are turned off
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_LocalTSBEnabled)).WillOnce(Return(false));

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingInt>(_))).WillRepeatedly(Return(0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingFloat>(_))).WillRepeatedly(Return(0.0));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(testing::Matcher<AAMPConfigSettingString>(_)))
		.WillRepeatedly(Return(""));

	// Not expecting creation of TSBSessionManager when config off
	EXPECT_CALL(*g_mockTSBSessionManager, Init()).Times(0);
	const char *chunkedUrl = "http://localhost:80/low/manifest.mpd";

	// For low latency stream case
	AampLLDashServiceData llData;
	llData.lowLatencyMode = true;
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this, &llData] {
					this->mPrivateInstanceAAMP->SetLLDashServiceData(llData);
					return eAAMPSTATUS_OK;
				});
	mPrivateInstanceAAMP->Tune(chunkedUrl, true, "LINEAR_TV");
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsbInjection());
}

TEST_F(LocalTSBTests, Chunked_Without_LLD_And_Config_On)
{
	// Chunked key word should trigger TSBSessionManager creation
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));

	//We cannot expect TSBSessionManager Init ,if keyword is not present 
//	EXPECT_CALL(*g_mockTSBSessionManager, Init()).Times(1);
	const char *chunkedUrl = "http://localhost:80/low/manifest.mpd";

	// For non low latency stream case, by default mLowLatencyMode is false
	AampLLDashServiceData llData;
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this, &llData] {
					this->mPrivateInstanceAAMP->SetLLDashServiceData(llData);
					return eAAMPSTATUS_OK;
				});
	mPrivateInstanceAAMP->Tune(chunkedUrl, true, "LINEAR_TV");
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsbInjection());
}

TEST_F(LocalTSBTests, VOD_With_Config_On)
{
	// We expect TSB session to be created and used if Live, DASH, and TSB is enabled.
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockTSBSessionManager, Init()).Times(0);
	// For non low latency stream case, by default mLowLatencyMode is false
	AampLLDashServiceData llData;
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this, &llData] {
					this->mPrivateInstanceAAMP->SetLLDashServiceData(llData);
					return eAAMPSTATUS_OK;
				});

	const char *Url = "http://localhost:80/low/manifest.mpd";
	mPrivateInstanceAAMP->Tune(Url, true, "VOD");

	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsbInjection());
}

TEST_F(LocalTSBTests, IncreaseGSTBufferTest_1)
{
	const char *testUrl = "http://localhost:80/manifest.mpd";
	AampLLDashServiceData llData;
	llData.lowLatencyMode = true;
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this, &llData] {
					this->mPrivateInstanceAAMP->SetLLDashServiceData(llData);
					return eAAMPSTATUS_OK;
				});
	mPrivateInstanceAAMP->Tune(testUrl, true, "LINEAR_TV");
    #define GETCFG(x) mPrivateInstanceAAMP->mConfig->GetConfigValue(x)
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_BWToGstBufferFactor)).WillRepeatedly(Return(0.8));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_GstVideoBufBytes)).WillRepeatedly(Return(GST_VIDEOBUFFER_SIZE_BYTES));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetMaxBitrate()).WillOnce(Return(8000000)); //8 Mbps
	int newBuffer = 8000000 * 0.8;
	EXPECT_CALL(*g_mockAampConfig, SetConfigValue(eAAMPConfig_GstVideoBufBytes,newBuffer)).Times(0);	//UTest addressed, With CONTENT_4K_SUPPORTED is set default, newBuffer < minVideoBuffer, hence the buffer config shouldnt be modified.
	mPrivateInstanceAAMP->IncreaseGSTBufferSize();

	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetMaxBitrate()).WillOnce(Return(13000000)); //13 Mbps
	newBuffer = 13000000 * 0.8;
	EXPECT_CALL(*g_mockAampConfig, SetConfigValue(eAAMPConfig_GstVideoBufBytes,newBuffer)).Times(0);
	mPrivateInstanceAAMP->IncreaseGSTBufferSize();

	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetMaxBitrate()).WillOnce(Return(18000000)); // 18 Mbps
	newBuffer = 18000000 * 0.8;
	EXPECT_CALL(*g_mockAampConfig, SetConfigValue(eAAMPConfig_GstVideoBufBytes,newBuffer)).Times(0);
	mPrivateInstanceAAMP->IncreaseGSTBufferSize();

	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetMaxBitrate()).WillOnce(Return(30000000)); //30 Mbps
	newBuffer = 30000000 * 0.8;
	EXPECT_CALL(*g_mockAampConfig, SetConfigValue(eAAMPConfig_GstVideoBufBytes,newBuffer));
	mPrivateInstanceAAMP->IncreaseGSTBufferSize();

	//GST_VIDEOBUFFER_SIZE_MAX_BYTES
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetMaxBitrate()).WillOnce(Return(100000000)); //100 Mbps should top out to 25 Mb
	newBuffer = GST_VIDEOBUFFER_SIZE_MAX_BYTES;
	EXPECT_CALL(*g_mockAampConfig, SetConfigValue(eAAMPConfig_GstVideoBufBytes,newBuffer));
	mPrivateInstanceAAMP->IncreaseGSTBufferSize();
}

TEST_F(LocalTSBTests, IncreaseGSTBufferTest_2)
{
	const char *testUrl = "http://localhost:80/manifest.mpd";
	AampLLDashServiceData llData;
	llData.lowLatencyMode = true;
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this, &llData] {
					this->mPrivateInstanceAAMP->SetLLDashServiceData(llData);
					return eAAMPSTATUS_OK;
				});
	mPrivateInstanceAAMP->Tune(testUrl, true, "LINEAR_TV");
    #define GETCFG(x) mPrivateInstanceAAMP->mConfig->GetConfigValue(x)
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_BWToGstBufferFactor)).WillRepeatedly(Return(0.8));
	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_GstVideoBufBytes)).WillRepeatedly(Return(GST_VIDEOBUFFER_SIZE_BYTES));
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, GetMaxBitrate()).WillOnce(Return(1000000)); //1 Mbps - Negative case should default and change config value
	int newBuffer = GST_VIDEOBUFFER_SIZE_BYTES;
	EXPECT_CALL(*g_mockAampConfig, SetConfigValue(eAAMPConfig_GstVideoBufBytes,0)).Times(0); // Should not be called
	mPrivateInstanceAAMP->IncreaseGSTBufferSize();
}

TEST_F(LocalTSBTests, ScheduleRetuneTest)
{
	//initializing scheduler for single test
	g_mockAampScheduler = new NiceMock<MockAampScheduler>();
	AampScheduler mScheduler{};
	mPrivateInstanceAAMP->SetScheduler(&mScheduler);

	std::string taskName;

	const char *testUrl = "http://localhost:80/manifest.mpd";
	AampLLDashServiceData llData;
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this, &llData] {
					this->mPrivateInstanceAAMP->SetLLDashServiceData(llData);
					return eAAMPSTATUS_OK;
				});

	mPrivateInstanceAAMP->Tune(testUrl, true, "LINEAR_TV");
	mPrivateInstanceAAMP->SetState(eSTATE_PLAYING);
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_EnableCurlStore)).WillRepeatedly(Return(true));// uninteresting expect call

	//trickplay case
	mPrivateInstanceAAMP->rate=-30;
	EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_))
				.WillOnce(Invoke([&taskName](AsyncTaskObj obj) -> int {
					taskName = obj.mTaskName;
					return 1;
				}));
	mPrivateInstanceAAMP->ScheduleRetune(eGST_ERROR_GST_PIPELINE_INTERNAL, eMEDIATYPE_VIDEO);
	EXPECT_EQ(taskName, "PrivateInstanceAAMP_Retune");

	//paused state
	mPrivateInstanceAAMP->rate=0;
	EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0); // should not be called
	mPrivateInstanceAAMP->ScheduleRetune(eGST_ERROR_GST_PIPELINE_INTERNAL, eMEDIATYPE_VIDEO);

	//trickplay but not internal pipeline error
	mPrivateInstanceAAMP->rate=2;
	EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);
	mPrivateInstanceAAMP->ScheduleRetune(eGST_ERROR_UNDERFLOW, eMEDIATYPE_VIDEO);

	//mContentType == ContentType_EAS -> should not trigger retune
	mPrivateInstanceAAMP->rate=4;
	mPrivateInstanceAAMP->SetContentType("EAS");
	EXPECT_CALL(*g_mockAampScheduler, ScheduleTask(_)).Times(0);
	mPrivateInstanceAAMP->ScheduleRetune(eGST_ERROR_GST_PIPELINE_INTERNAL, eMEDIATYPE_VIDEO);

	delete g_mockAampScheduler;
	g_mockAampScheduler = nullptr;
}













