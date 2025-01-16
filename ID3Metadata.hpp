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
#include <functional>
#include <cstdint> // for std::uint8_t

namespace aamp
{
namespace id3_metadata
{
namespace helpers
{
/**
 * @brief  Checks if the media type is compatible with an ID3 tag
 * @param mediaType The segment's media type
 * @return True if the media type is valid
 */
bool IsValidMediaType(AampMediaType mediaType);

/** 
 * @brief Checks if the packet's header is a valid ID3 packet's header
 * @param[in] data The packet's data
 * @param[in] data_len The packet's length
 * @return True if the packet has a valid ID3 header
 */ 
bool IsValidHeader(const uint8_t* data, size_t data_len);

/**
 * @brief Extracts from the packet the size of the data
 * @param[in] data The packet's data
 * @return The size of the data in the ID3 packet
*/
size_t DataSize(const uint8_t *data);

/**
 * Converts the content of the ID3 tag into a std::string
 * @param[in] data The packet's data
 * @param[in] data_len The packet's length
 */
std::string ToString(const uint8_t* data, size_t data_len);

} // namespace helpers

/**
 * Class for caching the metadata info
 */
class MetadataCache
{
public:
	/// Default constructor
	MetadataCache();
	
	/// Default destructor
	~MetadataCache();
	
	/// Resets the cache
	void Reset();

	/**
	 * Checks if the given metadata packet is already present in the cache
	 * @param[in] mediaType The packet's media type
	 * @param[in] data The packet's data
	 * @return True if the packet is not present in the cache
	 */	
	bool CheckNewMetadata(AampMediaType mediaType, const std::vector<uint8_t> & data) const;
	
	/**
	 * Updates the cache with the given packet
	 * @param[in] mediaType The packet's media type
	 * @param[in] data The data to insert into the cache
	 */
	void UpdateMetadataCache(AampMediaType mediaType, std::vector<uint8_t> data);
	
private:

	///	Cache of ID3 data, for each media type
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

// Forward declaration of SegmentInfo_t type
class SegmentInfo_t;

/// Signature of function to call to invoke to handle the ID3 metadata
using id3_callback_t = std::function<void (AampMediaType mediaType, const uint8_t * ptr, size_t pkt_len, const SegmentInfo_t & info, const char * scheme_uri)>;

#endif // AAMP_HELPERS_ID3METADATA_HPP
