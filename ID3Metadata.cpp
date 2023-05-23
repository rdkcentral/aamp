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

namespace aamp
{
namespace id3_metadata
{
namespace helpers
{
constexpr size_t min_id3_header_length = 3u;
constexpr size_t id3v2_header_size = 10u;


bool IsValidMediaType(MediaType mediaType)
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
	
	// bufferSize is encoded as a syncsafe integer - this means that bit 7 is always zeroed
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
		
		size |= data[0] << 21;
		size |= data[1] << 14;
		size |= data[2] << 7;
		size |= data[3];
		
		return size;
	};
	
	ss << data[0] << data[1] << data[2];    // ID3
	ss << std::setfill('0') << std::setw(2);
	ss << "v" << std::to_string(data[3]) << std::to_string(data[4]) << " ";   // Revision
	for (auto idx = 5; idx < 10; idx++)
	{
		ss << std::to_string(data[idx]) << " ";
	}
	
	auto frame_parser = [&ss, get_size](const uint8_t* data)
	{
		const auto frame_size = get_size(&data[4]);
		
		ss << " - frame: " << data[0] << data[1] << data[2] << data[3];
		
		for (auto idx = 4; idx < 10; idx++)
		{
			ss << std::to_string(data[idx]) << " ";
		}
		
		return frame_size + 10;
	};
	
	// // Frame (assuming no extended header...)
	// if (extended_header)
	// {}
	// else
	// {
	// }
	
	int64_t unparsed = get_size(&data[6]);
	//uint8_t * data_ptr = const_cast<uint8_t *>(data);
	
	while (unparsed > 0)
	{
		auto parsed_size = frame_parser(data);
		
		unparsed -= parsed_size;
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

bool MetadataCache::CheckNewMetadata(MediaType mediaType, const std::vector<uint8_t> & data) const
{
	const auto & cache = mCache[mediaType];
	return (data != cache);
}

void MetadataCache::UpdateMedatadaCache(MediaType mediaType, std::vector<uint8_t> data)
{
	auto & cache = mCache[mediaType];
	cache.clear();
	cache = std::move(data);
}


} // namespace id3_metadata
} // namespace aamp
