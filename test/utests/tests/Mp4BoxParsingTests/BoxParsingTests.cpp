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
	EXPECT_EQ(samples[0].mData.size(), 32) << "Sample 0 should be 32 bytes";
	EXPECT_NEAR(samples[0].mPts, 0.0, 1e-6) << "Sample 0 PTS should be 0";
	EXPECT_NEAR(samples[0].mDts, 0.0, 1e-6) << "Sample 0 DTS should be 0";
	EXPECT_NEAR(samples[0].mDuration, 0.1, 1e-6) << "Sample 0 duration should be 0.1";
	EXPECT_FALSE(samples[0].mDrmMetadata.mIsEncrypted) << "Sample 0 should not be encrypted";
	
	// Validate Sample 1
	EXPECT_EQ(samples[1].mData.size(), 64) << "Sample 1 should be 64 bytes";
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
	bool result = mDemuxer->Parse(encryptedFragmentWithSenc, sizeof(encryptedFragmentWithSenc));
	EXPECT_TRUE(result) << "Parse should succeed for encrypted fragment with SENC";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	auto samples = mDemuxer->GetSamples();
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
	bool result = mDemuxer->Parse(encryptedFragmentWithSaioSaiz, sizeof(encryptedFragmentWithSaioSaiz));
	ASSERT_TRUE(result) << "Parse should succeed for encrypted fragment with SAIO/SAIZ";
	EXPECT_EQ(mDemuxer->GetLastError(), MP4_PARSE_OK);
	
	auto samples = mDemuxer->GetSamples();
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

// ---- helpers (local) ----
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
	ASSERT_TRUE(d.Parse(buf.data(), buf.size())) << "Extended-size box should parse cleanly";  // exercises size==1 path in DemuxHelper
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);
}

// B) size==0 mdat (extends to EOF)
TEST(Mp4Demux_Gaps, SizeZeroMdatToEOF) {
	std::vector<uint8_t> buf;
	{ Box ftyp(buf, "ftyp"); write4cc(buf,"isom"); write32be(buf,0); write4cc(buf,"isom"); write4cc(buf,"iso2"); ftyp.close(); }
	write32be(buf, 0); write4cc(buf, "mdat");   // size == 0
	for (int i=0;i<32;++i) buf.push_back(uint8_t(i)); // payload
	Mp4Demux d;
	ASSERT_TRUE(d.Parse(buf.data(), buf.size()));
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
	
	ASSERT_TRUE(d.Parse(buf.data(), buf.size()));
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
	ASSERT_TRUE(d.Parse(buf.data(), buf.size()));
	auto info = d.GetCodecInfo();
	
	EXPECT_EQ(info.mCodecFormat, GST_FORMAT_AUDIO_ES_AC4);
	ASSERT_EQ(info.mCodecData.size(), 5u);
	EXPECT_EQ(info.mCodecData[0], 0x10);
	EXPECT_EQ(info.mCodecData[4], 0x14);
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
	bool ok = d.Parse(buf.data(), buf.size());
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
	ASSERT_TRUE(d.Parse(buf.data(), buf.size()))
		<< "LLD [moof][mdat][moof][mdat] must parse without error";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK);

	auto samples = d.GetSamples();
	ASSERT_EQ(samples.size(), 3u) << "Expected 3 samples total (2 from moof1 + 1 from moof2)";

	// ---- validate moof1 samples (data from mdat1) ----
	ASSERT_EQ(samples[0].mData.size(), 10u) << "Sample 0: 10 bytes from mdat1";
	EXPECT_EQ(samples[0].mData.GetVector()[0], uint8_t(0xAA))
		<< "Sample 0 first byte should match first byte of mdat1 payload";

	ASSERT_EQ(samples[1].mData.size(), 10u) << "Sample 1: 10 bytes from mdat1";
	EXPECT_EQ(samples[1].mData.GetVector()[0], uint8_t(0xAA + 10))
		<< "Sample 1 first byte should match second chunk of mdat1 payload";

	// ---- validate moof2 sample (data from mdat2, NOT mdat1) ----
	ASSERT_EQ(samples[2].mData.size(), 15u) << "Sample 2: 15 bytes from mdat2";
	EXPECT_EQ(samples[2].mData.GetVector()[0], uint8_t(0xBB))
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
	bool ok = d.Parse(buf.data(), buf.size());
	EXPECT_TRUE(ok) << "Multi-moof+mdat segment (LL-DASH) should parse without errors";
	EXPECT_EQ(d.GetLastError(), MP4_PARSE_OK) << "Should not raise DATA_BOUNDARY_MISMATCH";

	auto samples = d.GetSamples();
	ASSERT_EQ(samples.size(), 2u) << "Should extract one sample per moof+mdat pair";

	// Validate sample 0 is bound to mdat1 payload (0xA0–0xA7)
	EXPECT_EQ(samples[0].mData.size(), 8u) << "Sample 0 should be 8 bytes (mdat1 payload)";
	const auto& s0 = samples[0].mData.GetVector();
	for (int i = 0; i < 8; ++i)
	{
		EXPECT_EQ(s0[i], uint8_t(0xA0 + i))
			<< "Sample 0 byte[" << i << "] should be mdat1 payload (0x"
			<< std::hex << (0xA0 + i) << ")";
	}

	// Validate sample 1 is bound to mdat2 payload (0xB0–0xB7)
	EXPECT_EQ(samples[1].mData.size(), 8u) << "Sample 1 should be 8 bytes (mdat2 payload)";
	const auto& s1 = samples[1].mData.GetVector();
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
    write16be(buf,1); write16be(buf,16); write32be(buf,48);
    // aux info sample 1: 8-byte IV + 1 subsample entry
    buf.insert(buf.end(), {0xA1,0xB2,0xC3,0xD4,0xE5,0xF6,0x07,0x09}); // IV[1]
    write16be(buf,1); write16be(buf,16); write32be(buf,48);
    // sample data
    for (int i = 0; i < 64; ++i) buf.push_back(uint8_t(i & 0xFF));
 
    // Parse WITHOUT an init segment → ivSize stays 0
    Mp4Demux d;
    bool ok = d.Parse(buf.data(), buf.size()); // must NOT crash
 
    // ProcessAuxiliaryInformation: cencAuxInfoSizes[i]=16 > 0==ivSize
    EXPECT_FALSE(ok);
    EXPECT_EQ(d.GetLastError(), MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH);
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
    bool ok = d.Parse(buf.data(), buf.size()); // must NOT crash
 
    // subsample_count=0xFFFF → 393,210 bytes needed, far beyond buffer end;
    // the parser must reject this with DATA_BOUNDARY_MISMATCH, not crash.
    EXPECT_FALSE(ok);
    EXPECT_EQ(d.GetLastError(), MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH);
}
 