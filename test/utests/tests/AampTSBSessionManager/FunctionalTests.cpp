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
#include <chrono>

// Core headers
#include "AampTSBSessionManager.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "AampMediaType.h"
#include "AampTsbDataManager.h"
#include "AampTsbMetaDataManager.h"
#include "AampTsbReader.h"
#include "AampTsbAdMetaData.h"
#include "AampTsbAdPlacementMetaData.h"
#include "AampTsbAdReservationMetaData.h"

// Mock headers
#include "MockTSBStore.h"
#include "MockMediaStreamContext.h"
#include "MockPrivateInstanceAAMP.h"
#include "MockAampConfig.h"
#include "MockAampUtils.h"
#include "MockTsbMetaDataManager.h"
#include "MockAampTsbAdMetaData.h"

#include <thread>
#include <unistd.h>

using ::testing::_;
using ::testing::DoAll;
using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArgPointee;
using ::testing::SetArgPointee;
using ::testing::NiceMock;
using ::testing::Invoke;

AampConfig *gpGlobalConfig{nullptr};

class FunctionalTests : public ::testing::Test
{
protected:
	AampTSBSessionManager *mAampTSBSessionManager;
	PrivateInstanceAAMP *aamp{};
	static constexpr const char *TEST_BASE_URL = "http://server/";
	static constexpr const char *TEST_DATA = "This is a dummy data";
	std::string TEST_PERIOD_ID = "1";
	std::shared_ptr<TSB::Store> mTSBStore;

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}
		g_mockAampConfig = new NiceMock<MockAampConfig>();
		// Set TSB log level to TRACE
		EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_TsbLogLevel))
			.WillOnce(Return(static_cast<int>(TSB::LogLevel::TRACE)));

		aamp = new PrivateInstanceAAMP(gpGlobalConfig);
		mAampTSBSessionManager = new AampTSBSessionManager(aamp);
		TSB::Store::Config config;
		mTSBStore = std::make_shared<TSB::Store>(config, AampLogManager::aampLogger, TSB::LogLevel::TRACE);
		g_mockTSBStore = new MockTSBStore();
		g_mockMediaStreamContext = new StrictMock<MockMediaStreamContext>();
		g_mockPrivateInstanceAAMP = new StrictMock<MockPrivateInstanceAAMP>();
		g_mockAampUtils = new NiceMock<MockAampUtils>();
		g_mockAampTsbMetaDataManager = new StrictMock<MockAampTsbMetaDataManager>();

		EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetTSBStore(_,_,_)).WillRepeatedly(Return(mTSBStore));
		mAampTSBSessionManager->SetTsbLength(5);
		mAampTSBSessionManager->SetTsbLocation("/tmp");
		mAampTSBSessionManager->SetTsbMinFreePercentage(5);

		// Mock metadata manager initialization
		EXPECT_CALL(*g_mockAampTsbMetaDataManager, Initialize())
			.WillOnce(Return());

		// Mock successful registration of AD_METADATA_TYPE
		EXPECT_CALL(*g_mockAampTsbMetaDataManager, RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, true))
			.WillOnce(Return(true));

		// Initialize necessary objects and configurations
		mAampTSBSessionManager->Init();
		// Wait to mWriteThread to start, we need to optimize this later
		std::this_thread::sleep_for(std::chrono::milliseconds(25));
	}

	void TearDown() override
	{
		EXPECT_CALL(*g_mockTSBStore, Flush()).Times(1);
		mAampTSBSessionManager->Flush();

		delete g_mockAampTsbMetaDataManager;
		g_mockAampTsbMetaDataManager = nullptr;

		delete g_mockAampUtils;
		g_mockAampUtils = nullptr;

		delete g_mockPrivateInstanceAAMP;
		g_mockPrivateInstanceAAMP = nullptr;

		delete mAampTSBSessionManager;
		mAampTSBSessionManager = nullptr;

		mTSBStore = nullptr;

		delete g_mockTSBStore;
		g_mockTSBStore = nullptr;

		delete g_mockMediaStreamContext;
		g_mockMediaStreamContext = nullptr;

		delete g_mockAampConfig;
		g_mockAampConfig = nullptr;
	}

};

TEST_F(FunctionalTests, ConvertMediaType)
{
	AampMediaType convertedType = mAampTSBSessionManager->ConvertMediaType(eMEDIATYPE_INIT_VIDEO);
	EXPECT_EQ(convertedType, eMEDIATYPE_VIDEO);

	convertedType = mAampTSBSessionManager->ConvertMediaType(eMEDIATYPE_INIT_AUDIO);
	EXPECT_EQ(convertedType, eMEDIATYPE_AUDIO);

	convertedType = mAampTSBSessionManager->ConvertMediaType(eMEDIATYPE_INIT_SUBTITLE);
	EXPECT_EQ(convertedType, eMEDIATYPE_SUBTITLE);

	convertedType = mAampTSBSessionManager->ConvertMediaType(eMEDIATYPE_INIT_AUX_AUDIO);
	EXPECT_EQ(convertedType, eMEDIATYPE_AUX_AUDIO);

	convertedType = mAampTSBSessionManager->ConvertMediaType(eMEDIATYPE_INIT_IFRAME);
	EXPECT_EQ(convertedType, eMEDIATYPE_IFRAME);
}

TEST_F(FunctionalTests, TSBWriteTests)
{
	std::shared_ptr<CachedFragment> cachedFragment = std::make_shared<CachedFragment>();
	double FRAG_DURATION = 3.0;

	cachedFragment->initFragment = true;
	cachedFragment->duration = 0;
	cachedFragment->position = 0;
	cachedFragment->absPosition = 1234.0;
	cachedFragment->fragment.AppendBytes(TEST_DATA, strlen(TEST_DATA));

	// Add video init fragment to TSB successfullly
	const std::string INIT_URL = std::string(TEST_BASE_URL) + std::string("vinit.mp4");
	const std::string UNIQUE_INIT_URL = INIT_URL + std::string(".1234");

	cachedFragment->type = eMEDIATYPE_INIT_VIDEO;

	EXPECT_CALL(*g_mockTSBStore, Write(UNIQUE_INIT_URL, TEST_DATA, strlen(TEST_DATA))).WillOnce(Return(TSB::Status::OK));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetVidTimeScale()).WillRepeatedly(Return(1));
	mAampTSBSessionManager->EnqueueWrite(INIT_URL, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	// Add video init fragment to TSB which already exists
	EXPECT_CALL(*g_mockTSBStore, Write(UNIQUE_INIT_URL, TEST_DATA, strlen(TEST_DATA))).WillOnce(Return(TSB::Status::ALREADY_EXISTS));
	mAampTSBSessionManager->EnqueueWrite(INIT_URL, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	// Add video fragment 1 to TSB successfully
	cachedFragment->type = eMEDIATYPE_VIDEO;
	cachedFragment->initFragment = 0;
	cachedFragment->duration = FRAG_DURATION;  // 3
	cachedFragment->position += FRAG_DURATION; // pos = 0
	const std::string VIDEO1_URL = std::string(TEST_BASE_URL) + std::string("video1.mp4");
	const std::string UNIQUE_VIDEO1_URL = VIDEO1_URL + ".1234";

	EXPECT_CALL(*g_mockTSBStore, Write(UNIQUE_VIDEO1_URL,_,_)).WillOnce(Return(TSB::Status::OK));
	mAampTSBSessionManager->EnqueueWrite(VIDEO1_URL, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	// Add video fragment 2 to TSB successfully
	const std::string VIDEO2_URL = std::string(TEST_BASE_URL) + std::string("video2.mp4");
	const std::string UNIQUE_VIDEO2_URL = VIDEO2_URL + ".1234";
	cachedFragment->position += FRAG_DURATION; // pos = 3

	EXPECT_CALL(*g_mockTSBStore, Write(UNIQUE_VIDEO2_URL,_,_)).WillOnce(Return(TSB::Status::ALREADY_EXISTS));
	mAampTSBSessionManager->EnqueueWrite(VIDEO2_URL, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));
	double TSBDuration = mAampTSBSessionManager->GetTotalStoreDuration(eMEDIATYPE_VIDEO);
	EXPECT_DOUBLE_EQ(TSBDuration, FRAG_DURATION);

	// Add video fragment 3 to TSB which fails with no space and then writes on next iteration
	const std::string VIDEO3_URL = std::string(TEST_BASE_URL) + std::string("video3.mp4");
	const std::string UNIQUE_VIDEO3_URL = VIDEO3_URL + ".1234";
	cachedFragment->position += FRAG_DURATION; // pos = 6

	EXPECT_CALL(*g_mockTSBStore, Write(UNIQUE_VIDEO3_URL,_,_))
		.WillOnce(Return(TSB::Status::NO_SPACE))
		.WillOnce(Return(TSB::Status::OK));

	EXPECT_CALL(*g_mockTSBStore, Delete(UNIQUE_INIT_URL)).Times(1);
	EXPECT_CALL(*g_mockTSBStore, Delete(UNIQUE_VIDEO1_URL)).Times(1);
	mAampTSBSessionManager->EnqueueWrite(VIDEO3_URL, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	// Check the final TSB store duration is updated
	TSBDuration = mAampTSBSessionManager->GetTotalStoreDuration(eMEDIATYPE_VIDEO);
	EXPECT_DOUBLE_EQ(TSBDuration, FRAG_DURATION);

	// Reinitialise TSB and check that it is empty
	EXPECT_CALL(*g_mockTSBStore, Flush()).Times(1);
	mAampTSBSessionManager->Flush();
	EXPECT_FALSE(mAampTSBSessionManager->IsActive());
	TSBDuration = mAampTSBSessionManager->GetTotalStoreDuration(eMEDIATYPE_VIDEO);
	EXPECT_DOUBLE_EQ(TSBDuration, 0);

	EXPECT_CALL(*g_mockAampConfig, GetConfigValue(eAAMPConfig_TsbLogLevel))
		.WillOnce(Return(static_cast<int>(TSB::LogLevel::TRACE)));
	EXPECT_CALL(*g_mockAampTsbMetaDataManager, Initialize())
		.WillOnce(Return());
	// Mock successful registration of AD_METADATA_TYPE
	EXPECT_CALL(*g_mockAampTsbMetaDataManager, RegisterMetaDataType(AampTsbMetaData::Type::AD_METADATA_TYPE, true))
		.WillOnce(Return(true));
	mAampTSBSessionManager->Init();
	EXPECT_TRUE(mAampTSBSessionManager->IsActive());
}

TEST_F(FunctionalTests, Cullsegments)
{
	double FRAG_DURATION = 3.0;
	double MANIFEST_DURATION = 30.0;
	std::shared_ptr<CachedFragment> cachedFragment = std::make_shared<CachedFragment>();
	cachedFragment->initFragment = true;
	cachedFragment->fragment.AppendBytes(TEST_DATA, strlen(TEST_DATA));

	EXPECT_CALL(*g_mockTSBStore, Write(_,_,_)).WillRepeatedly(Return(TSB::Status::OK));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetVidTimeScale()).WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetAudTimeScale()).WillRepeatedly(Return(1));

	const std::string initUrl = std::string(TEST_BASE_URL) + std::string("init.mp4");
	cachedFragment->type = eMEDIATYPE_INIT_VIDEO;
	mAampTSBSessionManager->EnqueueWrite(initUrl, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	cachedFragment->type = eMEDIATYPE_INIT_AUDIO;
	mAampTSBSessionManager->EnqueueWrite(initUrl, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	std::string videoUrl = std::string(TEST_BASE_URL) + std::string("video.mp4");
	std::string audioUrl = std::string(TEST_BASE_URL) + std::string("audio.mp4");

	// int(cachedFragment->absPosition) is appended to make url unique
	const std::string videoUrl_unique = videoUrl + std::string(".4567");
	const std::string audioUrl_unique = audioUrl + std::string(".4567");

	cachedFragment->duration = FRAG_DURATION;
	cachedFragment->initFragment = false;
	cachedFragment->type = eMEDIATYPE_VIDEO;
	cachedFragment->absPosition = 4567;

	mAampTSBSessionManager->EnqueueWrite(videoUrl, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	cachedFragment->type = eMEDIATYPE_AUDIO;
	mAampTSBSessionManager->EnqueueWrite(audioUrl, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	// Add another set of video and audio fragments to exceed TSB length
	cachedFragment->position += FRAG_DURATION;
	cachedFragment->absPosition += FRAG_DURATION;
	cachedFragment->type = eMEDIATYPE_VIDEO;
	mAampTSBSessionManager->EnqueueWrite(videoUrl, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	cachedFragment->type = eMEDIATYPE_AUDIO;
	mAampTSBSessionManager->EnqueueWrite(audioUrl, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	double TSBDuration = mAampTSBSessionManager->GetTotalStoreDuration(eMEDIATYPE_VIDEO);
	EXPECT_DOUBLE_EQ(TSBDuration, FRAG_DURATION * 2);

	EXPECT_CALL(*g_mockTSBStore, Delete(videoUrl_unique)).Times(1);
	EXPECT_CALL(*g_mockTSBStore, Delete(audioUrl_unique)).Times(1);

	// Add expectation for metadata removal during culling with explicit matcher type
	EXPECT_CALL(*g_mockAampTsbMetaDataManager, RemoveMetaData(testing::An<const AampTime&>()))
		.WillRepeatedly(Return(true));

	mAampTSBSessionManager->UpdateProgress(MANIFEST_DURATION, 0);

	// Check TSB store duration after culling. Only one fragment each should be present.
	TSBDuration = mAampTSBSessionManager->GetTotalStoreDuration(eMEDIATYPE_VIDEO);
	EXPECT_DOUBLE_EQ(TSBDuration, FRAG_DURATION);
	TSBDuration = mAampTSBSessionManager->GetTotalStoreDuration(eMEDIATYPE_AUDIO);
	EXPECT_DOUBLE_EQ(TSBDuration, FRAG_DURATION);
}

TEST_F(FunctionalTests, TSBReadTests)
{
	constexpr double FRAG_FIRST_POS = 99.0;
	constexpr double FRAG_FIRST_ABS_POS = 999.0;
	constexpr double FRAG_DURATION = 2.0;
	constexpr double FRAG_FIRST_PTS = 69.0;
	constexpr double FRAG_PTS_OFFSET = -50.0;
	size_t TEST_DATA_LEN = strlen(TEST_DATA);
	class MediaStreamContext videoCtx(eTRACK_VIDEO, NULL, aamp, "video");

	std::shared_ptr<CachedFragment> cachedFragment = std::make_shared<CachedFragment>();
	cachedFragment->initFragment = true;
	cachedFragment->absPosition = FRAG_FIRST_ABS_POS;
	cachedFragment->fragment.AppendBytes(TEST_DATA, TEST_DATA_LEN);

	EXPECT_CALL(*g_mockTSBStore, Write(_,_,_)).WillRepeatedly(Return(TSB::Status::OK));
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, GetVidTimeScale()).WillRepeatedly(Return(1));
	EXPECT_CALL(*g_mockAampUtils, RecalculatePTS(eMEDIATYPE_INIT_VIDEO,_,_,_)).Times(1).WillOnce(Return(0.0));
	EXPECT_CALL(*g_mockAampUtils, RecalculatePTS(eMEDIATYPE_VIDEO,_,_,_)).Times(2).WillRepeatedly(Return(FRAG_FIRST_PTS));

	const std::string initUrl = std::string(TEST_BASE_URL) + std::string("init.mp4");
	const std::string videoUrl = std::string(TEST_BASE_URL) + std::string("video.mp4");

	// int(cachedFragment->absPosition) is appended to make url unique
	const std::string uniqueInitUrl = initUrl + std::string(".999");
	const std::string videoUrl_unique = videoUrl + std::string(".999");

	cachedFragment->type = eMEDIATYPE_INIT_VIDEO;
	mAampTSBSessionManager->EnqueueWrite(initUrl, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	cachedFragment->position = FRAG_FIRST_POS;
	cachedFragment->duration = FRAG_DURATION;
	cachedFragment->PTSOffsetSec = FRAG_PTS_OFFSET;
	cachedFragment->initFragment = false;
	cachedFragment->type = eMEDIATYPE_VIDEO;
	mAampTSBSessionManager->EnqueueWrite(videoUrl, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	cachedFragment->position += FRAG_DURATION;
	cachedFragment->absPosition += FRAG_DURATION;
	cachedFragment->PTSOffsetSec = FRAG_PTS_OFFSET;
	cachedFragment->type = eMEDIATYPE_VIDEO;
	mAampTSBSessionManager->EnqueueWrite(videoUrl, cachedFragment, TEST_PERIOD_ID);
	std::this_thread::sleep_for(std::chrono::milliseconds(25));

	double pos = FRAG_FIRST_ABS_POS;
	AAMPStatusType status = mAampTSBSessionManager->InvokeTsbReaders(pos, 1.0, eTUNETYPE_NEW_NORMAL);
	EXPECT_EQ(eAAMPSTATUS_OK, status);
	EXPECT_DOUBLE_EQ(FRAG_FIRST_ABS_POS, pos);

	EXPECT_TRUE(mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_VIDEO)->TrackEnabled());
	EXPECT_FALSE(mAampTSBSessionManager->GetTsbReader(eMEDIATYPE_AUDIO)->TrackEnabled());

	EXPECT_CALL(*g_mockTSBStore, GetSize(_)).WillRepeatedly(Return(TEST_DATA_LEN));
	EXPECT_CALL(*g_mockTSBStore, Read(uniqueInitUrl, _, _)).WillOnce(Return(TSB::Status::OK));
	EXPECT_CALL(*g_mockTSBStore, Read(videoUrl_unique, _, _)).WillOnce(Return(TSB::Status::OK));

	EXPECT_CALL(*g_mockMediaStreamContext, CacheTsbFragment(_))
		.Times(2)
		.WillOnce(Return(true))
		.WillOnce(Invoke([](std::shared_ptr<CachedFragment> fragment)
		{
			EXPECT_DOUBLE_EQ(fragment->position, FRAG_FIRST_PTS + FRAG_PTS_OFFSET);
			return true;
		}));

	std::list<std::shared_ptr<AampTsbMetaData>> metadataList;
	EXPECT_CALL(*g_mockAampTsbMetaDataManager,
		IsRegisteredType(AampTsbMetaData::Type::AD_METADATA_TYPE, _, _))
		.WillOnce(DoAll(
			testing::SetArgReferee<1>(true),  // Set isTransient to true
			testing::SetArgPointee<2>(&metadataList),  // Set metadata list pointer
			testing::Return(true)  // Return true from the function
		));

	bool result = mAampTSBSessionManager->PushNextTsbFragment(&videoCtx, 2);
	EXPECT_TRUE(result);
}

/**
 * @brief Test ad reservation and placement functionality
 */
TEST_F(FunctionalTests, AdMetadataTest)
{
	const std::string TEST_AD_BREAK_ID = "break123";
	const std::string TEST_AD_ID = "ad123";
	const uint64_t TEST_PERIOD_POSITION = 1000;
	const uint32_t TEST_REL_POSITION = 500;
	const uint32_t TEST_OFFSET = 100;
	const double TEST_DURATION = 30.0;
	AampTime TEST_ABS_POSITION(1234.0);

	// Set up expectations for all metadata additions
	EXPECT_CALL(*g_mockAampTsbMetaDataManager, AddMetaData(testing::An<const std::shared_ptr<AampTsbMetaData>&>()))
		.Times(5)  // We expect 5 calls for: StartAdReservation, EndAdReservation, StartAdPlacement, EndAdPlacement, EndAdPlacementWithError
		.WillRepeatedly(Return(true));

	// Test StartAdReservation
	EXPECT_TRUE(mAampTSBSessionManager->StartAdReservation(
		TEST_AD_BREAK_ID, TEST_PERIOD_POSITION, TEST_ABS_POSITION));

	// Test EndAdReservation
	EXPECT_TRUE(mAampTSBSessionManager->EndAdReservation(
		TEST_AD_BREAK_ID, TEST_PERIOD_POSITION, TEST_ABS_POSITION));

	// Test StartAdPlacement
	EXPECT_TRUE(mAampTSBSessionManager->StartAdPlacement(
		TEST_AD_ID, TEST_REL_POSITION, TEST_ABS_POSITION, TEST_DURATION, TEST_OFFSET));

	// Test EndAdPlacement
	EXPECT_TRUE(mAampTSBSessionManager->EndAdPlacement(
		TEST_AD_ID, TEST_REL_POSITION, TEST_ABS_POSITION, TEST_DURATION, TEST_OFFSET));

	// Test EndAdPlacementWithError
	EXPECT_TRUE(mAampTSBSessionManager->EndAdPlacementWithError(
		TEST_AD_ID, TEST_REL_POSITION, TEST_ABS_POSITION, TEST_DURATION, TEST_OFFSET));
}

/**
 * @brief Test ad metadata event processing order
 */
TEST_F(FunctionalTests, AdEventProcessingOrderTest)
{
	const std::string TEST_AD_BREAK_ID = "break123";
	const std::string TEST_AD_ID = "ad123";
	const uint64_t TEST_PERIOD_POSITION = 1000;
	const uint32_t TEST_REL_POSITION = 500;
	const uint32_t TEST_OFFSET = 100;
	const double TEST_DURATION = 30.0;

	// Add events at different positions
	AampTime pos1(10.0);
	AampTime pos2(20.0);
	AampTime pos3(30.0);

	// Setup expectations for metadata additions in non-chronological order
	// Note: testing::An<const std::shared_ptr<AampTsbMetaData>&>() matches the shared_ptr argument
	EXPECT_CALL(*g_mockAampTsbMetaDataManager, AddMetaData(testing::An<const std::shared_ptr<AampTsbMetaData>&>()))
		.Times(3)  // Expect 3 calls for StartAdPlacement, StartReservation, EndReservation
		.WillRepeatedly(Return(true));

	// Add events in non-chronological order
	EXPECT_TRUE(mAampTSBSessionManager->StartAdPlacement(
		TEST_AD_ID, TEST_REL_POSITION, pos2, TEST_DURATION, TEST_OFFSET));
	EXPECT_TRUE(mAampTSBSessionManager->StartAdReservation(
		TEST_AD_BREAK_ID, TEST_PERIOD_POSITION, pos1));
	EXPECT_TRUE(mAampTSBSessionManager->EndAdReservation(
		TEST_AD_BREAK_ID, TEST_PERIOD_POSITION, pos3));
}

/**
 * @brief Test ShiftFutureAdEvents functionality
 */
TEST_F(FunctionalTests, ShiftFutureAdEventsTest)
{
	const std::string TEST_AD_BREAK_ID = "break123";
	const std::string TEST_AD_ID = "ad123";
	const uint64_t TEST_PERIOD_POSITION = 1000;
	const uint32_t TEST_REL_POSITION = 500;
	const uint32_t TEST_OFFSET = 100;
	const double TEST_DURATION = 30.0;

	// Add events at different positions
	AampTime pos1(10.0);
	AampTime pos2(20.0);
	AampTime pos3(30.0);

	// Create metadata objects that will be returned by GetMetaDataByType
	auto reservation = std::make_shared<AampTsbAdReservationMetaData>(
		AampTsbAdMetaData::EventType::START, pos1, TEST_AD_BREAK_ID, TEST_PERIOD_POSITION);
	auto placement = std::make_shared<AampTsbAdPlacementMetaData>(
		AampTsbAdMetaData::EventType::START, pos2, TEST_DURATION, TEST_AD_ID, TEST_REL_POSITION, TEST_OFFSET);
	auto endPlacement = std::make_shared<AampTsbAdPlacementMetaData>(
		AampTsbAdMetaData::EventType::END, pos3, TEST_DURATION, TEST_AD_ID, TEST_REL_POSITION, TEST_OFFSET);

	std::list<std::shared_ptr<AampTsbMetaData>> metadataList = {reservation, placement, endPlacement};

	// Set up mock expectations
	EXPECT_CALL(*g_mockAampTsbMetaDataManager,
		IsRegisteredType(AampTsbMetaData::Type::AD_METADATA_TYPE, _, _))
		.WillOnce(DoAll(
			testing::SetArgReferee<1>(true),  // Set isTransient to true
			testing::SetArgPointee<2>(&metadataList),  // Set metadata list pointer
			testing::Return(true)  // Return true from the function
		));

	// Mock position change operation
	EXPECT_CALL(*g_mockAampTsbMetaDataManager,
		ChangeMetaDataPosition(testing::ContainerEq(metadataList), _))
		.WillOnce(Return(true));

	// Test the shift operation
	mAampTSBSessionManager->ShiftFutureAdEvents();
}

/**
 * @brief Test boundary conditions for ad metadata
 */
TEST_F(FunctionalTests, AdMetadataBoundaryTest)
{
	const std::string TEST_AD_BREAK_ID = "break123";
	const std::string TEST_AD_ID = "ad123";
	const uint64_t TEST_PERIOD_POSITION = 1000;
	const uint32_t TEST_REL_POSITION = 500;
	const uint32_t TEST_OFFSET = 100;
	const double TEST_DURATION = 30.0;

	// Test with zero position
	AampTime zeroPos(0.0);

	// Setup expectations for all metadata additions to succeed
	EXPECT_CALL(*g_mockAampTsbMetaDataManager, AddMetaData(_))
		.WillRepeatedly(Return(true));

	// Test cases start here
	EXPECT_TRUE(mAampTSBSessionManager->StartAdReservation(
		TEST_AD_BREAK_ID, TEST_PERIOD_POSITION, zeroPos));

	// Test with very large position
	AampTime largePos(999999.0);
	EXPECT_TRUE(mAampTSBSessionManager->StartAdPlacement(
		TEST_AD_ID, TEST_REL_POSITION, largePos, TEST_DURATION, TEST_OFFSET));

	// Test with empty ad break ID
	EXPECT_TRUE(mAampTSBSessionManager->StartAdReservation(
		"", TEST_PERIOD_POSITION, zeroPos));

	// Test with empty ad ID
	EXPECT_TRUE(mAampTSBSessionManager->StartAdPlacement(
		"", TEST_REL_POSITION, zeroPos, TEST_DURATION, TEST_OFFSET));

	// Test with zero duration
	EXPECT_TRUE(mAampTSBSessionManager->StartAdPlacement(
		TEST_AD_ID, TEST_REL_POSITION, zeroPos, 0.0, TEST_OFFSET));
}
