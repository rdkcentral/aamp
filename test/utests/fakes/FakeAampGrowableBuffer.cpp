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

// Static members
bool AampGrowableBuffer::gbEnableLogging = false;
int AampGrowableBuffer::gNetMemoryCount = 0;
int AampGrowableBuffer::gNetMemoryHighWatermark = 0;

// Flag to enable realistic copying behavior matching real implementation
// When false: simple mode (fast, relaxed memory tracking)
// When true: realistic mode (matches real implementation exactly)
static bool g_enableMemoryCopying = false;

void AampGrowableBuffer_EnableMemoryCopying(bool enable)
{
	g_enableMemoryCopying = enable;
}

void AampGrowableBuffer_ClearGlobalStorage()
{
	// No global storage needed with std::vector implementation
}

void AampGrowableBuffer::EnableLogging( bool enable )
{
	gbEnableLogging = enable;
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
	if (g_enableMemoryCopying)
	{
		// Realistic mode: track memory
		if( !buffer.empty() )
		{
			NETMEMORY_MINUS();
			if( gbEnableLogging )
			{
				printf("AampGrowableBuffer::%s(%s:%d)\n", "Free", name, gNetMemoryCount);
			}
		}
	}
	
	// Always clear buffer
	buffer.clear();
	buffer.shrink_to_fit();
}

void AampGrowableBuffer::ReserveBytes( size_t numBytes )
{
	if( numBytes == 0 )
	{
		return;
	}

	if (g_enableMemoryCopying)
	{
		// Realistic mode: track first allocation like real implementation
		bool isFirstAllocation = buffer.empty() && buffer.capacity() == 0;
		
		buffer.reserve(numBytes);
		
		if( isFirstAllocation && buffer.capacity() > 0 )
		{
			NETMEMORY_PLUS();
			if( gbEnableLogging )
			{
				printf("AampGrowableBuffer::%s(%s:%d)\n", "ReserveBytes", name, gNetMemoryCount);
			}
		}
	}
	else
	{
		// Simple mode: just reserve, no memory tracking
		buffer.reserve(numBytes);
	}
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
		// Realistic mode: match real implementation exactly
		bool isFirstAllocation = buffer.empty() && buffer.capacity() == 0;
		size_t required = buffer.size() + srcLen;
		
		if( buffer.capacity() < required )
		{
			size_t newCapacity = buffer.capacity() * 2;
			if( newCapacity < required )
			{
				newCapacity = required * 2;
			}
			
			buffer.reserve(newCapacity);
			
			if( isFirstAllocation )
			{
				NETMEMORY_PLUS();
				if( gbEnableLogging )
				{
					printf("AampGrowableBuffer::%s(%s:%d)\n", "AppendBytes", name, gNetMemoryCount);
				}
			}
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

void AampGrowableBuffer::clear( void )
{
	buffer.clear();
}

void AampGrowableBuffer::Replace( AampGrowableBuffer *src )
{
	buffer = std::move(src->buffer);
	src->buffer.clear();
	src->buffer.shrink_to_fit();
}

std::vector<uint8_t> AampGrowableBuffer::ExtractVector( void )
{
	if (g_enableMemoryCopying)
	{
		// Realistic mode: track memory
		if( !buffer.empty() )
		{
			NETMEMORY_MINUS();
			if( gbEnableLogging )
			{
				printf("AampGrowableBuffer::%s(%s:%d)\n", "ExtractVector", name, gNetMemoryCount);
			}
		}
	}
	
	std::vector<uint8_t> extracted(std::move(buffer));
	buffer.clear();
	buffer.shrink_to_fit();
	
	return extracted; // RVO/NRVO will optimize this
}
