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

#include <inttypes.h>
#include "AampSCTE35.h"
#include "_base64.h"

SCTE35Decoder::SCTE35Decoder(std::string string) : mParent(nullptr), mLoop(nullptr),mOffset(0),mMaxOffset(),
													mData(),mJsonObj(cJSON_CreateObject()),mKey()
{
	size_t len = 0;
	mData = base64_Decode(string.c_str(), &len);
	mMaxOffset = len*8;
}

SCTE35Decoder::SCTE35Decoder(SCTE35Decoder *parent, SCTE35DecoderDescriptorLoop *descriptorLoop, size_t loopMaxOffset) : mParent(parent), mLoop(descriptorLoop)
							,mOffset(parent->mOffset),mMaxOffset(loopMaxOffset),mData(parent->mData),mJsonObj(cJSON_CreateObject()),mKey()
{
}

SCTE35Decoder::SCTE35Decoder(const char *key, SCTE35Decoder *parent, int len) : mParent(parent), mLoop(nullptr), mKey(key)
								,mOffset(parent->mOffset),mMaxOffset(parent->mOffset + (len*8)),mData(parent->mData),mJsonObj(cJSON_CreateObject())
{
}

SCTE35Decoder::~SCTE35Decoder()
{
	cJSON_Delete(mJsonObj);
	if (mParent == NULL)
	{
		free(mData);
	}
}

SCTE35Section *SCTE35Decoder::Subsection(const char *key, int length)
{
	if (mOffset & 7)
	{
		throw SCTE35DataException(std::string(key) + " not byte aligned");
	}
	else if ((mOffset + (length*8)) > mMaxOffset)
	{
		throw SCTE35DataException(std::string(key) + std::string(" overflow"));
	}
	else
	{
		return new SCTE35Decoder(key, this, length);
	}
}

SCTE35DescriptorLoop *SCTE35Decoder::DescriptorLoop(const char *key, int length)
{
	if (mOffset & 7)
	{
		throw SCTE35DataException("Descriptor loop not byte aligned");
	}
	else if ((mOffset + (length*8)) > mMaxOffset)
	{
		throw SCTE35DataException("Descriptor loop overflow");
	}
	else
	{
		return new SCTE35DecoderDescriptorLoop(key, this, mOffset + (length*8));
	}
}

bool SCTE35Decoder::Bool(const char *key)
{
	uint8_t mask;
	bool value;

	if (mOffset >= mMaxOffset)
	{
		throw SCTE35DataException(std::string(key) + " overflow");
	}
	else
	{
		mask = 0x80 >> (mOffset & 7);
		value = (mData[mOffset/8] & mask) == mask;
		mOffset++;
		if (NULL == cJSON_AddBoolToObject(mJsonObj, key, value))
		{
			AAMPLOG_WARN("Failed to add JSON Bool");
		}
	}

	return value;
}

uint8_t SCTE35Decoder::Byte(const char *key)
{
	uint8_t value;

	if (mOffset & 7)
	{
		throw SCTE35DataException(std::string(key) + std::string(" not byte aligned"));
	}
	else if ((mOffset + 8) > mMaxOffset)
	{
		throw SCTE35DataException(std::string(key) + " overflow");
	}
	else
	{
		value = mData[mOffset/8];
		mOffset += 8;
		if (NULL == cJSON_AddNumberToObject(mJsonObj, key, (double)value))
		{
			AAMPLOG_WARN("Failed to add JSON Number");
		}
	}

	return value;
}

uint16_t SCTE35Decoder::Short(const char *key)
{
	uint16_t value;

	if (mOffset & 7)
	{
		throw SCTE35DataException(std::string(key) + " not byte aligned");
	}
	else if ((mOffset + 16) > mMaxOffset)
	{
		throw SCTE35DataException(std::string(key) + " overflow");
	}
	else
	{
		value = (uint16_t)mData[mOffset/8];
		mOffset += 8;
		value = (value << 8) | (uint16_t)mData[mOffset/8];
		mOffset += 8;
		if (NULL == cJSON_AddNumberToObject(mJsonObj, key, (double)value))
		{
			AAMPLOG_WARN("Failed to add JSON Number");
		}
	}

	return value;
}

uint32_t SCTE35Decoder::Integer(const char *key)
{
	uint32_t value;

	if (mOffset & 7)
	{
		throw SCTE35DataException(std::string(key) + " not byte aligned");
	}
	else if ((mOffset + 32) > mMaxOffset)
	{
		throw SCTE35DataException(std::string(key) + " overflow");
	}
	else
	{
		value = (uint32_t)mData[mOffset/8];
		mOffset += 8;
		value = (value << 8) | (uint32_t)mData[mOffset/8];
		mOffset += 8;
		value = (value << 8) | (uint32_t)mData[mOffset/8];
		mOffset += 8;
		value = (value << 8) | (uint32_t)mData[mOffset/8];
		mOffset += 8;
		if (NULL == cJSON_AddNumberToObject(mJsonObj, key, (double)value))
		{
			AAMPLOG_WARN("Failed to add JSON Number");
		}
	}

	return value;
}

uint32_t SCTE35Decoder::CRC32()
{
	uint32_t value;
	uint32_t len;
	uint32_t crc32;
	uint32_t got;

	/* The CRC32 for the whole data, including the stored CRC32 value, should be
	 * zero.
	 */
	value = Integer("CRC32");
	len = (uint32_t)mMaxOffset/8;
	crc32 = aamp_ComputeCRC32((const uint8_t *)mData, len);
	if (crc32 != 0)
	{
		/* On error, calculate the expected CRC32 value. */
		got = aamp_ComputeCRC32((const uint8_t *)mData, len - 4);
		AAMPLOG_WARN("CRC32 mismatch got 0x%" PRIx32 " expected 0x%" PRIx32, got, value);
		throw SCTE35DataException("CRC32 mismatch");
	}

	return value;
}

uint64_t SCTE35Decoder::Bits(const char *key, int bits)
{
	uint64_t value;
	int bit;
	uint8_t mask;

	if ((mOffset + bits) > mMaxOffset)
	{
		throw SCTE35DataException(std::string(key) + " overflow");
	}
	else
	{
		value = 0;
		for (bit = 0; bit < bits; bit++)
		{
			mask = 0x80 >> (mOffset & 7);
			if ((mData[mOffset/8] & mask) == mask)
			{
				value = (value << 1) | 1;
			}
			else
			{
				value = value << 1;
			}
			mOffset++;
		}

		/* Note that this conversion may lose bits. */
		if (NULL == cJSON_AddNumberToObject(mJsonObj, key, (double)value))
		{
			AAMPLOG_WARN("Failed to add JSON Number");
		}
	}

	return value;
}

void SCTE35Decoder::ReservedBits(int bits)
{
	int bit;
	uint8_t mask;

	if ((mOffset + bits) > mMaxOffset)
	{
		throw SCTE35DataException(std::string("reserved overflow"));
	}
	else
	{
		for (bit = 0; bit < bits; bit++)
		{
			mask = 0x80 >> (mOffset & 7);
			if ((mData[mOffset/8] & mask) == 0)
			{
				throw SCTE35DataException(std::string("reserved bit zero"));
			}
			mOffset++;
		}
	}
}

void SCTE35Decoder::SkipBytes(int bytes)
{
	if (mOffset & 7)
	{
		throw SCTE35DataException("skipped bytes not byte aligned");
	}
	else if ((mOffset + (bytes*8)) > mMaxOffset)
	{
		throw SCTE35DataException("skipped bytes overflow");
	}
	else
	{
		mOffset += bytes*8;
	}
}

std::string SCTE35Decoder::String(const char *key, int bytes)
{
	std::string value;

	if (mOffset & 7)
	{
		throw SCTE35DataException(std::string(key) + " not byte aligned");
	}
	else if ((mOffset + (bytes*8)) > mMaxOffset)
	{
		throw SCTE35DataException(std::string(key) + " overflow");
	}
	else if (bytes <= 0)
	{
		if (NULL == cJSON_AddStringToObject(mJsonObj, key, ""))
		{
			AAMPLOG_WARN("Failed to add JSON String");
		}
	}
	else
	{
		value = std::string((char *)&mData[mOffset/8], bytes);
		mOffset += bytes*8;
		if (NULL == cJSON_AddStringToObject(mJsonObj, key, value.c_str()))
		{
			AAMPLOG_WARN("Failed to add JSON String");
		}
	}

	return value;
}

void SCTE35Decoder::End()
{
	if (mParent)
	{
		/* At the end of a subsection, update the parent's bit offset. */
		mParent->mOffset = mOffset;

		if (mLoop)
		{
			/* Add this descriptor to the descriptor loop. */
			/* Ownership is transferred to the loop object. */
			mLoop->add(mJsonObj);
			mJsonObj = NULL;
		}
		else
		{
			if (mOffset != mMaxOffset)
			{
				AAMPLOG_WARN("SCTE-35 %s data underflow", mKey.c_str());
				throw SCTE35DataException("Underflow");
			}

			/* Add this subsection object to the parent. Ownership is
			 * transferred to the parent object.
			 */
			if (false == cJSON_AddItemToObject(mParent->mJsonObj, mKey.c_str(), mJsonObj))
			{
				AAMPLOG_WARN("Failed to add JSON item");
			}
			mJsonObj = NULL;
		}
	}
	else
	{
		if (mOffset != mMaxOffset)
		{
			AAMPLOG_WARN("SCTE-35 section data underflow");
			throw SCTE35DataException("Underflow");
		}
	}
}

cJSON *SCTE35Decoder::getJson()
{
	/* Ownership is transferred to the caller. */
	cJSON *value = mJsonObj;
	mJsonObj = NULL;
	return value;
}

void SCTE35Decoder::addDescriptors(const char *key, cJSON *objects, size_t expectedOffset)
{
	if (mOffset != expectedOffset)
	{
		throw SCTE35DataException(std::string(key) + " underflow");
	}

	if (false == cJSON_AddItemToObject(mJsonObj, key, objects))
	{
		AAMPLOG_WARN("Failed to add JSON item");
	}
}

bool SCTE35Decoder::checkOffset(size_t mMaxOffset)
{
	return mOffset < mMaxOffset;
}

bool SCTE35Decoder::isEnd()
{
	return (mOffset == mMaxOffset);
}

SCTE35DecoderDescriptorLoop::SCTE35DecoderDescriptorLoop(const std::string &key, SCTE35Decoder *decoder, size_t maxOffset) :
														 mKey(key),
														 mDecoder(decoder),
														 mMaxOffset(maxOffset),
														 mObjects(cJSON_CreateArray())
{
}

SCTE35DecoderDescriptorLoop::~SCTE35DecoderDescriptorLoop()
{
	cJSON_Delete(mObjects);
}

bool SCTE35DecoderDescriptorLoop::hasAnotherDescriptor()
{
	return mDecoder->checkOffset(mMaxOffset);
}

void SCTE35DecoderDescriptorLoop::add(cJSON *json)
{
	if (false == cJSON_AddItemToArray(mObjects, json))
	{
		AAMPLOG_WARN("Failed to add JSON Item");
	}
}

SCTE35Section *SCTE35DecoderDescriptorLoop::Descriptor()
{
	if (!hasAnotherDescriptor())
	{
		throw SCTE35DataException(mKey + " overflow");
	}

	return new SCTE35Decoder(mDecoder, this, mMaxOffset);
}

void SCTE35DecoderDescriptorLoop::End()
{
	mDecoder->addDescriptors(mKey.c_str(), mObjects, mMaxOffset);
	mObjects = NULL;
}

/**
 * @brief Parse segmentation upids
 *
 * @param[in] section SCTE-35 data section
 * @throw SCTE35DataException on error
 */
static void SCTE35ParseUpids(SCTE35Section *section)
{
	uint8_t segmentation_upid_type = section->Byte("segmentation_upid_type");
	uint8_t segmentation_upid_length = section->Byte("segmentation_upid_length");

	if (segmentation_upid_length)
	{
		switch (segmentation_upid_type)
		{
			case 0x0d:
			{
				// MID
				std::unique_ptr<SCTE35DescriptorLoop> descriptorLoop(section->DescriptorLoop("MID", segmentation_upid_length));
				while (descriptorLoop->hasAnotherDescriptor())
				{
					std::unique_ptr<SCTE35Section> descriptor(descriptorLoop->Descriptor());
					SCTE35ParseUpids(descriptor.get());
					descriptor->End();
				}
				descriptorLoop->End();
				break;
			}
			case 0x0e:
				// ADS information.
				section->String("ADS", segmentation_upid_length);
				break;
			case 0x0f:
				// URI
				section->String("URI", segmentation_upid_length);
				break;
			default:
				AAMPLOG_INFO("Ignoring segmentation upid type 0x%x", segmentation_upid_type);
				section->SkipBytes(segmentation_upid_length);
				break;
		}
	}
}

/**
 * @brief Parse SCTE-35 splice signal section data
 *
 * @param[in] section Section data
 * @throw SCTE35DataException on error or unsupported format
 */
static void SCTE35ParseSection(SCTE35Section *section)
{
	uint8_t splice_command_type;
	int splice_command_length;
	std::unique_ptr<SCTE35Section> splice_command;
	int descriptor_loop_length;
	std::unique_ptr<SCTE35DescriptorLoop> descriptorLoop;
	std::unique_ptr<SCTE35Section> descriptor;
	bool segment_duration_flag;
	uint8_t table_id;
	uint8_t descriptor_tag;
	int descriptor_length;
	int segmentation_type_id;

	table_id = section->Byte("table_id");
	if (table_id != 0xfc)
	{
		AAMPLOG_WARN("Unexpected SCTE-35 table_id %x, expected 0xfc", table_id);
		throw SCTE35DataException(std::string("Unexpected SCTE-35 table id"));
	}
	(void)section->Bool("section_syntax_indicator");
	(void)section->Bool("private_indicator");
	(void)section->Bits("sap_type", 2);
	(void)section->Bits("section_length", 12);
	(void)section->Byte("protocol_version");
	(void)section->Bool("encrypted_packet");
	(void)section->Bits("encryption_algorithm", 6);
	(void)section->Bits("pts_adjustment", 33);
	(void)section->Byte("cw_index");
	(void)section->Bits("tier", 12);

	splice_command_length = (int)section->Bits("splice_command_length", 12);
	splice_command_type = section->Byte("splice_command_type");
	splice_command.reset(section->Subsection("splice_command", splice_command_length));
	if (splice_command_type == 0x06)
	{
		// time_signal
		if (splice_command->Bool("time_specified_flag"))
		{
			splice_command->ReservedBits(6);
			(void)splice_command->Bits("pts_time", 33);
		}
		else
		{
			(void)splice_command->ReservedBits(7);
		}
	}
	else
	{
		AAMPLOG_INFO("Ignoring unsupported SCTE-35 splice command type 0x%x", splice_command_type);
		splice_command->SkipBytes(splice_command_length);
	}
	splice_command->End();

	descriptor_loop_length = section->Short("descriptor_loop_length");
	descriptorLoop.reset(section->DescriptorLoop("descriptors", descriptor_loop_length));
	while (descriptorLoop->hasAnotherDescriptor())
	{
		descriptor.reset(descriptorLoop->Descriptor());
		descriptor_tag = descriptor->Byte("splice_descriptor_tag");
		descriptor_length = descriptor->Byte("descriptor_length");
		if (descriptor_tag == 0x02)
		{
			// segmentation_descriptor
			(void)descriptor->Integer("identifier");
			(void)descriptor->Integer("segmentation_event_id");
			if (!descriptor->Bool("segmentation_event_cancel_indicator"))
			{
				descriptor->ReservedBits(7);
				if (!descriptor->Bool("program_segmentation_flag"))
				{
					AAMPLOG_WARN("SCTE-35 program_segmentation_flag zero not supported");
					throw SCTE35DataException(std::string("program_segmentation_flag zero not supported"));
				}

				segment_duration_flag = descriptor->Bool("segmentation_duration_flag");
				if (!descriptor->Bool("delivery_not_restricted_flag"))
				{
					(void)descriptor->Bool("web_delivery_allowed_flag");
					(void)descriptor->Bool("no_regional_blackout_flag");
					(void)descriptor->Bool("archive_allowed_flag");
					(void)descriptor->Bits("device_restrictions", 2);
				}
				else
				{
					descriptor->ReservedBits(5);
				}

				// program_segmentation_flag not supported.

				if (segment_duration_flag)
				{
					(void)descriptor->Bits("segmentation_duration", 40);
				}

				SCTE35ParseUpids(descriptor.get());

				segmentation_type_id = descriptor->Byte("segmentation_type_id");
				(void)descriptor->Byte("segment_num");
				(void)descriptor->Byte("segments_expected");
				// sub_segment_num and sub_segments_expected are optional.
				// Check if the descriptor is not at the end, before reading them to avoid throwing error
				if (!descriptor->isEnd())
				{
					switch (segmentation_type_id)
					{
						case 0x34:
						case 0x36:
						case 0x38:
						case 0x3a:
							(void)descriptor->Byte("sub_segment_num");
							(void)descriptor->Byte("sub_segments_expected");
							break;
						default:
							break;
					}
				}
			}
			else
			{
				descriptor->ReservedBits(7);
			}
		}
		else
		{
			AAMPLOG_INFO("Ignoring SCTE-35 descriptor tag %x", descriptor_tag);
			descriptor->SkipBytes(descriptor_length);
		}
		descriptor->End();
	}

	descriptorLoop->End();
	(void)section->CRC32();
	section->End();
}

SCTE35SpliceInfo::SCTE35SpliceInfo(const std::string &string) :mJsonObj(NULL)
{
	
	try
	{
		std::unique_ptr<SCTE35Decoder> decoder(new SCTE35Decoder(string));
		SCTE35ParseSection(decoder.get());
		mJsonObj = decoder->getJson();
	}
	catch(SCTE35DataException &e)
	{
		AAMPLOG_ERR("Failed to decode SCTE-35 data - %s", e.what());
	}
	catch(const std::exception &e)
	{
		AAMPLOG_ERR("Failed to decode SCTE-35 data - %s", e.what());
	}
}

SCTE35SpliceInfo::~SCTE35SpliceInfo()
{
	cJSON_Delete(mJsonObj);
}

std::string SCTE35SpliceInfo::getJsonString(bool formatted)
{
	std::string value;
	char *cstr;

	if (mJsonObj)
	{
		if (formatted)
		{
			cstr = cJSON_Print(mJsonObj);
		}
		else
		{
			cstr = cJSON_PrintUnformatted(mJsonObj);
		}

		if (cstr)
		{
			value = std::string(cstr);
			cJSON_free(cstr);
		}
	}

	return value;
}

void SCTE35SpliceInfo::getSummary(std::vector<SCTE35SpliceInfo::Summary> &summary)
{
	int numberOfDescriptors;
	int idx;
	cJSON *obj;
	cJSON *command;
	cJSON *descriptors;
	cJSON *descriptor;
	double pts_adjustment = 0.0;
	SCTE35SpliceInfo::Summary splice;

	summary.clear();

	descriptors = cJSON_GetObjectItem(mJsonObj, "descriptors");
	if (descriptors)
	{
		numberOfDescriptors = cJSON_GetArraySize(descriptors);
		for (idx = 0; idx < numberOfDescriptors; idx++)
		{
			descriptor = cJSON_GetArrayItem(descriptors, idx);
			if (descriptor)
			{
				/* Set default values. */
				splice.type = SCTE35SpliceInfo::SEGMENTATION_TYPE::NOT_INDICATED;
				splice.time = 0.0;
				splice.duration = 0.0;
				splice.event_id = 0;

				obj = cJSON_GetObjectItem(mJsonObj, "pts_adjustment");
				if (obj)
				{
					pts_adjustment = cJSON_GetNumberValue(obj);
				}

				command = cJSON_GetObjectItem(mJsonObj, "splice_command");
				if (command)
				{
					obj = cJSON_GetObjectItem(command, "pts_time");
					if (obj)
					{
						splice.time = fmod((pts_adjustment + cJSON_GetNumberValue(obj))/TIMESCALE, PTS_WRAP_TIME);
					}
				}

				obj = cJSON_GetObjectItem(descriptor, "segmentation_type_id");
				if (obj)
				{
					splice.type = static_cast<SEGMENTATION_TYPE>(cJSON_GetNumberValue(obj));
				}

				obj = cJSON_GetObjectItem(descriptor, "segmentation_duration");
				if (obj)
				{
					splice.duration = cJSON_GetNumberValue(obj)/TIMESCALE;
				}

				obj = cJSON_GetObjectItem(descriptor, "segmentation_event_id");
				if (obj)
				{
					splice.event_id = cJSON_GetNumberValue(obj);
				}

				summary.push_back(splice);
			}
		}
	}
}
