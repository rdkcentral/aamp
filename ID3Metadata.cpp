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

#include "ID3Metadata.hpp"

#include <sstream>
#include <iomanip>
#include <cstring>
#include <utility>

namespace aamp
{
namespace id3_metadata
{
namespace helpers
{
constexpr size_t min_id3_header_length = 4u;
constexpr size_t id3v2_header_size = 10u;


bool IsValidMediaType(AampMediaType mediaType)
{
	return mediaType == eMEDIATYPE_AUDIO || mediaType == eMEDIATYPE_VIDEO || mediaType == eMEDIATYPE_DSM_CC;
}

bool IsValidHeader(const uint8_t* data, size_t data_len)
{
	if (data_len >= min_id3_header_length)
	{
		/* Check file identifier ("ID3" = ID3v2) and major revision matches (>= ID3v2.2.x). */
		/* The ID3 header has first three bytes as "ID3" and in the next two bytes first byte is the major version and second byte is its revision number */
		if (*data++ == 'I' && *data++ == 'D' && *data++ == '3' && *data++ >= 2)
		{
			return true;
		}
	}

	return false;
}

size_t DataSize(const uint8_t *data)
{
	size_t bufferSize = 0;
	uint8_t tagSize[4];
	
	std::memcpy(tagSize, data+6, 4);
	
	// bufferSize is encoded as a syncsafe integer - this means that bit 7 is always zero
	// Check for any 1s in bit 7
	if (tagSize[0] > 0x7f || tagSize[1] > 0x7f || tagSize[2] > 0x7f || tagSize[3] > 0x7f)
	{
		// AAMPLOG_WARN("Bad header format");
		return 0;
	}
	
	bufferSize = tagSize[0] << 21;
	bufferSize += tagSize[1] << 14;
	bufferSize += tagSize[2] << 7;
	bufferSize += tagSize[3];
	bufferSize += id3v2_header_size;
	
	return bufferSize;
}

std::string ToString(const uint8_t* data, size_t data_len)
{
	std::string out {};
	std::stringstream ss;
	
	//bool extended_header{false};
	
	// Size - it's the size of the tag/frame excluding the header's size (10 bytes)
	// i.e. the actual data of the tag/frame
	auto get_size = [](const uint8_t * data)
	{
		uint32_t size {0};
		
		size |= (data[0] & 0x7f) << 21;
		size |= (data[1] & 0x7f) << 14;
		size |= (data[2] & 0x7f) << 7;
		size |= data[3] & 0x7f;
		
		return size;
	};
	
	ss << data[0] << data[1] << data[2];    // ID3
	ss << "v" << std::to_string(data[3]) << std::to_string(data[4]) << " hdr: ";   // Revision
	for (auto idx = 5; idx < 10; idx++)
	{
		ss << std::to_string(data[idx]) << " ";
	}
	
	auto frame_parser = [get_size](const uint8_t* frame_ptr) -> std::pair<uint32_t, std::string>
	{
		std::string ret{};
		const auto frame_size = get_size(&frame_ptr[4]);

		// Print frame the header
		{
			const char * data_ptr = reinterpret_cast<const char*>(&frame_ptr[0]);
			const std::string frame_id{data_ptr, 4};
			ret += "- frame: " + frame_id + " [" + std::to_string(frame_size) + "] [";
			for (auto idx = 4; idx < 9; idx++)
			{
				ret += std::to_string(data_ptr[idx]) + " ";
			}
			ret += std::to_string(data_ptr[9]) + "] ";
		}

		// Print frame the content
		{
			const char * data_ptr = reinterpret_cast<const char*>(&frame_ptr[10]);
			const std::string frame_data{data_ptr, frame_size};
			ret += "data: " + frame_data + " ";
		}		

		return {frame_size + 10, ret};
	};
	
	// // Frame (assuming no extended header...)
	// if (extended_header)
	// {}
	// else
	// {
	// }
	
	const uint32_t tag_size = get_size(&data[6]);
	uint32_t parsed_size{10};

	while (parsed_size < tag_size)
	{
		const auto frame_ret = frame_parser(&data[parsed_size]);

		ss << frame_ret.second;
		parsed_size += frame_ret.first;
	}
	
	return ss.str();
}

} // namespace helpers

MetadataCache::MetadataCache()
: mCache{}
{
	Reset();
}

MetadataCache::~MetadataCache()
{
	Reset();
}

void MetadataCache::Reset()
{
	for (auto & e : mCache)
	{
		e.clear();
	}
}

bool MetadataCache::CheckNewMetadata(AampMediaType mediaType, const std::vector<uint8_t> & data) const
{
	const auto & cache = mCache[mediaType];
	return (data != cache);
}

void MetadataCache::UpdateMetadataCache(AampMediaType mediaType, std::vector<uint8_t> data)
{
	auto & cache = mCache[mediaType];
	cache.clear();
	cache = std::move(data);
}


} // namespace id3_metadata
} // namespace aamp
