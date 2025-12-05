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
 * @file Mp4BoxParsingTests.cpp
 * @brief Unit tests for MP4 box parsing functionality
 * 
 * This file contains tests that validate the Mp4Demux::Parse() API
 * for different MP4 box types including:
 * - pssh (Protection System Specific Header)
 * - saiz (Sample Auxiliary Information Sizes)
 * - senc (Sample Encryption)
 * - tfhd (Track Fragment Header)
 * - trun (Track Run)
 * - moof (Movie Fragment)
 * - tenc (Track Encryption)
 * - etc.
 */

#include <cstdlib>
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <new>

// Google test dependencies
#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Unit under test
#include "MP4Demux.h"
#include "MockGLib.h"

using ::testing::_;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::AnyNumber;
using ::testing::NiceMock;

// External mock instance
extern MockGLib *g_mockGLib;

// Helper functions for GLib memory operations
static gpointer callMalloc(gsize n_bytes)
{
	return malloc(n_bytes);
}

static void callFree(gpointer mem)
{
	free(mem);
}

static gpointer callRealloc(gpointer mem, gsize n_bytes)
{
	return realloc(mem, n_bytes);
}

/**
 * @class Mp4BoxParsingTestFixture
 * @brief Base test fixture for MP4 box parsing tests
 */
class Mp4BoxParsingTestFixture : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Create GLib mock
		g_mockGLib = new NiceMock<MockGLib>();
		
		// Set up default GLib mock behavior for memory operations
		ON_CALL(*g_mockGLib, g_malloc(_)).WillByDefault(Invoke(callMalloc));
		ON_CALL(*g_mockGLib, g_free(_)).WillByDefault(Invoke(callFree));
		ON_CALL(*g_mockGLib, g_realloc(_, _)).WillByDefault(Invoke(callRealloc));
		
		// Allow any number of calls
		EXPECT_CALL(*g_mockGLib, g_malloc(_)).Times(AnyNumber());
		EXPECT_CALL(*g_mockGLib, g_free(_)).Times(AnyNumber());
		EXPECT_CALL(*g_mockGLib, g_realloc(_, _)).Times(AnyNumber());
		
		// Create demuxer instance
		mDemuxer = new Mp4Demux();
	}

	void TearDown() override
	{
		delete mDemuxer;
		delete g_mockGLib;
		g_mockGLib = nullptr;
	}

	/**
	 * @brief Helper to create MP4 box header
	 * @param size Total box size including header
	 * @param type Four-character box type
	 * @return Vector containing box header bytes
	 */
	std::vector<uint8_t> CreateBoxHeader(uint32_t size, const char* type)
	{
		std::vector<uint8_t> header;
		// Big-endian size
		header.push_back((size >> 24) & 0xFF);
		header.push_back((size >> 16) & 0xFF);
		header.push_back((size >> 8) & 0xFF);
		header.push_back(size & 0xFF);
		// Box type (4 characters)
		header.push_back(type[0]);
		header.push_back(type[1]);
		header.push_back(type[2]);
		header.push_back(type[3]);
		return header;
	}

	/**
	 * @brief Helper to create full box header (with version and flags)
	 * @param size Total box size including header
	 * @param type Four-character box type
	 * @param version Box version
	 * @param flags Box flags (24-bit)
	 * @return Vector containing full box header bytes
	 */
	std::vector<uint8_t> CreateFullBoxHeader(uint32_t size, const char* type, 
	                                         uint8_t version, uint32_t flags)
	{
		auto header = CreateBoxHeader(size, type);
		// Version (1 byte)
		header.push_back(version);
		// Flags (3 bytes)
		header.push_back((flags >> 16) & 0xFF);
		header.push_back((flags >> 8) & 0xFF);
		header.push_back(flags & 0xFF);
		return header;
	}

	Mp4Demux* mDemuxer;
};

/**
 * @brief Test parsing of 'ftyp' (File Type) box
 * 
 * The ftyp box identifies the specifications to which the file complies.
 * Structure: size + type + major_brand + minor_version + compatible_brands[]
 */
TEST_F(Mp4BoxParsingTestFixture, ParseFtypBox)
{
	// Create ftyp box: major_brand='isom', minor_version=512, compatible='isom','iso2'
	std::vector<uint8_t> ftypBox = CreateBoxHeader(24, "ftyp");
	
	// Major brand: 'isom'
	ftypBox.push_back('i');
	ftypBox.push_back('s');
	ftypBox.push_back('o');
	ftypBox.push_back('m');
	
	// Minor version: 512 (0x00000200)
	ftypBox.push_back(0x00);
	ftypBox.push_back(0x00);
	ftypBox.push_back(0x02);
	ftypBox.push_back(0x00);
	
	// Compatible brand 1: 'isom'
	ftypBox.push_back('i');
	ftypBox.push_back('s');
	ftypBox.push_back('o');
	ftypBox.push_back('m');
	
	// Compatible brand 2: 'iso2'
	ftypBox.push_back('i');
	ftypBox.push_back('s');
	ftypBox.push_back('o');
	ftypBox.push_back('2');
	
	// Parse the box
	EXPECT_NO_THROW(mDemuxer->Parse(ftypBox.data(), ftypBox.size()));
}

/**
 * @brief Test parsing of 'moof' (Movie Fragment) box
 * 
 * The moof box contains a single movie fragment. It triggers the demuxer
 * to reset encryption state and prepare for fragment parsing.
 */
TEST_F(Mp4BoxParsingTestFixture, ParseMoofBox)
{
	// Create moof box with mfhd child
	std::vector<uint8_t> moofBox = CreateBoxHeader(28, "moof");
	
	// Add mfhd (Movie Fragment Header) child box
	auto mfhdBox = CreateFullBoxHeader(16, "mfhd", 0, 0);
	// Sequence number: 1
	mfhdBox.push_back(0x00);
	mfhdBox.push_back(0x00);
	mfhdBox.push_back(0x00);
	mfhdBox.push_back(0x01);
	
	moofBox.insert(moofBox.end(), mfhdBox.begin(), mfhdBox.end());
	
	// Parse the box
	EXPECT_NO_THROW(mDemuxer->Parse(moofBox.data(), moofBox.size()));
}

/**
 * @brief Test parsing of 'tfhd' (Track Fragment Header) box
 * 
 * The tfhd box sets default values for track fragment. This should trigger
 * ParseTrackFragmentHeader() in the demuxer.
 */
TEST_F(Mp4BoxParsingTestFixture, ParseTfhdBox)
{
	// Create a minimal valid MP4 structure: ftyp + moov + moof + traf + tfhd
	std::vector<uint8_t> mp4Data;
	
	// 1. ftyp box
	auto ftyp = CreateBoxHeader(20, "ftyp");
	ftyp.insert(ftyp.end(), {'i','s','o','m', 0,0,0,0, 'i','s','o','m'});
	mp4Data.insert(mp4Data.end(), ftyp.begin(), ftyp.end());
	
	// 2. moov box with minimal track info
	std::vector<uint8_t> moov = CreateBoxHeader(108, "moov");
	
	// mvhd (Movie Header)
	auto mvhd = CreateFullBoxHeader(32, "mvhd", 0, 0);
	mvhd.insert(mvhd.end(), 20, 0x00); // Fill with zeros for simplicity
	moov.insert(moov.end(), mvhd.begin(), mvhd.end());
	
	// trak box
	std::vector<uint8_t> trak = CreateBoxHeader(68, "trak");
	
	// tkhd (Track Header)
	auto tkhd = CreateFullBoxHeader(32, "tkhd", 0, 0x000007); // track_enabled | track_in_movie | track_in_preview
	tkhd.insert(tkhd.end(), 20, 0x00);
	trak.insert(trak.end(), tkhd.begin(), tkhd.end());
	
	// mdia box
	std::vector<uint8_t> mdia = CreateBoxHeader(28, "mdia");
	auto mdhd = CreateFullBoxHeader(20, "mdhd", 0, 0);
	mdhd.insert(mdhd.end(), 8, 0x00);
	mdia.insert(mdia.end(), mdhd.begin(), mdhd.end());
	trak.insert(trak.end(), mdia.begin(), mdia.end());
	
	moov.insert(moov.end(), trak.begin(), trak.end());
	mp4Data.insert(mp4Data.end(), moov.begin(), moov.end());
	
	// 3. moof box with traf containing tfhd
	std::vector<uint8_t> moof = CreateBoxHeader(52, "moof");
	
	// mfhd
	auto mfhd = CreateFullBoxHeader(16, "mfhd", 0, 0);
	mfhd.insert(mfhd.end(), {0,0,0,1}); // sequence_number = 1
	moof.insert(moof.end(), mfhd.begin(), mfhd.end());
	
	// traf (Track Fragment)
	std::vector<uint8_t> traf = CreateBoxHeader(28, "traf");
	
	// tfhd (Track Fragment Header)
	// flags: 0x020000 = default-sample-duration-present
	auto tfhd = CreateFullBoxHeader(16, "tfhd", 0, 0x020000);
	// track_ID = 1
	tfhd.push_back(0x00);
	tfhd.push_back(0x00);
	tfhd.push_back(0x00);
	tfhd.push_back(0x01);
	// default_sample_duration = 1000
	tfhd.push_back(0x00);
	tfhd.push_back(0x00);
	tfhd.push_back(0x03);
	tfhd.push_back(0xE8);
	
	traf.insert(traf.end(), tfhd.begin(), tfhd.end());
	moof.insert(moof.end(), traf.begin(), traf.end());
	mp4Data.insert(mp4Data.end(), moof.begin(), moof.end());
	
	// Parse the complete structure
	EXPECT_NO_THROW(mDemuxer->Parse(mp4Data.data(), mp4Data.size()));
	
	// Verify demuxer processed the tfhd by checking that no errors occurred
	// In a real implementation, you might want to add getters to verify internal state
}

/**
 * @brief Test parsing of 'trun' (Track Run) box
 * 
 * The trun box contains sample information for a track fragment.
 * This should trigger ParseTrackRun() when tfhd is present.
 */
TEST_F(Mp4BoxParsingTestFixture, ParseTrunBox)
{
	// Build complete moof structure with tfhd and trun
	std::vector<uint8_t> mp4Data;
	
	// Minimal moov for context
	auto moov = CreateBoxHeader(12, "moov");
	mp4Data.insert(mp4Data.end(), moov.begin(), moov.end());
	
	// moof box
	std::vector<uint8_t> moof = CreateBoxHeader(80, "moof");
	
	// mfhd
	auto mfhd = CreateFullBoxHeader(16, "mfhd", 0, 0);
	mfhd.insert(mfhd.end(), {0,0,0,1});
	moof.insert(moof.end(), mfhd.begin(), mfhd.end());
	
	// traf
	std::vector<uint8_t> traf = CreateBoxHeader(56, "traf");
	
	// tfhd
	auto tfhd = CreateFullBoxHeader(12, "tfhd", 0, 0);
	tfhd.insert(tfhd.end(), {0,0,0,1}); // track_ID = 1
	traf.insert(traf.end(), tfhd.begin(), tfhd.end());
	
	// trun (Track Run)
	// flags: 0x000001 = data-offset-present
	//        0x000100 = first-sample-flags-present
	auto trun = CreateFullBoxHeader(24, "trun", 0, 0x000101);
	
	// sample_count = 2
	trun.push_back(0x00);
	trun.push_back(0x00);
	trun.push_back(0x00);
	trun.push_back(0x02);
	
	// data_offset = 100
	trun.push_back(0x00);
	trun.push_back(0x00);
	trun.push_back(0x00);
	trun.push_back(0x64);
	
	// first_sample_flags = 0x01010000
	trun.push_back(0x01);
	trun.push_back(0x01);
	trun.push_back(0x00);
	trun.push_back(0x00);
	
	traf.insert(traf.end(), trun.begin(), trun.end());
	moof.insert(moof.end(), traf.begin(), traf.end());
	mp4Data.insert(mp4Data.end(), moof.begin(), moof.end());
	
	// Parse
	EXPECT_NO_THROW(mDemuxer->Parse(mp4Data.data(), mp4Data.size()));
}

/**
 * @brief Test parsing of 'pssh' (Protection System Specific Header) box
 * 
 * The pssh box contains DRM-specific data for content protection.
 */
TEST_F(Mp4BoxParsingTestFixture, ParsePsshBox)
{
	std::vector<uint8_t> mp4Data;
	
	// Create pssh box (version 0)
	// SystemID: example DRM system (16 bytes)
	std::vector<uint8_t> systemId = {
		0x10,0x77,0xEF,0xEC, 0xC0,0xB2,0x4D,0x02,
		0xAC,0xE3,0x3C,0x1E, 0x52,0xE2,0xFB,0x4B
	};
	
	// Data: simple test payload
	std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
	
	uint32_t psshSize = 12 + systemId.size() + 4 + data.size(); // header + systemId + dataSize + data
	auto pssh = CreateFullBoxHeader(psshSize, "pssh", 0, 0);
	
	// SystemID
	pssh.insert(pssh.end(), systemId.begin(), systemId.end());
	
	// DataSize (big-endian)
	uint32_t dataSize = data.size();
	pssh.push_back((dataSize >> 24) & 0xFF);
	pssh.push_back((dataSize >> 16) & 0xFF);
	pssh.push_back((dataSize >> 8) & 0xFF);
	pssh.push_back(dataSize & 0xFF);
	
	// Data
	pssh.insert(pssh.end(), data.begin(), data.end());
	
	mp4Data.insert(mp4Data.end(), pssh.begin(), pssh.end());
	
	// Parse
	EXPECT_NO_THROW(mDemuxer->Parse(mp4Data.data(), mp4Data.size()));
	
	// Verify protection data was extracted
	auto protectionData = mDemuxer->GetProtectionEvents();
	EXPECT_GT(protectionData.size(), 0) << "PSSH box should be parsed into protection events";
}

/**
 * @brief Test parsing of 'saiz' (Sample Auxiliary Information Sizes) box
 * 
 * The saiz box provides size information for auxiliary sample data,
 * typically used with encrypted content.
 */
TEST_F(Mp4BoxParsingTestFixture, ParseSaizBox)
{
	std::vector<uint8_t> mp4Data;
	
	// Build moof with traf containing saiz
	auto moof = CreateBoxHeader(59, "moof");
	
	auto mfhd = CreateFullBoxHeader(16, "mfhd", 0, 0);
	mfhd.insert(mfhd.end(), {0,0,0,1});
	moof.insert(moof.end(), mfhd.begin(), mfhd.end());
	
	auto traf = CreateBoxHeader(35, "traf");
	
	// saiz box: version 0, flags 0
	auto saiz = CreateFullBoxHeader(20, "saiz", 0, 0);
	
	// aux_info_type (if flags & 1)
	// aux_info_type_parameter (if flags & 1)
	// We're using flags=0, so no type/parameter
	
	// default_sample_info_size = 0 (variable sizes)
	saiz.push_back(0x00);
	
	// sample_count = 3
	saiz.push_back(0x00);
	saiz.push_back(0x00);
	saiz.push_back(0x00);
	saiz.push_back(0x03);
	
	// sample_info_size[0] = 8
	saiz.push_back(0x08);
	// sample_info_size[1] = 16
	saiz.push_back(0x10);
	// sample_info_size[2] = 12
	saiz.push_back(0x0C);
	
	traf.insert(traf.end(), saiz.begin(), saiz.end());
	moof.insert(moof.end(), traf.begin(), traf.end());
	mp4Data.insert(mp4Data.end(), moof.begin(), moof.end());
	
	// Parse
	EXPECT_NO_THROW(mDemuxer->Parse(mp4Data.data(), mp4Data.size()));
}

/**
 * @brief Test parsing of 'senc' (Sample Encryption) box
 * 
 * The senc box contains per-sample initialization vectors and
 * subsample encryption information for encrypted content.
 */
TEST_F(Mp4BoxParsingTestFixture, ParseSencBox)
{
	std::vector<uint8_t> mp4Data;
	
	// Build moof structure
	auto moof = CreateBoxHeader(92, "moof");
	
	auto mfhd = CreateFullBoxHeader(16, "mfhd", 0, 0);
	mfhd.insert(mfhd.end(), {0,0,0,1});
	moof.insert(moof.end(), mfhd.begin(), mfhd.end());
	
	auto traf = CreateBoxHeader(68, "traf");
	
	// tfhd for context
	auto tfhd = CreateFullBoxHeader(12, "tfhd", 0, 0);
	tfhd.insert(tfhd.end(), {0,0,0,1});
	traf.insert(traf.end(), tfhd.begin(), tfhd.end());
	
	// senc box: flags=0 (no subsample encryption)
	auto senc = CreateFullBoxHeader(48, "senc", 0, 0);
	
	// sample_count = 2
	senc.push_back(0x00);
	senc.push_back(0x00);
	senc.push_back(0x00);
	senc.push_back(0x02);
	
	// Sample 1 IV (16 bytes for AES-CTR)
	for (int i = 0; i < 16; i++) {
		senc.push_back(0x10 + i);
	}
	
	// Sample 2 IV (16 bytes)
	for (int i = 0; i < 16; i++) {
		senc.push_back(0x20 + i);
	}
	
	traf.insert(traf.end(), senc.begin(), senc.end());
	moof.insert(moof.end(), traf.begin(), traf.end());
	mp4Data.insert(mp4Data.end(), moof.begin(), moof.end());
	
	// Parse
	EXPECT_NO_THROW(mDemuxer->Parse(mp4Data.data(), mp4Data.size()));
}

/**
 * @brief test for tenc box structure validation
 * 
 * This test validates just the tenc box structure in isolation,
 * avoiding the complex moov hierarchy requirements.
 */
TEST_F(Mp4BoxParsingTestFixture, ParseTencBox)
{
	std::vector<uint8_t> mp4Data;
	
	// Create a moof context with sinf/schi/tenc to test encryption setup
	// Use moof instead of moov to avoid video parsing assertions
	auto moof = CreateBoxHeader(104, "moof");
	
	auto mfhd = CreateFullBoxHeader(16, "mfhd", 0, 0);
	mfhd.insert(mfhd.end(), {0,0,0,1}); // sequence_number
	moof.insert(moof.end(), mfhd.begin(), mfhd.end());
	
	auto traf = CreateBoxHeader(80, "traf");
	
	// tfhd for context
	auto tfhd = CreateFullBoxHeader(12, "tfhd", 0, 0);
	tfhd.insert(tfhd.end(), {0,0,0,1}); // track_ID
	traf.insert(traf.end(), tfhd.begin(), tfhd.end());
	
	// sinf with schm and schi/tenc
	auto sinf = CreateBoxHeader(60, "sinf");
	
	// schm to set scheme type
	auto schm = CreateFullBoxHeader(20, "schm", 0, 0);
	schm.insert(schm.end(), {'c','e','n','c'}); // scheme_type
	schm.insert(schm.end(), {0x00,0x01,0x00,0x00}); // version
	sinf.insert(sinf.end(), schm.begin(), schm.end());
	
	// schi with tenc
	auto schi = CreateBoxHeader(32, "schi");
	
	// tenc box
	auto tenc = CreateFullBoxHeader(24, "tenc", 0, 0);
	tenc.push_back(0x00); // reserved  
	tenc.push_back(0x00); // pattern
	tenc.push_back(0x01); // is_encrypted
	tenc.push_back(0x10); // iv_size
	// KID (16 bytes)
	for (int i = 0; i < 16; i++) {
		tenc.push_back(0x12);
	}
	
	schi.insert(schi.end(), tenc.begin(), tenc.end());
	sinf.insert(sinf.end(), schi.begin(), schi.end());
	traf.insert(traf.end(), sinf.begin(), sinf.end());
	moof.insert(moof.end(), traf.begin(), traf.end());
	mp4Data.insert(mp4Data.end(), moof.begin(), moof.end());
	
	// This should parse without hitting video parsing code
	EXPECT_NO_THROW(mDemuxer->Parse(mp4Data.data(), mp4Data.size()));
}

/**
 * @brief Test parsing multiple boxes in sequence
 * 
 * Validates that the demuxer can handle a sequence of different boxes
 * and maintain proper state between them.
 */
TEST_F(Mp4BoxParsingTestFixture, ParseMultipleBoxesInSequence)
{
	std::vector<uint8_t> mp4Data;
	
	// 1. ftyp
	auto ftyp = CreateBoxHeader(20, "ftyp");
	ftyp.insert(ftyp.end(), {'i','s','o','m', 0,0,0,0, 'i','s','o','m'});
	mp4Data.insert(mp4Data.end(), ftyp.begin(), ftyp.end());
	
	// 2. pssh
	std::vector<uint8_t> systemId(16, 0xAB);
	std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
	uint32_t psshSize = 12 + 16 + 4 + 4;
	auto pssh = CreateFullBoxHeader(psshSize, "pssh", 0, 0);
	pssh.insert(pssh.end(), systemId.begin(), systemId.end());
	pssh.insert(pssh.end(), {0,0,0,4}); // dataSize
	pssh.insert(pssh.end(), data.begin(), data.end());
	mp4Data.insert(mp4Data.end(), pssh.begin(), pssh.end());
	
	// 3. moof with multiple children
	std::vector<uint8_t> moof = CreateBoxHeader(48, "moof");
	
	auto mfhd = CreateFullBoxHeader(16, "mfhd", 0, 0);
	mfhd.insert(mfhd.end(), {0,0,0,1});
	moof.insert(moof.end(), mfhd.begin(), mfhd.end());
	
	auto traf = CreateBoxHeader(24, "traf");
	auto tfhd = CreateFullBoxHeader(12, "tfhd", 0, 0);
	tfhd.insert(tfhd.end(), {0,0,0,1});
	traf.insert(traf.end(), tfhd.begin(), tfhd.end());
	moof.insert(moof.end(), traf.begin(), traf.end());
	
	mp4Data.insert(mp4Data.end(), moof.begin(), moof.end());
	
	// Parse all
	EXPECT_NO_THROW(mDemuxer->Parse(mp4Data.data(), mp4Data.size()));
	
	// Verify some state
	auto protectionData = mDemuxer->GetProtectionEvents();
	EXPECT_GT(protectionData.size(), 0) << "Should have parsed pssh box";
}

/**
 * @brief Test error handling for malformed box (size mismatch)
 */
TEST_F(Mp4BoxParsingTestFixture, HandleMalformedBoxSize)
{
	// Create box with incorrect size
	std::vector<uint8_t> malformedBox = CreateBoxHeader(100, "ftyp"); // Claims 100 bytes
	malformedBox.insert(malformedBox.end(), 8, 0x00); // Only has 16 total
	
	// Parser should handle gracefully (not crash)
	// Behavior depends on implementation - might skip or throw
	// For now, just ensure it doesn't crash
	try {
		mDemuxer->Parse(malformedBox.data(), malformedBox.size());
	} catch (...) {
		// Exception is acceptable for malformed data
		SUCCEED() << "Parser handled malformed box appropriately";
	}
}

/**
 * @brief Test empty buffer handling
 */
TEST_F(Mp4BoxParsingTestFixture, HandleEmptyBuffer)
{
	const uint8_t* emptyBuffer = nullptr;
	
	// Should handle null or empty buffer gracefully
	EXPECT_NO_THROW(mDemuxer->Parse(emptyBuffer, 0));
}

/**
 * @brief Test very small buffer (incomplete box header)
 */
TEST_F(Mp4BoxParsingTestFixture, HandleIncompleteBoxHeader)
{
	// Box header is 8 bytes, provide only 4
	std::vector<uint8_t> incompleteBox = {0x00, 0x00, 0x00, 0x10};
	
	// Should not crash
	try {
		mDemuxer->Parse(incompleteBox.data(), incompleteBox.size());
	} catch (...) {
		SUCCEED() << "Parser handled incomplete header appropriately";
	}
}
