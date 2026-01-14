
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
//constexpr size_t min_id3_header_length = 3u;
//constexpr size_t id3v2_header_size = 10u;

bool IsValidMediaType(AampMediaType)
{
	return true;
}

bool IsValidHeader(const uint8_t*, size_t)
{
	return true;
}

size_t DataSize(const uint8_t*)
{
	return 0;
}

std::string ToString(const uint8_t*, size_t)
{
	std::string out{};
	return out;
}

} // namespace helpers

MetadataCache::MetadataCache() : mCache{}
{
}

MetadataCache::~MetadataCache()
{
}

void MetadataCache::Reset()
{
}

bool MetadataCache::CheckNewMetadata(AampMediaType mediaType, const std::vector<uint8_t>& data) const
{
	return false;
}

void MetadataCache::UpdateMetadataCache(AampMediaType mediaType, std::vector<uint8_t> data)
{
}

} // namespace id3_metadata
} // namespace aamp
