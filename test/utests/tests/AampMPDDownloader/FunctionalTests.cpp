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
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <vector>

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

// Live MPD with a single Period and SegmentTimeline, used to validate
// manifest refresh status handling across periodic MPD updates.
static const char *kLiveMpdManifestForRefreshStatusTests = R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011"
     type="dynamic"
     profiles="urn:mpeg:dash:profile:isoff-live:2011"
     minBufferTime="PT2.000S"
     maxSegmentDuration="PT0H0M1.92S"
     minimumUpdatePeriod="PT0.5S"
     availabilityStartTime="1977-05-25T18:00:00.000Z"
     timeShiftBufferDepth="PT0H0M30.000S"
     publishTime="2024-11-08T12:53:09.725Z">

    <Period id="901591170" start="PT416006H37M27.854S">

        <EventStream schemeIdUri="urn:example:test:2024"
                     timescale="1000"/>

        <AdaptationSet id="2"
                       contentType="video"
                       mimeType="video/mp4"
                       segmentAlignment="true"
                       startWithSAP="1">

            <Role schemeIdUri="urn:mpeg:dash:role:2011"
                  value="main"/>

            <SegmentTemplate
                initialization="init-$RepresentationID$.mp4"
                media="seg-$Number$.m4s"
                timescale="90000"
                startNumber="901599260"
                presentationTimeOffset="20213">

                <SegmentTimeline>
                    <S t="1377581813" d="172800" r="14"/>
                </SegmentTimeline>

            </SegmentTemplate>

            <Representation id="root_video4"
                            bandwidth="562800"
                            codecs="hvc1.1.6.L63.90"
                            width="640"
                            height="360"
                            frameRate="25000/1000"/>

        </AdaptationSet>

    </Period>

</MPD>
)";

static const char *kMalformedMpdManifest =
"<MPD xmlns='urn:mpeg:dash:schema:mpd:2011'><Period>";

/**
* @brief Wait for a new manifest download response different from the previous one.
*
* Polls GetManifest() until a new manifest is available, a null response is
* received, or the specified timeout expires.
*
* Note: sleep_for is used deliberately here. These tests exercise real downloader
* threads and must wait for asynchronous results. There is no condvar or callback
* API exposed on AampMPDDownloader that would let us block without polling, so
* the brief sleep avoids busy-spinning while staying within the deadline.
*/
static ManifestDownloadResponsePtr WaitForNextManifest(
	AampMPDDownloader *downloader,
	const ManifestDownloadResponsePtr &previousManifest,
	int timeoutMs)
{
	auto deadline = std::chrono::steady_clock::now() +
		std::chrono::milliseconds(timeoutMs);
	do
	{
		ManifestDownloadResponsePtr nextManifest =
			downloader->GetManifest(false, 0);
		if (!nextManifest ||
			nextManifest.get() != previousManifest.get())
		{
			return nextManifest;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	} while (std::chrono::steady_clock::now() < deadline);
	return previousManifest;
}

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
		EXPECT_NO_THROW(mAampMPDDownloader->Initialize(inpData, appName, [this]() -> std::pair<std::string,int> {
			return mPrivateInstanceAAMP1->SendManifestPreProcessEvent();
		}));
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
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011" minBufferTime="PT2.000S" maxSegmentDuration="PT0H0M1.92S" minimumUpdatePeriod="PT0.5S" availabilityStartTime="1977-05-25T18:00:00.000Z" timeShiftBufferDepth="PT0H0M30.000S" publishTime="2024-11-08T12:53:09.725Z">
    <Period id="901591170" start="PT416006H37M27.854S">
        <EventStream schemeIdUri="urn:example:test:2024" timescale="1000"/>
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
	auto preProcessCallback = [&preProcessCount]() -> std::pair<std::string,int>
	{
		if (preProcessCount.fetch_add(1) == 0)
		{
			return { std::string(kLiveMpdManifest), 200 };
		}
		return { std::string(), CURLE_OPERATION_TIMEDOUT };
	};

	mAampMPDDownloader->Initialize(inpData, appName, preProcessCallback);
	mAampMPDDownloader->Start();

	ManifestDownloadResponsePtr firstManifest =
		mAampMPDDownloader->GetManifest(true, 2000);
	ASSERT_TRUE(firstManifest != nullptr);
	ASSERT_TRUE(IS_HTTP_SUCCESS(firstManifest->mMPDDownloadResponse->iHttpRetValue));
	ASSERT_TRUE(firstManifest->mIsLiveManifest);

	// Wait up to 2 seconds for the second manifest to become available.
	// Since GetManifest(false, ...) does not block for new data, poll
	// until the returned pointer differs from the first manifest or the
	// deadline is reached.
	ManifestDownloadResponsePtr secondManifest;
	{
		auto deadline = std::chrono::steady_clock::now() +
			std::chrono::milliseconds(2000);
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

	// Wait up to 1.5 seconds for the next manifest refresh. As above,
	// poll until a different manifest pointer is observed or the
	// deadline expires.
	ManifestDownloadResponsePtr thirdManifest;
	{
		auto deadline = std::chrono::steady_clock::now() +
			std::chrono::milliseconds(1500);
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

TEST_F(FunctionalTests,
    AampMPDDownloader_LiveRefreshRetriesWhenFailureIsCurlRecvError)
{
    static const char *kLiveMpdManifest =
    R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011" minBufferTime="PT2.000S" maxSegmentDuration="PT0H0M1.92S" minimumUpdatePeriod="PT0.5S" availabilityStartTime="1977-05-25T18:00:00.000Z" timeShiftBufferDepth="PT0H0M30.000S" publishTime="2024-11-08T12:53:09.725Z">
    <Period id="901591170" start="PT416006H37M27.854S">
        <EventStream schemeIdUri="urn:example:test:2024" timescale="1000"/>
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

	// First call returns a valid live manifest; all subsequent calls simulate
	// CURLE_RECV_ERROR (curl 56) — the regression being tested is that the
	// downloader loop must NOT exit on this error class.
	std::atomic<int> preProcessCount(0);
	const char *liveMpdManifestPtr = kLiveMpdManifest;
	auto preProcessCallback = [&preProcessCount, liveMpdManifestPtr]() -> std::pair<std::string,int>
	{
		if (preProcessCount.fetch_add(1) == 0)
		{
			return { std::string(liveMpdManifestPtr), 200 };
		}
		return { std::string(), CURLE_RECV_ERROR };
	};

	mAampMPDDownloader->Initialize(inpData, appName, preProcessCallback);
	mAampMPDDownloader->Start();

	ManifestDownloadResponsePtr firstManifest =
		mAampMPDDownloader->GetManifest(true, 2000);
	ASSERT_TRUE(firstManifest != nullptr);
	ASSERT_TRUE(IS_HTTP_SUCCESS(firstManifest->mMPDDownloadResponse->iHttpRetValue));
	ASSERT_TRUE(firstManifest->mIsLiveManifest);

	// Wait up to 2 seconds for the second manifest attempt (the CURLE_RECV_ERROR one).
	ManifestDownloadResponsePtr secondManifest;
	{
		auto deadline = std::chrono::steady_clock::now() +
			std::chrono::milliseconds(2000);
		do
		{
			secondManifest = mAampMPDDownloader->GetManifest(false, 0);
			if (!secondManifest ||
				secondManifest.get() != firstManifest.get())
			{
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		} while (std::chrono::steady_clock::now() < deadline);
	}
	ASSERT_TRUE(secondManifest != nullptr);
	ASSERT_TRUE(secondManifest.get() != firstManifest.get());
	ASSERT_EQ(secondManifest->mMPDDownloadResponse->iHttpRetValue, CURLE_RECV_ERROR);

	// REGRESSION ASSERTION: the loop must have continued and produced a third
	// manifest attempt within 1.5 seconds (fast 500ms retry, not a full
	// mRefreshInterval stall).
	ManifestDownloadResponsePtr thirdManifest;
	{
		auto deadline = std::chrono::steady_clock::now() +
			std::chrono::milliseconds(1500);
		do
		{
			thirdManifest = mAampMPDDownloader->GetManifest(false, 0);
			if (!thirdManifest ||
				thirdManifest.get() != secondManifest.get())
			{
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		} while (std::chrono::steady_clock::now() < deadline);
	}

	EXPECT_TRUE(thirdManifest != nullptr);
	EXPECT_TRUE(thirdManifest.get() != secondManifest.get());
	EXPECT_EQ(thirdManifest->mMPDDownloadResponse->iHttpRetValue, CURLE_RECV_ERROR);

	mAampMPDDownloader->Release();
}

// p0 declares a 10s duration but its audio timeline ends with a 3.2s segment starting 0.2s before that end.
static const char *kTailOverhangManifest =
	"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
	"<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"static\" mediaPresentationDuration=\"PT20S\">"
	"<Period id=\"p0\" start=\"PT0S\" duration=\"PT10S\"><AdaptationSet contentType=\"audio\" mimeType=\"audio/mp4\">"
	"<SegmentTemplate timescale=\"1000\" initialization=\"init.mp4\" media=\"$Time$.m4s\"><SegmentTimeline>"
	"<S t=\"0\" d=\"2000\" r=\"3\" /><S t=\"8000\" d=\"1800\" /><S t=\"9800\" d=\"3200\" />"
	"</SegmentTimeline></SegmentTemplate><Representation id=\"1\" bandwidth=\"1000\" codecs=\"x\"/></AdaptationSet></Period></MPD>";

static size_t AudioTimelineEntries(const _manifestDownloadResponse &response)
{
	return response.mMPDInstance->GetPeriods().at(0)->GetAdaptationSets().at(0)->GetSegmentTemplate()->GetSegmentTimeline()->GetTimelines().size();
}

static void LoadTailOverhangManifest(_manifestDownloadResponse &response, bool trimPeriodTailSegments)
{
	const std::string manifest = kTailOverhangManifest;
	response.mMPDDownloadResponse->mDownloadData.assign(manifest.begin(), manifest.end());
	response.mMPDDownloadResponse->sEffectiveUrl = "http://host/asset/manifest.mpd";
	response.mTrimPeriodTailSegments = trimPeriodTailSegments;
}

TEST_F(FunctionalTests, ParseMPD_TrimsPeriodTailWhenEnabled)
{
	_manifestDownloadResponse response;
	LoadTailOverhangManifest(response, true);

	response.parseMPD();

	ASSERT_EQ(response.mMPDStatus, AAMPStatusType::eAAMPSTATUS_OK);
	EXPECT_EQ(AudioTimelineEntries(response), 2u);
}

TEST_F(FunctionalTests, ParseMPD_LeavesPeriodTailWhenDisabled)
{
	_manifestDownloadResponse response;
	LoadTailOverhangManifest(response, false);

	response.parseMPD();

	ASSERT_EQ(response.mMPDStatus, AAMPStatusType::eAAMPSTATUS_OK);
	EXPECT_EQ(AudioTimelineEntries(response), 3u);
}

TEST_F(FunctionalTests, ParseMPD_CloneKeepsTrimSetting)
{
	_manifestDownloadResponse original;
	LoadTailOverhangManifest(original, true);

	std::shared_ptr<_manifestDownloadResponse> cloned = original.clone();

	EXPECT_TRUE(cloned->mTrimPeriodTailSegments);
	ASSERT_EQ(cloned->mMPDStatus, AAMPStatusType::eAAMPSTATUS_OK);
	EXPECT_EQ(AudioTimelineEntries(*cloned), 2u);
}

TEST_F(FunctionalTests, ManifestDownloadConfig_CopyKeepsTrimSetting)
{
	ManifestDownloadConfig config(-1);
	EXPECT_FALSE(config.mTrimPeriodTailSegments);
	config.mTrimPeriodTailSegments = true;

	ManifestDownloadConfig copy(config);

	EXPECT_TRUE(copy.mTrimPeriodTailSegments);
}

// Live manifest with a 30s minimumUpdatePeriod; the downloader caps its refresh wait at ~3s.
static const char *kLongRefreshLiveMpd =
	R"(<?xml version="1.0" encoding="UTF-8"?>
<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="dynamic" profiles="urn:mpeg:dash:profile:isoff-live:2011" minBufferTime="PT2.000S" maxSegmentDuration="PT0H0M1.92S" minimumUpdatePeriod="PT30S" availabilityStartTime="1977-05-25T18:00:00.000Z" timeShiftBufferDepth="PT0H0M30.000S" publishTime="2024-11-08T12:53:09.725Z">
    <Period id="901591170" start="PT416006H37M27.854S">
        <AdaptationSet id="2" contentType="video" mimeType="video/mp4" segmentAlignment="true" startWithSAP="1">
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

struct NotifyContext
{
	std::mutex mtx;
	std::condition_variable cv;
	int count{0};
};

static void CountingNotifyCallback(void *arg)
{
	auto *ctx = static_cast<NotifyContext *>(arg);
	{
		std::lock_guard<std::mutex> lock(ctx->mtx);
		++ctx->count;
	}
	ctx->cv.notify_all();
}

// Calls Release() on scope exit so downloader threads never outlive test locals.
struct DownloaderReleaseGuard
{
	AampMPDDownloader *downloader;
	~DownloaderReleaseGuard() { downloader->Release(); }
};

static std::function<std::pair<std::string,int>()> MakeLiveManifestPreProcess(std::shared_ptr<std::atomic<int>> calls)
{
	return [calls]() -> std::pair<std::string,int>
	{
		calls->fetch_add(1);
		return { std::string(kLongRefreshLiveMpd), 200 };
	};
}

// A GetManifest(wait) caller blocked on an empty queue must receive the first manifest pushed by the downloader.
TEST_F(FunctionalTests, GetManifest_WaitReceivesManifestPushedByDownloader)
{
	auto calls = std::make_shared<std::atomic<int>>(0);
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig>(-1);
	inpData->mTuneUrl = url1;
	mAampMPDDownloader->Initialize(inpData, appName, MakeLiveManifestPreProcess(calls));
	DownloaderReleaseGuard guard{mAampMPDDownloader};

	std::atomic<bool> waiterStarted(false);
	std::promise<ManifestDownloadResponsePtr> result;
	std::future<ManifestDownloadResponsePtr> resultFuture = result.get_future();
	std::thread waiter([&]()
	{
		waiterStarted = true;
		result.set_value(mAampMPDDownloader->GetManifest(true, 10000));
	});
	while (!waiterStarted)
	{
		std::this_thread::yield();
	}

	mAampMPDDownloader->Start();

	const bool delivered = resultFuture.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
	mAampMPDDownloader->Release();
	waiter.join();

	ASSERT_TRUE(delivered);
	ManifestDownloadResponsePtr manifest = resultFuture.get();
	ASSERT_TRUE(manifest != nullptr);
	EXPECT_EQ(manifest->mMPDStatus, AAMPStatusType::eAAMPSTATUS_OK);
	EXPECT_TRUE(IS_HTTP_SUCCESS(manifest->mMPDDownloadResponse->iHttpRetValue));
	EXPECT_TRUE(manifest->mIsLiveManifest);
}

// Verify timeout of GetManifest(wait) returns curl 28.
TEST_F(FunctionalTests, GetManifest_ManifestTimeout)
{
	auto calls = std::make_shared<std::atomic<int>>(0);
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig>(-1);
	inpData->mTuneUrl = url1;
	mAampMPDDownloader->Initialize(inpData, appName, MakeLiveManifestPreProcess(calls));
	DownloaderReleaseGuard guard{mAampMPDDownloader};

	std::atomic<bool> waiterStarted(false);
	std::promise<ManifestDownloadResponsePtr> result;
	std::future<ManifestDownloadResponsePtr> resultFuture = result.get_future();
	std::thread waiter([&]()
	{
		waiterStarted = true;
		result.set_value(mAampMPDDownloader->GetManifest(true, 1000));
	});
	while (!waiterStarted)
	{
		std::this_thread::yield();
	}

	const bool delivered = resultFuture.wait_for(std::chrono::seconds(10)) == std::future_status::ready;

	waiter.join();

	ASSERT_TRUE(delivered);
	ManifestDownloadResponsePtr manifest = resultFuture.get();
	ASSERT_TRUE(manifest != nullptr);
	EXPECT_EQ(manifest->mMPDStatus, AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR);
	EXPECT_EQ(manifest->mMPDDownloadResponse->iHttpRetValue,CURLE_OPERATION_TIMEDOUT);

}

// Release() must wake a GetManifest(wait) caller instead of leaving it to run out its full timeout.
TEST_F(FunctionalTests, GetManifest_WaitReturnsPromptlyWhenReleaseCalled)
{
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig>(-1);
	inpData->mTuneUrl = url1;
	mAampMPDDownloader->Initialize(inpData);

	const int waitMs = 30000;
	const long long maxElapsedMs = 5000;
	std::atomic<bool> waiterStarted(false);
	ManifestDownloadResponsePtr manifest;
	long long elapsedMs = -1;
	std::thread waiter([&]()
	{
		waiterStarted = true;
		const auto start = std::chrono::steady_clock::now();
		manifest = mAampMPDDownloader->GetManifest(true, waitMs);
		elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	});
	while (!waiterStarted)
	{
		std::this_thread::yield();
	}

	mAampMPDDownloader->Release();
	waiter.join();

	ASSERT_TRUE(manifest != nullptr);
	EXPECT_LT(elapsedMs, maxElapsedMs);
	EXPECT_EQ(manifest->mMPDStatus, AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR);
	EXPECT_FALSE(IS_HTTP_SUCCESS(manifest->mMPDDownloadResponse->iHttpRetValue));
	// A timeout result would mean the Release() wakeup was missed.
	EXPECT_NE(manifest->mMPDDownloadResponse->iHttpRetValue, CURLE_OPERATION_TIMEDOUT);
}

// Release() must interrupt the refresh-interval wait and stop the download loop without another refresh.
TEST_F(FunctionalTests, Release_InterruptsRefreshWaitWithoutFurtherRefresh)
{
	auto calls = std::make_shared<std::atomic<int>>(0);
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig>(-1);
	inpData->mTuneUrl = url1;
	mAampMPDDownloader->Initialize(inpData, appName, MakeLiveManifestPreProcess(calls));
	DownloaderReleaseGuard guard{mAampMPDDownloader};
	mAampMPDDownloader->Start();

	ManifestDownloadResponsePtr first = mAampMPDDownloader->GetManifest(true, 5000);
	ASSERT_TRUE(first != nullptr);
	ASSERT_TRUE(IS_HTTP_SUCCESS(first->mMPDDownloadResponse->iHttpRetValue));
	ASSERT_TRUE(first->mIsLiveManifest);

	// The downloader now waits ~3s before its next refresh; Release() must not sit out that interval.
	const auto start = std::chrono::steady_clock::now();
	mAampMPDDownloader->Release();
	const long long releaseMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

	EXPECT_LT(releaseMs, 2000);
	EXPECT_EQ(calls->load(), 1);
	EXPECT_TRUE(mAampMPDDownloader->IsDownloaderDisabled());
}

// The notifier thread must invoke the registered callback when the downloader publishes a manifest.
TEST_F(FunctionalTests, RegisterCallback_InvokedWhenManifestPublished)
{
	NotifyContext ctx;
	auto calls = std::make_shared<std::atomic<int>>(0);
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig>(-1);
	inpData->mTuneUrl = url1;
	mAampMPDDownloader->Initialize(inpData, appName, MakeLiveManifestPreProcess(calls));
	DownloaderReleaseGuard guard{mAampMPDDownloader};
	mAampMPDDownloader->RegisterCallback(CountingNotifyCallback, &ctx);
	mAampMPDDownloader->Start();

	bool notified;
	{
		std::unique_lock<std::mutex> lock(ctx.mtx);
		notified = ctx.cv.wait_for(lock, std::chrono::seconds(10), [&ctx] { return ctx.count >= 1; });
	}

	EXPECT_TRUE(notified);
	mAampMPDDownloader->UnRegisterCallback();
	mAampMPDDownloader->Release();
}

// Waking the notifier thread for unregister/release must not be reported to the callback as a manifest update.
TEST_F(FunctionalTests, RegisterCallback_NotInvokedWhenNoManifestPublished)
{
	NotifyContext ctx;
	std::shared_ptr<ManifestDownloadConfig> inpData = std::make_shared<ManifestDownloadConfig>(-1);
	inpData->mTuneUrl = url1;
	mAampMPDDownloader->Initialize(inpData);
	DownloaderReleaseGuard guard{mAampMPDDownloader};
	mAampMPDDownloader->RegisterCallback(CountingNotifyCallback, &ctx);

	mAampMPDDownloader->UnRegisterCallback();
	mAampMPDDownloader->Release();

	std::lock_guard<std::mutex> lock(ctx.mtx);
	EXPECT_EQ(ctx.count, 0);
}

/**
 * @brief Verifies that a successful refresh clears any in-progress failure streak.
 *
 * Scenario:
 * - First update succeeds (live manifest).
 * - Next update fails once (CURLE_RECV_ERROR).
 * - Following update succeeds again.
 *
 * Oracle:
 * - After a single failure, GetManifestRefreshStatus() must still return empty
 *   status because threshold is not reached.
 * - After recovery (status OK), retry-failure count must reset to 0 and
 *   GetManifestRefreshStatus() must remain empty (OK/0).
 *
 * Why this matters:
 * - Prevents stale one-off refresh failures from being surfaced during buffering
 *   after stream recovery.
 */
TEST_F(FunctionalTests, AampMPDDownloader_RefreshStatus_OkResetsCounterAndStatus)
{
	std::shared_ptr<ManifestDownloadConfig> inpData =
	std::make_shared<ManifestDownloadConfig>(-1);
	inpData->mTuneUrl = url1;
	std::vector<std::pair<std::string, int>> callbackSequence =
	{
		{std::string(kLiveMpdManifestForRefreshStatusTests), 200},
		{std::string(), CURLE_RECV_ERROR},
		{std::string(kLiveMpdManifestForRefreshStatusTests), 200}
	};

	std::atomic<size_t> callbackIndex(0);
	auto preProcessCallback = [&callbackSequence, &callbackIndex]()
		-> std::pair<std::string, int>
	{
		size_t index = callbackIndex.fetch_add(1);
		if (index >= callbackSequence.size())
		{
			index = callbackSequence.size() - 1;
		}
		return callbackSequence[index];
	};

	mAampMPDDownloader->Initialize(inpData, appName, preProcessCallback);
	mAampMPDDownloader->Start();

	ManifestDownloadResponsePtr firstManifest =
		mAampMPDDownloader->GetManifest(true, 2000);
	ASSERT_TRUE(firstManifest != nullptr);
	ASSERT_TRUE(firstManifest->mIsLiveManifest);

	ManifestDownloadResponsePtr secondManifest =
		WaitForNextManifest(mAampMPDDownloader, firstManifest, 2000);
	ASSERT_TRUE(secondManifest != nullptr);
	ASSERT_TRUE(secondManifest.get() != firstManifest.get());
	EXPECT_EQ(secondManifest->mMPDDownloadResponse->iHttpRetValue,
		CURLE_RECV_ERROR);

	ManifestRefreshStatus statusAfterFirstFailure =
		mAampMPDDownloader->GetManifestRefreshStatus();
	EXPECT_EQ(statusAfterFirstFailure.type, AAMPStatusType::eAAMPSTATUS_OK);
	EXPECT_EQ(statusAfterFirstFailure.errorCode, 0);

	ManifestDownloadResponsePtr thirdManifest =
		WaitForNextManifest(mAampMPDDownloader, secondManifest, 2000);
	ASSERT_TRUE(thirdManifest != nullptr);
	ASSERT_TRUE(thirdManifest.get() != secondManifest.get());
	EXPECT_TRUE(IS_HTTP_SUCCESS(thirdManifest->mMPDDownloadResponse->iHttpRetValue));

	ManifestRefreshStatus statusAfterRecovery =
		mAampMPDDownloader->GetManifestRefreshStatus();
	EXPECT_EQ(statusAfterRecovery.type, AAMPStatusType::eAAMPSTATUS_OK);
	EXPECT_EQ(statusAfterRecovery.errorCode, 0);

	mAampMPDDownloader->Release();
}

/**
 * @brief Verifies threshold gating for repeated identical failures.
 *
 * Scenario:
 * - First update succeeds (live manifest).
 * - Next two updates fail with the same error (CURLE_RECV_ERROR).
 *
 * Oracle:
 * - After first identical failure: status must remain empty (count=1, below threshold 2).
 * - After second identical failure: status must be exposed as
 *   MANIFEST_DOWNLOAD_ERROR with CURLE_RECV_ERROR.
 *
 * Why this matters:
 * - Confirms that manifest error propagation occurs only after consistent,
 *   repeated failure, reducing false-positive fatal signaling.
 */
TEST_F(FunctionalTests, AampMPDDownloader_RefreshStatus_SameFailureTwice_ReachesThreshold)
{
	std::shared_ptr<ManifestDownloadConfig> inpData =
	std::make_shared<ManifestDownloadConfig>(-1);
	inpData->mTuneUrl = url1;
	std::vector<std::pair<std::string, int>> callbackSequence =
	{
		{std::string(kLiveMpdManifestForRefreshStatusTests), 200},
		{std::string(), CURLE_RECV_ERROR},
		{std::string(), CURLE_RECV_ERROR}
	};

	std::atomic<size_t> callbackIndex(0);
	auto preProcessCallback = [&callbackSequence, &callbackIndex]()
		-> std::pair<std::string, int>
	{
		size_t index = callbackIndex.fetch_add(1);
		if (index >= callbackSequence.size())
		{
			index = callbackSequence.size() - 1;
		}
		return callbackSequence[index];
	};

	mAampMPDDownloader->Initialize(inpData, appName, preProcessCallback);
	mAampMPDDownloader->Start();

	ManifestDownloadResponsePtr firstManifest =
		mAampMPDDownloader->GetManifest(true, 2000);
	ASSERT_TRUE(firstManifest != nullptr);
	ASSERT_TRUE(firstManifest->mIsLiveManifest);

	ManifestDownloadResponsePtr secondManifest =
		WaitForNextManifest(mAampMPDDownloader, firstManifest, 2000);
	ASSERT_TRUE(secondManifest != nullptr);
	ASSERT_TRUE(secondManifest.get() != firstManifest.get());
	EXPECT_EQ(secondManifest->mMPDDownloadResponse->iHttpRetValue,
		CURLE_RECV_ERROR);

	ManifestRefreshStatus statusAfterFirstFailure =
		mAampMPDDownloader->GetManifestRefreshStatus();
	EXPECT_EQ(statusAfterFirstFailure.type, AAMPStatusType::eAAMPSTATUS_OK);
	EXPECT_EQ(statusAfterFirstFailure.errorCode, 0);

	ManifestDownloadResponsePtr thirdManifest =
		WaitForNextManifest(mAampMPDDownloader, secondManifest, 2000);
	ASSERT_TRUE(thirdManifest != nullptr);
	ASSERT_TRUE(thirdManifest.get() != secondManifest.get());
	EXPECT_EQ(thirdManifest->mMPDDownloadResponse->iHttpRetValue,
		CURLE_RECV_ERROR);

	ManifestRefreshStatus statusAfterSecondSameFailure =
		mAampMPDDownloader->GetManifestRefreshStatus();
	EXPECT_EQ(statusAfterSecondSameFailure.type,
	AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR);
	EXPECT_EQ(statusAfterSecondSameFailure.errorCode, CURLE_RECV_ERROR);

	mAampMPDDownloader->Release();
}

/**
 * @brief Verifies that different consecutive failure types do not accumulate.
 *
 * Scenario:
 * - First update succeeds (live manifest).
 * - Next update fails with transport/download error (CURLE_RECV_ERROR).
 * - Next update fails with a different class (parse/content via malformed MPD).
 *
 * Oracle:
 * - First failure alone must not surface status.
 * - Changed failure type must start a new streak (count=1), so status must still
 *   remain empty.
 *
 * Why this matters:
 * - Ensures buffering-side manifest error emission is not triggered by mixed,
 *   unrelated transient failures.
 */
TEST_F(FunctionalTests, AampMPDDownloader_RefreshStatus_FailureTypeChanges_DoesNotAccumulate)
{
	std::shared_ptr<ManifestDownloadConfig> inpData =
	std::make_shared<ManifestDownloadConfig>(-1);
	inpData->mTuneUrl = url1;
	std::vector<std::pair<std::string, int>> callbackSequence =
	{
		{std::string(kLiveMpdManifestForRefreshStatusTests), 200},
		{std::string(), CURLE_RECV_ERROR},
		{std::string(), CURLE_COULDNT_CONNECT}
	};

	std::atomic<size_t> callbackIndex(0);
	auto preProcessCallback = [&callbackSequence, &callbackIndex]()
		-> std::pair<std::string, int>
	{
		size_t index = callbackIndex.fetch_add(1);
		if (index >= callbackSequence.size())
		{
			index = callbackSequence.size() - 1;
		}
		return callbackSequence[index];
	};

	mAampMPDDownloader->Initialize(inpData, appName, preProcessCallback);
	mAampMPDDownloader->Start();

	ManifestDownloadResponsePtr firstManifest =
		mAampMPDDownloader->GetManifest(true, 2000);
	ASSERT_TRUE(firstManifest != nullptr);
	ASSERT_TRUE(firstManifest->mIsLiveManifest);

	ManifestDownloadResponsePtr secondManifest =
		WaitForNextManifest(mAampMPDDownloader, firstManifest, 2000);
	ASSERT_TRUE(secondManifest != nullptr);
	ASSERT_TRUE(secondManifest.get() != firstManifest.get());
	EXPECT_EQ(secondManifest->mMPDDownloadResponse->iHttpRetValue,
 		CURLE_RECV_ERROR);

	ManifestDownloadResponsePtr thirdManifest =
		WaitForNextManifest(mAampMPDDownloader, secondManifest, 2000);
	ASSERT_TRUE(thirdManifest != nullptr);
	ASSERT_TRUE(thirdManifest.get() != secondManifest.get());
	EXPECT_EQ(thirdManifest->mMPDDownloadResponse->iHttpRetValue,
		CURLE_COULDNT_CONNECT);
	EXPECT_NE(thirdManifest->mMPDStatus, AAMPStatusType::eAAMPSTATUS_OK);

	ManifestRefreshStatus statusAfterChangedFailure =
		mAampMPDDownloader->GetManifestRefreshStatus();
	EXPECT_EQ(statusAfterChangedFailure.type, AAMPStatusType::eAAMPSTATUS_OK);
	EXPECT_EQ(statusAfterChangedFailure.errorCode, 0);

	mAampMPDDownloader->Release();
}

/**
 * @brief Verifies streak reset-and-rebuild behavior after failure-type switch.
 *
 * Scenario:
 * - First update succeeds (live manifest).
 * - Failure A occurs once (CURLE_RECV_ERROR).
 * - Failure B occurs once (CURLE_COULDNT_CONNECT) -> streak should reset.
 * - Failure B occurs again consecutively.
 *
 * Oracle:
 * - After first occurrence of failure B: status must remain empty (count=1 for B).
 * - After second consecutive occurrence of failure B: status must be exposed as
 *   MANIFEST_DOWNLOAD_ERROR with CURLE_COULDNT_CONNECT.
 *
 * Why this matters:
 * - Confirms that only consecutive same-error failures cross threshold, exactly
 *   matching production suppression intent.
 */
TEST_F(FunctionalTests, AampMPDDownloader_RefreshStatus_AfterChange_NewTypeNeedsOwnStreak)
{
	std::shared_ptr<ManifestDownloadConfig> inpData =
	std::make_shared<ManifestDownloadConfig>(-1);
	inpData->mTuneUrl = url1;
	std::vector<std::pair<std::string, int>> callbackSequence =
	{
		{std::string(kLiveMpdManifestForRefreshStatusTests), 200},
		{std::string(), CURLE_RECV_ERROR},
		{std::string(), CURLE_COULDNT_CONNECT},
		{std::string(), CURLE_COULDNT_CONNECT}
	};

	std::atomic<size_t> callbackIndex(0);
	auto preProcessCallback = [&callbackSequence, &callbackIndex]()
		-> std::pair<std::string, int>
	{
		size_t index = callbackIndex.fetch_add(1);
		if (index >= callbackSequence.size())
		{
			index = callbackSequence.size() - 1;
		}
		return callbackSequence[index];
	};

	mAampMPDDownloader->Initialize(inpData, appName, preProcessCallback);
	mAampMPDDownloader->Start();

	ManifestDownloadResponsePtr firstManifest =
		mAampMPDDownloader->GetManifest(true, 2000);
	ASSERT_TRUE(firstManifest != nullptr);
	ASSERT_TRUE(firstManifest->mIsLiveManifest);

	ManifestDownloadResponsePtr secondManifest =
		WaitForNextManifest(mAampMPDDownloader, firstManifest, 2000);
	ASSERT_TRUE(secondManifest != nullptr);
	ASSERT_TRUE(secondManifest.get() != firstManifest.get());

	ManifestDownloadResponsePtr thirdManifest =
		WaitForNextManifest(mAampMPDDownloader, secondManifest, 2000);
	ASSERT_TRUE(thirdManifest != nullptr);
	ASSERT_TRUE(thirdManifest.get() != secondManifest.get());

	ManifestRefreshStatus statusAfterFirstFailureB =
		mAampMPDDownloader->GetManifestRefreshStatus();
	EXPECT_EQ(statusAfterFirstFailureB.type, AAMPStatusType::eAAMPSTATUS_OK);
	EXPECT_EQ(statusAfterFirstFailureB.errorCode, 0);

	ManifestDownloadResponsePtr fourthManifest =
		WaitForNextManifest(mAampMPDDownloader, thirdManifest, 2000);
	ASSERT_TRUE(fourthManifest != nullptr);
	ASSERT_TRUE(fourthManifest.get() != thirdManifest.get());

	ManifestRefreshStatus statusAfterSecondFailureB =
		mAampMPDDownloader->GetManifestRefreshStatus();
	EXPECT_EQ(statusAfterSecondFailureB.type,
	AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR);
	EXPECT_EQ(statusAfterSecondFailureB.errorCode, CURLE_COULDNT_CONNECT);

	mAampMPDDownloader->Release();
}
