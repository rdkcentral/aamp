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

#include "scte35/AampSCTE35.h"

SCTE35Decoder::SCTE35Decoder(std::string string) : mParent(nullptr), mLoop(nullptr),mOffset(0),mMaxOffset(),
													mData(),mJsonObj(),mKey()
{
}

SCTE35Decoder::SCTE35Decoder(SCTE35Decoder *parent, SCTE35DecoderDescriptorLoop *descriptorLoop, size_t loopMaxOffset) : mParent(parent), mLoop(descriptorLoop)
							,mOffset(parent->mOffset),mMaxOffset(loopMaxOffset),mData(parent->mData),mJsonObj(),mKey()
{
}

SCTE35Decoder::SCTE35Decoder(const char *key, SCTE35Decoder *parent, int len) : mParent(parent), mLoop(nullptr), mKey(key)
								,mOffset(parent->mOffset),mMaxOffset(parent->mOffset + (len*8)),mData(parent->mData),mJsonObj()
{
}

SCTE35Decoder::~SCTE35Decoder()
{
}

SCTE35Section *SCTE35Decoder::Subsection(const char *key, int length)
{
	return nullptr;
}

SCTE35DescriptorLoop *SCTE35Decoder::DescriptorLoop(const char *key, int length)
{
	return nullptr;
}

bool SCTE35Decoder::Bool(const char *key)
{
	return false;
}

uint8_t SCTE35Decoder::Byte(const char *key)
{
	return 0;
}

uint16_t SCTE35Decoder::Short(const char *key)
{
	return 0;
}

uint32_t SCTE35Decoder::Integer(const char *key)
{
	return 0;
}

uint32_t SCTE35Decoder::CRC32()
{
    return 0;
}

uint64_t SCTE35Decoder::Bits(const char *key, int bits)
{
	return 0;
}

void SCTE35Decoder::ReservedBits(int bits)
{
}

void SCTE35Decoder::SkipBytes(int bytes)
{
}

std::string SCTE35Decoder::String(const char *key, int bytes)
{
    return std::string();
}

void SCTE35Decoder::End()
{
}

cJSON *SCTE35Decoder::getJson()
{
    return nullptr;
}

void SCTE35Decoder::addDescriptors(const char *key, cJSON *objects, size_t expectedOffset)
{
}

bool SCTE35Decoder::checkOffset(size_t mMaxOffset)
{
	return false;
}

bool SCTE35Decoder::isEnd()
{
	return false;
}

SCTE35DecoderDescriptorLoop::SCTE35DecoderDescriptorLoop(const std::string &key, SCTE35Decoder *decoder, size_t maxOffset) :
														 mKey(key),
														 mDecoder(decoder),
														 mMaxOffset(maxOffset),
														 mObjects()
{
}

SCTE35DecoderDescriptorLoop::~SCTE35DecoderDescriptorLoop()
{
}

bool SCTE35DecoderDescriptorLoop::hasAnotherDescriptor()
{
	return false;
}

void SCTE35DecoderDescriptorLoop::add(cJSON *json)
{
}

SCTE35Section *SCTE35DecoderDescriptorLoop::Descriptor()
{
    return nullptr;
}

void SCTE35DecoderDescriptorLoop::End()
{
}

SCTE35SpliceInfo::SCTE35SpliceInfo(const std::string &string) :mJsonObj(NULL)
{
}

SCTE35SpliceInfo::~SCTE35SpliceInfo()
{
}

std::string SCTE35SpliceInfo::getJsonString(bool formatted)
{
	return std::string();
}

void SCTE35SpliceInfo::getSummary(std::vector<SCTE35SpliceInfo::Summary> &summary)
{
}
