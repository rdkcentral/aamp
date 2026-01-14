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
 * @file EsdsReadLenTests.cpp
 * @brief Tests for ESDS variable-length parsing via Mp4Demux::Parse()
 */
#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <string>
#include "MP4Demux.h"

// Utility: append a big-endian 32-bit value
static void append_u32(std::vector<uint8_t>& buf, uint32_t v)
{
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

// Utility: append a FourCC from ASCII string of length 4
static void append_type(std::vector<uint8_t>& buf, const char type[4])
{
    buf.insert(buf.end(), type, type + 4);
}

// Utility: encode ISO BMFF descriptor length using base-128 big-endian varint
static std::vector<uint8_t> encode_len(uint32_t value)
{
    std::vector<uint8_t> out;
    // Collect 7-bit groups in reverse
    uint32_t v = value;
    std::vector<uint8_t> groups;
    do {
        groups.push_back(static_cast<uint8_t>(v & 0x7F));
        v >>= 7;
    } while (v != 0);
    // Emit big-endian with continuation bits
    for (size_t i = groups.size(); i-- > 0; )
    {
        uint8_t b = groups[i];
        if (i != 0) b |= 0x80; // set continuation for all but last
        out.push_back(b);
    }
    return out;
}

// Build an ESDS box payload with given DecoderSpecificInfo length field bytes and payload size
static std::vector<uint8_t> build_esds_payload(const std::vector<uint8_t>& decSpecLenBytes,
                                               size_t decSpecPayloadSize)
{
    std::vector<uint8_t> esdsPayload;
    // FullBox header: version(1) + flags(3)
    esdsPayload.insert(esdsPayload.end(), {0x00, 0x00, 0x00, 0x00});

    // 0x03 ES_Descriptor: minimal content length 3 (ES_ID(2) + flags(1))
    esdsPayload.push_back(0x03);                           // tag
    auto len03 = encode_len(3);
    esdsPayload.insert(esdsPayload.end(), len03.begin(), len03.end());
    esdsPayload.push_back(0x00);                           // ES_ID MSB
    esdsPayload.push_back(0x02);                           // ES_ID LSB
    esdsPayload.push_back(0x00);                           // flags

    // 0x04 DecoderConfigDescriptor: length 13, contents don't matter for parser
    esdsPayload.push_back(0x04);                           // tag
    auto len04 = encode_len(13);
    esdsPayload.insert(esdsPayload.end(), len04.begin(), len04.end());
    esdsPayload.push_back(0x40);                           // objectTypeIndication (AAC LC)
    esdsPayload.push_back(0x15);                           // streamType(6) + upStream(1) + reserved(1)
    esdsPayload.push_back(0x00);                           // bufferSizeDB[24]
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);                           // maxBitrate[32]
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);                           // avgBitrate[32]
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);

    // 0x05 DecoderSpecificInfo: variable length provided by decSpecLenBytes
    esdsPayload.push_back(0x05);
    esdsPayload.insert(esdsPayload.end(), decSpecLenBytes.begin(), decSpecLenBytes.end());
    // payload bytes (e.g., AudioSpecificConfig)
    for (size_t i = 0; i < decSpecPayloadSize; ++i)
    {
        esdsPayload.push_back(static_cast<uint8_t>(i & 0xFF));
    }

    // 0x06 SLConfigDescriptor: length 1, one dummy byte
    esdsPayload.push_back(0x06);
    auto len06 = encode_len(1);
    esdsPayload.insert(esdsPayload.end(), len06.begin(), len06.end());
    esdsPayload.push_back(0x02);

    return esdsPayload;
}

// Optional tail box ('free') with arbitrary payload to keep overall buffer large enough
static std::vector<uint8_t> build_free_box(uint32_t payloadSize)
{
    std::vector<uint8_t> box;
    uint32_t size = 8 + payloadSize;
    append_u32(box, size);
    append_type(box, "free");
    box.resize(size, 0x00);
    return box;
}

// Build an mp4a sample entry with provided ESDS child payload and optional tail 'free' box
static std::vector<uint8_t> build_mp4a_box(const std::vector<uint8_t>& esdsPayload,
                                           uint32_t tailFreePayloadSize = 0)
{
    std::vector<uint8_t> mp4aPayload;
    // reserved[6]
    for (int i = 0; i < 6; ++i) mp4aPayload.push_back(0x00);
    // data_reference_index (u16)
    mp4aPayload.push_back(0x00);
    mp4aPayload.push_back(0x00);
    // reserved[2] (8 bytes)
    for (int i = 0; i < 8; ++i) mp4aPayload.push_back(0x00);
    // channelcount (u16)
    mp4aPayload.push_back(0x00);
    mp4aPayload.push_back(0x02);
    // sample_size (u16)
    mp4aPayload.push_back(0x00);
    mp4aPayload.push_back(0x10);
    // pre_defined/reserved (u32)
    mp4aPayload.push_back(0x00);
    mp4aPayload.push_back(0x00);
    mp4aPayload.push_back(0x00);
    mp4aPayload.push_back(0x00);
    // sample_rate (32-bit 16.16 fixed): 48000.0 -> 0xBB80 0000
    mp4aPayload.push_back(0xBB);
    mp4aPayload.push_back(0x80);
    mp4aPayload.push_back(0x00);
    mp4aPayload.push_back(0x00);

    // Child box: esds
    std::vector<uint8_t> esdsBox;
    // size and type
    uint32_t esdsSize = 8 + static_cast<uint32_t>(esdsPayload.size());
    append_u32(esdsBox, esdsSize);
    append_type(esdsBox, "esds");
    esdsBox.insert(esdsBox.end(), esdsPayload.begin(), esdsPayload.end());

    // Optional tail box to keep buffer large enough for intentional over-read
    std::vector<uint8_t> tailBox;
    if (tailFreePayloadSize > 0)
    {
        tailBox = build_free_box(tailFreePayloadSize);
    }

    // Wrap mp4a box
    std::vector<uint8_t> mp4aBox;
    uint32_t mp4aSize = 8 + static_cast<uint32_t>(mp4aPayload.size() + esdsBox.size() + tailBox.size());
    append_u32(mp4aBox, mp4aSize);
    append_type(mp4aBox, "mp4a");
    mp4aBox.insert(mp4aBox.end(), mp4aPayload.begin(), mp4aPayload.end());
    mp4aBox.insert(mp4aBox.end(), esdsBox.begin(), esdsBox.end());
    mp4aBox.insert(mp4aBox.end(), tailBox.begin(), tailBox.end());
    return mp4aBox;
}

// Build an stsd box containing one mp4a sample entry
static std::vector<uint8_t> build_stsd_with_mp4a(const std::vector<uint8_t>& mp4aBox)
{
    std::vector<uint8_t> stsdPayload;
    // FullBox header
    stsdPayload.insert(stsdPayload.end(), {0x00, 0x00, 0x00, 0x00});
    // entry count (u32 = 1)
    append_u32(stsdPayload, 1);
    // sample entry box
    stsdPayload.insert(stsdPayload.end(), mp4aBox.begin(), mp4aBox.end());

    // Wrap stsd box
    std::vector<uint8_t> stsdBox;
    uint32_t stsdSize = 8 + static_cast<uint32_t>(stsdPayload.size());
    append_u32(stsdBox, stsdSize);
    append_type(stsdBox, "stsd");
    stsdBox.insert(stsdBox.end(), stsdPayload.begin(), stsdPayload.end());
    return stsdBox;
}

// Build a minimal buffer that Mp4Demux can parse: just an stsd->mp4a->esds chain
static std::vector<uint8_t> build_buffer_with_esds(const std::vector<uint8_t>& decSpecLenBytes,
                                                   size_t decSpecPayloadSize,
                                                   uint32_t tailFreePayloadSize = 0)
{
    auto esdsPayload = build_esds_payload(decSpecLenBytes, decSpecPayloadSize);
    auto mp4aBox = build_mp4a_box(esdsPayload, tailFreePayloadSize);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    // We can place stsd at top-level; DemuxHelper doesn't enforce strict nesting
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), stsdBox.begin(), stsdBox.end());
    return buf;
}

class EsdsReadLenTests : public ::testing::Test {};

TEST_F(EsdsReadLenTests, SingleOctetLength_0x7F)
{
    // Length encoded in one octet: 0x7F = 127
    std::vector<uint8_t> lenBytes = encode_len(0x7F);
    ASSERT_EQ(lenBytes.size(), 1u);
    auto buffer = build_buffer_with_esds(lenBytes, 127);

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());
    EXPECT_TRUE(ok) << "Parse should succeed for 1-octet length";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_OK);

    auto info = demux.GetCodecInfo();
    EXPECT_EQ(info.mCodecFormat, GST_FORMAT_AUDIO_ES_AAC_RAW);
    EXPECT_EQ(info.mCodecData.size(), 127u) << "AudioSpecificConfig length mismatch";
}

TEST_F(EsdsReadLenTests, TwoOctetLength_0x81_0x02_Value130)
{
    // Manually craft two-octet length for 130: 0x81 0x02
    std::vector<uint8_t> lenBytes = {0x81, 0x02};
    auto buffer = build_buffer_with_esds(lenBytes, 130);

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());
    EXPECT_TRUE(ok) << "Parse should succeed for 2-octet length";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_OK);

    auto info = demux.GetCodecInfo();
    EXPECT_EQ(info.mCodecFormat, GST_FORMAT_AUDIO_ES_AAC_RAW);
    EXPECT_EQ(info.mCodecData.size(), 130u);
}

TEST_F(EsdsReadLenTests, Overflow_FiveOctetsTriggersVariableLengthOverflow)
{
    // Provide a 5-octet varint which will exceed 32 bits in Mp4Demux::ReadLen()
    // e.g., 0xFF 0xFF 0xFF 0xFF 0x7F (continuations then terminal)
    std::vector<uint8_t> lenBytes = {0xFF, 0xFF, 0xFF, 0xFF, 0x7F};
    auto buffer = build_buffer_with_esds(lenBytes, 0 /*payload not required*/);

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());
    EXPECT_FALSE(ok) << "Parse should fail due to VARIABLE_LENGTH_OVERFLOW";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_ERROR_VARIABLE_LENGTH_OVERFLOW);
}

TEST_F(EsdsReadLenTests, BoundaryMismatch_DecSpecificLenExceedsEsdsBox)
{
    // Declare DecoderSpecificInfo length as 256, but provide only 8 bytes of payload.
    // Add a tail 'free' box to ensure ptr+len stays within the overall buffer to avoid segfault.
    auto lenBytes = encode_len(256);
    auto buffer = build_buffer_with_esds(lenBytes, /*actual*/ 8, /*tailFreePayloadSize*/ 1024);

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());

    // Expected: DemuxHelper detects that after parsing 'esds', ptr != next and sets
    // MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH.
    EXPECT_FALSE(ok) << "Parse should fail due to DATA_BOUNDARY_MISMATCH (esds over-read)";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH);
}


// Build an intentionally invalid 'free' box with size < 8 (short box)
static std::vector<uint8_t> build_free_box_invalid_short(uint32_t declaredSize)
{
    std::vector<uint8_t> box;
    // declaredSize is intentionally < 8 to trigger INVALID_BOX
    append_u32(box, declaredSize);
    append_type(box, "free");
    // No payload; size field itself is invalid.
    return box;
}

TEST_F(EsdsReadLenTests, InvalidShortFreeBoxTriggersInvalidBoxError)
{
    // Build a valid stsd->mp4a->esds chain with small, correct DecoderSpecific length
    auto lenBytes = encode_len(8);
    auto esdsPayload = build_esds_payload(lenBytes, /*payload*/ 8);
    auto mp4aBox = build_mp4a_box(esdsPayload);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    // Append an invalid 'free' box whose declared size is 6 (< 8 minimum)
    auto badFree = build_free_box_invalid_short(6);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), stsdBox.begin(), stsdBox.end());
    buffer.insert(buffer.end(), badFree.begin(), badFree.end());

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());
    EXPECT_FALSE(ok) << "Parse should fail due to INVALID_BOX for short 'free'";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_ERROR_INVALID_BOX);
}


// Build a 'free' box using 32-bit size==1 followed by 64-bit large size
static std::vector<uint8_t> build_free_box_large_size(uint64_t largeSize)
{
    std::vector<uint8_t> box;
    // 32-bit size==1 indicates presence of 64-bit largesize
    append_u32(box, 1);
    append_type(box, "free");
    // append 64-bit largesize big-endian
    box.push_back(static_cast<uint8_t>((largeSize >> 56) & 0xFF));
    box.push_back(static_cast<uint8_t>((largeSize >> 48) & 0xFF));
    box.push_back(static_cast<uint8_t>((largeSize >> 40) & 0xFF));
    box.push_back(static_cast<uint8_t>((largeSize >> 32) & 0xFF));
    box.push_back(static_cast<uint8_t>((largeSize >> 24) & 0xFF));
    box.push_back(static_cast<uint8_t>((largeSize >> 16) & 0xFF));
    box.push_back(static_cast<uint8_t>((largeSize >> 8) & 0xFF));
    box.push_back(static_cast<uint8_t>(largeSize & 0xFF));
    // The remainder of the box payload is (largeSize - 16) bytes.
    if (largeSize >= 16)
    {
        box.resize(static_cast<size_t>(largeSize), 0x00);
    }
    return box;
}

TEST_F(EsdsReadLenTests, LargeSizeFreeBoxInvalidWhenLargesizeBelow16)
{
    // Valid stsd->mp4a->esds first
    auto lenBytes = encode_len(8);
    auto esdsPayload = build_esds_payload(lenBytes, /*payload*/ 8);
    auto mp4aBox = build_mp4a_box(esdsPayload);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    // Build a free box with size==1 and 64-bit largesize=8 (which is <16 and invalid)
    auto badFree = build_free_box_large_size(8);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), stsdBox.begin(), stsdBox.end());
    buffer.insert(buffer.end(), badFree.begin(), badFree.end());

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());
    EXPECT_FALSE(ok) << "Parse should fail: large-size free box with largesize < 16";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_ERROR_INVALID_BOX);
}

TEST_F(EsdsReadLenTests, LargeSizeFreeBoxValidWhenLargesizeIs16)
{
    // Valid stsd->mp4a->esds first
    auto lenBytes = encode_len(8);
    auto esdsPayload = build_esds_payload(lenBytes, /*payload*/ 8);
    auto mp4aBox = build_mp4a_box(esdsPayload);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    // Build a free box with size==1 and 64-bit largesize=16 (header only, no payload)
    auto goodFree = build_free_box_large_size(16);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), stsdBox.begin(), stsdBox.end());
    buffer.insert(buffer.end(), goodFree.begin(), goodFree.end());

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());
    EXPECT_TRUE(ok) << "Parse should succeed: large-size free box with minimal valid size (16)";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_OK);
}


TEST_F(EsdsReadLenTests, LargeSizeFreeBoxBoundaryMismatchWhenBufferShorterThanLargesize)
{
    // Build a valid stsd->mp4a->esds prelude
    auto lenBytes = encode_len(8);
    auto esdsPayload = build_esds_payload(lenBytes, /*payload*/ 8);
    auto mp4aBox = build_mp4a_box(esdsPayload);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    // Construct a large-size free box with largesize=32 (valid),
    // then intentionally truncate the appended bytes to be smaller than 32.
    auto freeBoxFull = build_free_box_large_size(32); // should be 32 bytes total

    // Truncate to 24 bytes to simulate buffer ending before 'next'
    std::vector<uint8_t> freeBoxTruncated(freeBoxFull.begin(), freeBoxFull.begin() + 24);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), stsdBox.begin(), stsdBox.end());
    buffer.insert(buffer.end(), freeBoxTruncated.begin(), freeBoxTruncated.end());

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());

    // Expected: DemuxHelper computes next = ptr + (32 - 16) and finds next > fin,
    // thus sets MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH.
    EXPECT_FALSE(ok) << "Parse should fail: largesize valid but buffer shorter than declared size";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH);
}


// Build a 'free' box with size==0, which extends to end of file/buffer
static std::vector<uint8_t> build_free_box_size0(uint32_t payloadSize)
{
    std::vector<uint8_t> box;
    append_u32(box, 0);               // size==0 => box runs to end
    append_type(box, "free");
    box.resize(8 + payloadSize, 0x00);
    return box;
}

TEST_F(EsdsReadLenTests, SizeZeroFreeBoxExtendsToEndAndParsesSuccessfully)
{
    // Prelude: valid stsd->mp4a->esds
    auto lenBytes = encode_len(8);
    auto esdsPayload = build_esds_payload(lenBytes, /*payload*/ 8);
    auto mp4aBox = build_mp4a_box(esdsPayload);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    // Create a size==0 free box with 256 bytes of payload
    auto zeroFree = build_free_box_size0(256);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), stsdBox.begin(), stsdBox.end());
    buffer.insert(buffer.end(), zeroFree.begin(), zeroFree.end());

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());
    EXPECT_TRUE(ok) << "Parse should succeed: size==0 free box extends to end of buffer";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_OK);
}


// Build a zero-size box with an unknown type (not handled in switch)
static std::vector<uint8_t> build_zero_size_unknown_box(const char type[4], uint32_t payloadSize)
{
    std::vector<uint8_t> box;
    append_u32(box, 0); // size==0 -> box extends to end of buffer
    append_type(box, type);
    box.resize(8 + payloadSize, 0x00);
    return box;
}

TEST_F(EsdsReadLenTests, ZeroSizeUnknownTypeTriggersDataBoundaryMismatch)
{
    // Prelude: a valid stsd->mp4a->esds so parser is in a good state
    auto lenBytes = encode_len(8);
    auto esdsPayload = build_esds_payload(lenBytes, /*payload*/ 8);
    auto mp4aBox = build_mp4a_box(esdsPayload);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    // Create a zero-size box with an unrecognized type 'zzzz' and some payload
    auto unknownZero = build_zero_size_unknown_box("zzzz", 64);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), stsdBox.begin(), stsdBox.end());
    buffer.insert(buffer.end(), unknownZero.begin(), unknownZero.end());

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());

    // Rationale: For size==0, DemuxHelper sets next=fin. Since the type is unknown,
    // it falls into default: (no ptr advance). The final ptr!=next check fires and sets
    // MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH.
    EXPECT_FALSE(ok) << "Parse should fail: zero-size box with unknown type leaves ptr!=next";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH);
}


// Generic builder: size==0 box for a given valid type
static std::vector<uint8_t> build_size0_box(const char type[4], uint32_t payloadSize)
{
    std::vector<uint8_t> box;
    append_u32(box, 0); // size==0 => extends to end of buffer
    append_type(box, type);
    box.resize(8 + payloadSize, 0x00);
    return box;
}

TEST_F(EsdsReadLenTests, SizeZeroMdatExtendsToEndAndParsesSuccessfully)
{
    // Prelude: valid stsd->mp4a->esds ensures DemuxHelper is functioning
    auto lenBytes = encode_len(8);
    auto esdsPayload = build_esds_payload(lenBytes, /*payload*/ 8);
    auto mp4aBox = build_mp4a_box(esdsPayload);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    // Build an 'mdat' box with size==0 and 512 bytes payload
    auto zeroMdat = build_size0_box("mdat", 512);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), stsdBox.begin(), stsdBox.end());
    buffer.insert(buffer.end(), zeroMdat.begin(), zeroMdat.end());

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());

    // Expected: DemuxHelper sets next=fin for size==0; in 'mdat' case it records
    // mdatStart/mdatEnd and sets ptr=next. Final ptr==next check passes, parse OK.
    EXPECT_TRUE(ok) << "Parse should succeed: size==0 mdat extends to end of buffer";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_OK);
}


TEST_F(EsdsReadLenTests, SizeZeroUdtaExtendsToEndAndParsesSuccessfully)
{
    // Prelude: valid stsd->mp4a->esds
    auto lenBytes = encode_len(8);
    auto esdsPayload = build_esds_payload(lenBytes, /*payload*/ 8);
    auto mp4aBox = build_mp4a_box(esdsPayload);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    // Build a 'udta' box with size==0 and 128 bytes payload
    auto zeroUdta = build_size0_box("udta", 128);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), stsdBox.begin(), stsdBox.end());
    buffer.insert(buffer.end(), zeroUdta.begin(), zeroUdta.end());

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());

    // Expected: size==0 -> next=fin. For 'udta', DemuxHelper case sets ptr=next directly.
    // Final ptr==next passes, parse OK.
    EXPECT_TRUE(ok) << "Parse should succeed: size==0 udta extends to end of buffer";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_OK);
}


TEST_F(EsdsReadLenTests, SizeZeroEsdsWithCorruptedDescriptorTagFails)
{
    // Build a top-level size==0 'esds' box whose payload starts with an invalid tag (0x01),
    // which should trigger MP4_PARSE_ERROR_INVALID_ESDS_TAG inside ParseEsdsCodecConfigHelper().

    // Construct corrupted ESDS payload: FullBox header(4) + invalid tag(0x01) + len(1) + 1 byte
    std::vector<uint8_t> corrupted;
    // FullBox header: version(1) + flags(3)
    corrupted.insert(corrupted.end(), {0x00, 0x00, 0x00, 0x00});
    // Invalid tag
    corrupted.push_back(0x01);
    // length=1 (varint)
    corrupted.push_back(0x01);
    // one arbitrary data byte
    corrupted.push_back(0xFF);

    // Wrap into size==0 'esds' box
    std::vector<uint8_t> esdsBox;
    append_u32(esdsBox, 0);           // size==0 -> extends to end
    append_type(esdsBox, "esds");
    esdsBox.insert(esdsBox.end(), corrupted.begin(), corrupted.end());

    // Buffer: include a small valid stsd->mp4a->esds prelude so demuxer has context, then corrupted esds
    auto lenBytes = encode_len(8);
    auto goodEsdsPayload = build_esds_payload(lenBytes, /*payload*/ 8);
    auto mp4aBox = build_mp4a_box(goodEsdsPayload);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), stsdBox.begin(), stsdBox.end());
    buffer.insert(buffer.end(), esdsBox.begin(), esdsBox.end());

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());

    // Expected: DemuxHelper sets next=fin for size==0 esds and dispatches to ParseCodecConfigurationBox.
    // The first descriptor tag is invalid -> setParseError(INVALID_ESDS_TAG), Parse() returns false.
    EXPECT_FALSE(ok) << "Parse should fail: size==0 esds with corrupted tag";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_ERROR_INVALID_ESDS_TAG);
}


TEST_F(EsdsReadLenTests, SizeZeroEsdsValidTagButShortDataTriggersBoundaryMismatch)
{
    // Build a top-level size==0 'esds' box with valid tags (0x03, 0x04, 0x05)
    // but declare a DecoderSpecificInfo (0x05) length larger than available bytes.
    // Expect MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH at box boundary check.

    std::vector<uint8_t> esdsPayload;
    // FullBox header: version(1) + flags(3)
    esdsPayload.insert(esdsPayload.end(), {0x00, 0x00, 0x00, 0x00});

    // 0x03 ES_Descriptor: len=3, content: ES_ID(2) + flags(1)
    esdsPayload.push_back(0x03);
    esdsPayload.push_back(0x03); // varint len=3
    esdsPayload.push_back(0x00); // ES_ID MSB
    esdsPayload.push_back(0x02); // ES_ID LSB
    esdsPayload.push_back(0x00); // flags

    // 0x04 DecoderConfigDescriptor: len=13, we provide full 13 bytes
    esdsPayload.push_back(0x04);
    esdsPayload.push_back(0x0D); // varint len=13
    esdsPayload.push_back(0x40); // objectTypeIndication
    esdsPayload.push_back(0x15); // streamType/upStream/reserved
    // bufferSizeDB (3 bytes)
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);
    // maxBitrate (4 bytes)
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);
    // avgBitrate (4 bytes)
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);
    esdsPayload.push_back(0x00);

    // 0x05 DecoderSpecificInfo: declare len=20 but only provide 5 bytes
    esdsPayload.push_back(0x05);
    esdsPayload.push_back(0x14); // varint len=20
    esdsPayload.push_back(0x11);
    esdsPayload.push_back(0x22);
    esdsPayload.push_back(0x33);
    esdsPayload.push_back(0x44);
    esdsPayload.push_back(0x55);
    // No SLConfigDescriptor here; we intentionally cut short to provoke boundary mismatch

    // Wrap into size==0 'esds' box
    std::vector<uint8_t> esdsBox;
    append_u32(esdsBox, 0);           // size==0 -> extends to end
    append_type(esdsBox, "esds");
    esdsBox.insert(esdsBox.end(), esdsPayload.begin(), esdsPayload.end());

    // Buffer: include a valid prelude then corrupted size==0 esds
    auto lenBytes = encode_len(8);
    auto goodEsdsPayload = build_esds_payload(lenBytes, /*payload*/ 8);
    auto mp4aBox = build_mp4a_box(goodEsdsPayload);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), stsdBox.begin(), stsdBox.end());
    buffer.insert(buffer.end(), esdsBox.begin(), esdsBox.end());

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());

    // Expected: ParseEsdsCodecConfigHelper copies 5 bytes for 0x05 but declared 20 bytes; ptr < next
    // After returning to DemuxHelper for 'esds', final ptr!=next sets DATA_BOUNDARY_MISMATCH.
    EXPECT_FALSE(ok) << "Parse should fail: valid ESDS tag with short data triggers boundary mismatch";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_ERROR_DATA_BOUNDARY_MISMATCH);
}


TEST_F(EsdsReadLenTests, SizeZeroFreeWithExtraPaddingParsesSuccessfully)
{
    // Prelude: valid stsd->mp4a->esds
    auto lenBytes = encode_len(8);
    auto esdsPayload = build_esds_payload(lenBytes, /*payload*/ 8);
    auto mp4aBox = build_mp4a_box(esdsPayload);
    auto stsdBox = build_stsd_with_mp4a(mp4aBox);

    // Create a size==0 'free' box whose payload is 256 bytes
    auto zeroFree = build_size0_box("free", 256);

    // Add extra padding bytes to the overall buffer (outside any explicit box structure).
    // Since size==0 extends the box to the end of the buffer, these bytes are still considered
    // part of the 'free' box payload and should parse without issue.
    std::vector<uint8_t> extraPadding(64, 0x00);

    std::vector<uint8_t> buffer;
    buffer.insert(buffer.end(), stsdBox.begin(), stsdBox.end());
    buffer.insert(buffer.end(), zeroFree.begin(), zeroFree.end());
    buffer.insert(buffer.end(), extraPadding.begin(), extraPadding.end());

    Mp4Demux demux;
    bool ok = demux.Parse(buffer.data(), buffer.size());

    // Expected: next=fin for size==0, ptr set to next for 'free'; extra padding is part of the box payload.
    EXPECT_TRUE(ok) << "Parse should succeed: size==0 free with extra padding still extends to end";
    EXPECT_EQ(demux.GetLastError(), MP4_PARSE_OK);
}
