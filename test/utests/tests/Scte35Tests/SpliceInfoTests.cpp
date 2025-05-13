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

#include <iostream>
#include <string>
#include <string.h>
#include <gtest/gtest.h>
#include "AampConfig.h"
#include "scte35/AampSCTE35.h"
#include "AampUtils.h"
#include "AampJsonObject.h"
#include "_base64.h"
#include "MockAampConfig.h"

class SpliceInfoTests : public ::testing::Test
{
protected:
	void SetUp() override
	{
		if(gpGlobalConfig == nullptr)
		{
			gpGlobalConfig =  new AampConfig();
		}
	}

	void TearDown() override
	{
		delete gpGlobalConfig;
		gpGlobalConfig = nullptr;
	}

public:
	/**
	 * @brief Create a base64 encoded SCTE-35 splice info section
	 *
	 * A CRC32 value is set in the last four bytes of the data.
	 *
	 * @brief param[in] data SCTE-35 splice info section data
	 */
	std::string EncodeSectionData(std::vector<uint8_t> &data)
	{
		std::string base64;

		/* Setup the section data CRC32. */
		size_t dataSize = data.size() - sizeof(uint32_t);
		uint32_t crc32 = aamp_ComputeCRC32((const uint8_t *)data.data(), dataSize);
		data[dataSize] = (crc32 >> 24) & 0xff;
		data[dataSize + 1] = (crc32 >> 16) & 0xff;
		data[dataSize + 2] = (crc32 >> 8) & 0xff;
		data[dataSize + 3] = crc32 & 0xff;

		/* Create the base64 encoded SCTE35 signal data. */
		char *encoded = base64_Encode((const unsigned char *)data.data(), data.size());
		if (encoded)
		{
			base64 = std::string(encoded);
			free(encoded);
		}

		return base64;
	}
};

/**
 * @brief splice_null test
 */
TEST_F(SpliceInfoTests, SpliceNull)
{
	std::vector<uint8_t> section =
	{
		0xfc,	/* "table_id":0xfc */
		0x30,	/* "section_syntax_indicator":0 */
				/* "private_indicator":0 */
				/* "sap_type":3 */
				/* "section_length":17 */
		0x11,
		0x00,	/* "protocol_version":0 */
		0x01,	/* "encrypted_packet":0 */
				/* "encryption_algorithm":0 */
				/* "pts_adjustment":0x123456789 */
		0x23,
		0x45,
		0x67,
		0x89,
		0x00,	/* "cw_index":0 */
		0xff,	/* "tier":0xfff */
		0xf0,	/* "splice_command_length":0 */
		0x00,
		0x00,	/* "splice_command_type":0 (splice_null) */
		0x00,	/* "descriptor_loop_length":0 */
		0x00,
		0x00,	/* "CRC32":0 (set by the test) */
		0x00,
		0x00,
		0x00
	};

	/* Parse the splice info section. */
	std::string base64 = EncodeSectionData(section);
	SCTE35SpliceInfo spliceInfo(base64);

	/* Extract JSON data. */
	std::string jsonString = spliceInfo.getJsonString();
	EXPECT_FALSE(jsonString.empty());
	AampJsonObject jsonObject(jsonString);

	/* Verify some data */
	int value = -1;
	double pts_adjustment = 0.0;
	std::vector<AampJsonObject> descriptors;

	EXPECT_TRUE(jsonObject.get("table_id", value));
	EXPECT_EQ(value, 0xfc);

	EXPECT_TRUE(jsonObject.get("sap_type", value));
	EXPECT_EQ(value, 3);

	EXPECT_TRUE(jsonObject.get("pts_adjustment", pts_adjustment));
	EXPECT_EQ(pts_adjustment, (double)0x123456789ll);

	EXPECT_TRUE(jsonObject.get("splice_command_type", value));
	EXPECT_EQ(value, 0);

	EXPECT_TRUE(jsonObject.get("descriptors", descriptors));
	ASSERT_EQ(descriptors.size(), 0);

	/* Verify the event summary. */
	std::vector<SCTE35SpliceInfo::Summary> summary;
	spliceInfo.getSummary(summary);
	EXPECT_EQ(summary.size(), 0);
}

/**
 * @brief Break start test
 */
TEST_F(SpliceInfoTests, BreakStart)
{
	std::vector<uint8_t> section =
	{
		0xfc,	/* "table_id":0xfc */
		0x30,	/* "section_syntax_indicator":0 */
				/* "private_indicator":0 */
				/* "sap_type":3 */
				/* "section_length":44 */
		0x2c,
		0x00,	/* "protocol_version":0 */
		0x00,	/* "encrypted_packet":0 */
				/* "encryption_algorithm":0 */
				/* "pts_adjustment":0x12345678 */
		0x12,
		0x34,
		0x56,
		0x78,
		0x00,	/* "cw_index":0 */
		0xff,	/* "tier":0xfff */
		0xf0,	/* "splice_command_length":5 */
		0x05,
		0x06,	/* "splice_command_type":6 (time_signal) */
		0xff,	/* "time_specified_flag":1 */
				/* "reserved":0x3f */
				/* "pts_time":0x123456789 */
		0x23,
		0x45,
		0x67,
		0x89,
		0x00,	/* "descriptor_loop_length":22 */
		0x16,
		0x02,	/* "splice_descriptor_tag":2 (segmentation_descriptor) */
		0x14,	/* "descriptor_length":20 */
		0x43,	/* "identifier":'CUEI' */
		0x55,
		0x45,
		0x49,
		0x01,	/* "segmentation_event_id":0x01234567 */
		0x23,
		0x45,
		0x67,
		0x7f,	/* "segmentation_event_cancel_indicator":0 */
				/* "reserved":0x7f */
		0xc0,	/* "program_segmentation_flag":1 */
				/* "segmentation_duration_flag":1 */
				/* "delivery_not_restricted_flag":0 */
				/* "web_delivery_allowed_flag":0 */
				/* "no_regional_blackout_flag":0 */
				/* "archive_allowed_flag": 0 */
				/* "device_restrictions":0 */
		0x00,	/* "segmentation_duration":360000 */
		0x00,
		0x05,
		0x7e,
		0x40,
		0x00,	/* "segmentation_upid_type":0 */
		0x00,	/* "segmentation_upid_length":0 */
		0x22,	/* "segmentation_type_id":0x22 (Break Start) */
		0x00,	/* "segment_num":0 */
		0x00,	/* "segments_expected":0 */
		0x00,	/* "CRC32":0 (set by the test) */
		0x00,
		0x00,
		0x00
	};

	/* Parse the splice info section. */
	std::string base64 = EncodeSectionData(section);
	SCTE35SpliceInfo spliceInfo(base64);

	/* Extract JSON data. */
	std::string jsonString = spliceInfo.getJsonString();
	EXPECT_FALSE(jsonString.empty());
	AampJsonObject jsonObject(jsonString);

	/* Verify some data. */
	int value = -1;
	double pts_adjustment = 0.0;
	double pts_time = 0.0;
	double duration = 0.0;
	int segmentation_event_id = 0;
	AampJsonObject timeSignal;
	std::vector<AampJsonObject> descriptors;

	EXPECT_TRUE(jsonObject.get("pts_adjustment", pts_adjustment));
	EXPECT_EQ(pts_adjustment, (double)0x12345678);

	EXPECT_TRUE(jsonObject.get("splice_command_type", value));
	EXPECT_EQ(value, 6);

	EXPECT_TRUE(jsonObject.get("splice_command", timeSignal));

	EXPECT_TRUE(timeSignal.get("pts_time", pts_time));
	EXPECT_EQ(pts_time, (double)0x123456789);

	EXPECT_TRUE(jsonObject.get("descriptors", descriptors));
	ASSERT_EQ(descriptors.size(), 1);

	EXPECT_TRUE(descriptors[0].get("splice_descriptor_tag", value));
	EXPECT_EQ(value, 0x02);

	EXPECT_TRUE(descriptors[0].get("identifier", value));
	EXPECT_EQ(value, 'CUEI');

	EXPECT_TRUE(descriptors[0].get("segmentation_event_id", segmentation_event_id));
	EXPECT_EQ(segmentation_event_id, 0x01234567);

	EXPECT_TRUE(descriptors[0].get("segmentation_duration", duration));
	EXPECT_EQ(duration, 360000);

	EXPECT_TRUE(descriptors[0].get("segmentation_type_id", value));
	EXPECT_EQ(value, 0x22);

	/* Verify the event summary. */
	std::vector<SCTE35SpliceInfo::Summary> summary;
	spliceInfo.getSummary(summary);
	EXPECT_EQ(summary.size(), 1);
	EXPECT_EQ(summary[0].type, SCTE35SpliceInfo::SEGMENTATION_TYPE::BREAK_START);
	EXPECT_EQ(summary[0].time, (pts_time + pts_adjustment)/90000.0);
	EXPECT_EQ(summary[0].duration, duration/90000.0);
	EXPECT_EQ(summary[0].event_id, segmentation_event_id);
}

/**
 * @brief Ad start test
 *
 * Note that the reported event time wraps due the limited range of PTS values.
 */
TEST_F(SpliceInfoTests, AdStart)
{
	std::vector<uint8_t> section =
	{
		0xfc,	/* "table_id":0xfc */
		0x30,	/* "section_syntax_indicator":0 */
				/* "private_indicator":0 */
				/* "sap_type":3 */
				/* "section_length":54 */
		0x36,
		0x00,	/* "protocol_version":0 */
		0x01,	/* "encrypted_packet":0 */
				/* "encryption_algorithm":0 */
				/* "pts_adjustment":0x123456789 */
		0x23,
		0x45,
		0x67,
		0x89,
		0x00,	/* "cw_index":0 */
		0xff,	/* "tier":0xfff */
		0xf0,	/* "splice_command_length":5 */
		0x05,
		0x06,	/* "splice_command_type":6 (time_signal) */
		0xff,	/* "time_specified_flag":1 */
				/* "reserved":0x3f */
				/* "pts_time":0x123456789 */
		0x23,
		0x45,
		0x67,
		0x89,
		0x00,	/* "descriptor_loop_length":32 */
		0x20,
		0x02,	/* "splice_descriptor_tag":2 (segmentation_descriptor) */
		0x1e,	/* "descriptor_length":30 */
		0x43,	/* "identifier":'CUEI' */
		0x55,
		0x45,
		0x49,
		0x01,	/* "segmentation_event_id":0x01234567 */
		0x23,
		0x45,
		0x67,
		0x7f,	/* "segmentation_event_cancel_indicator":0 */
				/* "reserved":0x7f */
		0xc0,	/* "program_segmentation_flag":1 */
				/* "segmentation_duration_flag":1 */
				/* "delivery_not_restricted_flag":0 */
				/* "web_delivery_allowed_flag":0 */
				/* "no_regional_blackout_flag":0 */
				/* "archive_allowed_flag": 0 */
				/* "device_restrictions":0 */
		0x00,	/* "segmentation_duration":2700000 */
		0x00,
		0x29,
		0x32,
		0xe0,
		0x0d,	/* "segmentation_upid_type":0x0d (MID) */
		0x0a,	/* "segmentation_upid_length":10 */
		0x0e,	/* "segmentation_upid_type":0x0e (ADS Information) */
		0x03,	/* "segmentation_upid_length":3 */
		0x41,	/* "ADS" */
		0x44,
		0x53,
		0x0f,	/* "segmentation_upid_type":0x0f (URI) */
		0x03,	/* "segmentation_upid_length":3 */
		0x55,	/* "URI" */
		0x52,
		0x49,
		0x30,	/* "segmentation_type_id":0x30 (Provider Advertisement Start) */
		0x00,	/* "segment_num":0 */
		0x00,	/* "segments_expected":0 */
		0x00,	/* "CRC32":0 (set by the test) */
		0x00,
		0x00,
		0x00
	};

	/* Parse the splice info section. */
	std::string base64 = EncodeSectionData(section);
	SCTE35SpliceInfo spliceInfo(base64);

	/* Extract JSON data. */
	std::string jsonString = spliceInfo.getJsonString();
	EXPECT_FALSE(jsonString.empty());
	AampJsonObject jsonObject(jsonString);

	/* Verify some data. */
	int value = -1;
	double pts_adjustment = 0.0;
	double pts_time = 0.0;
	double duration = 0.0;
	int segmentation_event_id = 0;
	std::string string;
	AampJsonObject timeSignal;
	std::vector<AampJsonObject> descriptors;
	std::vector<AampJsonObject> mid;

	EXPECT_TRUE(jsonObject.get("pts_adjustment", pts_adjustment));
	EXPECT_EQ(pts_adjustment, (double)0x123456789);

	EXPECT_TRUE(jsonObject.get("splice_command_type", value));
	EXPECT_EQ(value, 6);

	EXPECT_TRUE(jsonObject.get("splice_command", timeSignal));

	EXPECT_TRUE(timeSignal.get("pts_time", pts_time));
	EXPECT_EQ(pts_time, (double)0x123456789);

	EXPECT_TRUE(jsonObject.get("descriptors", descriptors));
	ASSERT_EQ(descriptors.size(), 1);

	EXPECT_TRUE(descriptors[0].get("splice_descriptor_tag", value));
	EXPECT_EQ(value, 0x02);

	EXPECT_TRUE(descriptors[0].get("segmentation_event_id", segmentation_event_id));
	EXPECT_EQ(segmentation_event_id, 0x01234567);

	EXPECT_TRUE(descriptors[0].get("segmentation_duration", duration));
	EXPECT_EQ(duration, 2700000);

	EXPECT_TRUE(descriptors[0].get("segmentation_type_id", value));
	EXPECT_EQ(value, 0x30);

	EXPECT_TRUE(descriptors[0].get("segmentation_upid_type", value));
	EXPECT_EQ(value, 0x0d);

	EXPECT_TRUE(descriptors[0].get("MID", mid));
	ASSERT_EQ(mid.size(), 2);

	EXPECT_TRUE(mid[0].get("segmentation_upid_type", value));
	EXPECT_EQ(value, 0x0e);

	EXPECT_TRUE(mid[0].get("ADS", string));
	EXPECT_STREQ(string.c_str(), "ADS");

	EXPECT_TRUE(mid[1].get("segmentation_upid_type", value));
	EXPECT_EQ(value, 0x0f);

	EXPECT_TRUE(mid[1].get("URI", string));
	EXPECT_STREQ(string.c_str(), "URI");

	/* Verify the event summary.
	 * Note that the reported time wraps.
	 */
	std::vector<SCTE35SpliceInfo::Summary> summary;
	spliceInfo.getSummary(summary);
	EXPECT_EQ(summary.size(), 1);
	EXPECT_EQ(summary[0].type, SCTE35SpliceInfo::SEGMENTATION_TYPE::PROVIDER_ADVERTISEMENT_START);
	EXPECT_EQ(summary[0].time, fmod((pts_time + pts_adjustment)/90000.0, (double)0x200000000/90000.0));
	EXPECT_EQ(summary[0].duration, duration/90000.0);
	EXPECT_EQ(summary[0].event_id, segmentation_event_id);
}

/**
 * @brief Ad end test
 */
TEST_F(SpliceInfoTests, AdEnd)
{
	std::vector<uint8_t> section =
	{
		0xfc,	/* "table_id":0xfc */
		0x30,	/* "section_syntax_indicator":0 */
				/* "private_indicator":0 */
				/* "sap_type":3 */
				/* "section_length":49 */
		0x31,
		0x00,	/* "protocol_version":0 */
		0x00,	/* "encrypted_packet":0 */
				/* "encryption_algorithm":0 */
				/* "pts_adjustment":0x12345678 */
		0x12,
		0x34,
		0x56,
		0x78,
		0x00,	/* "cw_index":0 */
		0xff,	/* "tier":0xfff */
		0xf0,	/* "splice_command_length":5 */
		0x05,
		0x06,	/* "splice_command_type":6 (time_signal) */
		0xff,	/* "time_specified_flag":1 */
				/* "reserved":0x3f */
				/* "pts_time":0x123456789 */
		0x23,
		0x45,
		0x67,
		0x89,
		0x00,	/* "descriptor_loop_length":27 */
		0x1b,
		0x02,	/* "splice_descriptor_tag":2 (segmentation_descriptor) */
		0x19,	/* "descriptor_length":25 */
		0x43,	/* "identifier":'CUEI' */
		0x55,
		0x45,
		0x49,
		0x01,	/* "segmentation_event_id":0x01234567 */
		0x23,
		0x45,
		0x67,
		0x7f,	/* "segmentation_event_cancel_indicator":0 */
				/* "reserved":0x7f */
		0x80,	/* "program_segmentation_flag":1 */
				/* "segmentation_duration_flag":0 */
				/* "delivery_not_restricted_flag":0 */
				/* "web_delivery_allowed_flag":0 */
				/* "no_regional_blackout_flag":0 */
				/* "archive_allowed_flag": 0 */
				/* "device_restrictions":0 */
		0x0d,	/* "segmentation_upid_type":0x0d (MID) */
		0x0a,	/* "segmentation_upid_length":10 */
		0x0e,	/* "segmentation_upid_type":0x0e (ADS Information) */
		0x03,	/* "segmentation_upid_length":3 */
		0x41,	/* "ADS" */
		0x44,
		0x53,
		0x0f,	/* "segmentation_upid_type":0x0f (URI) */
		0x03,	/* "segmentation_upid_length":3 */
		0x55,	/* "URI" */
		0x52,
		0x49,
		0x31,	/* "segmentation_type_id":0x31 (Provider Advertisement End) */
		0x00,	/* "segment_num":0 */
		0x00,	/* "segments_expected":0 */
		0x00,	/* "CRC32":0 (set by the test) */
		0x00,
		0x00,
		0x00
	};

	/* Parse the splice info section. */
	std::string base64 = EncodeSectionData(section);
	SCTE35SpliceInfo spliceInfo(base64);

	/* Extract JSON data. */
	std::string jsonString = spliceInfo.getJsonString();
	EXPECT_FALSE(jsonString.empty());
	AampJsonObject jsonObject(jsonString);

	/* Verify some data. */
	int value = -1;
	double pts_adjustment = 0.0;
	double pts_time = 0.0;
	int segmentation_event_id = 0;
	std::string string;
	AampJsonObject timeSignal;
	std::vector<AampJsonObject> descriptors;
	std::vector<AampJsonObject> mid;

	EXPECT_TRUE(jsonObject.get("pts_adjustment", pts_adjustment));
	EXPECT_EQ(pts_adjustment, (double)0x12345678);

	EXPECT_TRUE(jsonObject.get("splice_command_type", value));
	EXPECT_EQ(value, 6);

	EXPECT_TRUE(jsonObject.get("splice_command", timeSignal));

	EXPECT_TRUE(timeSignal.get("pts_time", pts_time));
	EXPECT_EQ(pts_time, (double)0x123456789);

	EXPECT_TRUE(jsonObject.get("descriptors", descriptors));
	ASSERT_EQ(descriptors.size(), 1);

	EXPECT_TRUE(descriptors[0].get("splice_descriptor_tag", value));
	EXPECT_EQ(value, 0x02);

	EXPECT_TRUE(descriptors[0].get("segmentation_event_id", segmentation_event_id));
	EXPECT_EQ(segmentation_event_id, 0x01234567);

	EXPECT_TRUE(descriptors[0].get("segmentation_type_id", value));
	EXPECT_EQ(value, 0x31);

	EXPECT_TRUE(descriptors[0].get("segmentation_upid_type", value));
	EXPECT_EQ(value, 0x0d);

	EXPECT_TRUE(descriptors[0].get("MID", mid));
	ASSERT_EQ(mid.size(), 2);

	EXPECT_TRUE(mid[0].get("segmentation_upid_type", value));
	EXPECT_EQ(value, 0x0e);

	EXPECT_TRUE(mid[0].get("ADS", string));
	EXPECT_STREQ(string.c_str(), "ADS");

	EXPECT_TRUE(mid[1].get("segmentation_upid_type", value));
	EXPECT_EQ(value, 0x0f);

	EXPECT_TRUE(mid[1].get("URI", string));
	EXPECT_STREQ(string.c_str(), "URI");

	/* Verify the event summary. */
	std::vector<SCTE35SpliceInfo::Summary> summary;
	spliceInfo.getSummary(summary);
	EXPECT_EQ(summary.size(), 1);
	EXPECT_EQ(summary[0].type, SCTE35SpliceInfo::SEGMENTATION_TYPE::PROVIDER_ADVERTISEMENT_END);
	EXPECT_EQ(summary[0].time, (pts_adjustment + pts_time)/90000.0);
	EXPECT_EQ(summary[0].duration, 0.0);
	EXPECT_EQ(summary[0].event_id, segmentation_event_id);
}

/**
 * @brief Break end test
 */
TEST_F(SpliceInfoTests, BreakEnd)
{
	std::vector<uint8_t> section =
	{
		0xfc,	/* "table_id":0xfc */
		0x30,	/* "section_syntax_indicator":0 */
				/* "private_indicator":0 */
				/* "sap_type":3 */
				/* "section_length":44 */
		0x2c,
		0x00,	/* "protocol_version":0 */
		0x00,	/* "encrypted_packet":0 */
				/* "encryption_algorithm":0 */
				/* "pts_adjustment":0x12345678 */
		0x12,
		0x34,
		0x56,
		0x78,
		0x00,	/* "cw_index":0 */
		0xff,	/* "tier":0xfff */
		0xf0,	/* "splice_command_length":5 */
		0x05,
		0x06,	/* "splice_command_type":6 (time_signal) */
		0xff,	/* "time_specified_flag":1 */
				/* "reserved":0x3f */
				/* "pts_time":0x123456789 */
		0x23,
		0x45,
		0x67,
		0x89,
		0x00,	/* "descriptor_loop_length":17 */
		0x11,
		0x02,	/* "splice_descriptor_tag":2 (segmentation_descriptor) */
		0x0f,	/* "descriptor_length":15 */
		0x43,	/* "identifier":"CUEI" */
		0x55,
		0x45,
		0x49,
		0x01,	/* "segmentation_event_id":0x01234567 */
		0x23,
		0x45,
		0x67,
		0x7f,	/* "segmentation_event_cancel_indicator":0 */
				/* "reserved":0x7f */
		0x80,	/* "program_segmentation_flag":1 */
				/* "segmentation_duration_flag":0 */
				/* "delivery_not_restricted_flag":0 */
				/* "web_delivery_allowed_flag":0 */
				/* "no_regional_blackout_flag":0 */
				/* "archive_allowed_flag": 0 */
				/* "device_restrictions":0 */
		0x00,	/* "segmentation_upid_type":0 */
		0x00,	/* "segmentation_upid_length":0 */
		0x23,	/* "segmentation_type_id":0x23 (Break End) */
		0x00,	/* "segment_num":0 */
		0x00,	/* "segments_expected":0 */
		0x00,	/* "CRC32":0 (set by the test) */
		0x00,
		0x00,
		0x00
	};

	/* Parse the splice info section. */
	std::string base64 = EncodeSectionData(section);
	SCTE35SpliceInfo spliceInfo(base64);

	/* Extract JSON data. */
	std::string jsonString = spliceInfo.getJsonString();
	EXPECT_FALSE(jsonString.empty());
	AampJsonObject jsonObject(jsonString);

	/* Verify some data. */
	int value = -1;
	double pts_adjustment = 0.0;
	double pts_time = 0.0;
	int segmentation_event_id = 0;
	AampJsonObject timeSignal;
	std::vector<AampJsonObject> descriptors;

	EXPECT_TRUE(jsonObject.get("pts_adjustment", pts_adjustment));
	EXPECT_EQ(pts_adjustment, (double)0x12345678);

	EXPECT_TRUE(jsonObject.get("splice_command_type", value));
	EXPECT_EQ(value, 6);

	EXPECT_TRUE(jsonObject.get("splice_command", timeSignal));

	EXPECT_TRUE(timeSignal.get("pts_time", pts_time));
	EXPECT_EQ(pts_time, (double)0x123456789);

	EXPECT_TRUE(jsonObject.get("descriptors", descriptors));
	ASSERT_EQ(descriptors.size(), 1);

	EXPECT_TRUE(descriptors[0].get("splice_descriptor_tag", value));
	EXPECT_EQ(value, 0x02);

	EXPECT_TRUE(descriptors[0].get("segmentation_event_id", segmentation_event_id));
	EXPECT_EQ(segmentation_event_id, 0x01234567);

	EXPECT_TRUE(descriptors[0].get("segmentation_type_id", value));
	EXPECT_EQ(value, 0x23);

	/* Verify the event summary. */
	std::vector<SCTE35SpliceInfo::Summary> summary;
	spliceInfo.getSummary(summary);
	EXPECT_EQ(summary.size(), 1);
	EXPECT_EQ(summary[0].type, SCTE35SpliceInfo::SEGMENTATION_TYPE::BREAK_END);
	EXPECT_EQ(summary[0].time, (pts_adjustment + pts_time)/90000.0);
	EXPECT_EQ(summary[0].duration, 0.0);
	EXPECT_EQ(summary[0].event_id, segmentation_event_id);
}

/**
 * @brief Bad signal test
 */
TEST_F(SpliceInfoTests, BadSignal)
{
	/* Parse a bad splice info section signal. */
	SCTE35SpliceInfo spliceInfo(",,");

	/* Extract JSON data. */
	std::string jsonString = spliceInfo.getJsonString();
	EXPECT_TRUE(jsonString.empty());

	/* Verify the event summary. */
	std::vector<SCTE35SpliceInfo::Summary> summary;
	spliceInfo.getSummary(summary);
	EXPECT_EQ(summary.size(), 0);
}
