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

std::string url1 = "https://bitmovin-a.akamaihd.net/content/MI201109210084_1/mpds/f08e80da-bf1d-4e3d-8899-f0f6155f6efa.mpd";
std::string url2 = "https://bitmovin-a.akamaihd.net/content/MI201109210084_1/mpds/../video/720_2400000/dash/segment_2.m4s";
std::string url3 = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main.mpd";
std::string url4 = "https://cpetestutility.stb.r53.xcal.tv/VideoTestStream/main1.mpd";
std::string url5 = "https://cpetestutility.stb.r53.xcal1.tv/VideoTestStream/main.mpd";
std::string url6 = "http://ccr.cdvr-rio-fre2-ncs.xcr.comcast.net/V5222022325268822195L200602220528140022S9133.mpd";
std::string url7 = "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/Sintel.mp4";

typedef std::shared_ptr<DownloadResponse> DownloadResponsePtr;
typedef std::shared_ptr<DownloadConfig> DownloadConfigPtr;

class FunctionalTests : public ::testing::Test
{
protected:    
	AampCurlDownloader *mAampCurlDownloader = nullptr;

	void SetUp() override
	{
		mLogObj = new AampLogManager();
		mAampCurlDownloader = new AampCurlDownloader();
	}

	void TearDown() override
	{
		delete mAampCurlDownloader;
		mAampCurlDownloader = nullptr;

		delete mLogObj;
		mLogObj=nullptr;
	}

public:

};


// Testing simple good case where no 4K streams are present.
TEST_F(FunctionalTests, AampCurlDownloader_PreDownloadTest_1)
{
	EXPECT_EQ(mAampCurlDownloader->IsDownloadActive(), false);
}

TEST_F(FunctionalTests, AampCurlDownloader_PreDownloadTest_2)
{
	std::string testStr;
        EXPECT_EQ(mAampCurlDownloader->GetDataString(testStr), 0);
}

TEST_F(FunctionalTests, AampCurlDownloader_PreDownloadTest_3)
{
        std::string testStr;
        EXPECT_EQ(mAampCurlDownloader->Download(testStr,nullptr), 0);
}

TEST_F(FunctionalTests, AampCurlDownloader_PreDownloadTest_4)
{
	CURL *handle = NULL;
	CURLINFO info;
	EXPECT_EQ(aamp_CurlEasyGetinfoInt(handle, info), 0);
	EXPECT_EQ(aamp_CurlEasyGetinfoDouble(handle, info), 0.0);
	EXPECT_EQ(aamp_CurlEasyGetinfoLong(handle, info), -1);
	EXPECT_EQ(aamp_CurlEasyGetinfoString(handle, info), nullptr);
}

TEST_F(FunctionalTests, AampCurlDownloader_InitializeTest_5)
{
	// Negative test , pass the null ptr
	mAampCurlDownloader->Initialize(nullptr);
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
	mAampCurlDownloader->Initialize(inpData);
	inpData->bIgnoreResponseHeader	= true;
	inpData->eRequestType = eCURL_DELETE;
	inpData->show();
	mAampCurlDownloader->Initialize(inpData);
	mAampCurlDownloader->Release();
	//2nd time call , check for any crash
	mAampCurlDownloader->Release();
	
	mAampCurlDownloader->Clear();
	//2nd time check
	mAampCurlDownloader->Clear();
}

TEST_F(FunctionalTests, AampCurlDownloader_DownloadTest_6)
{
	// Failure scenarios , without Initialize download APIs called
	std::string remoteUrl ;
	mAampCurlDownloader->Download(remoteUrl, nullptr);
	remoteUrl = url1;
	mAampCurlDownloader->Download(remoteUrl, nullptr);
	DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();
	mAampCurlDownloader->Download(remoteUrl, respData);
	
	// With Initialize
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
	inpData->bNeedDownloadMetrics = true;
	inpData->bIgnoreResponseHeader = true;
	mAampCurlDownloader->Initialize(inpData);
	mAampCurlDownloader->Download(remoteUrl, respData);
	EXPECT_EQ(0, respData->curlRetValue);
	EXPECT_EQ(200, respData->iHttpRetValue);
	respData->show();
	respData->clear();
	mAampCurlDownloader->Download(url2, respData);
	EXPECT_EQ(0, respData->curlRetValue);
	EXPECT_EQ(200, respData->iHttpRetValue);
	respData->show();
	respData->clear();
	mAampCurlDownloader->Download(url3, respData);
	if(respData->curlRetValue == 0)
	{
		EXPECT_EQ(200, respData->iHttpRetValue);
		respData->show();
		respData->clear();
	}
	mAampCurlDownloader->Download(url4, respData);
	EXPECT_EQ(0, respData->curlRetValue);
	EXPECT_EQ(404, respData->iHttpRetValue);
	respData->show();
	respData->clear();
	mAampCurlDownloader->Download(url5, respData);
	EXPECT_EQ(6, respData->curlRetValue);
	EXPECT_EQ(0, respData->iHttpRetValue);
	respData->show();
	respData->clear();
	
	inpData->bNeedDownloadMetrics = true;
	inpData->bIgnoreResponseHeader = false;
	inpData->iStallTimeout=0;
	inpData->iStartTimeout=0;
	inpData->iLowBWTimeout=0;
	mAampCurlDownloader->Initialize(inpData);
	mAampCurlDownloader->Download(url6, respData);
	EXPECT_EQ(0, respData->curlRetValue);
	EXPECT_EQ(200, respData->iHttpRetValue);
	respData->show();
	respData->clear();
}


void task1(void * instance,int usleepTime)
{
	AampCurlDownloader *mpinstance  =  (AampCurlDownloader *)instance;
	usleep(usleepTime);
	mpinstance->Release();
}

TEST_F(FunctionalTests, AampCurlDownloader_DownloadTest_7)
{

	
	DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();
	// With Initialize
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
	inpData->bNeedDownloadMetrics = true;
	inpData->bIgnoreResponseHeader = false;
	mAampCurlDownloader->Initialize(inpData);
        std::thread t1(task1, (void *)mAampCurlDownloader,500000);
        mAampCurlDownloader->Download(url7, respData);
        EXPECT_EQ(42, respData->curlRetValue);
        if((respData->downloadCompleteMetrics.dlSize == respData->mDownloadData.size()) && (respData->downloadCompleteMetrics.dlSize !=0))
                EXPECT_EQ(200, respData->iHttpRetValue);
        else
                EXPECT_EQ(0, respData->iHttpRetValue);
        respData->show();
        respData->clear();
        t1.join();
}

TEST_F(FunctionalTests, AampCurlDownloader_DownloadTest_8) {
    DownloadResponsePtr respData = std::make_shared<DownloadResponse>();
    DownloadConfigPtr inpData = std::make_shared<DownloadConfig>();
    inpData->bNeedDownloadMetrics = true;
    inpData->bIgnoreResponseHeader = false;

    //Check for timeout config values
    EXPECT_EQ(2,inpData->iStallTimeout);
    EXPECT_EQ(2,inpData->iStartTimeout);
    EXPECT_EQ(2,inpData->iLowBWTimeout);
    inpData->show();

    //Start Timeout case
    mAampCurlDownloader->Initialize(inpData);
    std::thread t1(task1, (void*)mAampCurlDownloader,3000000);
    mAampCurlDownloader->Download(url7, respData);
    EXPECT_EQ(42, respData->curlRetValue);
    if((respData->downloadCompleteMetrics.dlSize == respData->mDownloadData.size()) && (respData->downloadCompleteMetrics.dlSize !=0))
                EXPECT_EQ(200, respData->iHttpRetValue);
     else
                EXPECT_EQ(0, respData->iHttpRetValue);
    EXPECT_EQ(eCURL_ABORT_REASON_NONE ,respData->mAbortReason);
    respData->show();
    respData->clear();
    t1.join();

}
// Test for stall timeout
TEST_F(FunctionalTests, AampCurlDownloader_DownloadTest_9) {
    DownloadResponsePtr respData = std::make_shared<DownloadResponse>();
    DownloadConfigPtr inpData = std::make_shared<DownloadConfig>();
    inpData->bNeedDownloadMetrics = true;
    inpData->bIgnoreResponseHeader = false;
    inpData->iStartTimeout = 2;  // Set start timeout to 2 seconds
    inpData->iStallTimeout = 3;  // Set stall timeout to 3 seconds

    mAampCurlDownloader->Initialize(inpData);
    std::thread t1([&]() {
        // Wait for 1 second before starting the download
       std::this_thread::sleep_for(std::chrono::seconds(1));
        mAampCurlDownloader->Download(url2, respData);
	respData->show();
    });

    // Wait for 3 seconds and check if download has stalled
    std::this_thread::sleep_for(std::chrono::seconds(3));
    EXPECT_FALSE(mAampCurlDownloader->IsDownloadActive());

    t1.join();
}
TEST_F(FunctionalTests, PerformanceTest) {

    DownloadResponsePtr respData = std::make_shared<DownloadResponse>();
    DownloadConfigPtr inpData = std::make_shared<DownloadConfig>();
    inpData->bNeedDownloadMetrics = true;
    inpData->bIgnoreResponseHeader = false;

    mAampCurlDownloader->Initialize(inpData);

    int iterations = 20;

    // To calculate average time
    double total_time = 0;
    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        mAampCurlDownloader->Download(url2, respData);
	respData->show();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        total_time += elapsed_seconds.count();
        EXPECT_EQ(200, respData->iHttpRetValue);
    }

}
