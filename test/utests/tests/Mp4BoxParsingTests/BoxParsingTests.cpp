/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

/**
 * @file BoxParsingTests.cpp
 * @brief Functional tests for MP4 demuxer
 * 
 * This file validates core MP4 parsing scenarios:
 * 1. PSSH box parsing and protection events
 * 2. Initialization segment parsing (codec data, timescale)
 * 3. Fragment parsing (sample extraction)
 * 4. Encrypted fragment with SENC box
 * 5. Encrypted fragment with SAIO/SAIZ boxes
 */

#include <gtest/gtest.h>
#include "MP4Demux.h"
#include "Mp4DemuxTestData.h"
#include <vector>
#include <cstring>

/**
 * @class Mp4DemuxFunctionalTests
 * @brief Functional test fixture for MP4 demuxer
 */
class Mp4DemuxFunctionalTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		mDemuxer = new Mp4Demux();
	}

	void TearDown() override
	{
		delete mDemuxer;
	}

	Mp4Demux* mDemuxer;
};

/**
 * @brief Test 1: Parse PSSH box and validate GetProtectionEvents()
 * 
 * Validates that PSSH box parsing correctly extracts DRM protection data
 * and makes it available through GetProtectionEvents().
 */
TEST_F(Mp4DemuxFunctionalTests, ParsePsshBoxAndValidateProtectionEvents)
{
	// Parse Widevine PSSH box
	bool result = mDemuxer->Parse(psshBoxWidevine, sizeof(psshBoxWidevine));
	ASSERT_TRUE(result) << "Parse should succeed for valid PSSH box";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	// Verify protection events were extracted
	auto protectionEvents = mDemuxer->GetProtectionEvents();
	ASSERT_EQ(protectionEvents.size(), 1) << "Should have exactly one PSSH entry";
	
	// Validate system ID format (UUID with hyphens)
	EXPECT_FALSE(protectionEvents[0].systemID.empty()) << "System ID should not be empty";
	EXPECT_EQ(protectionEvents[0].systemID.length(), 36) << "System ID should be 36 chars (UUID format)";
	EXPECT_NE(protectionEvents[0].systemID.find('-'), std::string::npos) << "System ID should contain hyphens";
	
	// Validate PSSH data was extracted
	EXPECT_FALSE(protectionEvents[0].pssh.empty()) << "PSSH data should not be empty";
	EXPECT_GT(protectionEvents[0].pssh.size(), 0) << "PSSH data should have content";
}

/**
 * @brief Test 2: Parse initialization segment and validate codec info
 * 
 * Validates that moov box parsing correctly extracts:
 * - Codec configuration data (avcC/hvcC/esds)
 * - Timescale from mvhd or mdhd
 * - Video/audio stream parameters
 */
TEST_F(Mp4DemuxFunctionalTests, ParseInitSegmentAndValidateCodecData)
{
	// Parse the segment
	bool result = mDemuxer->Parse(initSegmentWithAvcC, sizeof(initSegmentWithAvcC));
	EXPECT_TRUE(result) << "Parse should succeed for valid init segment";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	// For complete test, we need full moov structure
	// This validates the timescale was extracted
	uint32_t timescale = mDemuxer->GetTimeScale();
	EXPECT_GT(timescale, 0) << "Timescale should be greater than 0";
	EXPECT_EQ(timescale, 30000) << "Timescale from MDHD should be 30000";

	auto samples = mDemuxer->GetSamples();
	EXPECT_EQ(samples.size(), 0) << "Sample count should be zero";

	auto codecInfo = mDemuxer->GetCodecInfo();
	EXPECT_EQ(codecInfo.mCodecFormat, GST_FORMAT_VIDEO_ES_H264) << "Codec format should be H.264";
	EXPECT_FALSE(codecInfo.mCodecData.empty()) << "Codec data (avcC) should not be empty";
	EXPECT_EQ(codecInfo.mInfo.video.mWidth, 1280) << "Video width should be 1280";
	EXPECT_EQ(codecInfo.mInfo.video.mHeight, 720) << "Video height should be 720";
}

/**
 * @brief Test 3: Parse fragment and validate sample extraction
 * 
 * Validates that moof/mdat parsing correctly extracts:
 * - Sample count matches expected
 * - Sample data is present
 * - Sample timing (PTS/DTS) is calculated correctly
 */
TEST_F(Mp4DemuxFunctionalTests, ParseFragmentAndValidateSamples)
{
	// Parse initialization segment first to set timescale 30000
	bool result = mDemuxer->Parse(initSegmentWithAvcC, sizeof(initSegmentWithAvcC));
	EXPECT_TRUE(result) << "Parse should succeed for valid init segment";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);

	// Parse fragment with moof and mdat containing 2 samples
	result = mDemuxer->Parse(fragmentWithSamples, sizeof(fragmentWithSamples));
	EXPECT_TRUE(result) << "Parse should succeed for valid fragment";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	// Get samples
	auto samples = mDemuxer->GetSamples();
	
	// Validate sample count
	EXPECT_EQ(samples.size(), 2) << "Should have exactly 2 samples";
	
	// Validate Sample 0
	EXPECT_EQ(samples[0].mData.GetLen(), 32) << "Sample 0 should be 32 bytes";
	EXPECT_EQ(samples[0].mPts, 0) << "Sample 0 PTS should be 0";
	EXPECT_EQ(samples[0].mDts, 0) << "Sample 0 DTS should be 0";
	EXPECT_EQ(samples[0].mDuration, 0.1) << "Sample 0 duration should be 0.1";
	EXPECT_FALSE(samples[0].mDrmMetadata.mIsEncrypted) << "Sample 0 should not be encrypted";
	
	// Validate Sample 1
	EXPECT_EQ(samples[1].mData.GetLen(), 64) << "Sample 1 should be 64 bytes";
	EXPECT_EQ(samples[1].mPts, 0.1) << "Sample 1 PTS should be 0.1";
	EXPECT_EQ(samples[1].mDts, 0.1) << "Sample 1 DTS should be 0.1";
	EXPECT_EQ(samples[1].mDuration, 0.1) << "Sample 1 duration should be 0.1";
	EXPECT_FALSE(samples[1].mDrmMetadata.mIsEncrypted) << "Sample 1 should not be encrypted";
}

/**
 * @brief Test 4: Parse encrypted fragment with SENC and validate DRM metadata
 * 
 * Validates that encrypted fragments with SENC box correctly populate:
 * - mIsEncrypted flag
 * - mKeyId (default KID)
 * - mIV (initialization vector)
 * - mCipher (cenc/cbcs)
 * - mSubSamples (if present)
 */
TEST_F(Mp4DemuxFunctionalTests, ParseEncryptedFragmentWithSencBox)
{
	bool result = mDemuxer->Parse(encryptedFragmentWithSenc, sizeof(encryptedFragmentWithSenc));
	EXPECT_TRUE(result) << "Parse should succeed for encrypted fragment with SENC";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	auto samples = mDemuxer->GetSamples();
	EXPECT_EQ(samples.size(), 2) << "Should have exactly 2 samples";
	// Validate DRM metadata for each sample
	EXPECT_TRUE(samples[0].mDrmMetadata.mIsEncrypted) << "Sample should be marked as encrypted";
	EXPECT_FALSE(samples[0].mDrmMetadata.mKeyId.empty()) << "Sample should have Key ID";
	EXPECT_FALSE(samples[0].mDrmMetadata.mIV.empty()) << "Sample should have IV";
	EXPECT_FALSE(samples[0].mDrmMetadata.mCipher.empty()) << "Sample should have cipher type";
	EXPECT_EQ(samples[0].mDrmMetadata.mSubSamples.size(), 6) << "Sample should have subsample encryption data";
	EXPECT_EQ(samples[0].mDrmMetadata.mNumSubSamples, 1) << "Sample should have 1 subsamples";

	EXPECT_TRUE(samples[1].mDrmMetadata.mIsEncrypted) << "Sample should be marked as encrypted";
	EXPECT_FALSE(samples[1].mDrmMetadata.mKeyId.empty()) << "Sample should have Key ID";
	EXPECT_FALSE(samples[1].mDrmMetadata.mIV.empty()) << "Sample should have IV";
	EXPECT_FALSE(samples[1].mDrmMetadata.mCipher.empty()) << "Sample should have cipher type";
	EXPECT_EQ(samples[1].mDrmMetadata.mSubSamples.size(), 12) << "Sample should have subsample encryption data";
	EXPECT_EQ(samples[1].mDrmMetadata.mNumSubSamples, 2) << "Sample should have 2 subsamples";
}

/**
 * @brief Test 5: Parse encrypted fragment with SAIO/SAIZ and validate DRM metadata
 * 
 * Validates that encrypted fragments with SAIO/SAIZ boxes correctly:
 * - Parse auxiliary information offsets
 * - Parse auxiliary information sizes
 * - Process auxiliary info to populate sample DRM metadata
 * - Handle IV and subsample encryption data
 */
TEST_F(Mp4DemuxFunctionalTests, ParseEncryptedFragmentWithSaioSaizBoxes)
{
	bool result = mDemuxer->Parse(encryptedFragmentWithSaioSaiz, sizeof(encryptedFragmentWithSaioSaiz));
	ASSERT_TRUE(result) << "Parse should succeed for encrypted fragment with SAIO/SAIZ";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	auto samples = mDemuxer->GetSamples();
	EXPECT_EQ(samples.size(), 2) << "Should have exactly 2 samples";
	// Validate DRM metadata for each sample
	EXPECT_TRUE(samples[0].mDrmMetadata.mIsEncrypted) << "Sample should be marked as encrypted";
	EXPECT_FALSE(samples[0].mDrmMetadata.mKeyId.empty()) << "Sample should have Key ID";
	EXPECT_FALSE(samples[0].mDrmMetadata.mIV.empty()) << "Sample should have IV";
	EXPECT_FALSE(samples[0].mDrmMetadata.mCipher.empty()) << "Sample should have cipher type";
	EXPECT_EQ(samples[0].mDrmMetadata.mSubSamples.size(), 6) << "Sample should have subsample encryption data";
	EXPECT_EQ(samples[0].mDrmMetadata.mNumSubSamples, 1) << "Sample should have 1 subsamples";

	EXPECT_TRUE(samples[1].mDrmMetadata.mIsEncrypted) << "Sample should be marked as encrypted";
	EXPECT_FALSE(samples[1].mDrmMetadata.mKeyId.empty()) << "Sample should have Key ID";
	EXPECT_FALSE(samples[1].mDrmMetadata.mIV.empty()) << "Sample should have IV";
	EXPECT_FALSE(samples[1].mDrmMetadata.mCipher.empty()) << "Sample should have cipher type";
	EXPECT_EQ(samples[1].mDrmMetadata.mSubSamples.size(), 6) << "Sample should have subsample encryption data";
	EXPECT_EQ(samples[1].mDrmMetadata.mNumSubSamples, 1) << "Sample should have 1 subsamples";
}

/**
 * @brief Helper test: Validate PSSH version 1 with KIDs
 */
TEST_F(Mp4DemuxFunctionalTests, ParsePsshV1WithKID)
{
	bool result = mDemuxer->Parse(psshBoxV1WithKID, sizeof(psshBoxV1WithKID));
	ASSERT_TRUE(result) << "Parse should succeed for PSSH v1";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	auto protectionEvents = mDemuxer->GetProtectionEvents();
	ASSERT_EQ(protectionEvents.size(), 1) << "Should have one PSSH entry";
	EXPECT_FALSE(protectionEvents[0].systemID.empty());
	EXPECT_FALSE(protectionEvents[0].pssh.empty());
}


/**
 * @brief Error handling: Empty buffer
 */
TEST_F(Mp4DemuxFunctionalTests, HandleEmptyBuffer)
{
	bool result = mDemuxer->Parse(nullptr, 0);
	EXPECT_FALSE(result) << "Empty buffer should be handled with error";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_ERROR_INVALID_INPUT);
}

/**
 * @brief Error handling: Truncated box
 */
TEST_F(Mp4DemuxFunctionalTests, HandleTruncatedBox)
{
	// Truncate PSSH box to only 20 bytes (incomplete)
	std::vector<uint8_t> truncated(psshBoxWidevine, psshBoxWidevine + 20);
	
	bool result = mDemuxer->Parse(truncated.data(), truncated.size());
	// Either should succeed (graceful handling) or fail with error
	EXPECT_FALSE(result) << "Truncated box should be handled with error";
	EXPECT_NE(mDemuxer->GetLastError(), MP4_PARSE_OK) << "Should report error for truncated data";
}