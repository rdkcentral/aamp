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
#include <thread>
#include <unistd.h>

using ::testing::_;
using ::testing::WithParamInterface;
using ::testing::An;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using ::testing::Invoke;
using ::testing::Return;

AampConfig *gpGlobalConfig{nullptr};
AampLogManager *mLogObj{nullptr};

std::string url1 = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd";
std::string url2 = "http://g004-sle-us-cmaf-stg-cf.cdn.peacocktv.com/Content/CMAF_S2-CTR-4s-v2/Live/channel(UHDtoHDR10SLE3202382fd53b0ade)/60_master_2hr.m3u8?c3.ri=5002728478659375252&audio=all&subtitle=all&forcedNarrative=true";
std::string url3 = "https://livesim.dashif.org/livesim/scte35_2/testpic_2s/Manifest.mpd" ;
std::string url4 = "https://cdn-ec-pan-011.linear-nat-pil-hd.xcr.comcast.net/GOLFD_HD_NAT_16403_0_6257457287223774163.mpd";
class FunctionalTests : public ::testing::Test
{
protected:
	AampMPDDownloader *mAampMPDDownloader = nullptr;

	void SetUp() override
	{
		mLogObj = new AampLogManager();
		mAampMPDDownloader = new AampMPDDownloader();
	}

	void TearDown() override
	{
		delete mAampMPDDownloader;
		mAampMPDDownloader = nullptr;

		delete mLogObj;
		mLogObj=nullptr;
	}
public:

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
	//EXPECT_NO_THROW(mAampMPDDownloader->Initialize(nullptr));
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig> ();
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
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig> ();
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
 	std::shared_ptr<ManifestDownloadResponse> respData = std::make_shared<ManifestDownloadResponse> ();
	// Live Test
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig> ();
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
	std::shared_ptr<ManifestDownloadResponse> respData = std::make_shared<ManifestDownloadResponse> ();
        // Live Test
        std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig> ();
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
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig> ();
	std::shared_ptr<ManifestDownloadResponse> respData = nullptr;
	std::shared_ptr<ManifestDownloadResponse> respData1 = nullptr;
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
