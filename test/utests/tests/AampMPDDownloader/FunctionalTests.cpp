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
#include "downloader/AampCurlDownloader.h"
#include "AampMPDDownloader.h"
#include "AampDefine.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "priv_aamp.h"
#include <thread>
#include <unistd.h>

using ::testing::_;
using ::testing::An;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgReferee;
using ::testing::WithParamInterface;

AampConfig *gpGlobalConfig{nullptr};


std::string url1 = "https://example.com/VideoTestStream/xyz.mpd";
std::string url2 = "http://example.com/Content/CMAF_S2-CTR-4s-v2/Live/channel(exampleChannel)/60_master_2hr.m3u8?c3.ri=example-ri&audio=all&subtitle=all&forcedNarrative=true";
std::string url3 = "https://example-livesim.org/livesim/Manifest.mpd";
std::string url4 = "https://example.com/GOLFD_HD_NAT_16403_0_example.mpd";

class FunctionalTests : public ::testing::Test
{
protected:
    AampMPDDownloader *mAampMPDDownloader = nullptr;
    std::string appName;
    ManifestDownloadConfigPtr mpdDnldCfg;
    ManifestDownloadResponsePtr dnldManifest;
    PrivateInstanceAAMP *mPrivateInstanceAAMP1{};
    void SetUp() override
    {
        mAampMPDDownloader = new AampMPDDownloader();
    }

    void TearDown() override
    {
        delete mAampMPDDownloader;
        mAampMPDDownloader = nullptr;
    }

public:
    int mLatencyValue;
};

TEST_F(FunctionalTests, AampMPDDownloader_PreInitTest_1)
{
    EXPECT_NO_THROW(mAampMPDDownloader->SetNetworkTimeout(5));
    EXPECT_NO_THROW(mAampMPDDownloader->SetStallTimeout(5));
    EXPECT_NO_THROW(mAampMPDDownloader->SetStartTimeout(5));
    EXPECT_NO_THROW(mAampMPDDownloader->Release());
    EXPECT_NO_THROW(mAampMPDDownloader->Start());
}

TEST_F(FunctionalTests, AampMPDDownloader_PreInitTest_2)
{
    // EXPECT_NO_THROW(mAampMPDDownloader->Initialize(nullptr));
    std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig>(-1);
    EXPECT_NO_THROW(mAampMPDDownloader->Initialize(inpData));
    EXPECT_NO_THROW(mAampMPDDownloader->Start());

    inpData->mTuneUrl = url1;
    inpData->mDnldConfig->bNeedDownloadMetrics = true;
    EXPECT_NO_THROW(mAampMPDDownloader->Initialize(inpData));
    EXPECT_NO_THROW(mAampMPDDownloader->Start());
    EXPECT_NO_THROW(mAampMPDDownloader->Release());
}

TEST_F(FunctionalTests, AampMPDDownloader_PreInitTest_3)
{
    // VOD Test
    std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig>(-1);
    inpData->mTuneUrl = url2;
    inpData->mDnldConfig->bNeedDownloadMetrics = true;
    EXPECT_NO_THROW(mAampMPDDownloader->Initialize(inpData));
    EXPECT_NO_THROW(mAampMPDDownloader->Start());
    EXPECT_NO_THROW(mAampMPDDownloader->Release());
}
// Commented below tests to avoid more wait duaration
#if 0
TEST_F(FunctionalTests, AampMPDDownloader_PreInitTest_4)
{
	ManifestDownloadResponsePtr respData = MakeSharedManifestDownloadResponsePtr();
	// Live Test
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig> (-1);
	inpData->mTuneUrl = url1;
	inpData->mDnldConfig->bNeedDownloadMetrics = true;
	EXPECT_NO_THROW(mAampMPDDownloader->Initialize(inpData));
	EXPECT_NO_THROW(mAampMPDDownloader->Start());
	sleep(20);
        AAMPStatusType errVal = AAMPStatusType::eAAMPSTATUS_OK;
        bool bWait = true;
        int iWaitDuration = 50;
        respData = mAampMPDDownloader->GetManifest(bWait, iWaitDuration);
        EXPECT_NE(respData->mMPDInstance, nullptr);
        EXPECT_NO_THROW(mAampMPDDownloader->Release());

}

TEST_F(FunctionalTests, AampMPDDownloader_PreInitTest_5)
{
	ManifestDownloadResponsePtr respData = MakeSharedManifestDownloadResponsePtr();
        // Live Test
        std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig> (-1);
        inpData->mTuneUrl = url3;
        inpData->mDnldConfig->bNeedDownloadMetrics = true;
        EXPECT_NO_THROW(mAampMPDDownloader->Initialize(inpData));
        EXPECT_NO_THROW(mAampMPDDownloader->Start());
        sleep(5);
        // Call GetManifest function
        AAMPStatusType errVal = AAMPStatusType::eAAMPSTATUS_OK;
        bool bWait = true;
        int iWaitDuration = 50;
        respData = mAampMPDDownloader->GetManifest(bWait, iWaitDuration);

        // Check if manifest is valid
        EXPECT_NE(respData->mMPDInstance, nullptr);

        EXPECT_NO_THROW(mAampMPDDownloader->Release());
}


TEST_F(FunctionalTests, AampMPDDownloader_PushDownloadDataToQueue)
{
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig> (-1);
	ManifestDownloadResponsePtr respData = nullptr;
	ManifestDownloadResponsePtr respData1 = nullptr;
	//1st mMPDData
	inpData->mTuneUrl = url4;
	inpData->mDnldConfig->bNeedDownloadMetrics = true;
        mAampMPDDownloader->Initialize(inpData);
        mAampMPDDownloader->Start();
	sleep(2);
	AAMPStatusType errVal = AAMPStatusType::eAAMPSTATUS_OK;
        bool bWait = true;
        int iWaitDuration = 50;
        respData = mAampMPDDownloader->GetManifest(bWait, iWaitDuration);
	printf("After First GetManifest\n");
        // Check if manifest is valid
        EXPECT_NE(respData->mMPDInstance, nullptr);

	iWaitDuration = 3000;
        respData1 = mAampMPDDownloader->GetManifest(bWait, iWaitDuration);

        // Check if manifest is valid
        //EXPECT_NE(respData->mMPDInstance, respData1->mMPDInstance);

	mAampMPDDownloader->Release();
}
#endif
TEST_F(FunctionalTests, InitializeWithValidConfig)
{
    EXPECT_NO_THROW(mAampMPDDownloader->Initialize(mpdDnldCfg, appName));
}

TEST_F(FunctionalTests, InitializeWithNullConfig)
{
    ManifestDownloadConfigPtr nullCfg = nullptr;
    mAampMPDDownloader->Initialize(nullCfg, appName);
}

TEST_F(FunctionalTests, SetBufferAvailabilityTest)
{
    int expectedLatency = 100;
    EXPECT_NO_THROW(mAampMPDDownloader->SetBufferAvailability(expectedLatency));
}

TEST_F(FunctionalTests, SetBufferAvailability)
{
    int durationMilliSec = 1000;
    mAampMPDDownloader->SetBufferAvailability(durationMilliSec);
}

TEST_F(FunctionalTests, GetManifestWhenNotReleased)
{
    ManifestDownloadResponsePtr response = mAampMPDDownloader->GetManifest(false, 1000, -1);
    ASSERT_EQ(response->mMPDStatus, AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR);
}

TEST_F(FunctionalTests, GetManifestWithHttpErrorSimulation)
{
    ManifestDownloadResponsePtr respPtr = mAampMPDDownloader->GetManifest(false, 1000, 404);
    ASSERT_EQ(respPtr->mMPDDownloadResponse->iHttpRetValue, 0);
}

TEST_F(FunctionalTests, GetManifestWithWaitTimeout)
{
    ManifestDownloadResponsePtr response = mAampMPDDownloader->GetManifest(true, 1000, -1);
    ASSERT_EQ(response->mMPDDownloadResponse->iHttpRetValue, 0);
}

TEST_F(FunctionalTests, IsMPDLowLatencyWithData)
{
    AampLLDashServiceData llDashData;
    llDashData.lowLatencyMode = true;
    AampLLDashServiceData resultLLDashData;
    bool retVal = mAampMPDDownloader->IsMPDLowLatency(resultLLDashData);
}

TEST_F(FunctionalTests, IsMPDLowLatencyWithoutData)
{
    AampLLDashServiceData resultLLDashData;
    bool retVal = mAampMPDDownloader->IsMPDLowLatency(resultLLDashData);
    ASSERT_FALSE(retVal);
    ASSERT_EQ(resultLLDashData.lowLatencyMode, false);
}

TEST_F(FunctionalTests, ParseMpdDocumentTest)
{
    _manifestDownloadResponse response;
    std::string sampleMpdString = "<MPD>...</MPD>";
    response.parseMpdDocument();
}

TEST_F(FunctionalTests, CloneTest)
{
    _manifestDownloadResponse originalResponse;
    std::shared_ptr<_manifestDownloadResponse> clonedResponse = originalResponse.clone();
}

TEST_F(FunctionalTests, ShowFunctionTest)
{
    _manifestDownloadResponse originalResponse;
    originalResponse.show();
}

TEST_F(FunctionalTests, UnRegisterCallbackTest)
{
    mAampMPDDownloader->UnRegisterCallback();
}

TEST_F(FunctionalTests, RegisterCallbackTest)
{
    ManifestUpdateCallbackFunc callback = NULL;
    void *callbackArg = NULL;
    EXPECT_NO_THROW(mAampMPDDownloader->RegisterCallback(callback, callbackArg));
}

TEST_F(FunctionalTests, SetStallTimeout1)
{
    EXPECT_NO_THROW(mAampMPDDownloader->SetStallTimeout(5));
}

TEST_F(FunctionalTests, ParseMpdDocumentTest1)
{
    ManifestDownloadResponsePtr manifestResponse = std::make_shared<_manifestDownloadResponse>();
    EXPECT_NO_THROW(manifestResponse->parseMpdDocument());
}

TEST_F(FunctionalTests, AampMPDDownloader_PreInitTest_6)
{
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig> (-1);
	inpData->mPreProcessedManifest = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"";
	if(!inpData->mPreProcessedManifest.empty())
	{
		EXPECT_NO_THROW(mAampMPDDownloader->Initialize(inpData, appName,std::bind(&PrivateInstanceAAMP::SendManifestPreProcessEvent, mPrivateInstanceAAMP1)));
	}
	else
	{
		EXPECT_NO_THROW(mAampMPDDownloader->Initialize(inpData));
	}
	EXPECT_NO_THROW(mAampMPDDownloader->Start());
	EXPECT_NO_THROW(mAampMPDDownloader->Release());
}

TEST_F(FunctionalTests, AampMPDDownloader_NotifyLockup)
{
    std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig>(-1);
    EXPECT_NO_THROW(mAampMPDDownloader->Initialize(inpData));
//    EXPECT_NO_THROW(mAampMPDDownloader->Start());
	
	EXPECT_NO_THROW(mAampMPDDownloader->RegisterCallback( [](void *arg){ ASSERT_TRUE(0); }, NULL));
	usleep(100000); // allow thread to start

	EXPECT_NO_THROW(mAampMPDDownloader->UnRegisterCallback());
	EXPECT_NO_THROW(mAampMPDDownloader->Release());
}
