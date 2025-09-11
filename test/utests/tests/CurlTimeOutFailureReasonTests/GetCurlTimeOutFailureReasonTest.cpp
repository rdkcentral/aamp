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

/**/

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "fragmentcollector_mpd.h"
#include "MediaStreamContext.h"
#include "MockAampConfig.h"
#include "MockAampUtils.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampMPDDownloader.h"

using ::testing::_;
using ::testing::An;
using ::testing::SetArgReferee;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::WithArgs;
using ::testing::WithoutArgs;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;

AampConfig *gpGlobalConfig{nullptr};

/**
 * @brief Functional tests common base class.
 */
class FunctionalTestsBase
{
protected:
	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	StreamAbstractionAAMP_MPD *mStreamAbstractionAAMP_MPD;
	CDAIObject *mCdaiObj;
	const char *mManifest;
	static constexpr const char *TEST_HOST_URL = "http://host/";
	static constexpr const char *TEST_BASE_URL = "http://host/asset/";
	static constexpr const char *TEST_MANIFEST_URL = "http://host/asset/manifest.mpd";
	std::string mManifestUrl {TEST_MANIFEST_URL};
	ManifestDownloadResponsePtr mResponse =  MakeSharedManifestDownloadResponsePtr();
	using BoolConfigSettings = std::map<AAMPConfigSettingBool, bool>;
	using IntConfigSettings = std::map<AAMPConfigSettingInt, int>;

	BoolConfigSettings mBoolConfigSettings;
	IntConfigSettings mIntConfigSettings;

	void SetUp()
	{
		mStreamAbstractionAAMP_MPD = nullptr;
		mManifest = nullptr;
		mResponse = nullptr;
	}

	void TearDown()
	{
		mManifest = nullptr;
	}

public:
	/**
	 * @brief Get manifest helper method
	 *
	 * @param[in] remoteUrl Manifest url
	 * @param[out] buffer Buffer containing manifest data
	 * @retval true on success
	*/
	bool GetManifest(std::string remoteUrl, AampGrowableBuffer *buffer)
	{
		EXPECT_STREQ(remoteUrl.c_str(), mManifestUrl.c_str());

		/* Setup fake AampGrowableBuffer contents. */
		buffer->Clear();
		buffer->AppendBytes((char *)mManifest, strlen(mManifest));

		return true;
	}


	void GetMPDFromManifest(ManifestDownloadResponsePtr response)
	{
		dash::mpd::MPD* mpd = nullptr;
		std::string manifestStr = std::string( response->mMPDDownloadResponse->mDownloadData.begin(), response->mMPDDownloadResponse->mDownloadData.end());

		xmlTextReaderPtr reader = xmlReaderForMemory( (char *)manifestStr.c_str(), (int) manifestStr.length(), NULL, NULL, 0);
		if (reader != NULL)
		{
			if (xmlTextReaderRead(reader))
			{
				response->mRootNode = MPDProcessNode(&reader, mManifestUrl);
				if(response->mRootNode != NULL)
				{
					mpd = response->mRootNode->ToMPD();
					if (mpd)
					{
						std::shared_ptr<dash::mpd::IMPD> tmp_ptr(mpd);
						response->mMPDInstance	=	tmp_ptr;
						response->GetMPDParseHelper()->Initialize(mpd);
					}
				}
			}
		}
		xmlFreeTextReader(reader);
	}

	/**
	 * @brief Get manifest helper method for MPDDownloader
	 *
	 * @param[in] remoteUrl Manifest url
	 * @param[out] buffer Buffer containing manifest data
	 * @retval true on success
	*/
	ManifestDownloadResponsePtr GetManifestForMPDDownloaderForDNSTimeout()
	{
		ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
		response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
		response->mMPDDownloadResponse->iHttpRetValue = CURLE_OPERATION_TIMEDOUT;
		response->mMPDDownloadResponse->mCurlTimeoutFailureReason = eCURL_TIMEOUT_DNS;
		response->mMPDDownloadResponse->sEffectiveUrl = mManifestUrl;
		response->mMPDDownloadResponse->mDownloadData.assign((uint8_t*)mManifest, (uint8_t*)(mManifest + strlen(mManifest)));
		GetMPDFromManifest(response);
		mResponse = response;
		return response;
	}
	/**
	 * @brief Get manifest helper method for MPDDownloader
	 *
	 * @param[in] remoteUrl Manifest url
	 * @param[out] buffer Buffer containing manifest data
	 * @retval true on success
	*/
	ManifestDownloadResponsePtr GetManifestForMPDDownloaderForConnectTimeout()
	{
		ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
		response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
		response->mMPDDownloadResponse->iHttpRetValue = CURLE_OPERATION_TIMEDOUT;
		response->mMPDDownloadResponse->mCurlTimeoutFailureReason = eCURL_TIMEOUT_CONNECT;
		response->mMPDDownloadResponse->sEffectiveUrl = mManifestUrl;
		response->mMPDDownloadResponse->mDownloadData.assign((uint8_t*)mManifest, (uint8_t*)(mManifest + strlen(mManifest)));
		GetMPDFromManifest(response);
		mResponse = response;
		return response;
	}
	/**
	 * @brief Get manifest helper method for MPDDownloader
	 *
	 * @param[in] remoteUrl Manifest url
	 * @param[out] buffer Buffer containing manifest data
	 * @retval true on success
	*/
	ManifestDownloadResponsePtr GetManifestForMPDDownloaderForDataTimeout()
	{
		ManifestDownloadResponsePtr response = MakeSharedManifestDownloadResponsePtr();
		response->mMPDStatus = AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR;
		response->mMPDDownloadResponse->iHttpRetValue = CURLE_OPERATION_TIMEDOUT;
		response->mMPDDownloadResponse->mCurlTimeoutFailureReason = eCURL_TIMEOUT_DATA;
		response->mMPDDownloadResponse->sEffectiveUrl = mManifestUrl;
		response->mMPDDownloadResponse->mDownloadData.assign((uint8_t*)mManifest, (uint8_t*)(mManifest + strlen(mManifest)));
		GetMPDFromManifest(response);
		mResponse = response;
		return response;
	}
};
class StreamAbstractionAAMP_MPDTest_1 : public FunctionalTestsBase,
										public ::testing::Test
{
protected:

	class TestableStreamAbstractionAAMP_MPD_1 : public StreamAbstractionAAMP_MPD
	{
	public:
		// Constructor to pass parameters to the base class constructor
		TestableStreamAbstractionAAMP_MPD_1(PrivateInstanceAAMP *aamp,
											double seekpos, float rate)
			: StreamAbstractionAAMP_MPD(aamp, seekpos, rate)
		{
		}
		AAMPStatusType CallFetchDashManifest()
		{
			return FetchDashManifest();
		}
	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP;
	TestableStreamAbstractionAAMP_MPD_1 *mStreamAbstractionAAMP_MPD;

	void SetUp() override
	{
		// Set up your objects before each test case
		mPrivateInstanceAAMP = new PrivateInstanceAAMP();
		g_mockPrivateInstanceAAMP = new NiceMock<MockPrivateInstanceAAMP>();
		mStreamAbstractionAAMP_MPD = new TestableStreamAbstractionAAMP_MPD_1(mPrivateInstanceAAMP, 0.0, 1.0);

		g_mockAampMPDDownloader = new StrictMock<MockAampMPDDownloader>();
		g_mockAampUtils = new StrictMock<MockAampUtils>();
	}

	void TearDown() override
	{
		// Clean up your objects after each test
		delete mStreamAbstractionAAMP_MPD;
		mStreamAbstractionAAMP_MPD = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		delete g_mockAampMPDDownloader;
		g_mockAampMPDDownloader = nullptr;

		delete g_mockAampUtils;
		g_mockAampUtils = nullptr;
	}
};

TEST_F(StreamAbstractionAAMP_MPDTest_1, FetchDashManifest_DNSTimeout_WithManifestDownloadError)
{
    EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest (_, _, _))
			.WillOnce(WithoutArgs(Invoke(this, &FunctionalTestsBase::GetManifestForMPDDownloaderForDNSTimeout)));

    // Mock error handling
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
        .WillOnce(Return(true));
		
	//Ensure proper error event is sent
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDownloadErrorEvent(AAMP_TUNE_DNS_RESOLVE_TIMEOUT, CURLE_OPERATION_TIMEDOUT))
        .Times(1);
    
    // Execute test
    AAMPStatusType result = mStreamAbstractionAAMP_MPD->CallFetchDashManifest();
    EXPECT_EQ(result, AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR);
}
TEST_F(StreamAbstractionAAMP_MPDTest_1, FetchDashManifest_ConnectTimeout_WithManifestDownloadError)
{
    EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest (_, _, _))
			.WillOnce(WithoutArgs(Invoke(this, &FunctionalTestsBase::GetManifestForMPDDownloaderForConnectTimeout)));
    
    // Mock error handling
    EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
        .WillOnce(Return(true));

	//Ensure proper error event is sent	
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDownloadErrorEvent(AAMP_TUNE_CURL_CONNECTION_TIMEOUT, CURLE_OPERATION_TIMEDOUT))
        .Times(1);
    
    // Execute test
    AAMPStatusType result = mStreamAbstractionAAMP_MPD->CallFetchDashManifest();
    EXPECT_EQ(result, AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR);
}
TEST_F(StreamAbstractionAAMP_MPDTest_1, FetchDashManifest_DataTimeout_WithManifestDownloadError)
{
	EXPECT_CALL(*g_mockAampMPDDownloader, GetManifest (_, _, _))
			.WillOnce(WithoutArgs(Invoke(this, &FunctionalTestsBase::GetManifestForMPDDownloaderForDataTimeout)));
	
	// Mock error handling
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, DownloadsAreEnabled())
		.WillOnce(Return(true));

	//Ensure proper error event is sent
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SendDownloadErrorEvent(AAMP_TUNE_DATA_TRANSFER_TIMEOUT, CURLE_OPERATION_TIMEDOUT))
        .Times(1);
	
	// Execute test
	AAMPStatusType result = mStreamAbstractionAAMP_MPD->CallFetchDashManifest();
	EXPECT_EQ(result, AAMPStatusType::eAAMPSTATUS_MANIFEST_DOWNLOAD_ERROR);
}