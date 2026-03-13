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
#include "AampUtils.h"
#include "priv_aamp.h"
#include <thread>
#include <unistd.h>
#include <atomic>

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

/**
 * @class TestableAampMPDDownloader
 * @brief Test subclass exposing protected members of AampMPDDownloader
 *        for white-box unit testing.
 */
class TestableAampMPDDownloader : public AampMPDDownloader
{
public:
	/**
	 * @brief Public wrapper for the protected
	 *        getNextLLDManifestRefreshInterval() method.
	 */
	uint32_t CallGetNextLLDManifestRefreshInterval(
		ManifestDownloadResponsePtr manifest)
	{
		return getNextLLDManifestRefreshInterval(manifest);
	}

	/**
	 * @brief Directly set mPublishTime for test control.
	 */
	void SetPublishTime(uint64_t publishTimeMs)
	{
		mPublishTime = publishTimeMs;
	}
};

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
// Commented below tests to avoid more wait duration
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

TEST_F(FunctionalTests,
    AampMPDDownloader_LiveRefreshRetriesWhenFailureIsTimeoutClass)
{
    static const char *kLiveMpdManifest =
    R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011" minBufferTime="PT2.000S" maxSegmentDuration="PT0H0M1.92S" minimumUpdatePeriod="PT0H0M3.0S" availabilityStartTime="1977-05-25T18:00:00.000Z" timeShiftBufferDepth="PT0H0M30.000S" publishTime="2024-11-08T12:53:09.725Z">
    <Period id="901591170" start="PT416006H37M27.854S">
        <AdaptationSet id="2" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
            <Role schemeIdUri="urn:mpeg:dash:role:2011" value="main"/>
            <SegmentTemplate initialization="init-$RepresentationID$.mp4" media="seg-$Number$.m4s" timescale="90000" startNumber="901599260" presentationTimeOffset="20213">
                <SegmentTimeline>
                    <S t="1377581813" d="172800" r="14"/>
                </SegmentTimeline>
            </SegmentTemplate>
            <Representation id="root_video4" bandwidth="562800" codecs="hvc1.1.6.L63.90" width="640" height="360" frameRate="25000/1000"/>
        </AdaptationSet>
    </Period>
</MPD>
)";

	std::shared_ptr<ManifestDownloadConfig> inpData =
		std::make_shared<ManifestDownloadConfig>(-1);
	inpData->mTuneUrl = url1;

	std::atomic<int> preProcessCount(0);
	auto preProcessCallback = [&preProcessCount]() -> std::string
	{
		if (preProcessCount.fetch_add(1) == 0)
		{
			return std::string(kLiveMpdManifest);
		}
		return std::string();
	};

	mAampMPDDownloader->Initialize(inpData, appName, preProcessCallback);
	mAampMPDDownloader->Start();

	ManifestDownloadResponsePtr firstManifest =
		mAampMPDDownloader->GetManifest(true, 2000);
	ASSERT_TRUE(firstManifest != nullptr);
	ASSERT_TRUE(IS_HTTP_SUCCESS(firstManifest->mMPDDownloadResponse->iHttpRetValue));
	ASSERT_TRUE(firstManifest->mIsLiveManifest);

	// Wait up to 8 seconds for the second manifest to become available.
	// Since GetManifest(false, ...) does not block for new data, poll
	// until the returned pointer differs from the first manifest or the
	// deadline is reached.
	ManifestDownloadResponsePtr secondManifest;
	{
		auto deadline = std::chrono::steady_clock::now() +
			std::chrono::milliseconds(8000);
		do
		{
			secondManifest = mAampMPDDownloader->GetManifest(false, 0);
			if (!secondManifest ||
				secondManifest.get() != firstManifest.get())
			{
				break;
			}
			std::this_thread::sleep_for(
				std::chrono::milliseconds(50));
		} while (std::chrono::steady_clock::now() < deadline);
	}
	ASSERT_TRUE(secondManifest != nullptr);
	ASSERT_TRUE(secondManifest.get() != firstManifest.get());
	ASSERT_TRUE(IsCurlTimeoutFailure(
		secondManifest->mMPDDownloadResponse->iHttpRetValue));

	// Wait up to 1.8 seconds for the next manifest refresh. As above,
	// poll until a different manifest pointer is observed or the
	// deadline expires.
	ManifestDownloadResponsePtr thirdManifest;
	{
		auto deadline = std::chrono::steady_clock::now() +
			std::chrono::milliseconds(1800);
		do
		{
			thirdManifest = mAampMPDDownloader->GetManifest(false, 0);
			if (!thirdManifest ||
				thirdManifest.get() != secondManifest.get())
			{
				break;
			}
			std::this_thread::sleep_for(
				std::chrono::milliseconds(50));
		} while (std::chrono::steady_clock::now() < deadline);
	}

	EXPECT_TRUE(thirdManifest != nullptr);
	EXPECT_TRUE(thirdManifest.get() != secondManifest.get());
	EXPECT_TRUE(IsCurlTimeoutFailure(
		thirdManifest->mMPDDownloadResponse->iHttpRetValue));

	mAampMPDDownloader->Release();
}

// Confirms that the next manifest refresh interval will be approximately equal
// to (minimumUpdatePeriod - elapsedSincePublish) plus a jitter in
// [0, MAX_LLD_MANIFEST_REFRESH_JITTER_MS].
//
// Setup:
//   minimumUpdatePeriod = PT2.00S  (2 000 ms)
//   mPublishTime        = nowMs - 1 000 ms
//
// Expected base interval:
//   nextPublishTimeMs - nowMs = (nowMs - 1000 + 2000) - nowMs = ~1 000 ms
//
// After jitter [0, 500 ms] the result must be in [~1 000, ~1 500] ms.
// A lower bound of 500 ms is used to absorb any CI timing variance.
TEST_F(FunctionalTests, AampMPDDownloader_LLDManifestRefreshIntervalTest1)
{
	static const char *kMpdWithMinUpdatePeriod =
	R"xml(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     type="dynamic"
     minimumUpdatePeriod="PT2.00S"
     profiles="urn:mpeg:dash:profile:isoff-live:2011"
     availabilityStartTime="1977-05-25T18:00:00Z">
  <Period id="1" start="PT0S">
    <AdaptationSet id="1" mimeType="video/mp4">
      <Representation id="r1" bandwidth="500000" codecs="avc1.640028"/>
    </AdaptationSet>
  </Period>
</MPD>)xml";

	// Build a ManifestDownloadResponse with the MPD above and parse it so
	// mMPDInstance is populated (required by getNextLLDManifestRefreshInterval).
	ManifestDownloadResponsePtr respData =
		std::make_shared<_manifestDownloadResponse>();
	std::string mpdStr(kMpdWithMinUpdatePeriod);
	respData->mMPDDownloadResponse->mDownloadData.assign(
		mpdStr.begin(), mpdStr.end());
	respData->parseMPD();
	ASSERT_NE(respData->mMPDInstance, nullptr)
		<< "MPD failed to parse – check the test XML";

	// Set mPublishTime to 1 second in the past so the expected base interval
	// is approximately 1 000 ms (2 000 ms period minus 1 000 ms elapsed).
	TestableAampMPDDownloader testableDownloader;
	uint64_t publishTimeMs =
		static_cast<uint64_t>(aamp_GetCurrentTimeMS()) - 1000ULL;
	testableDownloader.SetPublishTime(publishTimeMs);

	uint32_t refreshIntervalMs =
		testableDownloader.CallGetNextLLDManifestRefreshInterval(respData);

	// Base ≈ 1 000 ms; jitter adds up to 500 ms → expected range [1 000, 1 500].
	// Lower bound is relaxed to 500 ms to tolerate CI scheduling latency.
	EXPECT_GE(refreshIntervalMs, 500u)
		<< "Refresh interval too short: " << refreshIntervalMs << " ms";
	EXPECT_LE(refreshIntervalMs, 1500u)
		<< "Refresh interval too long: " << refreshIntervalMs << " ms";
}

// Confirms that when (mPublishTime + minimumUpdatePeriod) has already elapsed
// the refresh interval is clamped to MIN_DELAY_BETWEEN_MPD_UPDATE_MS (500 ms).
//
// Setup:
//   minimumUpdatePeriod = PT2.00S  (2 000 ms)
//   mPublishTime        = nowMs - 5 000 ms  (deadline was 3 000 ms ago)
//
// Expected base interval:
//   nextPublishTimeMs - nowMs < 0  → base = 0
//
// After jitter [0, 500 ms] the value is in [0, 500]; the clamp to
// MIN_DELAY_BETWEEN_MPD_UPDATE_MS (500 ms) brings it to exactly [500, 1000].
TEST_F(FunctionalTests, AampMPDDownloader_LLDManifestRefreshIntervalTest2_ElapsedDeadline)
{
	static const char *kMpdWithMinUpdatePeriod =
	R"xml(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     type="dynamic"
     minimumUpdatePeriod="PT2.00S"
     profiles="urn:mpeg:dash:profile:isoff-live:2011"
     availabilityStartTime="1977-05-25T18:00:00Z">
  <Period id="1" start="PT0S">
    <AdaptationSet id="1" mimeType="video/mp4">
      <Representation id="r1" bandwidth="500000" codecs="avc1.640028"/>
    </AdaptationSet>
  </Period>
</MPD>)xml";

	// Build and parse the response so mMPDInstance is populated.
	ManifestDownloadResponsePtr respData =
		std::make_shared<_manifestDownloadResponse>();
	std::string mpdStr(kMpdWithMinUpdatePeriod);
	respData->mMPDDownloadResponse->mDownloadData.assign(
		mpdStr.begin(), mpdStr.end());
	respData->parseMPD();
	ASSERT_NE(respData->mMPDInstance, nullptr)
		<< "MPD failed to parse – check the test XML";

	// Set mPublishTime 5 000 ms in the past.
	// nextPublishTimeMs = (now - 5000) + 2000 = now - 3000, which is already
	// overdue, so the implementation sets base = 0 before applying jitter.
	TestableAampMPDDownloader testableDownloader;
	uint64_t publishTimeMs =
		static_cast<uint64_t>(aamp_GetCurrentTimeMS()) - 5000ULL;
	testableDownloader.SetPublishTime(publishTimeMs);

	uint32_t refreshIntervalMs =
		testableDownloader.CallGetNextLLDManifestRefreshInterval(respData);

	// base=0, jitter∈[0,500] → pre-clamp value∈[0,500].
	// MIN_DELAY_BETWEEN_MPD_UPDATE_MS clamp raises any value below 500 to 500.
	// Upper bound is 500 (base) + 500 (max jitter) = 1000 ms.
	EXPECT_GE(refreshIntervalMs, static_cast<uint32_t>(MIN_DELAY_BETWEEN_MPD_UPDATE_MS))
		<< "Refresh interval below minimum: " << refreshIntervalMs << " ms";
	EXPECT_LE(refreshIntervalMs, 1000u)
		<< "Refresh interval too long for overdue deadline: "
		<< refreshIntervalMs << " ms";
}