/*
* If not stated otherwise in this file or this component's license file the
* following copyright and licenses apply:
*
* Copyright 2022 RDK Management
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

#include "MockAampGrowableBuffer.h"
#include <cstdlib>
#include <cstring>
#include <vector>
#include <unordered_map>

MockAampGrowableBuffer *g_mockAampGrowableBuffer;

// Flag to enable realistic appending behavior matching real implementation.
// When false: simple mode — AppendBytes clears then sets (fast, for test compatibility).
// When true: realistic mode — AppendBytes accumulates data (matches real implementation).
static bool g_enableMemoryCopying = false;

void AampGrowableBuffer_EnableMemoryCopying(bool enable)
{
	g_enableMemoryCopying = enable;
}

void AampGrowableBuffer_ClearGlobalStorage()
{
	// No global storage needed with std::vector implementation
}

AampGrowableBuffer::~AampGrowableBuffer( void )
{
	if (g_mockAampGrowableBuffer)
	{
		g_mockAampGrowableBuffer->dtor();
	}
	Free();
}

/**
 * @brief release any resource associated with AampGrowableBuffer, resetting back to constructed state
 */
void AampGrowableBuffer::Free( void )
{
	buffer.clear();
	buffer.shrink_to_fit();
}

void AampGrowableBuffer::ReserveBytes( size_t numBytes )
{
	if( numBytes == 0 )
	{
		return;
	}

	buffer.reserve(numBytes);
}

void AampGrowableBuffer::AppendBytes( const void *srcPtr, size_t srcLen )
{
	if( srcLen == 0 )
	{
		return;
	}

	const uint8_t* bytes = static_cast<const uint8_t*>(srcPtr);

	if (g_enableMemoryCopying)
	{
		// Realistic mode: match real implementation growth strategy
		size_t required = buffer.size() + srcLen;

		if( buffer.capacity() < required )
		{
			size_t newCapacity = buffer.capacity() * 2;
			if( newCapacity < required )
			{
				newCapacity = required * 2;
			}

			buffer.reserve(newCapacity);
		}

		buffer.insert(buffer.end(), bytes, bytes + srcLen);
	}
	else
	{
		// Simple mode: clear then set (mimics old ptr= behavior for test compatibility)
		buffer.clear();
		buffer.insert(buffer.end(), bytes, bytes + srcLen);
	}
}

void AampGrowableBuffer::Replace( AampGrowableBuffer *src )
{
	buffer = std::move(src->buffer);
	src->buffer.clear();
	src->buffer.shrink_to_fit();
}

std::vector<uint8_t> AampGrowableBuffer::ExtractVector( void )
{
	std::vector<uint8_t> extracted(std::move(buffer));
	buffer.clear();
	buffer.shrink_to_fit();

	return extracted;
}
