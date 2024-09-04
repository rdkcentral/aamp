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
#include "MockAampConfig.h"
#include "MockAampGstPlayer.h"
#include "MockStreamAbstractionAAMP_MPD.h"
#include "MockTSBSessionManager.h"

using ::testing::_;
using ::testing::WithParamInterface;
using ::testing::Return;
using ::testing::NiceMock;

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

		g_mockTSBSessionManager = new MockTSBSessionManager(mLogObj, mPrivateInstanceAAMP);

        g_mockAampGstPlayer = new MockAAMPGstPlayer( mPrivateInstanceAAMP);
        g_mockStreamAbstractionAAMP_MPD = new NiceMock<MockStreamAbstractionAAMP_MPD>(mLogObj, mPrivateInstanceAAMP, 0, 0);

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
	mPrivateInstanceAAMP->Tune(lldUrl, false);

	EXPECT_EQ(mPrivateInstanceAAMP->mNetworkTimeoutMs, networkTimeout * 1000);
	EXPECT_EQ(mPrivateInstanceAAMP->mManifestTimeoutMs, networkTimeout * 1000);
	EXPECT_EQ(mPrivateInstanceAAMP->mPlaylistTimeoutMs, networkTimeout * 1000);
}

TEST_F(LocalTSBTests, Chunked_With_LLD_And_Config_On)
{
	// Chunked key word should trigger TSBSessionManager creation
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_LocalTSBEnabled)).WillOnce(Return(true));

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
	mPrivateInstanceAAMP->Tune(chunkedUrl, true);
	EXPECT_TRUE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
	EXPECT_TRUE(mPrivateInstanceAAMP->IsLocalAAMPTsbInjection());
}

TEST_F(LocalTSBTests, Chunked_With_LLD_And_Config_Off)
{
	// All configs are turned off
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));
	// EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_LocalTSBEnabled)).WillOnce(Return(true));

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
	mPrivateInstanceAAMP->Tune(chunkedUrl, true);
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsbInjection());
}

TEST_F(LocalTSBTests, No_Chunked_With_LLD)
{
	// Chunked key word should trigger TSBSessionManager creation
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_LocalTSBEnabled)).Times(0);

	EXPECT_CALL(*g_mockTSBSessionManager, Init()).Times(0);
	const char *chunkedUrl = "http://localhost:80/manifest.mpd";

	// For low latency stream case
	AampLLDashServiceData llData;
	llData.lowLatencyMode = true;
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this, &llData] {
					this->mPrivateInstanceAAMP->SetLLDashServiceData(llData);
					return eAAMPSTATUS_OK;
				});
	mPrivateInstanceAAMP->Tune(chunkedUrl, true);
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsbInjection());
}

TEST_F(LocalTSBTests, Chunked_Without_LLD_And_Config_On)
{
	// Chunked key word should trigger TSBSessionManager creation
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(_)).WillRepeatedly(Return(false));
	EXPECT_CALL(*g_mockAampConfig, IsConfigSet(eAAMPConfig_LocalTSBEnabled)).WillOnce(Return(true));

	EXPECT_CALL(*g_mockTSBSessionManager, Init()).Times(1);
	const char *chunkedUrl = "http://localhost:80/low/manifest.mpd";

	// For non low latency stream case, by default mLowLatencyMode is false
	AampLLDashServiceData llData;
	EXPECT_CALL(*g_mockStreamAbstractionAAMP_MPD, Init(_))
		.WillOnce([this, &llData] {
					this->mPrivateInstanceAAMP->SetLLDashServiceData(llData);
					return eAAMPSTATUS_OK;
				});
	mPrivateInstanceAAMP->Tune(chunkedUrl, true);
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsb());
	EXPECT_FALSE(mPrivateInstanceAAMP->IsLocalAAMPTsbInjection());
}
















