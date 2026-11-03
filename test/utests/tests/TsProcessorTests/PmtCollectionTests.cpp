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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include "priv_aamp.h"
#include "AampConfig.h"
#include "tsprocessor.h"
#include "MockPrivateInstanceAAMP.h"

using ::testing::_;
using ::testing::NiceMock;

extern AampConfig *gpGlobalConfig;

namespace
{
constexpr int kPacketSize = 188;
constexpr int kPmtPid = 0x1000;
constexpr int kProgram = 1;
constexpr int kVideoPid = 0x100;
// Section length chosen so a payload-only PUSI packet (180 section bytes) needs exactly one more byte
constexpr int kSectionLength = 181;

/**
 * @brief Build a PMT section (starting at program_number, i.e. after section_length)
 *        with a single H.264 video stream, padded to kSectionLength with a program info descriptor.
 */
std::vector<uint8_t> BuildPmtSection()
{
	std::vector<uint8_t> section;
	section.push_back((kProgram >> 8) & 0xFF);
	section.push_back(kProgram & 0xFF);
	section.push_back(0xC1); // version 0, current_next_indicator 1
	section.push_back(0x00); // section_number
	section.push_back(0x00); // last_section_number
	section.push_back(0xE0 | ((kVideoPid >> 8) & 0x1F)); // PCR PID
	section.push_back(kVideoPid & 0xFF);
	const int streamLoopSize = 5;
	const int crcSize = 4;
	const int infoLength = kSectionLength - 9 - streamLoopSize - crcSize;
	section.push_back(0xF0 | ((infoLength >> 8) & 0x0F));
	section.push_back(infoLength & 0xFF);
	section.push_back(0x80); // user private descriptor tag
	section.push_back(infoLength - 2);
	section.insert(section.end(), infoLength - 2, 0xAA);
	section.push_back(0x1B); // H.264 stream type
	section.push_back(0xE0 | ((kVideoPid >> 8) & 0x1F));
	section.push_back(kVideoPid & 0xFF);
	section.push_back(0xF0); // ES_info_length = 0
	section.push_back(0x00);
	section.insert(section.end(), crcSize, 0x00);
	return section;
}

/**
 * @brief Build a PMT PID TS packet.
 * @param pusi payload_unit_start_indicator
 * @param cc continuity counter
 * @param payloadBytes number of bytes after the header/adaptation field (184 => no adaptation field)
 * @param data payload bytes copied into the payload region (truncated to fit)
 */
std::vector<uint8_t> BuildPmtPacket(bool pusi, int cc, int payloadBytes, const std::vector<uint8_t> &data)
{
	std::vector<uint8_t> packet(kPacketSize, 0xFF);
	packet[0] = 0x47;
	packet[1] = (pusi ? 0x40 : 0x00) | ((kPmtPid >> 8) & 0x1F);
	packet[2] = kPmtPid & 0xFF;
	int payloadOffset = 4;
	if (payloadBytes < kPacketSize - 4)
	{
		packet[3] = 0x30 | (cc & 0x0F);
		packet[4] = (uint8_t)(kPacketSize - 5 - payloadBytes); // adaptation_field_length
		if (packet[4] > 0)
		{
			packet[5] = 0x00; // adaptation flags
		}
		payloadOffset = kPacketSize - payloadBytes;
	}
	else
	{
		packet[3] = 0x10 | (cc & 0x0F);
	}
	size_t copyLen = std::min(data.size(), (size_t)(kPacketSize - payloadOffset));
	std::copy(data.begin(), data.begin() + copyLen, packet.begin() + payloadOffset);
	return packet;
}

/**
 * @brief PUSI packet payload: pointer_field, table_id, section_length, then section data.
 */
std::vector<uint8_t> BuildPusiPayload(const std::vector<uint8_t> &section)
{
	std::vector<uint8_t> payload;
	payload.push_back(0x00); // pointer_field
	payload.push_back(0x02); // table_id: PMT
	payload.push_back(0xB0 | ((kSectionLength >> 8) & 0x0F));
	payload.push_back(kSectionLength & 0xFF);
	payload.insert(payload.end(), section.begin(), section.end());
	return payload;
}
} // namespace

class PmtCollectionTests : public ::testing::Test
{
protected:
	class TestTSProcessor : public TSProcessor
	{
	public:
		TestTSProcessor(PrivateInstanceAAMP *aamp) : TSProcessor(aamp, eStreamOp_NONE, nullptr)
		{
			m_program = kProgram;
			m_pmtPid = kPmtPid;
			m_havePAT = true;
		}

		bool Process(std::vector<uint8_t> packet)
		{
			bool insPatPmt = false;
			return processBuffer(packet.data(), (int)packet.size(), insPatPmt, false);
		}

		bool HavePMT() const { return m_havePMT; }
		int CollectorOffset() const { return m_pmtCollectorOffset; }
		int VideoPid() const { return m_videoPid; }
	};

	PrivateInstanceAAMP *mPrivateInstanceAAMP{};
	TestTSProcessor *mTSProcessor{};
	std::vector<uint8_t> mSection;
	std::vector<uint8_t> mPusiPayload;

	void SetUp() override
	{
		if (gpGlobalConfig == nullptr)
		{
			gpGlobalConfig = new AampConfig();
		}
		g_mockPrivateInstanceAAMP = std::make_shared<NiceMock<MockPrivateInstanceAAMP>>();
		mPrivateInstanceAAMP = new PrivateInstanceAAMP(gpGlobalConfig);
		mTSProcessor = new TestTSProcessor(mPrivateInstanceAAMP);
		mSection = BuildPmtSection();
		ASSERT_EQ(mSection.size(), (size_t)kSectionLength);
		mPusiPayload = BuildPusiPayload(mSection);
	}

	void TearDown() override
	{
		delete mTSProcessor;
		mTSProcessor = nullptr;
		delete mPrivateInstanceAAMP;
		mPrivateInstanceAAMP = nullptr;
		g_mockPrivateInstanceAAMP.reset();
		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;
	}

	/** Bytes of the section not carried by a PUSI packet with the given payload size */
	std::vector<uint8_t> SectionRemainder(int pusiPayloadBytes) const
	{
		return std::vector<uint8_t>(mSection.begin() + (pusiPayloadBytes - 4), mSection.end());
	}

	void ExpectValidSequenceCompletes(int cc)
	{
		EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(true, cc, 184, mPusiPayload)));
		EXPECT_EQ(mTSProcessor->CollectorOffset(), 180);
		EXPECT_FALSE(mTSProcessor->HavePMT());
		EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(false, cc + 1, 1, SectionRemainder(184))));
		EXPECT_TRUE(mTSProcessor->HavePMT());
		EXPECT_EQ(mTSProcessor->CollectorOffset(), 0);
		EXPECT_EQ(mTSProcessor->VideoPid(), kVideoPid);
	}
};

TEST_F(PmtCollectionTests, PusiWithSevenBytesRemainingStartsCollection)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamFormat(FORMAT_VIDEO_ES_H264, FORMAT_INVALID)).Times(1);

	EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(true, 0, 7, mPusiPayload)));
	EXPECT_EQ(mTSProcessor->CollectorOffset(), 3);
	EXPECT_FALSE(mTSProcessor->HavePMT());

	EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(false, 1, 184, SectionRemainder(7))));
	EXPECT_TRUE(mTSProcessor->HavePMT());
	EXPECT_EQ(mTSProcessor->CollectorOffset(), 0);
	EXPECT_EQ(mTSProcessor->VideoPid(), kVideoPid);
}

TEST_F(PmtCollectionTests, PusiWithSixBytesRemainingIsRejectedAndDiscardsPartialPmt)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamFormat(_, _)).Times(1);

	// Begin a valid collection, then interrupt it with a truncated PUSI packet
	EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(true, 0, 184, mPusiPayload)));
	EXPECT_EQ(mTSProcessor->CollectorOffset(), 180);

	EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(true, 1, 6, mPusiPayload)));
	EXPECT_EQ(mTSProcessor->CollectorOffset(), 0);
	EXPECT_FALSE(mTSProcessor->HavePMT());

	// Continuation of the discarded section must not complete it
	EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(false, 1, 1, SectionRemainder(184))));
	EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(false, 2, 1, SectionRemainder(184))));
	EXPECT_FALSE(mTSProcessor->HavePMT());
	EXPECT_EQ(mTSProcessor->CollectorOffset(), 0);

	ExpectValidSequenceCompletes(3);
}

TEST_F(PmtCollectionTests, ContinuationWithOneByteRemainingCompletesPmt)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamFormat(FORMAT_VIDEO_ES_H264, FORMAT_INVALID)).Times(1);

	ExpectValidSequenceCompletes(0);
}

TEST_F(PmtCollectionTests, ContinuationWithZeroBytesRemainingIsRejectedAndDiscardsPartialPmt)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamFormat(_, _)).Times(1);

	EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(true, 0, 184, mPusiPayload)));
	EXPECT_EQ(mTSProcessor->CollectorOffset(), 180);

	EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(false, 1, 0, {})));
	EXPECT_EQ(mTSProcessor->CollectorOffset(), 0);
	EXPECT_FALSE(mTSProcessor->HavePMT());

	// The byte that would have completed the section must be ignored
	EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(false, 1, 1, SectionRemainder(184))));
	EXPECT_TRUE(mTSProcessor->Process(BuildPmtPacket(false, 2, 1, SectionRemainder(184))));
	EXPECT_FALSE(mTSProcessor->HavePMT());
	EXPECT_EQ(mTSProcessor->CollectorOffset(), 0);

	ExpectValidSequenceCompletes(3);
}

TEST_F(PmtCollectionTests, InvalidPacketInSameBufferInterruptsCollection)
{
	EXPECT_CALL(*g_mockPrivateInstanceAAMP, SetStreamFormat(_, _)).Times(1);

	std::vector<uint8_t> buffer;
	for (const auto &packet : {
		BuildPmtPacket(true, 0, 184, mPusiPayload),
		BuildPmtPacket(false, 1, 0, {}),
		BuildPmtPacket(false, 1, 1, SectionRemainder(184)) })
	{
		buffer.insert(buffer.end(), packet.begin(), packet.end());
	}
	EXPECT_TRUE(mTSProcessor->Process(buffer));
	EXPECT_FALSE(mTSProcessor->HavePMT());
	EXPECT_EQ(mTSProcessor->CollectorOffset(), 0);

	ExpectValidSequenceCompletes(2);
}
