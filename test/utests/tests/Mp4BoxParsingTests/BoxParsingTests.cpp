/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
#include <cmath>

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
	bool result = mDemuxer->Parse(std::make_shared<std::vector<uint8_t>>(psshBoxWidevine, psshBoxWidevine + sizeof(psshBoxWidevine)));
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

	// Reconstructed box must be byte-identical to the input (size, type,
	// version/flags, and data), since Rialto's parsePssh re-parses it as a
	// standalone ISO BMFF box.
	std::vector<uint8_t> expectedBox(psshBoxWidevine, psshBoxWidevine + sizeof(psshBoxWidevine));
	EXPECT_EQ(protectionEvents[0].pssh, expectedBox) << "Reconstructed PSSH box should match input byte-for-byte";
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
	bool result = mDemuxer->Parse(std::make_shared<std::vector<uint8_t>>(initSegmentWithAvcC, initSegmentWithAvcC + sizeof(initSegmentWithAvcC)));
	EXPECT_TRUE(result) << "Parse should succeed for valid init segment";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	// For complete test, we need full moov structure
	// This validates the timescale was extracted
	uint32_t timescale = mDemuxer->GetTimeScale();
	EXPECT_GT(timescale, 0) << "Timescale should be greater than 0";
	EXPECT_EQ(timescale, 30000) << "Timescale from MDHD should be 30000";
	
	auto samples = mDemuxer->GetSamples(); // mData aliased shared_ptr keeps the backing buffer alive for these samples
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
	bool result = mDemuxer->Parse(std::make_shared<std::vector<uint8_t>>(initSegmentWithAvcC, initSegmentWithAvcC + sizeof(initSegmentWithAvcC)));
	EXPECT_TRUE(result) << "Parse should succeed for valid init segment";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	// Parse fragment with moof and mdat containing 2 samples
	result = mDemuxer->Parse(std::make_shared<std::vector<uint8_t>>(fragmentWithSamples, fragmentWithSamples + sizeof(fragmentWithSamples)));
	EXPECT_TRUE(result) << "Parse should succeed for valid fragment";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	// Get samples
	auto samples = mDemuxer->GetSamples(); // mData aliased shared_ptr keeps the backing buffer alive for these samples
	
	// Validate sample count
	EXPECT_EQ(samples.size(), 2) << "Should have exactly 2 samples";
	
	// Validate Sample 0
	EXPECT_EQ(samples[0].mDataSize, 32u) << "Sample 0 should be 32 bytes";
	EXPECT_NEAR(samples[0].mPts, 0.0, 1e-6) << "Sample 0 PTS should be 0";
	EXPECT_NEAR(samples[0].mDts, 0.0, 1e-6) << "Sample 0 DTS should be 0";
	EXPECT_NEAR(samples[0].mDuration, 0.1, 1e-6) << "Sample 0 duration should be 0.1";
	EXPECT_FALSE(samples[0].mDrmMetadata.mIsEncrypted) << "Sample 0 should not be encrypted";
	
	// Validate Sample 1
	EXPECT_EQ(samples[1].mDataSize, 64u) << "Sample 1 should be 64 bytes";
	EXPECT_NEAR(samples[1].mPts, 0.1, 1e-6) << "Sample 1 PTS should be 0.1";
	EXPECT_NEAR(samples[1].mDts, 0.1, 1e-6) << "Sample 1 DTS should be 0.1";
	EXPECT_NEAR(samples[1].mDuration, 0.1, 1e-6) << "Sample 1 duration should be 0.1";
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
	bool result = mDemuxer->Parse(std::make_shared<std::vector<uint8_t>>(encryptedFragmentWithSenc, encryptedFragmentWithSenc + sizeof(encryptedFragmentWithSenc)));
	EXPECT_TRUE(result) << "Parse should succeed for encrypted fragment with SENC";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	auto samples = mDemuxer->GetSamples(); // mData aliased shared_ptr keeps the backing buffer alive for these samples
	EXPECT_EQ(samples.size(), 2) << "Should have exactly 2 samples";
	
	// Validate DRM metadata for each sample
	EXPECT_TRUE(samples[0].mDrmMetadata.mIsEncrypted) << "Sample should be marked as encrypted";
	EXPECT_FALSE(samples[0].mDrmMetadata.mKeyId.empty()) << "Sample should have Key ID";
	EXPECT_FALSE(samples[0].mDrmMetadata.mIV.empty()) << "Sample should have IV";
	EXPECT_FALSE(samples[0].mDrmMetadata.mCipher == CIPHER_TYPE_NONE) << "Sample should have cipher type";
	EXPECT_EQ(samples[0].mDrmMetadata.mSubSamples.size(), 6) << "Sample should have subsample encryption data";
	EXPECT_EQ(samples[0].mDrmMetadata.mNumSubSamples, 1) << "Sample should have 1 subsamples";
	
	EXPECT_TRUE(samples[1].mDrmMetadata.mIsEncrypted) << "Sample should be marked as encrypted";
	EXPECT_FALSE(samples[1].mDrmMetadata.mKeyId.empty()) << "Sample should have Key ID";
	EXPECT_FALSE(samples[1].mDrmMetadata.mIV.empty()) << "Sample should have IV";
	EXPECT_FALSE(samples[1].mDrmMetadata.mCipher == CIPHER_TYPE_NONE) << "Sample should have cipher type";
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
	bool result = mDemuxer->Parse(std::make_shared<std::vector<uint8_t>>(encryptedFragmentWithSaioSaiz, encryptedFragmentWithSaioSaiz + sizeof(encryptedFragmentWithSaioSaiz)));
	ASSERT_TRUE(result) << "Parse should succeed for encrypted fragment with SAIO/SAIZ";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	auto samples = mDemuxer->GetSamples(); // mData aliased shared_ptr keeps the backing buffer alive for these samples
	EXPECT_EQ(samples.size(), 2) << "Should have exactly 2 samples";
	// Validate DRM metadata for each sample
	EXPECT_TRUE(samples[0].mDrmMetadata.mIsEncrypted) << "Sample should be marked as encrypted";
	EXPECT_FALSE(samples[0].mDrmMetadata.mKeyId.empty()) << "Sample should have Key ID";
	EXPECT_FALSE(samples[0].mDrmMetadata.mIV.empty()) << "Sample should have IV";
	EXPECT_FALSE(samples[0].mDrmMetadata.mCipher == CIPHER_TYPE_NONE) << "Sample should have cipher type";
	EXPECT_EQ(samples[0].mDrmMetadata.mSubSamples.size(), 6) << "Sample should have subsample encryption data";
	EXPECT_EQ(samples[0].mDrmMetadata.mNumSubSamples, 1) << "Sample should have 1 subsamples";
	
	EXPECT_TRUE(samples[1].mDrmMetadata.mIsEncrypted) << "Sample should be marked as encrypted";
	EXPECT_FALSE(samples[1].mDrmMetadata.mKeyId.empty()) << "Sample should have Key ID";
	EXPECT_FALSE(samples[1].mDrmMetadata.mIV.empty()) << "Sample should have IV";
	EXPECT_FALSE(samples[1].mDrmMetadata.mCipher == CIPHER_TYPE_NONE) << "Sample should have cipher type";
	EXPECT_EQ(samples[1].mDrmMetadata.mSubSamples.size(), 6) << "Sample should have subsample encryption data";
	EXPECT_EQ(samples[1].mDrmMetadata.mNumSubSamples, 1) << "Sample should have 1 subsamples";
}

/**
 * @brief Helper test: Validate PSSH version 1 with KIDs
 */
TEST_F(Mp4DemuxFunctionalTests, ParsePsshV1WithKID)
{
	bool result = mDemuxer->Parse(std::make_shared<std::vector<uint8_t>>(psshBoxV1WithKID, psshBoxV1WithKID + sizeof(psshBoxV1WithKID)));
	ASSERT_TRUE(result) << "Parse should succeed for PSSH v1";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	auto protectionEvents = mDemuxer->GetProtectionEvents();
	ASSERT_EQ(protectionEvents.size(), 1) << "Should have one PSSH entry";
	EXPECT_FALSE(protectionEvents[0].systemID.empty());
	EXPECT_FALSE(protectionEvents[0].pssh.empty());

	// Reconstructed box must be byte-identical to the input (size, type,
	// version/flags, KIDs, and data).
	std::vector<uint8_t> expectedBox(psshBoxV1WithKID, psshBoxV1WithKID + sizeof(psshBoxV1WithKID));
	EXPECT_EQ(protectionEvents[0].pssh, expectedBox) << "Reconstructed PSSH v1 box should match input byte-for-byte";
}


/**
 * @brief Error handling: Empty buffer
 */
TEST_F(Mp4DemuxFunctionalTests, HandleEmptyBuffer)
{
	bool result = mDemuxer->Parse(std::make_shared<std::vector<uint8_t>>());
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
	
	bool result = mDemuxer->Parse(std::make_shared<std::vector<uint8_t>>(std::move(truncated)));
	// Either should succeed (graceful handling) or fail with error
	EXPECT_FALSE(result) << "Truncated box should be handled with error";
	EXPECT_NE(mDemuxer->GetLastError(), MP4_PARSE_OK) << "Should report error for truncated data";
}

// ---- helpers (local) ----

/**
 * @brief Write the 78-byte Visual Sample Entry payload consumed by ParseVideoInformation().
 *
 * Layout (ISO 14496-12 §12.1.3 VisualSampleEntry):
 *   - 24 bytes: reserved[6](6) + data_reference_index(2) + pre_defined/reserved(16)
 *               → consumed by SkipBytes(24)
 *   - 2 bytes:  width
 *   - 2 bytes:  height
 *   - 48 bytes: horizresolution(4) + vertresolution(4) + reserved(4) + frame_count(2)
 *               + compressorname(32) + depth(2)  → consumed by SkipBytes(48)
 *   - 2 bytes:  pre_defined (0xFFFF padding marker)
 */
static void writeVideoSampleEntryFields(std::vector<uint8_t>& b, uint16_t w, uint16_t h)
{
	for (int i = 0; i < 24; ++i) b.push_back(0x00); // reserved + pre_defined (SkipBytes(24))
	b.push_back(uint8_t(w >> 8)); b.push_back(uint8_t(w & 0xFF)); // width
	b.push_back(uint8_t(h >> 8)); b.push_back(uint8_t(h & 0xFF)); // height
	for (int i = 0; i < 48; ++i) b.push_back(0x00); // resolutions + compressorname + depth (SkipBytes(48))
	b.push_back(0xFF); b.push_back(0xFF); // padding marker (VIDEO_PREDEFINED_PADDING_MARKER = 0xFFFF)
}

static void write32be(std::vector<uint8_t>& b, uint32_t v) {
	b.push_back((v >> 24) & 0xFF);
	b.push_back((v >> 16) & 0xFF);
	b.push_back((v >> 8)  & 0xFF);
	b.push_back((v >> 0)  & 0xFF);
}

static void write64be(std::vector<uint8_t>& b, uint64_t v) {
	for (int i = 7; i >= 0; --i) b.push_back(uint8_t((v >> (8*i)) & 0xFF));
}
static void write4cc(std::vector<uint8_t>& b, const char t[4]) {
	b.insert(b.end(), t, t+4);
}
struct Box {
	std::vector<uint8_t>& buf; size_t start{}; bool extended{};
	Box(std::vector<uint8_t>& b, const char type[4], bool forceExtended=false)
	: buf(b) {
		start = buf.size();
		write32be(buf, 0); write4cc(buf, type);
		extended = forceExtended;
		if (extended) { buf[start+3] = 1; write64be(buf, 0); }
	}
	void close() {
		uint64_t total = buf.size() - start;
		if (extended) {
			size_t p = start + 8;
			for (int i = 7; i >= 0; --i) buf[p + (7-i)] = uint8_t((total >> (8*i)) & 0xFF);
		} else {
			uint32_t t32 = static_cast<uint32_t>(total);
			buf[start+0] = (t32 >> 24) & 0xFF; buf[start+1] = (t32 >> 16) & 0xFF;
			buf[start+2] = (t32 >> 8) & 0xFF;  buf[start+3] = (t32 >> 0) & 0xFF;
		}
	}
};
static void writeFullBoxHeader(std::vector<uint8_t>& b, uint8_t v, uint32_t f) {
	b.push_back(v); b.push_back(uint8_t((f>>16)&0xFF)); b.push_back(uint8_t((f>>8)&0xFF)); b.push_back(uint8_t(f&0xFF));
}

// helper for 16-bit BE
static inline void write16be(std::vector<uint8_t>& b, uint16_t v) {
	b.push_back(uint8_t((v>>8)&0xFF));
	b.push_back(uint8_t(v&0xFF));
}

// A) Extended-size box (size==1)
TEST(Mp4Demux_Gaps, ExtendedSizeBox) {
	std::vector<uint8_t> buf;
	{ Box ftyp(buf, "ftyp"); write4cc(buf,"isom"); write32be(buf,0); write4cc(buf,"isom"); write4cc(buf,"iso2"); ftyp.close(); }
	{ Box freeBox(buf, "free", /*forceExtended=*/true); freeBox.close(); }
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf))) << "Extended-size box should parse cleanly";  // exercises size==1 path in DemuxHelper
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// B) size==0 mdat (extends to EOF)
TEST(Mp4Demux_Gaps, SizeZeroMdatToEOF) {
	std::vector<uint8_t> buf;
	{ Box ftyp(buf, "ftyp"); write4cc(buf,"isom"); write32be(buf,0); write4cc(buf,"isom"); write4cc(buf,"iso2"); ftyp.close(); }
	write32be(buf, 0); write4cc(buf, "mdat");   // size == 0
	for (int i=0;i<32;++i) buf.push_back(uint8_t(i)); // payload
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// C) ESDS varint (via minimal mp4a+esds)
TEST(Mp4Demux_Gaps, EsdsVarintDecode) {
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box stsd(buf, "stsd"); writeFullBoxHeader(buf,0,0); write32be(buf,1);
			{ Box mp4a(buf, "mp4a");
				// reserved[6] + data_reference_index(2) + reserved[8]
				for (int i=0;i<16;++i) buf.push_back(0);
				// channel_count(2) = 2
				write16be(buf, 2);
				// sample_size(2) + pre_defined/reserved(4) -> 6 bytes
				buf.insert(buf.end(), 6, 0);
				// sample_rate 16.16: upper16 = 0xAC44 (~44100), lower16 = 0
				write16be(buf, 0xAC44);
				write16be(buf, 0x0000);
				{ Box esds(buf, "esds"); writeFullBoxHeader(buf,0,0);
					buf.push_back(0x03); buf.push_back(0x81); buf.push_back(0x00); // len = 128 (varint)
					buf.insert(buf.end(), 3, 0x00);           // ES_ID + flags
					buf.push_back(0x04); buf.push_back(0x0D); // dec cfg len = 13
					buf.insert(buf.end(), 13, 0);
					buf.push_back(0x05); buf.push_back(0x04); // DecoderSpecificInfo len = 4
					buf.push_back(0x11); buf.push_back(0x22); buf.push_back(0x33); buf.push_back(0x44);
					buf.push_back(0x06); buf.push_back(0x01); buf.push_back(0x00);
					esds.close();
				}
				mp4a.close();
			}
			stsd.close();
		}
		moov.close();
	}
	Mp4Demux d;
	
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	auto info = d.GetCodecInfo();
	ASSERT_EQ(info.mCodecData.size(), 4u);
	EXPECT_EQ(info.mCodecData[0], 0x11);
	EXPECT_EQ(info.mCodecData[3], 0x44);
}

// D) AC-4 init: ac-4 + dac4
TEST(Mp4Demux_Gaps, AC4InitHasCodecData) {
	std::vector<uint8_t> buf;
	// Build a minimal init segment: moov -> stsd (1 entry) -> ac-4 (AudioSampleEntry) -> dac4 (decoder-specific)
	{
		Box moov(buf, "moov");
		{
			Box stsd(buf, "stsd");
			writeFullBoxHeader(buf, /*version*/0, /*flags*/0);
			write32be(buf, /*entry_count*/1);
			{
				Box ac4(buf, "ac-4");  // AudioSampleEntry per ISO/IEC 14496-12
				
				// reserved[6] + data_reference_index(2) + reserved[8] = 16 bytes
				for (int i=0; i<16; ++i) buf.push_back(0x00);
				
				// channel_count(2) = 2
				write16be(buf, 2);
				
				// sample_size(2) + pre_defined(2) + reserved(2) = 6 bytes
				buf.insert(buf.end(), 6, 0x00);
				
				// sample_rate (32-bit fixed-point 16.16): upper16 = 0xAC44 (~44100), lower16 = 0x0000
				write16be(buf, 0xAC44);
				write16be(buf, 0x0000);
				
				// Decoder-specific AC-4 box: 'dac4' with 5 bytes of payload
				{
					Box dac4(buf, "dac4");
					for (int i=0; i<5; ++i) buf.push_back(uint8_t(0x10 + i));
					dac4.close();
				}
				ac4.close();
			}
			stsd.close();
		}
		moov.close();
	}
	
	// Parse and validate
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	auto info = d.GetCodecInfo();
	
	EXPECT_EQ(info.mCodecFormat, GST_FORMAT_AUDIO_ES_AC4);
	ASSERT_EQ(info.mCodecData.size(), 5u);
	EXPECT_EQ(info.mCodecData[0], 0x10);
	EXPECT_EQ(info.mCodecData[4], 0x14);
}

// stsd: entry_count == 0 is invalid and must raise MP4_PARSE_ERROR_INVALID_ENTRY_COUNT.
TEST(Mp4Demux_Stsd, EntryCountZero_RaisesInvalidEntryCountError)
{
	std::vector<uint8_t> buf;
	{
		Box moov(buf, "moov");
		{
			Box stsd(buf, "stsd");
			writeFullBoxHeader(buf, /*version*/0, /*flags*/0);
			write32be(buf, /*entry_count*/0);
			stsd.close();
		}
		moov.close();
	}
	Mp4Demux d;
	EXPECT_FALSE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_ERROR_INVALID_ENTRY_COUNT);
}

// stsd: entry_count > 1 (e.g. clear/encrypted variants) must parse successfully,
// with the last sample entry's codec info taking effect.
TEST(Mp4Demux_Stsd, MultipleEntries_ParsesSuccessfully_LastEntryWins)
{
	std::vector<uint8_t> buf;
	{
		Box moov(buf, "moov");
		{
			Box stsd(buf, "stsd");
			writeFullBoxHeader(buf, /*version*/0, /*flags*/0);
			write32be(buf, /*entry_count*/2);
			for (uint8_t entryPayloadByte : { uint8_t(0xAA), uint8_t(0xBB) })
			{
				Box ac4(buf, "ac-4"); // AudioSampleEntry per ISO/IEC 14496-12

				// reserved[6] + data_reference_index(2) + reserved[8] = 16 bytes
				for (int i = 0; i < 16; ++i) buf.push_back(0x00);

				// channel_count(2) = 2
				write16be(buf, 2);

				// sample_size(2) + pre_defined(2) + reserved(2) = 6 bytes
				buf.insert(buf.end(), 6, 0x00);

				// sample_rate (32-bit fixed-point 16.16): upper16 = 0xAC44, lower16 = 0x0000
				write16be(buf, 0xAC44);
				write16be(buf, 0x0000);

				// Decoder-specific AC-4 box: 'dac4', payload distinguishes entries
				{
					Box dac4(buf, "dac4");
					for (int i = 0; i < 5; ++i) buf.push_back(uint8_t(entryPayloadByte + i));
					dac4.close();
				}
				ac4.close();
			}
			stsd.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
	auto info = d.GetCodecInfo();
	ASSERT_EQ(info.mCodecData.size(), 5u);
	// Last-parsed entry (payload starting at 0xBB) must be the one in effect.
	EXPECT_EQ(info.mCodecData[0], 0xBB);
}

// ---- TST_2009: HLS + Widevine DRM, useMp4Demux=true ----
//
// CBCS-encrypted HLS fMP4 init segments legitimately carry two stsd entries:
//   Entry 1 – clear sample entry  (e.g. 'avc1')
//   Entry 2 – encrypted sample entry ('encv') with sinf/frma/schm(cbcs)/schi/tenc
//
// Before the fix ParseSampleDescriptionBox() threw
// MP4_PARSE_ERROR_UNSUPPORTED_SAMPLE_ENTRY_COUNT for any count != 1, causing
// the entire demux of an HLS+Widevine stream to fail.
//
// The two tests below assert that:
//   (a) Parse() succeeds for dual-entry stsd in either entry order.
//   (b) mIsEncrypted is true after parsing the init segment – either directly
//       (when 'encv' is the last-parsed entry) or via the
//       handledEncryptedSamples force in Parse() (when 'encv' is first).

/**
 * @brief TST_2009 regression – clear 'avc1' first, encrypted 'encv'/CBCS second.
 *
 * This is the canonical CBCS HLS layout: the encrypted sample entry follows
 * the clear one.  The encrypted entry is the last parsed, so its tenc box
 * sets mIsEncrypted = true directly.
 */
TEST(Mp4Demux_Stsd, TST2009_HlsCbcs_ClearFirstEncryptedSecond_ParseSucceeds_IsEncrypted)
{
	std::vector<uint8_t> buf;

	static const uint8_t kid[16] = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00
	};
	// Different avcC payloads let us verify which entry's codec data is active.
	static const uint8_t avcCClear[4]      = { 0x01, 0x64, 0x00, 0x1F };
	static const uint8_t avcCEncrypted[4]  = { 0x01, 0x64, 0x00, 0x29 };

	{
		Box moov(buf, "moov");
		{
			Box stsd(buf, "stsd");
			writeFullBoxHeader(buf, /*version*/0, /*flags*/0);
			write32be(buf, /*entry_count*/2);

			// Entry 1: clear 'avc1'
			{
				Box avc1(buf, "avc1");
				writeVideoSampleEntryFields(buf, 1280, 720);
				{ Box avcC(buf, "avcC"); buf.insert(buf.end(), avcCClear, avcCClear + 4); avcC.close(); }
				avc1.close();
			}

			// Entry 2: encrypted 'encv' – CBCS scheme
			{
				Box encv(buf, "encv");
				writeVideoSampleEntryFields(buf, 1280, 720);
				{ Box avcC(buf, "avcC"); buf.insert(buf.end(), avcCEncrypted, avcCEncrypted + 4); avcC.close(); }
				{
					Box sinf(buf, "sinf");
					{ Box frma(buf, "frma"); write4cc(buf, "avc1"); frma.close(); }
					{ Box schm(buf, "schm"); writeFullBoxHeader(buf, 0, 0); write4cc(buf, "cbcs"); write32be(buf, 0x00010000); schm.close(); }
					{
						Box schi(buf, "schi");
						{
							Box tenc(buf, "tenc");
							writeFullBoxHeader(buf, /*version*/0, /*flags*/0);
							buf.push_back(0x00); // reserved
							buf.push_back(0x00); // pattern (crypt/skip nibbles – 0 for test)
							buf.push_back(0x01); // is_encrypted = 1
							buf.push_back(0x10); // iv_size = 16
							buf.insert(buf.end(), kid, kid + 16);
							tenc.close();
						}
						schi.close();
					}
					sinf.close();
				}
				encv.close();
			}
			stsd.close();
		}
		moov.close();
	}

	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)))
		<< "Dual-entry stsd (clear avc1 + encrypted encv/CBCS) must parse without error (TST_2009 regression)";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);

	auto info = d.GetCodecInfo();
	EXPECT_TRUE(info.mIsEncrypted)
		<< "mIsEncrypted must be true: tenc with is_encrypted=1 was the last-parsed entry";
	EXPECT_EQ(info.mCodecFormat, GST_FORMAT_VIDEO_ES_H264)
		<< "Codec format must be H.264 (avcC inside encv)";
	// Last-parsed entry wins for codec data
	ASSERT_EQ(info.mCodecData.size(), 4u);
	EXPECT_EQ(info.mCodecData[0], avcCEncrypted[0]);
}

/**
 * @brief TST_2009 regression – encrypted 'encv'/CBCS first, clear 'avc1' second.
 *
 * In this order the clear entry is last-parsed.  It has no tenc box so it
 * cannot reset mIsEncrypted.  handledEncryptedSamples (set by the first
 * entry's tenc) causes Parse() to force mIsEncrypted = true even though
 * the clear entry was seen most recently.
 */
TEST(Mp4Demux_Stsd, TST2009_HlsCbcs_EncryptedFirstClearSecond_ParseSucceeds_IsEncrypted)
{
	std::vector<uint8_t> buf;

	static const uint8_t kid[16] = {
		0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
		0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99
	};
	static const uint8_t avcCEncrypted[4]  = { 0x01, 0x64, 0x00, 0x29 };
	static const uint8_t avcCClear[4]      = { 0x01, 0x64, 0x00, 0x1F };

	{
		Box moov(buf, "moov");
		{
			Box stsd(buf, "stsd");
			writeFullBoxHeader(buf, /*version*/0, /*flags*/0);
			write32be(buf, /*entry_count*/2);

			// Entry 1: encrypted 'encv' – CBCS scheme
			{
				Box encv(buf, "encv");
				writeVideoSampleEntryFields(buf, 1280, 720);
				{ Box avcC(buf, "avcC"); buf.insert(buf.end(), avcCEncrypted, avcCEncrypted + 4); avcC.close(); }
				{
					Box sinf(buf, "sinf");
					{ Box frma(buf, "frma"); write4cc(buf, "avc1"); frma.close(); }
					{ Box schm(buf, "schm"); writeFullBoxHeader(buf, 0, 0); write4cc(buf, "cbcs"); write32be(buf, 0x00010000); schm.close(); }
					{
						Box schi(buf, "schi");
						{
							Box tenc(buf, "tenc");
							writeFullBoxHeader(buf, /*version*/0, /*flags*/0);
							buf.push_back(0x00); // reserved
							buf.push_back(0x00); // pattern
							buf.push_back(0x01); // is_encrypted = 1
							buf.push_back(0x10); // iv_size = 16
							buf.insert(buf.end(), kid, kid + 16);
							tenc.close();
						}
						schi.close();
					}
					sinf.close();
				}
				encv.close();
			}

			// Entry 2: clear 'avc1' (last-parsed; no tenc → mIsEncrypted must not be cleared)
			{
				Box avc1(buf, "avc1");
				writeVideoSampleEntryFields(buf, 1280, 720);
				{ Box avcC(buf, "avcC"); buf.insert(buf.end(), avcCClear, avcCClear + 4); avcC.close(); }
				avc1.close();
			}
			stsd.close();
		}
		moov.close();
	}

	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)))
		<< "Dual-entry stsd (encrypted encv/CBCS + clear avc1) must parse without error (TST_2009 regression)";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);

	auto info = d.GetCodecInfo();
	// handledEncryptedSamples in Parse() forces mIsEncrypted=true even though the
	// last-parsed entry was the clear avc1 with no tenc of its own.
	EXPECT_TRUE(info.mIsEncrypted)
		<< "mIsEncrypted must be true: handledEncryptedSamples forces it even when clear entry is last";
	EXPECT_EQ(info.mCodecFormat, GST_FORMAT_VIDEO_ES_H264);
}

// E) TRUN overrun detection (negative)
TEST(Mp4Demux_Gaps, TrunOverrunDetection) {
	std::vector<uint8_t> buf;
	size_t moofStartIdx;
	{ Box moof(buf, "moof"); moofStartIdx = moof.start;
		{ Box traf(buf, "traf");
			{ Box tfhd(buf, "tfhd"); writeFullBoxHeader(buf,0, 0x00008 | 0x00010); // default dur+size present
				write32be(buf, 1); write32be(buf, 90000/30); write32be(buf, 10); tfhd.close();
			}
			{ Box tfdt(buf, "tfdt"); writeFullBoxHeader(buf,0,0); write32be(buf,0); tfdt.close(); }
			{ Box trun(buf, "trun"); writeFullBoxHeader(buf,0, 0x0001 | 0x0200); // data_offset + sample_size
				write32be(buf, 1); write32be(buf, 0);             // placeholder data_offset
				write32be(buf, 12);                                // sample size larger than payload
				trun.close();
			}
			traf.close();
		}
		moof.close();
	}
	// small mdat
	write32be(buf, 16); write4cc(buf, "mdat"); // 8 header + 8 payload
	size_t mdatPayload = buf.size(); for (int i=0;i<8;++i) buf.push_back(uint8_t(i));
	// patch trun data_offset to point into mdat payload
	size_t trunPos=0; for (size_t i=0;i+3<buf.size(); ++i) if (buf[i]=='t'&&buf[i+1]=='r'&&buf[i+2]=='u'&&buf[i+3]=='n'){ trunPos=i-4; break; }
	ASSERT_NE(trunPos, 0u);
	size_t dataOffsetPos = trunPos + 4 + 4 + 4 + 4;
	int32_t dataOffset = int32_t(mdatPayload - moofStartIdx);
	buf[dataOffsetPos+0] = uint8_t((dataOffset>>24)&0xFF);
	buf[dataOffsetPos+1] = uint8_t((dataOffset>>16)&0xFF);
	buf[dataOffsetPos+2] = uint8_t((dataOffset>>8)&0xFF);
	buf[dataOffsetPos+3] = uint8_t((dataOffset>>0)&0xFF);
	
	Mp4Demux d;
	bool ok = d.Parse(std::make_shared<std::vector<uint8_t>>(buf));
	EXPECT_FALSE(ok);
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH);
}

// F) Multiple moof+mdat pairs with correct trun data_offset handling (positive case)
// Verifies that each sample's payload bytes are sourced from the correct mdat box in consecutive moof+mdat pairs.
TEST(Mp4Demux_Gaps, TST2052_LLDMultipleMoofMdatPairs) {
	std::vector<uint8_t> buf;
	
	// ---- helper to patch a 4-byte big-endian int32 at a specific index ----
	auto patchI32 = [&](size_t idx, int32_t v) {
		buf[idx+0] = uint8_t((v>>24)&0xFF);
		buf[idx+1] = uint8_t((v>>16)&0xFF);
		buf[idx+2] = uint8_t((v>>8) &0xFF);
		buf[idx+3] = uint8_t( v     &0xFF);
	};
	
	// ---- build moof1 ----
	// default_sample_duration = 3000, default_sample_size = 10
	// 2 samples → mdat1 payload = 20 bytes
	size_t moof1Start;
	size_t trun1DataOffsetPos;
	{
		Box moof(buf, "moof"); moof1Start = moof.start;
		{ Box mfhd(buf, "mfhd"); writeFullBoxHeader(buf,0,0); write32be(buf,1); mfhd.close(); }
		{ Box traf(buf, "traf");
			{ Box tfhd(buf, "tfhd");
				writeFullBoxHeader(buf, 0, 0x00008 | 0x00010); // default dur + default size
				write32be(buf, 1);     // track_ID
				write32be(buf, 3000);  // default_sample_duration (0.1 s @ 30000 Hz)
				write32be(buf, 10);    // default_sample_size
				tfhd.close();
			}
			{ Box tfdt(buf, "tfdt"); writeFullBoxHeader(buf,0,0); write32be(buf,0); tfdt.close(); }
			{ Box trun(buf, "trun");
				writeFullBoxHeader(buf, 0, 0x0001); // TRUN_DATA_OFFSET_PRESENT
				write32be(buf, 2);  // sample_count = 2
				trun1DataOffsetPos = buf.size();
				write32be(buf, 0);  // data_offset placeholder
				trun.close();
			}
			traf.close();
		}
		moof.close();
	}
	// patch trun1 data_offset: distance from moof1 start to mdat1 payload
	// mdat1 header is 8 bytes, so payload starts at buf.size() + 8
	patchI32(trun1DataOffsetPos, int32_t((buf.size() + 8) - moof1Start));
	
	// ---- mdat1: 20 bytes (2 samples × 10 bytes) ----
	write32be(buf, 28); write4cc(buf, "mdat"); // 8 header + 20 payload
	for (int i = 0; i < 20; ++i) buf.push_back(uint8_t(0xAA + i));
	
	// ---- build moof2 ----
	// default_sample_duration = 3000, default_sample_size = 15
	// 1 sample → mdat2 payload = 15 bytes
	size_t moof2Start;
	size_t trun2DataOffsetPos;
	{
		Box moof(buf, "moof"); moof2Start = moof.start;
		{ Box mfhd(buf, "mfhd"); writeFullBoxHeader(buf,0,0); write32be(buf,2); mfhd.close(); }
		{ Box traf(buf, "traf");
			{ Box tfhd(buf, "tfhd");
				writeFullBoxHeader(buf, 0, 0x00008 | 0x00010);
				write32be(buf, 1);     // track_ID
				write32be(buf, 3000);  // default_sample_duration
				write32be(buf, 15);    // default_sample_size
				tfhd.close();
			}
			// baseMediaDecodeTime = 6000 (2 samples × 3000)
			{ Box tfdt(buf, "tfdt"); writeFullBoxHeader(buf,0,0); write32be(buf,6000); tfdt.close(); }
			{ Box trun(buf, "trun");
				writeFullBoxHeader(buf, 0, 0x0001); // TRUN_DATA_OFFSET_PRESENT
				write32be(buf, 1);  // sample_count = 1
				trun2DataOffsetPos = buf.size();
				write32be(buf, 0);  // data_offset placeholder
				trun.close();
			}
			traf.close();
		}
		moof.close();
	}
	// patch trun2 data_offset: distance from moof2 start to mdat2 payload
	patchI32(trun2DataOffsetPos, int32_t((buf.size() + 8) - moof2Start));
	
	// ---- mdat2: 15 bytes (1 sample × 15 bytes) ----
	write32be(buf, 23); write4cc(buf, "mdat"); // 8 header + 15 payload
	for (int i = 0; i < 15; ++i) buf.push_back(uint8_t(0xBB + i));
	
	// ---- parse ----
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)))
	<< "LLD [moof][mdat][moof][mdat] must parse without error";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
	
	auto samples = d.GetSamples(); // mData aliased shared_ptr keeps the backing buffer alive for these samples
	ASSERT_EQ(samples.size(), 3u) << "Expected 3 samples total (2 from moof1 + 1 from moof2)";
	
	// ---- validate moof1 samples (data from mdat1) ----
	ASSERT_EQ(samples[0].mDataSize, 10u) << "Sample 0: 10 bytes from mdat1";
	EXPECT_EQ(samples[0].mData.get()[0], uint8_t(0xAA))
	<< "Sample 0 first byte should match first byte of mdat1 payload";
	
	ASSERT_EQ(samples[1].mDataSize, 10u) << "Sample 1: 10 bytes from mdat1";
	EXPECT_EQ(samples[1].mData.get()[0], uint8_t(0xAA + 10))
	<< "Sample 1 first byte should match second chunk of mdat1 payload";
	
	// ---- validate moof2 sample (data from mdat2, NOT mdat1) ----
	ASSERT_EQ(samples[2].mDataSize, 15u) << "Sample 2: 15 bytes from mdat2";
	EXPECT_EQ(samples[2].mData.get()[0], uint8_t(0xBB))
	<< "Sample 2 first byte must come from mdat2, not mdat1";
}

// G) LL-DASH regression: two consecutive moof+mdat pairs in one Parse() call must not produce DATA_BOUNDARY_MISMATCH.
// each fragment's pending payloads are only resolved against the mdat that belongs to that specific moof.
TEST(Mp4Demux_Gaps, MultiMoofMdatNoBoundaryError)
{
	std::vector<uint8_t> buf;
	
	size_t moof1StartIdx = 0, moof2StartIdx = 0;
	size_t trun1DataOffsetPos = 0, trun2DataOffsetPos = 0;
	size_t mdat1PayloadStart = 0, mdat2PayloadStart = 0;
	
	// --- moof1 + mdat1 ---
	{
		Box moof(buf, "moof"); moof1StartIdx = moof.start;
		{
			Box traf(buf, "traf");
			{
				// tfhd: default-sample-duration-present (0x8) | default-sample-size-present (0x10)
				Box tfhd(buf, "tfhd"); writeFullBoxHeader(buf, 0, 0x00008 | 0x00010);
				write32be(buf, 1);      // track_ID
				write32be(buf, 3000);   // default_sample_duration
				write32be(buf, 8);      // default_sample_size (8 bytes per sample)
				tfhd.close();
			}
			{
				Box tfdt(buf, "tfdt"); writeFullBoxHeader(buf, 0, 0);
				write32be(buf, 0);      // baseMediaDecodeTime = 0
				tfdt.close();
			}
			{
				// trun: flags = 0x0001 (data-offset-present only; sizes from tfhd default)
				Box trun(buf, "trun"); writeFullBoxHeader(buf, 0, 0x0001);
				write32be(buf, 1);      // sample_count = 1
				trun1DataOffsetPos = buf.size();
				write32be(buf, 0);      // placeholder: data_offset (patched below)
				trun.close();
			}
			traf.close();
		}
		moof.close();
	}
	// mdat1: 8-byte header + 8-byte payload
	write32be(buf, 8 + 8); write4cc(buf, "mdat");
	mdat1PayloadStart = buf.size();
	for (int i = 0; i < 8; ++i) buf.push_back(uint8_t(0xA0 + i));
	
	// --- moof2 + mdat2 ---
	{
		Box moof(buf, "moof"); moof2StartIdx = moof.start;
		{
			Box traf(buf, "traf");
			{
				Box tfhd(buf, "tfhd"); writeFullBoxHeader(buf, 0, 0x00008 | 0x00010);
				write32be(buf, 1);      // track_ID
				write32be(buf, 3000);   // default_sample_duration
				write32be(buf, 8);      // default_sample_size
				tfhd.close();
			}
			{
				Box tfdt(buf, "tfdt"); writeFullBoxHeader(buf, 0, 0);
				write32be(buf, 3000);   // baseMediaDecodeTime = 3000 (one chunk later)
				tfdt.close();
			}
			{
				Box trun(buf, "trun"); writeFullBoxHeader(buf, 0, 0x0001);
				write32be(buf, 1);      // sample_count = 1
				trun2DataOffsetPos = buf.size();
				write32be(buf, 0);      // placeholder: data_offset (patched below)
				trun.close();
			}
			traf.close();
		}
		moof.close();
	}
	// mdat2: 8-byte header + 8-byte payload
	write32be(buf, 8 + 8); write4cc(buf, "mdat");
	mdat2PayloadStart = buf.size();
	for (int i = 0; i < 8; ++i) buf.push_back(uint8_t(0xB0 + i));
	
	// Patch trun data_offset fields: offset is relative to the start of the owning moof box
	auto patch32 = [&](size_t pos, int32_t v) {
		buf[pos+0] = uint8_t((v >> 24) & 0xFF);
		buf[pos+1] = uint8_t((v >> 16) & 0xFF);
		buf[pos+2] = uint8_t((v >>  8) & 0xFF);
		buf[pos+3] = uint8_t((v >>  0) & 0xFF);
	};
	patch32(trun1DataOffsetPos, int32_t(mdat1PayloadStart - moof1StartIdx));
	patch32(trun2DataOffsetPos, int32_t(mdat2PayloadStart - moof2StartIdx));
	
	Mp4Demux d;
	bool ok = d.Parse(std::make_shared<std::vector<uint8_t>>(buf));
	EXPECT_TRUE(ok) << "Multi-moof+mdat segment (LL-DASH) should parse without errors";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK) << "Should not raise DATA_BOUNDARY_MISMATCH";
	
	auto samples = d.GetSamples(); // mData aliased shared_ptr keeps the backing buffer alive for these samples
	ASSERT_EQ(samples.size(), 2u) << "Should extract one sample per moof+mdat pair";
	
	// Validate sample 0 is bound to mdat1 payload (0xA0–0xA7)
	ASSERT_EQ(samples[0].mDataSize, 8u) << "Sample 0 should be 8 bytes (mdat1 payload)";
	const uint8_t* s0 = samples[0].mData.get();
	for (int i = 0; i < 8; ++i)
	{
		EXPECT_EQ(s0[i], uint8_t(0xA0 + i))
		<< "Sample 0 byte[" << i << "] should be mdat1 payload (0x"
		<< std::hex << (0xA0 + i) << ")";
	}
	
	// Validate sample 1 is bound to mdat2 payload (0xB0–0xB7)
	ASSERT_EQ(samples[1].mDataSize, 8u) << "Sample 1 should be 8 bytes (mdat2 payload)";
	const uint8_t* s1 = samples[1].mData.get();
	for (int i = 0; i < 8; ++i)
	{
		EXPECT_EQ(s1[i], uint8_t(0xB0 + i))
		<< "Sample 1 byte[" << i << "] should be mdat2 payload (0x"
		<< std::hex << (0xB0 + i) << ")";
	}
}

// H) No-init-segment: SAIO/SAIZ auxiliary info, no init segment fed.
TEST(Mp4Demux_NoInitSegment, SaioSaizFragment_WithoutInitSegment_NoCrash)
{
	std::vector<uint8_t> buf;
	size_t dataOffsetFieldPos = 0;
	size_t saioOffsetFieldPos = 0;
	
	{
		Box moof(buf, "moof");
		{ Box mfhd(buf, "mfhd"); writeFullBoxHeader(buf,0,0); write32be(buf,1); mfhd.close(); }
		{
			Box traf(buf, "traf");
			{ Box tfhd(buf, "tfhd"); writeFullBoxHeader(buf,0,0x000018);
				write32be(buf,1); write32be(buf,3000); write32be(buf,32); tfhd.close(); }
			{ Box tfdt(buf, "tfdt"); writeFullBoxHeader(buf,0,0); write32be(buf,0); tfdt.close(); }
			{ Box trun(buf, "trun"); writeFullBoxHeader(buf,0,0x0001);
				write32be(buf,2);
				dataOffsetFieldPos = buf.size();
				write32be(buf,0); // data_offset placeholder
				trun.close(); }
			// saiz: flags=0 (no aux_info_type), default_info_size=16, sample_count=2
			// Each aux entry = 8-byte IV + u16 count + 6-byte subsample = 16 bytes total
			{ Box saiz(buf, "saiz"); writeFullBoxHeader(buf,0,0);
				buf.push_back(16);    // default_info_size = 16
				write32be(buf,2);     // sample_count = 2
				saiz.close(); }
			// saio: flags=0, version=0, entry_count=1, offset placeholder
			{ Box saio(buf, "saio"); writeFullBoxHeader(buf,0,0);
				write32be(buf,1);                      // entry_count = 1
				saioOffsetFieldPos = buf.size();
				write32be(buf,0);                      // offset placeholder
				saio.close(); }
			traf.close();
		}
		moof.close();
	}
	
	size_t moofSize = buf.size();
	
	// Aux info size: 2 samples × (8-byte IV + u16 numSubs + 6-byte entry) = 32 bytes
	const size_t auxInfoSize = 32;
	
	// TRUN data_offset: past moof + mdat-header + aux info
	int32_t dataOffset = static_cast<int32_t>(moofSize + 8 + auxInfoSize);
	buf[dataOffsetFieldPos+0] = uint8_t((dataOffset>>24)&0xFF);
	buf[dataOffsetFieldPos+1] = uint8_t((dataOffset>>16)&0xFF);
	buf[dataOffsetFieldPos+2] = uint8_t((dataOffset>>8 )&0xFF);
	buf[dataOffsetFieldPos+3] = uint8_t((dataOffset>>0 )&0xFF);
	
	// SAIO offset: from start of moof to first byte of aux info (right after mdat header)
	uint32_t saioOffset = static_cast<uint32_t>(moofSize + 8);
	buf[saioOffsetFieldPos+0] = uint8_t((saioOffset>>24)&0xFF);
	buf[saioOffsetFieldPos+1] = uint8_t((saioOffset>>16)&0xFF);
	buf[saioOffsetFieldPos+2] = uint8_t((saioOffset>>8 )&0xFF);
	buf[saioOffsetFieldPos+3] = uint8_t((saioOffset>>0 )&0xFF);
	
	// Append mdat: aux info (32 bytes) then sample data (64 bytes)
	write32be(buf, static_cast<uint32_t>(8 + auxInfoSize + 64));
	write4cc(buf, "mdat");
	// aux info sample 0: 8-byte IV + 1 subsample entry
	buf.insert(buf.end(), {0xA1,0xB2,0xC3,0xD4,0xE5,0xF6,0x07,0x08}); // IV[0]
	write16be(buf,1);
	write16be(buf,16);
	write32be(buf,48);
	// aux info sample 1: 8-byte IV + 1 subsample entry
	buf.insert(buf.end(), {0xA1,0xB2,0xC3,0xD4,0xE5,0xF6,0x07,0x09}); // IV[1]
	write16be(buf,1);
	write16be(buf,16);
	write32be(buf,48);
	// sample data
	for (int i = 0; i < 64; ++i)
		buf.push_back(uint8_t(i & 0xFF));
	
	// Parse WITHOUT an init segment → ivSize stays 0
	Mp4Demux d;
	bool ok = d.Parse(std::make_shared<std::vector<uint8_t>>(buf)); // must NOT crash
	
	// ProcessAuxiliaryInformation: cencAuxInfoSizes[i]=16 > 0==ivSize
	EXPECT_FALSE(ok);
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH);
}

// Build a minimal moof+traf+tfhd+tfdt+trun+mdat fragment with one sample and
// no init segment (no moov/mvhd/mdhd), to exercise timeScale==0 handling.
static std::vector<uint8_t> BuildSingleSampleFragmentNoInit(uint32_t sampleDurationTicks, uint32_t sampleSize)
{
	std::vector<uint8_t> buf;
	size_t dataOffsetFieldPos = 0;
	{
		Box moof(buf, "moof");
		{ Box mfhd(buf, "mfhd"); writeFullBoxHeader(buf,0,0); write32be(buf,1); mfhd.close(); }
		{
			Box traf(buf, "traf");
			{ Box tfhd(buf, "tfhd"); writeFullBoxHeader(buf,0,0); write32be(buf,1); tfhd.close(); }
			{ Box tfdt(buf, "tfdt"); writeFullBoxHeader(buf,0,0); write32be(buf,0); tfdt.close(); }
			{ Box trun(buf, "trun"); writeFullBoxHeader(buf,0,0x0301); // data-offset + duration + size present
				write32be(buf,1); // sample_count
				dataOffsetFieldPos = buf.size();
				write32be(buf,0); // data_offset placeholder
				write32be(buf,sampleDurationTicks);
				write32be(buf,sampleSize);
				trun.close(); }
			traf.close();
		}
		moof.close();
	}
	size_t moofSize = buf.size();
	int32_t dataOffset = static_cast<int32_t>(moofSize + 8);
	buf[dataOffsetFieldPos+0] = uint8_t((dataOffset>>24)&0xFF);
	buf[dataOffsetFieldPos+1] = uint8_t((dataOffset>>16)&0xFF);
	buf[dataOffsetFieldPos+2] = uint8_t((dataOffset>>8 )&0xFF);
	buf[dataOffsetFieldPos+3] = uint8_t((dataOffset>>0 )&0xFF);

	write32be(buf, static_cast<uint32_t>(8 + sampleSize));
	write4cc(buf, "mdat");
	for (uint32_t i = 0; i < sampleSize; ++i)
	{
		buf.push_back(uint8_t(i & 0xFF));
	}
	return buf;
}

// Regression test for a subtitle stream whose manifest SegmentTemplate has no
// 'initialization' attribute: timeScale stays 0, so sample pts/dts/duration
// must be set to 0 rather than dividing by zero (which produced NaN/Inf).
TEST(Mp4Demux_NoInitSegment, NoTimeScale_SamplePtsAndDurationSetToZero_NotNanOrInf)
{
	std::vector<uint8_t> buf = BuildSingleSampleFragmentNoInit(/*sampleDurationTicks=*/48000, /*sampleSize=*/16);

	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetTimeScale(), 0u) << "No mvhd/mdhd parsed, timescale should remain 0";

	auto samples = d.GetSamples();
	ASSERT_EQ(samples.size(), 1u);
	EXPECT_TRUE(std::isfinite(samples[0].mPts)) << "pts must not be NaN/Inf when timescale is 0";
	EXPECT_TRUE(std::isfinite(samples[0].mDts)) << "dts must not be NaN/Inf when timescale is 0";
	EXPECT_TRUE(std::isfinite(samples[0].mDuration)) << "duration must not be NaN/Inf when timescale is 0";
	EXPECT_DOUBLE_EQ(samples[0].mPts, 0.0);
	EXPECT_DOUBLE_EQ(samples[0].mDts, 0.0);
	EXPECT_DOUBLE_EQ(samples[0].mDuration, 0.0);
}

// When no mvhd/mdhd is present, SetFallbackTimeScale() (manifest-declared
// timescale, e.g. DASH SegmentTemplate@timescale) should be used instead of 0.
TEST(Mp4Demux_NoInitSegment, FallbackTimeScale_UsedWhenNoMdhdMvhdParsed)
{
	std::vector<uint8_t> buf = BuildSingleSampleFragmentNoInit(/*sampleDurationTicks=*/48000, /*sampleSize=*/16);

	Mp4Demux d;
	d.SetFallbackTimeScale(48000);
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetTimeScale(), 0u) << "mdhd/mvhd still not parsed; box-derived timescale stays 0";

	auto samples = d.GetSamples();
	ASSERT_EQ(samples.size(), 1u);
	EXPECT_DOUBLE_EQ(samples[0].mDuration, 1.0) << "48000 ticks / 48000 fallback timescale == 1 second";
	EXPECT_DOUBLE_EQ(samples[0].mPts, 0.0);
	EXPECT_DOUBLE_EQ(samples[0].mDts, 0.0);
}


// I) SENC box with corrupted (huge) subsample_count
TEST(Mp4Demux_Gaps, SencHugeSubsampleCount)
{
	std::vector<uint8_t> buf;
	size_t dataOffsetFieldPos = 0;
	
	{
		Box moof(buf, "moof");
		{ Box mfhd(buf, "mfhd"); writeFullBoxHeader(buf, 0, 0); write32be(buf, 1); mfhd.close(); }
		{
			Box traf(buf, "traf");
			{
				// tfhd: default-sample-duration-present (0x8) | default-sample-size-present (0x10)
				Box tfhd(buf, "tfhd"); writeFullBoxHeader(buf, 0, 0x000018);
				write32be(buf, 1);     // track_ID
				write32be(buf, 3000); // default_sample_duration
				write32be(buf, 64);   // default_sample_size
				tfhd.close();
			}
			{
				Box tfdt(buf, "tfdt"); writeFullBoxHeader(buf, 0, 0);
				write32be(buf, 0);    // baseMediaDecodeTime = 0
				tfdt.close();
			}
			{
				// trun: data-offset-present only
				Box trun(buf, "trun"); writeFullBoxHeader(buf, 0, 0x0001);
				write32be(buf, 1);              // sample_count = 1
				dataOffsetFieldPos = buf.size();
				write32be(buf, 0);              // data_offset placeholder
				trun.close();
			}
			{
				// SENC: version=0, flags=0x000002 (use_subsamples), sample_count=1
				// Sample 0: 8-byte IV followed by subsample_count=0xFFFF (corrupted).
				// Attempting to read 0xFFFF*6 = 393,210 bytes must be caught by
				// the bounds guard before any out-of-range memory access occurs.
				Box senc(buf, "senc"); writeFullBoxHeader(buf, 0, 0x000002);
				write32be(buf, 1); // sample_count = 1
				// 8-byte IV
				buf.insert(buf.end(), {0xDE,0xAD,0xBE,0xEF,0xCA,0xFE,0xBA,0xBE});
				// subsample_count = 0xFFFF → requires 393,210 bytes → will overrun
				write16be(buf, 0xFFFF);
				senc.close();
			}
			traf.close();
		}
		moof.close();
	}
	
	size_t moofSize = buf.size();
	
	// Patch trun data_offset to point past the mdat header
	int32_t dataOffset = static_cast<int32_t>(moofSize + 8);
	buf[dataOffsetFieldPos+0] = uint8_t((dataOffset >> 24) & 0xFF);
	buf[dataOffsetFieldPos+1] = uint8_t((dataOffset >> 16) & 0xFF);
	buf[dataOffsetFieldPos+2] = uint8_t((dataOffset >>  8) & 0xFF);
	buf[dataOffsetFieldPos+3] = uint8_t((dataOffset >>  0) & 0xFF);
	
	// Append mdat: 64 bytes of sample payload
	write32be(buf, 8 + 64);
	write4cc(buf, "mdat");
	for (int i = 0; i < 64; ++i) buf.push_back(uint8_t(i & 0xFF));
	
	Mp4Demux d;
	bool ok = d.Parse(std::make_shared<std::vector<uint8_t>>(buf)); // must NOT crash
	
	// subsample_count=0xFFFF → 393,210 bytes needed, far beyond buffer end;
	// the parser must reject this with DATA_BOUNDARY_MISMATCH, not crash.
	EXPECT_FALSE(ok);
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH);
}

// ============================================================
// Tests for ParseMetaBox, ParseSampleGroupDescription (sgpd),
// and ParseSampleToGroup (sbgp) — added for VPAAMP-428 review.
// Each test wraps its target box inside a minimal 'moov' container
// so DemuxHelper recurses into it normally.
// ============================================================

// --- meta box tests ---

// meta QTFF short variant: bytes[4..7] of payload == 'hdlr'
// Detection: secondWord == 'hdlr'; children start at ptr (no FullBox header).
TEST(Mp4Demux_NewBoxParsers, MetaQtffVariant_ParsesWithoutError)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box meta(buf, "meta");
			// Build an hdlr child as the only content.
			// bytes[0..3] of meta payload = hdlr box size (set by Box::close)
			// bytes[4..7] of meta payload = 'hdlr'  <-- triggers QTFF detection
			{ Box hdlr(buf, "hdlr");
				buf.insert(buf.end(), 20, 0x00); // hdlr fixed fields
				hdlr.close();
			}
			meta.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)))
		<< "meta QTFF variant should parse without error";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// meta ISO BMFF full-atom variant: bytes[0..3] == 0x00000000 (version=0, flags=0)
TEST(Mp4Demux_NewBoxParsers, MetaIsoBmffVariant_ParsesWithoutError)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box meta(buf, "meta");
			writeFullBoxHeader(buf, 0, 0); // version+flags = 0x00000000
			{ Box hdlr(buf, "hdlr");
				buf.insert(buf.end(), 20, 0x00);
				hdlr.close();
			}
			meta.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)))
		<< "meta ISO BMFF variant should parse without error";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// meta ISO BMFF variant with 'ilst' child — regression for boundary-mismatch bug.
// 'ilst' is now in the explicit skip list; must not trigger an error.
TEST(Mp4Demux_NewBoxParsers, MetaIsoBmffWithIlstChild_NoBoundaryMismatch)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box meta(buf, "meta");
			writeFullBoxHeader(buf, 0, 0);
			{ Box ilst(buf, "ilst");
				buf.insert(buf.end(), 8, 0xCC); // arbitrary payload
				ilst.close();
			}
			meta.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)))
		<< "meta with ilst child must not raise boundary-mismatch";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// meta ISO BMFF variant with 'keys' child — same regression guard as ilst.
TEST(Mp4Demux_NewBoxParsers, MetaIsoBmffWithKeysChild_NoBoundaryMismatch)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box meta(buf, "meta");
			writeFullBoxHeader(buf, 0, 0);
			{ Box keys(buf, "keys");
				buf.insert(buf.end(), 8, 0xDD);
				keys.close();
			}
			meta.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)))
		<< "meta with keys child must not raise boundary-mismatch";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// meta unknown variant: neither firstWord==0 nor secondWord=='hdlr'.
// Must be consumed silently without error.
TEST(Mp4Demux_NewBoxParsers, MetaUnknownVariant_SilentlyConsumed)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box meta(buf, "meta");
			// bytes[0..3] != 0, bytes[4..7] != 'hdlr' → unknown branch: ptr = next
			buf.insert(buf.end(), {0x01,0x02,0x03,0x04, 0x05,0x06,0x07,0x08});
			buf.insert(buf.end(), 16, 0xAB); // trailing bytes
			meta.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)))
		<< "meta unknown variant must be silently consumed";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// --- sgpd tests ---

// sgpd v0: entries carry non-zero payload bytes that were previously left
// unread, triggering a boundary-mismatch in DemuxHelper.  After the fix,
// ParseSampleGroupDescription advances ptr to next for version 0.
TEST(Mp4Demux_NewBoxParsers, SgpdVersion0_TrailingBytesConsumedGracefully)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box sgpd(buf, "sgpd");
			writeFullBoxHeader(buf, 0, 0); // version=0, no defaultLength field
			write4cc(buf, "roll");         // grouping_type
			write32be(buf, 2);             // entry_count = 2
			// 'roll' entries are 2 bytes each; write 4 bytes per entry so that
			// any non-zero trailing bytes are present to prove the fix works.
			write32be(buf, 0xFFFF0001);    // entry 0 payload
			write32be(buf, 0xFFFF0002);    // entry 1 payload
			sgpd.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)))
		<< "sgpd v0 with non-empty entry payload must not trigger boundary-mismatch";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// sgpd v1 with fixed defaultLength: each entry occupies exactly defaultLength bytes.
TEST(Mp4Demux_NewBoxParsers, SgpdVersion1_FixedDefaultLength)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box sgpd(buf, "sgpd");
			writeFullBoxHeader(buf, 1, 0); // version=1
			write4cc(buf, "seig");         // grouping_type
			write32be(buf, 20);            // default_length = 20
			write32be(buf, 2);             // entry_count = 2
			buf.insert(buf.end(), 20, 0xAA); // entry 0
			buf.insert(buf.end(), 20, 0xBB); // entry 1
			sgpd.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// sgpd v1 with per-entry length (defaultLength == 0):
// each entry is preceded by a 4-byte description_length field.
TEST(Mp4Demux_NewBoxParsers, SgpdVersion1_PerEntryLength)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box sgpd(buf, "sgpd");
			writeFullBoxHeader(buf, 1, 0); // version=1
			write4cc(buf, "seig");         // grouping_type
			write32be(buf, 0);             // default_length = 0 → per-entry lengths follow
			write32be(buf, 2);             // entry_count = 2
			write32be(buf, 8);  buf.insert(buf.end(), 8,  0xAA); // entry 0: len=8
			write32be(buf, 12); buf.insert(buf.end(), 12, 0xBB); // entry 1: len=12
			sgpd.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// sgpd v2: adds default_group_description_index before entry_count.
TEST(Mp4Demux_NewBoxParsers, SgpdVersion2_ParsesWithoutError)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box sgpd(buf, "sgpd");
			writeFullBoxHeader(buf, 2, 0); // version=2
			write4cc(buf, "seig");
			write32be(buf, 20); // default_length = 20
			write32be(buf, 0);  // default_group_description_index
			write32be(buf, 1);  // entry_count = 1
			buf.insert(buf.end(), 20, 0xCC); // entry 0
			sgpd.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// sgpd v1 per-entry: description_length exceeds box boundary → DATA_BOUNDARY_MISMATCH.
TEST(Mp4Demux_NewBoxParsers, SgpdEntryExceedsBoundary_RaisesError)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box sgpd(buf, "sgpd");
			writeFullBoxHeader(buf, 1, 0);
			write4cc(buf, "seig");
			write32be(buf, 0);   // default_length = 0 → per-entry
			write32be(buf, 1);   // entry_count = 1
			write32be(buf, 999); // description_length = 999, far beyond box end
			// No actual entry bytes follow
			sgpd.close();
		}
		moov.close();
	}
	Mp4Demux d;
	EXPECT_FALSE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH);
}

// sgpd v1 with only 8 bytes of payload (missing default_length field) → INVALID_BOX.
// This validates the version-dependent minimum-payload check: v1 requires 12 bytes
// after the FullBox header, not just 8.
TEST(Mp4Demux_NewBoxParsers, SgpdVersion1_PayloadTooShortForVersion_RaisesError)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box sgpd(buf, "sgpd");
			writeFullBoxHeader(buf, 1, 0); // version=1 → needs 12 bytes
			write4cc(buf, "seig");         // grouping_type (4 bytes)
			write32be(buf, 0);             // only 8 bytes total — default_length missing
			sgpd.close();
		}
		moov.close();
	}
	Mp4Demux d;
	EXPECT_FALSE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_ERROR_INVALID_BOX);
}

// sgpd v2 with only 12 bytes of payload (missing default_group_description_index) → INVALID_BOX.
// v2 requires 16 bytes after the FullBox header.
TEST(Mp4Demux_NewBoxParsers, SgpdVersion2_PayloadTooShortForVersion_RaisesError)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box sgpd(buf, "sgpd");
			writeFullBoxHeader(buf, 2, 0); // version=2 → needs 16 bytes
			write4cc(buf, "seig");         // grouping_type (4 bytes)
			write32be(buf, 20);            // default_length (4 bytes)
			write32be(buf, 0);             // only 12 bytes total — default_group_description_index missing
			sgpd.close();
		}
		moov.close();
	}
	Mp4Demux d;
	EXPECT_FALSE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_ERROR_INVALID_BOX);
}

// --- sbgp tests ---

// sbgp v0: grouping_type_parameter absent; straightforward entry list.
TEST(Mp4Demux_NewBoxParsers, SbgpVersion0_ParsesWithoutError)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box sbgp(buf, "sbgp");
			writeFullBoxHeader(buf, 0, 0); // version=0
			write4cc(buf, "roll");         // grouping_type
			write32be(buf, 2);             // entry_count = 2
			write32be(buf, 10); write32be(buf, 1); // entry 0: sample_count=10, group_idx=1
			write32be(buf, 20); write32be(buf, 2); // entry 1: sample_count=20, group_idx=2
			sbgp.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// sbgp v1: grouping_type_parameter present (extra uint32 after grouping_type).
TEST(Mp4Demux_NewBoxParsers, SbgpVersion1_WithGroupingTypeParameter)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box sbgp(buf, "sbgp");
			writeFullBoxHeader(buf, 1, 0); // version=1
			write4cc(buf, "seig");
			write32be(buf, 0x00000001); // grouping_type_parameter (v1 only)
			write32be(buf, 1);          // entry_count = 1
			write32be(buf, 5); write32be(buf, 1); // sample_count=5, group_idx=1
			sbgp.close();
		}
		moov.close();
	}
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// sbgp: entry_count implies more bytes than the box contains → DATA_BOUNDARY_MISMATCH.
TEST(Mp4Demux_NewBoxParsers, SbgpEntriesExceedBoundary_RaisesError)
{
	std::vector<uint8_t> buf;
	{ Box moov(buf, "moov");
		{ Box sbgp(buf, "sbgp");
			writeFullBoxHeader(buf, 0, 0);
			write4cc(buf, "roll");
			write32be(buf, 0xFFFF); // entry_count = 65535 → needs 65535*8 bytes
			// No actual entry data
			sbgp.close();
		}
		moov.close();
	}
	Mp4Demux d;
	EXPECT_FALSE(d.Parse(std::make_shared<std::vector<uint8_t>>(buf)));
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH);
}

