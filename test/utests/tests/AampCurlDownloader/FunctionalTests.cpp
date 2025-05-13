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
#include "AampCurlDownloader.h"

#include "AampConfig.h"
#include "AampLogManager.h"
#include "MockCurl.h"
#include <thread>
#include <unistd.h>

using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::NotNull;
using ::testing::SetArgPointee;
using ::testing::SaveArgPointee;
using ::testing::Return;

AampConfig *gpGlobalConfig{nullptr};

typedef std::shared_ptr<DownloadResponse> DownloadResponsePtr;
typedef std::shared_ptr<DownloadConfig> DownloadConfigPtr;

class FunctionalTests : public ::testing::Test
{
protected:
	AampCurlDownloader *mAampCurlDownloader = nullptr;
	CURL *mCurlEasyHandle = nullptr;
	curl_progress_callback_t mCurlProgressCallback = nullptr;
	curl_write_func_t mCurlWriteFunc = nullptr;
	std::string mUrl = "https://some.server/manifest.mpd";

	void SetUp() override
	{
		mAampCurlDownloader = new AampCurlDownloader();
		g_mockCurl = new MockCurl();

		mCurlEasyHandle = malloc(1);		// use a valid address for the handle
	}

	void TearDown() override
	{
		delete mAampCurlDownloader;
		mAampCurlDownloader = nullptr;

		free(mCurlEasyHandle);

		delete g_mockCurl;
		g_mockCurl = nullptr;
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
	EXPECT_EQ(aamp_CurlEasyGetinfoDouble(handle, info), 0.0);
	EXPECT_EQ(aamp_CurlEasyGetinfoLong(handle, info), -1);
	EXPECT_EQ(aamp_CurlEasyGetinfoString(handle, info), nullptr);
}

TEST_F(FunctionalTests, AampCurlDownloader_InitializeTest_5)
{
	// Negative test , pass the null ptr
	mAampCurlDownloader->Initialize(nullptr);
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
	EXPECT_CALL(*g_mockCurl, curl_easy_init()).WillOnce(Return(mCurlEasyHandle));
	/* The curl easy handle will be cleaned when AampCurlDownloader is destroyed. */
	EXPECT_CALL(*g_mockCurl, curl_easy_cleanup(mCurlEasyHandle));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_PROGRESSDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_xferinfo(mCurlEasyHandle, CURLOPT_XFERINFOFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlProgressCallback), Return(CURLE_OK)));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_WRITEDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_write(mCurlEasyHandle, CURLOPT_WRITEFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlWriteFunc), Return(CURLE_OK)));
	mAampCurlDownloader->Initialize(inpData);
	inpData->bIgnoreResponseHeader	= true;
	inpData->eRequestType = eCURL_DELETE;
	inpData->show();
	/* Assert if the callbacks are null to avoid a crash if they are called by the test. */
	ASSERT_NE(mCurlProgressCallback, nullptr);
	ASSERT_NE(mCurlWriteFunc, nullptr);

	/* When Initialize is called a second time, the progress callback function is set in curl again. */
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_PROGRESSDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_xferinfo(mCurlEasyHandle, CURLOPT_XFERINFOFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlProgressCallback), Return(CURLE_OK)));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_WRITEDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_write(mCurlEasyHandle, CURLOPT_WRITEFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlWriteFunc), Return(CURLE_OK)));
	mAampCurlDownloader->Initialize(inpData);
	/* Assert if the callbacks are null to avoid a crash if they are called by the test. */
	ASSERT_NE(mCurlProgressCallback, nullptr);
	ASSERT_NE(mCurlWriteFunc, nullptr);

	mAampCurlDownloader->Release();
	//2nd time call , check for any crash
	mAampCurlDownloader->Release();

	mAampCurlDownloader->Clear();
	//2nd time check
	mAampCurlDownloader->Clear();
}

TEST_F(FunctionalTests, AampCurlDownloader_DownloadTest_NoInitialize)
{
	// Failure scenarios , without Initialize download APIs called
	std::string remoteUrl ;
	mAampCurlDownloader->Download(remoteUrl, nullptr);
	remoteUrl = mUrl;
	mAampCurlDownloader->Download(remoteUrl, nullptr);
	DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();
	mAampCurlDownloader->Download(remoteUrl, respData);
}

TEST_F(FunctionalTests, AampCurlDownloader_DownloadTest_6)
{
	DownloadResponsePtr respData = std::make_shared<DownloadResponse> ();
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig> ();
	inpData->bNeedDownloadMetrics = true;
	inpData->bIgnoreResponseHeader = true;
	EXPECT_CALL(*g_mockCurl, curl_easy_init()).WillOnce(Return(mCurlEasyHandle));
	/* The curl easy handle will be cleaned when AampCurlDownloader is destroyed. */
	EXPECT_CALL(*g_mockCurl, curl_easy_cleanup(mCurlEasyHandle));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_PROGRESSDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_xferinfo(mCurlEasyHandle, CURLOPT_XFERINFOFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlProgressCallback), Return(CURLE_OK)));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_WRITEDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_write(mCurlEasyHandle, CURLOPT_WRITEFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlWriteFunc), Return(CURLE_OK)));
	mAampCurlDownloader->Initialize(inpData);
	/* Assert if the callbacks are null to avoid a crash if they are called by the test. */
	ASSERT_NE(mCurlProgressCallback, nullptr);
	ASSERT_NE(mCurlWriteFunc, nullptr);

	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_str(mCurlEasyHandle, CURLOPT_URL, mUrl.c_str()))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_perform(mCurlEasyHandle)).WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_getinfo_int(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, NotNull()))
		.WillOnce(DoAll(SetArgPointee<2>(200), Return(CURLE_OK)));
	mAampCurlDownloader->Download(mUrl, respData);
	EXPECT_EQ(CURLE_OK, respData->curlRetValue);
	EXPECT_EQ(200, respData->iHttpRetValue);
	respData->show();
	respData->clear();

	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_str(mCurlEasyHandle, CURLOPT_URL, mUrl.c_str()))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_perform(mCurlEasyHandle)).WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_getinfo_int(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, NotNull()))
		.WillOnce(DoAll(SetArgPointee<2>(404), Return(CURLE_OK)));
	mAampCurlDownloader->Download(mUrl, respData);
	EXPECT_EQ(CURLE_OK, respData->curlRetValue);
	EXPECT_EQ(404, respData->iHttpRetValue);
	respData->show();
	respData->clear();

	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_str(mCurlEasyHandle, CURLOPT_URL, mUrl.c_str()))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_perform(mCurlEasyHandle)).WillOnce(Return(CURLE_COULDNT_RESOLVE_HOST));
	EXPECT_CALL(*g_mockCurl, curl_easy_getinfo_int(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, NotNull()))
		.Times(0); // This prevents the function from being called if CURL return value is not CURLE_OK
	mAampCurlDownloader->Download(mUrl, respData);
	EXPECT_EQ(CURLE_COULDNT_RESOLVE_HOST, respData->curlRetValue);
	EXPECT_NE(200, respData->iHttpRetValue);
	respData->show();
	respData->clear();

	inpData->bNeedDownloadMetrics = true;
	inpData->bIgnoreResponseHeader = false;
	inpData->iStallTimeout=0;
	inpData->iStartTimeout=0;
	inpData->iLowBWTimeout=0;
	/* When Initialize is called a second time, these data and callback functions are set in curl again. */
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_PROGRESSDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_xferinfo(mCurlEasyHandle, CURLOPT_XFERINFOFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlProgressCallback), Return(CURLE_OK)));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_WRITEDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_write(mCurlEasyHandle, CURLOPT_WRITEFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlWriteFunc), Return(CURLE_OK)));
	mAampCurlDownloader->Initialize(inpData);
	/* Assert if the callbacks are null to avoid a crash if they are called by the test.*/ 
	ASSERT_NE(mCurlProgressCallback, nullptr);
	ASSERT_NE(mCurlWriteFunc, nullptr);

	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_str(mCurlEasyHandle, CURLOPT_URL, mUrl.c_str()))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_perform(mCurlEasyHandle)).WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_getinfo_int(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, NotNull()))
		.WillOnce(DoAll(SetArgPointee<2>(200), Return(CURLE_OK)));
	mAampCurlDownloader->Download(mUrl, respData);
	EXPECT_EQ(CURLE_OK, respData->curlRetValue);
	EXPECT_EQ(200, respData->iHttpRetValue);
	respData->show();
	respData->clear();
}

TEST_F(FunctionalTests, AampCurlDownloader_DownloadTest_8)
{
	DownloadResponsePtr respData = std::make_shared<DownloadResponse>();
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig>();
	inpData->bNeedDownloadMetrics = true;
	inpData->bIgnoreResponseHeader = false;

	//Check for timeout config values
	EXPECT_EQ(0,inpData->iStallTimeout);
	EXPECT_EQ(0,inpData->iStartTimeout);
	EXPECT_EQ(0,inpData->iLowBWTimeout);
	inpData->show();

	//Start Timeout case
	EXPECT_CALL(*g_mockCurl, curl_easy_init()).WillOnce(Return(mCurlEasyHandle));
	/* The curl easy handle will be cleaned when AampCurlDownloader is destroyed. */
	EXPECT_CALL(*g_mockCurl, curl_easy_cleanup(mCurlEasyHandle));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_PROGRESSDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_xferinfo(mCurlEasyHandle, CURLOPT_XFERINFOFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlProgressCallback), Return(CURLE_OK)));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_WRITEDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_write(mCurlEasyHandle, CURLOPT_WRITEFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlWriteFunc), Return(CURLE_OK)));
	mAampCurlDownloader->Initialize(inpData);
	/* Assert if the callbacks are null to avoid a crash if they are called by the test. */
	ASSERT_NE(mCurlProgressCallback, nullptr);
	ASSERT_NE(mCurlWriteFunc, nullptr);

	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_str(mCurlEasyHandle, CURLOPT_URL, mUrl.c_str()))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_perform(mCurlEasyHandle))
		.WillOnce(Return(CURLE_ABORTED_BY_CALLBACK));
	EXPECT_CALL(*g_mockCurl, curl_easy_getinfo_int(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, NotNull()))
		.Times(0); // This prevents the function from being called if CURL return value is not CURLE_OK
	mAampCurlDownloader->Download(mUrl, respData);
	EXPECT_EQ(CURLE_ABORTED_BY_CALLBACK, respData->curlRetValue);
	EXPECT_NE(200, respData->iHttpRetValue);
	EXPECT_EQ(eCURL_ABORT_REASON_NONE ,respData->mAbortReason);
	respData->show();
	respData->clear();

}

TEST_F(FunctionalTests, AampCurlDownloader_DownloadTest_StallAtStart)
{
	DownloadResponsePtr respData = std::make_shared<DownloadResponse>();
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig>();
	inpData->bNeedDownloadMetrics = true;
	inpData->bIgnoreResponseHeader = false;
	inpData->iStartTimeout = 1;
	inpData->iStallTimeout = 1;

	EXPECT_CALL(*g_mockCurl, curl_easy_init()).WillOnce(Return(mCurlEasyHandle));
	/* The curl easy handle will be cleaned when AampCurlDownloader is destroyed. */
	EXPECT_CALL(*g_mockCurl, curl_easy_cleanup(mCurlEasyHandle));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_PROGRESSDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_xferinfo(mCurlEasyHandle, CURLOPT_XFERINFOFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlProgressCallback), Return(CURLE_OK)));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_WRITEDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_write(mCurlEasyHandle, CURLOPT_WRITEFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlWriteFunc), Return(CURLE_OK)));
	mAampCurlDownloader->Initialize(inpData);
	/* Assert if the callbacks are null to avoid a crash if they are called by the test. */
	ASSERT_NE(mCurlProgressCallback, nullptr);
	ASSERT_NE(mCurlWriteFunc, nullptr);

	int progress_callback_return = 0;
	/* Test a stall at start scenario:
	<startTimeout> seconds delay occurs without any bytes having been downloaded */
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_str(mCurlEasyHandle, CURLOPT_URL, mUrl.c_str()))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_perform(mCurlEasyHandle))
		.WillOnce(DoAll(
			InvokeWithoutArgs([&]()
			{
				/* Wait 1 sec without downloading any data, to cause a stall at start scenario */
				std::this_thread::sleep_for(std::chrono::seconds(1));
				progress_callback_return = mCurlProgressCallback(mAampCurlDownloader, 0, 0, 0, 0);
			}),
			Return(CURLE_ABORTED_BY_CALLBACK)));
	EXPECT_CALL(*g_mockCurl, curl_easy_getinfo_int(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, NotNull()))
		.Times(0); // This prevents the function from being called if CURL return value is not CURLE_OK
	mAampCurlDownloader->Download(mUrl, respData);
	respData->show();
	EXPECT_EQ(progress_callback_return, -1);
	EXPECT_EQ(CURLE_ABORTED_BY_CALLBACK, respData->curlRetValue);
	EXPECT_EQ(eCURL_ABORT_REASON_START_TIMEDOUT ,respData->mAbortReason);

	EXPECT_FALSE(mAampCurlDownloader->IsDownloadActive());
}

TEST_F(FunctionalTests, AampCurlDownloader_DownloadTest_Stall)
{
	DownloadResponsePtr respData = std::make_shared<DownloadResponse>();
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig>();
	inpData->bNeedDownloadMetrics = true;
	inpData->bIgnoreResponseHeader = false;
	inpData->iStartTimeout = 1;
	inpData->iStallTimeout = 1;

	EXPECT_CALL(*g_mockCurl, curl_easy_init()).WillOnce(Return(mCurlEasyHandle));
	/* The curl easy handle will be cleaned when AampCurlDownloader is destroyed. */
	EXPECT_CALL(*g_mockCurl, curl_easy_cleanup(mCurlEasyHandle));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_PROGRESSDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_xferinfo(mCurlEasyHandle, CURLOPT_XFERINFOFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlProgressCallback), Return(CURLE_OK)));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_WRITEDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_write(mCurlEasyHandle, CURLOPT_WRITEFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlWriteFunc), Return(CURLE_OK)));
	mAampCurlDownloader->Initialize(inpData);
	/* Assert if the callbacks are null to avoid a crash if they are called by the test. */
	ASSERT_NE(mCurlProgressCallback, nullptr);
	ASSERT_NE(mCurlWriteFunc, nullptr);

	int write_func_return = 0;
	size_t write_sz = 1;
	size_t write_nmemb = 1;
	void *write_buffer = malloc(write_sz * write_nmemb);
	int progress_callback_return = 0;
	/* Test a stall scenario:
	<stallTimeout> seconds delay occurs without downloading any bytes, after some have been downloaded */
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_str(mCurlEasyHandle, CURLOPT_URL, mUrl.c_str()))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_perform(mCurlEasyHandle))
		.WillOnce(DoAll(
			InvokeWithoutArgs([&]()
			{
				/* Simulate downloading some bytes, to avoid a stall at start */
				write_func_return = mCurlWriteFunc(write_buffer, write_sz, write_nmemb, mAampCurlDownloader);
				/* Wait 1 sec without downloading data, to cause a stall scenario */
				std::this_thread::sleep_for(std::chrono::seconds(1));
				progress_callback_return = mCurlProgressCallback(mAampCurlDownloader, 0, 0, 0, 0);
			}),
			Return(CURLE_ABORTED_BY_CALLBACK)));
	EXPECT_CALL(*g_mockCurl, curl_easy_getinfo_int(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, NotNull()))
		.Times(0); // This prevents the function from being called if CURL return value is not CURLE_OK
	mAampCurlDownloader->Download(mUrl, respData);
	respData->show();
	EXPECT_EQ(write_func_return, (write_sz * write_nmemb));
	EXPECT_EQ(progress_callback_return, -1);
	EXPECT_EQ(CURLE_ABORTED_BY_CALLBACK, respData->curlRetValue);
	EXPECT_EQ(eCURL_ABORT_REASON_STALL_TIMEDOUT ,respData->mAbortReason);
	free(write_buffer);

	EXPECT_FALSE(mAampCurlDownloader->IsDownloadActive());
}

TEST_F(FunctionalTests, AampCurlDownloader_Retry_502)
{
	/* test
	 * for a http 502 error then we will retry MANIFEST_DOWNLOAD_502_RETRY_COUNT times
	 * for other http erros then we retry DEFAULT_DOWNLOAD_RETRY_COUNT
	 * */
	DownloadResponsePtr respData = std::make_shared<DownloadResponse>();
	DownloadConfigPtr inpData = std::make_shared<DownloadConfig>();
	inpData->bNeedDownloadMetrics = true;
	inpData->bIgnoreResponseHeader = true;
	inpData->iDownload502RetryCount = MANIFEST_DOWNLOAD_502_RETRY_COUNT;

	// The first attempt is not a retry hence +1
	int triesExpected = MANIFEST_DOWNLOAD_502_RETRY_COUNT + 1;

	EXPECT_CALL(*g_mockCurl, curl_easy_init()).WillOnce(Return(mCurlEasyHandle));
	/* The curl easy handle will be cleaned when AampCurlDownloader is destroyed. */
	EXPECT_CALL(*g_mockCurl, curl_easy_cleanup(mCurlEasyHandle));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_PROGRESSDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_xferinfo(mCurlEasyHandle, CURLOPT_XFERINFOFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlProgressCallback), Return(CURLE_OK)));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_ptr(mCurlEasyHandle, CURLOPT_WRITEDATA, mAampCurlDownloader))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_func_write(mCurlEasyHandle, CURLOPT_WRITEFUNCTION, NotNull()))
		.WillOnce(DoAll(SaveArgPointee<2>(&mCurlWriteFunc), Return(CURLE_OK)));
	mAampCurlDownloader->Initialize(inpData);
	/* Assert if the callbacks are null to avoid a crash if they are called by the test. */
	ASSERT_NE(mCurlProgressCallback, nullptr);
	ASSERT_NE(mCurlWriteFunc, nullptr);

	// Test 502 until retries exhausted
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_str(mCurlEasyHandle, CURLOPT_URL, mUrl.c_str()))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_perform(mCurlEasyHandle))
		.Times(triesExpected)
		.WillRepeatedly(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_getinfo_int(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, NotNull()))
		.Times(triesExpected)
		.WillRepeatedly(DoAll(SetArgPointee<2>(502), Return(CURLE_OK)));
	mAampCurlDownloader->Download(mUrl, respData);
	EXPECT_EQ(CURLE_OK, respData->curlRetValue);
	EXPECT_EQ(502, respData->iHttpRetValue);
	respData->show();
	respData->clear();

	// Test return sequence 502,200 hence 1 retry then returns 200
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_str(mCurlEasyHandle, CURLOPT_URL, mUrl.c_str()))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_perform(mCurlEasyHandle))
		.Times(2)
		.WillRepeatedly(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_getinfo_int(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, NotNull()))
		.WillOnce(DoAll(SetArgPointee<2>(502), Return(CURLE_OK)))
		.WillOnce(DoAll(SetArgPointee<2>(200), Return(CURLE_OK)));
	mAampCurlDownloader->Download(mUrl, respData);
	EXPECT_EQ(CURLE_OK, respData->curlRetValue);
	EXPECT_EQ(200, respData->iHttpRetValue);
	respData->show();
	respData->clear();

	// For some unknown reason we allow 1 retry on 408 when the number of retries set in config = 0
	EXPECT_CALL(*g_mockCurl, curl_easy_setopt_str(mCurlEasyHandle, CURLOPT_URL, mUrl.c_str()))
		.WillOnce(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_perform(mCurlEasyHandle))
		.Times(2)
		.WillRepeatedly(Return(CURLE_OK));
	EXPECT_CALL(*g_mockCurl, curl_easy_getinfo_int(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, NotNull()))
		.WillOnce(DoAll(SetArgPointee<2>(408), Return(CURLE_OK)))
		.WillOnce(DoAll(SetArgPointee<2>(408), Return(CURLE_OK)));
	mAampCurlDownloader->Download(mUrl, respData);
	EXPECT_EQ(CURLE_OK, respData->curlRetValue);
	EXPECT_EQ(408, respData->iHttpRetValue);
	respData->show();
	respData->clear();
}