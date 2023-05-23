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

#ifndef AAMP_HELPERS_ID3METADATA_HPP
#define AAMP_HELPERS_ID3METADATA_HPP

#include "AampMediaType.h"

#include <string>
#include <stdlib.h>
#include <array>
#include <vector>


namespace aamp
{
namespace id3_metadata
{
namespace helpers
{
bool IsValidMediaType(MediaType mediaType);

bool IsValidHeader(const uint8_t* data, size_t data_len);

size_t DataSize(const uint8_t *data);

std::string ToString(const uint8_t* data, size_t data_len);

} // namespace helpers

class MetadataCache
{
public:
	MetadataCache();
	
	~MetadataCache();
	
	void Reset();
	
	bool CheckNewMetadata(MediaType mediaType, const std::vector<uint8_t> & data) const;
	
	void UpdateMedatadaCache(MediaType mediaType, std::vector<uint8_t> data);
	
private:
	
	std::array<std::vector<uint8_t>, eMEDIATYPE_DEFAULT> mCache;
	
};

/**
 * @class Id3CallbackData
 * @brief Holds id3 metadata callback specific variables.
 */
class CallbackData
{
public:
	CallbackData(
				 std::vector<uint8_t> data,
				 const char* schemeIdURI, const char* id3Value, uint64_t presTime,
				 uint32_t id3ID, uint32_t eventDur, uint32_t tScale, uint64_t tStampOffset)
	: mData(std::move(data)), schemeIdUri(), value(), presentationTime(presTime), id(id3ID), eventDuration(eventDur), timeScale(tScale), timestampOffset(tStampOffset)
	{
		if (schemeIdURI)
		{
			schemeIdUri = std::string(schemeIdURI);
		}
		
		if (id3Value)
		{
			value = std::string(id3Value);
		}
	}
	
	CallbackData(
				 const uint8_t* ptr, uint32_t len,
				 const char* schemeIdURI, const char* id3Value, uint64_t presTime,
				 uint32_t id3ID, uint32_t eventDur, uint32_t tScale, uint64_t tStampOffset)
	: mData(), schemeIdUri(), value(), presentationTime(presTime), id(id3ID), eventDuration(eventDur), timeScale(tScale), timestampOffset(tStampOffset)
	{
		mData = std::vector<uint8_t>(ptr, ptr + len);
		
		if (schemeIdURI)
		{
			schemeIdUri = std::string(schemeIdURI);
		}
		
		if (id3Value)
		{
			value = std::string(id3Value);
		}
	}
	
	CallbackData() = delete;
	CallbackData(const CallbackData&) = delete;
	CallbackData& operator=(const CallbackData&) = delete;
	
	std::vector<uint8_t> mData; /**<id3 metadata */
	std::string schemeIdUri;   /**< schemeIduri */
	std::string value;
	uint64_t presentationTime;
	uint32_t id;
	uint32_t eventDuration;
	uint32_t timeScale;
	uint64_t timestampOffset;
};

}// namespace id3_metadata
} // namespace aamp

#endif // AAMP_HELPERS_ID3METADATA_HPP
