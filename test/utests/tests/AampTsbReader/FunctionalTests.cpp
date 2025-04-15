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
#include <thread>
#include <chrono>
#include "AampTsbReader.h"
#include "AampTsbDataManager.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include "AampUtils.h"
#include "MockTSBDataManager.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;


AampConfig *gpGlobalConfig{nullptr};

class FunctionalTests : public ::testing::Test
{
protected:
	class TestableAampTsbReader : public AampTsbReader
	{
	public:
		TestableAampTsbReader(PrivateInstanceAAMP *aamp, std::shared_ptr<AampTsbDataManager> dataMgr, AampMediaType mediaType, std::string sessionId)
			: AampTsbReader(aamp, dataMgr, mediaType, sessionId)
		{
		}

		void CallCheckPeriodBoundary(TsbFragmentDataPtr currFragment)
		{
			CheckPeriodBoundary(currFragment);
		}
	};

	TestableAampTsbReader *mTestableTsbReader;
	TestableAampTsbReader *mTestableSecondaryTsbReader;
	std::shared_ptr<AampTsbDataManager> mDataMgr;
	PrivateInstanceAAMP *mPrivateInstanceAAMP;

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}

		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		mDataMgr = std::make_shared<AampTsbDataManager>();
		mTestableTsbReader = new TestableAampTsbReader(mPrivateInstanceAAMP, mDataMgr, eMEDIATYPE_VIDEO, "testSessionId");

		g_mockTSBDataManager = new testing::StrictMock<MockTSBDataManager>();
		mTestableSecondaryTsbReader = nullptr;
	}

	void TearDown() override
	{
		if (gpGlobalConfig)
		{
			delete gpGlobalConfig;
			gpGlobalConfig = nullptr;
		}

		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;

		if (mTestableTsbReader)
		{
			delete mTestableTsbReader;
			mTestableTsbReader = nullptr;
		}

		if (mTestableSecondaryTsbReader)
		{
			delete mTestableSecondaryTsbReader;
			mTestableSecondaryTsbReader = nullptr;
		}

		delete g_mockTSBDataManager;
		g_mockTSBDataManager = nullptr;

		mDataMgr.reset();
	}

	void InitializeSecondaryReader()
	{
		mTestableSecondaryTsbReader = new TestableAampTsbReader(mPrivateInstanceAAMP, mDataMgr, eMEDIATYPE_AUDIO, "testSessionId");
	}
};

/**
 * @test FunctionalTests::TsbFragmentData_Constructor
 * @brief Tests the TsbFragmentData constructor.
 *
 * This test case verifies that the TsbFragmentData constructor initializes the object correctly.
 */
TEST_F(FunctionalTests, TsbFragmentData_Constructor)
{
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	AampTime position = 1000.0;
	AampTime duration = 5.0;
	AampTime pts = 0.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	AampTime PTSOffsetSec = 123.4;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);

	// Create fragment
	TsbFragmentDataPtr fragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);

	// Check fragment data
	EXPECT_STREQ(fragment->GetUrl().c_str(), url.c_str());
	EXPECT_EQ(fragment->GetMediaType(), media);
	EXPECT_EQ(fragment->GetAbsPosition(), position);
	EXPECT_EQ(fragment->GetDuration(), duration);
	EXPECT_EQ(fragment->GetPTS(), pts);
	EXPECT_EQ(fragment->IsDiscontinuous(), discont);
	EXPECT_EQ(fragment->GetInitFragData(), initFragment);
	EXPECT_EQ(fragment->GetTimeScale(), timeScale);
	EXPECT_EQ(fragment->GetPTSOffsetSec(), PTSOffsetSec);
}

/**
 * @test FunctionalTests::Init_InValidStartPos
 * @brief Tests the Init method with an invalid start position.
 *
 * This test case verifies that the Init method of AampTsbReader handles the case where the start position is invalid.
 *
 * @expect The Init method should return eAAMPSTATUS_SEEK_RANGE_ERROR.
 */
TEST_F(FunctionalTests, Init_InvalidStartPos)
{
	double startPos = -1.0;
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;
	EXPECT_EQ(mTestableTsbReader->Init(startPos, rate, tuneType, nullptr), eAAMPSTATUS_SEEK_RANGE_ERROR);
}

/**
 * @test FunctionalTests::Init_ValidStartPos
 * @brief Tests the Init method with a valid start position.
 *
 * This test case verifies that the Init method of AampTsbReader handles the case where the start position is valid.
 *
 * @expect The Init method should return eAAMPSTATUS_OK.
 */
TEST_F(FunctionalTests, Init_ValidStartPos)
{
	double startPos = 1000.0;
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment()).WillOnce(Return(nullptr));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment()).WillOnce(Return(nullptr));
	EXPECT_EQ(mTestableTsbReader->Init(startPos, rate, tuneType, nullptr), eAAMPSTATUS_OK);
}

/**
 * @test FunctionalTests::Init_InvalidFirstLastFragment
 * @brief Tests the Init method with an invalid first or last fragment.
 *
 * This test case verifies that the Init method of AampTsbReader handles the case where the first or last fragment is invalid (nullptr).
 *
 * @expect The Init method should return eAAMPSTATUS_OK.
 */
TEST_F(FunctionalTests, Init_InvalidFirstLastFragment)
{
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	double position = 1000.0;
	double duration = 5.0;
	double pts = 0.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	double PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);

	// Create fragment
	TsbFragmentDataPtr fragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment())
		.WillOnce(Return(fragment))
		.WillOnce(Return(nullptr));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment())
		.WillOnce(Return(nullptr))
		.WillOnce(Return(fragment));
	EXPECT_EQ(mTestableTsbReader->Init(position, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(mTestableTsbReader->Init(position, rate, tuneType, nullptr), eAAMPSTATUS_OK);
}

/**
 * @test FunctionalTests::Init_SeekToStart
 * @brief Tests the Init method with a seek to start position.
 *
 * This test case verifies that the Init method of AampTsbReader handles the case where the seek position is at the start of the stream.
 *
 * @expect The Init method should return eAAMPSTATUS_OK.
 */
TEST_F(FunctionalTests, Init_SeekToStart)
{
	double seekPos = 1000.0;
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	AampTime position = 1000.0;
	AampTime duration = 5.0;
	AampTime pts = 0.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	AampTime PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);

	// Create fragment
	TsbFragmentDataPtr firstFragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	position += duration;
	TsbFragmentDataPtr lastFragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment())
		.WillOnce(Return(firstFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment())
		.WillOnce(Return(lastFragment));

	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(firstFragment->GetAbsPosition().inSeconds()))
		.WillOnce(Return(firstFragment));
	EXPECT_EQ(mTestableTsbReader->Init(seekPos, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_DOUBLE_EQ(seekPos, firstFragment->GetAbsPosition().inSeconds());
}

/**
 * @test FunctionalTests::Init_SeekToEnd
 * @brief Tests the Init method with a seek to end position.
 *
 * This test case verifies that the Init method of AampTsbReader handles the case where the seek position is at the end of the stream.
 *
 * @expect The Init method should return eAAMPSTATUS_OK.
 */
TEST_F(FunctionalTests, Init_SeekToEnd)
{
	double seekPos = 1005.0;
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	AampTime position = 1000.0;
	AampTime duration = 5.0;
	AampTime pts = 0.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	AampTime PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);

	// Create fragment
	TsbFragmentDataPtr firstFragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr lastFragment = std::make_shared<TsbFragmentData>(url, media, position + duration, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment())
		.WillOnce(Return(firstFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment())
		.WillOnce(Return(lastFragment));

	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(lastFragment->GetAbsPosition().inSeconds()))
		.WillOnce(Return(lastFragment));
	EXPECT_EQ(mTestableTsbReader->Init(seekPos, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_DOUBLE_EQ(seekPos, lastFragment->GetAbsPosition().inSeconds());
}

/**
 * @test FunctionalTests::Init_SeekOutOfRange
 * @brief Tests the Init method with a seek out of range.
 *
 * This test case verifies that the Init method of AampTsbReader handles the case where the seek position is out of range.
 *
 * @expect The Init method should return eAAMPSTATUS_OK.
 */
TEST_F(FunctionalTests, Init_SeekOutOfRange)
{
	double seekPos = 2000.0;
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	AampTime position = 1000.0;
	AampTime duration = 5.0;
	AampTime pts = 0.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	AampTime PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);

	// Create fragment
	TsbFragmentDataPtr firstFragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr lastFragment = std::make_shared<TsbFragmentData>(url, media, position + duration, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment())
		.WillOnce(Return(firstFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment())
		.WillOnce(Return(lastFragment));

	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(lastFragment->GetAbsPosition().inSeconds()))
		.WillOnce(Return(lastFragment));
	EXPECT_EQ(mTestableTsbReader->Init(seekPos, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_DOUBLE_EQ(seekPos, lastFragment->GetAbsPosition().inSeconds());
}

/**
 * @test FunctionalTests::Init_SeekWithAudioTrack
 * @brief Tests the Init method with a seek with audio track.
 *
 * This test case verifies that the Init method of AampTsbReader handles the case where the seek position is with an audio track.
 *
 * @expect The Init method should return eAAMPSTATUS_OK.
 */
TEST_F(FunctionalTests, Init_SeekWithAudioTrack)
{
	double seekPos = 1005.0;
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampTime position = 1000.0;
	AampTime duration = 5.0;
	AampTime pts = 250.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	AampTime PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr videoInitFragment = std::make_shared<TsbInitData>(url, eMEDIATYPE_VIDEO, streamInfo, periodId, profileIdx);
	TsbInitDataPtr audioInitFragment = std::make_shared<TsbInitData>(url, eMEDIATYPE_AUDIO, streamInfo, periodId, profileIdx);

	// Create video fragments
	TsbFragmentDataPtr firstVideoFragment = std::make_shared<TsbFragmentData>(url, eMEDIATYPE_VIDEO, position, duration, pts, discont, periodId, videoInitFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr lastVideoFragment = std::make_shared<TsbFragmentData>(url, eMEDIATYPE_VIDEO, position + duration, duration, pts + duration, discont, periodId, videoInitFragment, timeScale, PTSOffsetSec);

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment())
		.WillOnce(Return(firstVideoFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment())
		.WillOnce(Return(lastVideoFragment));

	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(lastVideoFragment->GetAbsPosition().inSeconds()))
		.WillOnce(Return(lastVideoFragment));
	EXPECT_EQ(mTestableTsbReader->Init(seekPos, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_DOUBLE_EQ(seekPos, lastVideoFragment->GetAbsPosition().inSeconds());
	EXPECT_EQ(mTestableTsbReader->GetFirstPTS(), lastVideoFragment->GetPTS());

	// Initialize secondary reader
	InitializeSecondaryReader();
	// Create audio fragments
	TsbFragmentDataPtr firstAudioFragment = std::make_shared<TsbFragmentData>(url, eMEDIATYPE_AUDIO, 1000.0, 5.0, 250.0, discont, periodId, audioInitFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr lastAudioFragment = std::make_shared<TsbFragmentData>(url, eMEDIATYPE_AUDIO, 1005.0, 5.0, 255.0, discont, periodId, audioInitFragment, timeScale, PTSOffsetSec);

	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment())
		.WillOnce(Return(firstAudioFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment())
		.WillOnce(Return(lastAudioFragment));

	// Return last fragment as nearest one, but PTS is above video
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(_))
		.WillOnce(Return(lastAudioFragment));
	std::shared_ptr<AampTsbReader> primaryReaderPtr(mTestableTsbReader, [](AampTsbReader *) {});
	EXPECT_EQ(mTestableSecondaryTsbReader->Init(seekPos, rate, tuneType, primaryReaderPtr), eAAMPSTATUS_OK);
	EXPECT_DOUBLE_EQ(seekPos, lastVideoFragment->GetAbsPosition().inSeconds());
	EXPECT_EQ(mTestableSecondaryTsbReader->GetFirstPTS(), lastAudioFragment->GetPTS());
}

/**
 * @test FunctionalTests::Init_SeekWithAudioTrackDiffInPTS
 * @brief Tests the Init method with a seek with an audio track having different PTS compared to video track.
 *
 * This test case verifies that the Init method of AampTsbReader handles the case where the seek position is with an audio track.
 *
 * @expect The Init method should return eAAMPSTATUS_OK even if there is a mismatch in PTS values.
 */
TEST_F(FunctionalTests, Init_SeekWithAudioTrackDiffInPTS)
{
	double seekPos = 1005.0;
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampTime position = 1000.0;
	AampTime duration = 5.0;
	AampTime pts = 250.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	AampTime PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr videoInitFragment = std::make_shared<TsbInitData>(url, eMEDIATYPE_VIDEO, streamInfo, periodId, profileIdx);
	TsbInitDataPtr audioInitFragment = std::make_shared<TsbInitData>(url, eMEDIATYPE_AUDIO, streamInfo, periodId, profileIdx);

	// Create video fragments
	TsbFragmentDataPtr firstVideoFragment = std::make_shared<TsbFragmentData>(url, eMEDIATYPE_VIDEO, position, duration, pts, discont, periodId, videoInitFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr lastVideoFragment = std::make_shared<TsbFragmentData>(url, eMEDIATYPE_VIDEO, position + duration, duration, pts + duration, discont, periodId, videoInitFragment, timeScale, PTSOffsetSec);

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment())
		.WillOnce(Return(firstVideoFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment())
		.WillOnce(Return(lastVideoFragment));

	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(lastVideoFragment->GetAbsPosition().inSeconds()))
		.WillOnce(Return(lastVideoFragment));
	EXPECT_EQ(mTestableTsbReader->Init(seekPos, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(seekPos, lastVideoFragment->GetAbsPosition());
	EXPECT_EQ(mTestableTsbReader->GetFirstPTS(), lastVideoFragment->GetPTS());

	// Initialize secondary reader
	InitializeSecondaryReader();
	// Create audio fragments
	TsbFragmentDataPtr firstAudioFragment = std::make_shared<TsbFragmentData>(url, eMEDIATYPE_AUDIO, position - 2.0, duration, pts - 2.0, discont, periodId, audioInitFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr secondAudioFragment = std::make_shared<TsbFragmentData>(url, eMEDIATYPE_AUDIO, position + 2.0, duration, pts + 2.0, discont, periodId, audioInitFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr lastAudioFragment = std::make_shared<TsbFragmentData>(url, eMEDIATYPE_AUDIO, position + 6.0, duration, pts + 6.0, discont, periodId, audioInitFragment, timeScale, PTSOffsetSec);
	firstAudioFragment->next = secondAudioFragment;
	secondAudioFragment->prev = firstAudioFragment;
	secondAudioFragment->next = lastAudioFragment;
	lastAudioFragment->prev = secondAudioFragment;
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment())
		.WillOnce(Return(firstAudioFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment())
		.WillOnce(Return(lastAudioFragment));

	// Return last fragment as nearest one, but PTS is above video
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(seekPos))
		.WillOnce(Return(lastAudioFragment));
	std::shared_ptr<AampTsbReader> primaryReaderPtr(mTestableTsbReader, [](AampTsbReader *) {});
	EXPECT_EQ(mTestableSecondaryTsbReader->Init(seekPos, rate, tuneType, primaryReaderPtr), eAAMPSTATUS_OK);

	// Even though last fragment is nearest, the second fragment should be returned as it has lower PTS compared to video
	EXPECT_EQ(mTestableSecondaryTsbReader->GetFirstPTS(), secondAudioFragment->GetPTS());
}

/**
 * @test FunctionalTests::ReadNext_NotInitialized
 * @brief Tests the ReadNext method when the reader is not initialized.
 *
 * This test case verifies that the ReadNext method of AampTsbReader returns nullptr when the reader is not initialized.
 *
 * @expect The ReadNext method should return nullptr.
 */
TEST_F(FunctionalTests, ReadNext_NotInitialized)
{
	EXPECT_EQ(mTestableTsbReader->ReadNext(), nullptr);
}

/**
 * @test FunctionalTests::ReadNext_ValidFragment
 * @brief Tests the ReadNext method with a valid fragment.
 *
 * This test case verifies that the ReadNext method of AampTsbReader returns a valid fragment.
 *
 * @expect The ReadNext method should return a valid fragment.
 */
TEST_F(FunctionalTests, ReadNext_ValidFragment)
{
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	double position = 1000.0;
	double seekPos = 1005.0;
	double duration = 5.0;
	double pts = 0.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	double PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);

	// Mock data manager
	TsbFragmentDataPtr firstFragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr secondFragment = std::make_shared<TsbFragmentData>(url, media, position + duration, duration, pts + duration, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr lastFragment = std::make_shared<TsbFragmentData>(url, media, position + 2 * duration, duration, pts + 2 * duration, discont, periodId, initFragment, timeScale, PTSOffsetSec);

	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment()).WillOnce(Return(firstFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment()).WillOnce(Return(lastFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(_)).WillOnce(Return(secondFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(_, _)).WillOnce(Return(secondFragment));

	EXPECT_EQ(mTestableTsbReader->Init(seekPos , rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), secondFragment);
}

/**
 * @test FunctionalTests::ReadNext_NullFragment
 * @brief Tests the ReadNext method with a null fragment.
 *
 * This test case verifies that the ReadNext returns nullptr.
 *
 * @expect The ReadNext method should return nullptr.
 */
TEST_F(FunctionalTests, ReadNext_NullFragment)
{
	double startPos = 0.0;
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment()).WillOnce(Return(nullptr));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment()).WillOnce(Return(nullptr));

	EXPECT_EQ(mTestableTsbReader->Init(startPos, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), nullptr);
}

/**
 * @test FunctionalTests::ReadNext_ForwardRate
 * @brief Tests the ReadNext method with a forward rate.
 *
 * This test case verifies that the ReadNext method of AampTsbReader handles the case where the rate is forward.
 *
 * @expect The ReadNext method should return the next fragment.
 */
TEST_F(FunctionalTests, ReadNext_ForwardRate)
{
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	double position = 1000.0;
	double duration = 5.0;
	double pts = 0.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	double PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);
	TsbFragmentDataPtr fragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr nextFragment = std::make_shared<TsbFragmentData>(url, media, position + duration, duration, pts + duration, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	fragment->next = nextFragment;

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment()).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment()).WillOnce(Return(nextFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(_)).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(_, _)).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(position + duration, _)).WillOnce(Return(nextFragment));

	EXPECT_EQ(mTestableTsbReader->Init(position, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), fragment);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), nextFragment);
	EXPECT_FALSE(mTestableTsbReader->IsDiscontinuous());
}

/**
 * @test FunctionalTests::ReadNext_RewindRate
 * @brief Tests the ReadNext method with a rewind rate.
 *
 * This test case verifies that the ReadNext method of AampTsbReader handles the case where the rate is rewind.
 *
 * @expect The ReadNext method should return the previous fragment.
 */
TEST_F(FunctionalTests, ReadNext_RewindRate)
{
	float rate = -1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	double position = 1000.0;
	double duration = 5.0;
	double pts = 0.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	double PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);
	TsbFragmentDataPtr fragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr prevFragment = std::make_shared<TsbFragmentData>(url, media, position - duration, duration, pts - duration, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	fragment->prev = prevFragment;

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment()).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment()).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(_)).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(_, _)).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(position - duration, _)).WillOnce(Return(prevFragment));

	EXPECT_EQ(mTestableTsbReader->Init(position, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), fragment);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), prevFragment);
	EXPECT_FALSE(mTestableTsbReader->IsDiscontinuous());
}

/**
 * @test FunctionalTests::ReadNext_Discontinuity
 * @brief Tests the ReadNext method with a discontinuity.
 *
 * This test case verifies that the ReadNext method of AampTsbReader handles the case where there is a discontinuity.
 *
 * @expect The ReadNext method should return the next fragment and detect discontinuity.
 */
TEST_F(FunctionalTests, ReadNext_Discontinuity)
{
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	double position = 1000.0;
	double duration = 5.0;
	double pts = 0.0;
	bool discont = true;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	double PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);
	TsbFragmentDataPtr fragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, false, periodId, initFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr nextFragment = std::make_shared<TsbFragmentData>(url, media, position + duration, duration, pts + duration, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	fragment->next = nextFragment;

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment()).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment()).WillOnce(Return(nextFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(_)).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(_, _)).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(position + duration, _)).WillOnce(Return(nextFragment));

	EXPECT_EQ(mTestableTsbReader->Init(position, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), fragment);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), nextFragment);
	EXPECT_TRUE(mTestableTsbReader->IsDiscontinuous());
}

/**
 * @test FunctionalTests::ReadNext_EOSReached
 * @brief Tests the ReadNext method when the end of stream (EOS) is reached.
 *
 * This test case verifies that the ReadNext method of AampTsbReader handles the case where the end of stream is reached.
 *
 * @expect The ReadNext method should return nullptr when EOS is reached.
 */
TEST_F(FunctionalTests, ReadNext_EOSReached)
{
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	double position = 1000.0;
	double duration = 5.0;
	double pts = 0.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	double PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);

	// Mock data manager
	TsbFragmentDataPtr firstFragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr secondFragment = std::make_shared<TsbFragmentData>(url, media, position + duration, duration, pts + duration, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	secondFragment->prev = firstFragment;
	firstFragment->next = secondFragment;
	TsbFragmentDataPtr lastFragment = std::make_shared<TsbFragmentData>(url, media, position + 2 * duration, duration, pts + 2 * duration, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	secondFragment->next = lastFragment;
	lastFragment->prev = secondFragment;

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment()).WillOnce(Return(firstFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment()).WillOnce(Return(lastFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(_)).WillOnce(Return(firstFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(_, _))
		.WillOnce(Return(firstFragment))
		.WillOnce(Return(secondFragment))
		.WillOnce(Return(lastFragment));

	EXPECT_EQ(mTestableTsbReader->Init(position, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), firstFragment);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), secondFragment);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), lastFragment);
	EXPECT_TRUE(mTestableTsbReader->IsEos());
}

/**
 * @test FunctionalTests::ReadNext_CorrectedPosition
 * @brief Tests the ReadNext method with a corrected position.
 *
 * This test case verifies that the ReadNext method of AampTsbReader handles the case where the position is corrected.
 *
 * @expect The ReadNext method should return the corrected fragment.
 */
TEST_F(FunctionalTests, ReadNext_CorrectedPosition)
{
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	double position = 1000.0;
	double correctedPosition = 1005.05;
	double duration = 5.0;
	double pts = 0.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	double PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);
	TsbFragmentDataPtr fragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr correctedFragment = std::make_shared<TsbFragmentData>(url, media, correctedPosition, duration, pts + duration, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	fragment->next = correctedFragment;

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment()).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment()).WillOnce(Return(correctedFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(_)).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(_, _)).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(position + duration, _)).WillOnce(Return(nullptr));
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(position + duration - FLOATING_POINT_EPSILON)).WillOnce(Return(correctedFragment));

	EXPECT_EQ(mTestableTsbReader->Init(position, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), fragment);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), correctedFragment); // Corrected position
	EXPECT_TRUE(mTestableTsbReader->IsEos());
}

/**
 * @test FunctionalTests::ReadNext_CorrectedPositionNegativeRate
 * @brief Tests the ReadNext method with a corrected position and negative rate.
 *
 * This test case verifies that the ReadNext method of AampTsbReader handles the case where the position is corrected and the rate is negative.
 *
 * @expect The ReadNext method should return the corrected fragment.
 */
TEST_F(FunctionalTests, ReadNext_CorrectedPositionNegativeRate)
{
	float rate = -1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	double position = 1000.0;
	double correctedPosition = 995.05;
	double duration = 5.0;
	double pts = 0.0;
	bool discont = false;
	std::string periodId = "testPeriodId";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	double PTSOffsetSec = 0.0;

	// Create init data and fragments
	TsbInitDataPtr initFragment = std::make_shared<TsbInitData>(url, media, streamInfo, periodId, profileIdx);
	TsbFragmentDataPtr fragment = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	TsbFragmentDataPtr correctedFragment = std::make_shared<TsbFragmentData>(url, media, correctedPosition, duration, pts - duration, discont, periodId, initFragment, timeScale, PTSOffsetSec);
	fragment->prev = correctedFragment;

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment()).WillOnce(Return(correctedFragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment()).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(_)).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(_, _)).WillOnce(Return(fragment));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(position - duration, _)).WillOnce(Return(nullptr));
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(position - duration - FLOATING_POINT_EPSILON)).WillOnce(Return(correctedFragment));

	EXPECT_EQ(mTestableTsbReader->Init(position, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), fragment);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), correctedFragment); // Corrected position
	EXPECT_TRUE(mTestableTsbReader->IsEos());
}

/**
 * @test FunctionalTests::ReadNext_PeriodBoundary
 * @brief Tests the ReadNext method with a period boundary.
 *
 * This test case verifies that the ReadNext method of AampTsbReader handles the case where the next fragment is from a different period.
 *
 * @expect The ReadNext method should return the next fragment and detect period boundary.
 */
TEST_F(FunctionalTests, ReadNext_PeriodBoundary)
{
	float rate = 1.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	double position = 1000.0;
	double duration = 5.0;
	double pts = 0.0;
	bool discont = false;
	std::string periodId1 = "testPeriodId1";
	std::string periodId2 = "testPeriodId2";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	double PTSOffsetSec = 0.0;

	// Create init data and fragments for period 1
	TsbInitDataPtr initFragment1 = std::make_shared<TsbInitData>(url, media, streamInfo, periodId1, profileIdx);
	TsbFragmentDataPtr fragment1 = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId1, initFragment1, timeScale, PTSOffsetSec);

	// Create init data and fragments for period 2
	TsbInitDataPtr initFragment2 = std::make_shared<TsbInitData>(url, media, streamInfo, periodId2, profileIdx);
	TsbFragmentDataPtr fragment2 = std::make_shared<TsbFragmentData>(url, media, position + duration, duration, pts, true, periodId2, initFragment2, timeScale, PTSOffsetSec);
	fragment1->next = fragment2;
	fragment2->prev = fragment1;

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment()).WillOnce(Return(fragment1));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment()).WillOnce(Return(fragment2));
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(_)).WillOnce(Return(fragment1));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(_, _)).WillOnce(Return(fragment1));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(position + duration, _)).WillOnce(Return(fragment2));

	// Initialize reader with fragment1
	EXPECT_EQ(mTestableTsbReader->Init(position, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), fragment1);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), fragment2);

	// Period boundary and discontinuity detected
	EXPECT_TRUE(mTestableTsbReader->IsPeriodBoundary());
	EXPECT_TRUE(mTestableTsbReader->IsDiscontinuous());
}

/**
 * @test FunctionalTests::ReadNext_PeriodBoundaryNegativeRate
 * @brief Tests the ReadNext method with trickplay rate.
 *
 * This test case verifies that the ReadNext method of AampTsbReader handles the case where the previous fragment is from a different period.
 *
 * @expect The ReadNext method should return the previous fragment without discontinuity as trickplay in progress.
 */
TEST_F(FunctionalTests, ReadNext_PeriodBoundaryTrickPlay)
{
	float rate = -4.0f;
	TuneType tuneType = eTUNETYPE_NEW_NORMAL;

	// Mock valid fragment data
	std::string url = "http://example.com";
	AampMediaType media = eMEDIATYPE_VIDEO;
	double position = 1000.0;
	double duration = 5.0;
	double pts = 0.0;
	bool discont = false;
	std::string periodId1 = "testPeriodId1";
	std::string periodId2 = "testPeriodId2";
	StreamInfo streamInfo;
	int profileIdx = 0;
	uint32_t timeScale = 240000;
	double PTSOffsetSec = 0.0;

	// Create init data and fragments for period 1
	TsbInitDataPtr initFragment1 = std::make_shared<TsbInitData>(url, media, streamInfo, periodId1, profileIdx);
	TsbFragmentDataPtr fragment1 = std::make_shared<TsbFragmentData>(url, media, position, duration, pts, discont, periodId1, initFragment1, timeScale, PTSOffsetSec);

	// Create init data and fragments for period 2
	TsbInitDataPtr initFragment2 = std::make_shared<TsbInitData>(url, media, streamInfo, periodId2, profileIdx);
	TsbFragmentDataPtr fragment2 = std::make_shared<TsbFragmentData>(url, media, position + duration, duration, pts, true, periodId2, initFragment2, timeScale, PTSOffsetSec);
	fragment1->next = fragment2;
	fragment2->prev = fragment1;

	// Mock data manager
	EXPECT_CALL(*g_mockTSBDataManager, GetFirstFragment()).WillOnce(Return(fragment1));
	EXPECT_CALL(*g_mockTSBDataManager, GetLastFragment()).WillOnce(Return(fragment2));
	EXPECT_CALL(*g_mockTSBDataManager, GetNearestFragment(_)).WillOnce(Return(fragment2));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(_, _)).WillOnce(Return(fragment2));
	EXPECT_CALL(*g_mockTSBDataManager, GetFragment(position, _)).WillOnce(Return(fragment1));

	// Initialize reader with fragment2
	double initPosition = position + duration;
	EXPECT_EQ(mTestableTsbReader->Init(initPosition, rate, tuneType, nullptr), eAAMPSTATUS_OK);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), fragment2);
	EXPECT_EQ(mTestableTsbReader->ReadNext(), fragment1);

	// Trickplay in progress, period boundary, discontinuity is detected from data manager
	EXPECT_TRUE(mTestableTsbReader->IsPeriodBoundary());
	EXPECT_TRUE(mTestableTsbReader->IsDiscontinuous());
}

/**
 * @test FunctionalTests::CheckForWaitIfReaderDone_AbortCheckForWaitIfReaderDone
 * @brief Tests the CheckForWaitIfReaderDone and AbortCheckForWaitIfReaderDone methods.
 *
 * This test case verifies that the CheckForWaitIfReaderDone method waits for the reader to be done and that the AbortCheckForWaitIfReaderDone method aborts the wait.
 *
 * @expect The CheckForWaitIfReaderDone method should wait for the reader to be done and the AbortCheckForWaitIfReaderDone method should abort the wait.
 */
TEST_F(FunctionalTests, CheckForWaitIfReaderDone_AbortCheckForWaitIfReaderDone)
{
	// Start a thread to call CheckForWaitIfReaderDone
	std::thread waitThread([this]()
						   { mTestableTsbReader->CheckForWaitIfReaderDone(); });

	// Give some time for the thread to start and wait
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// Call AbortCheckForWaitIfReaderDone to abort the wait
	mTestableTsbReader->AbortCheckForWaitIfReaderDone();

	// Join the thread
	waitThread.join();
	EXPECT_TRUE(mTestableTsbReader->IsEndFragmentInjected());
}

