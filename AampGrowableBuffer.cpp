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

/**
 * @file AampGrowableBuffer.cpp
 * @brief Implementation file of helper functions for Growable Buffer class
 */

#include "AampGrowableBuffer.h"
#include "AampConfig.h"
#include "AampLogManager.h"
#include <assert.h>

bool AampGrowableBuffer::gbEnableLogging = false;
int AampGrowableBuffer::gNetMemoryCount = 0;
int AampGrowableBuffer::gNetMemoryHighWatermark = 0;

void AampGrowableBuffer::EnableLogging(bool enable)
{
	gbEnableLogging = enable;
}

AampGrowableBuffer::~AampGrowableBuffer( void )
{
	Free();
}

/**
 * @brief release any resource associated with AampGrowableBuffer, resetting back to constructed state
 */
void AampGrowableBuffer::Free( void )
{
	if( buffer.capacity() > 0 )
	{
		NETMEMORY_MINUS();
		if( gbEnableLogging )
		{
			printf("AampGrowableBuffer::%s(%s:%d)\n", "Free",name,gNetMemoryCount);
		}
		buffer.clear();
	}
	buffer.shrink_to_fit();  // Release the allocated memory
}

void AampGrowableBuffer::ReserveBytes( size_t numBytes )
{
	assert( buffer.empty() && buffer.capacity() == 0 );
	if( numBytes > 0 )
	{
		try {
			buffer.reserve(numBytes);
			NETMEMORY_PLUS();
			if( gbEnableLogging )
			{
				printf("AampGrowableBuffer::%s(%s:%d)\n", "ReserveBytes",name,gNetMemoryCount);
			}
		}
		catch (const std::bad_alloc&)
		{
			AAMPLOG_ERR("Memory allocation failed!! Requested capacity: %zu", numBytes);
		}
	}
}

void AampGrowableBuffer::AppendBytes( const void *srcPtr, size_t srcLen )
{
	if( srcLen == 0 )
	{
		return;
	}

	bool isFirstAllocation = buffer.empty() && (buffer.capacity() == 0);
	size_t required = buffer.size() + srcLen;

	if( buffer.capacity() < required )
	{ // more memory needed - grow with same strategy as original implementation
		size_t newCapacity = buffer.capacity() * 2; // first try doubling
		if( newCapacity < required )
		{ // if still not enough, allocate double what's required
			newCapacity = required * 2;
		}

		try
		{
			buffer.reserve(newCapacity);

			if( isFirstAllocation )
			{
				NETMEMORY_PLUS();
				if( gbEnableLogging )
				{
					printf("AampGrowableBuffer::%s(%s:%d)\n", "AppendBytes",name,gNetMemoryCount);
				}
			}
		}
		catch (const std::bad_alloc&)
		{
			AAMPLOG_ERR("Memory re-allocation failed!! Requested capacity: %zu", newCapacity);
			return;
		}
	}

	// Append the data (reserve guarantees this won't throw or reallocate)
	const uint8_t* bytes = static_cast<const uint8_t*>(srcPtr);
	buffer.insert(buffer.end(), bytes, bytes + srcLen);
}

/**
 * @brief reset AampGrowableBuffer logical length without releasing reserved memory
 */
void AampGrowableBuffer::Clear( void )
{
	buffer.clear();
}

/**
 * @brief transfer content of one AampGrowableBuffer into another
 * @param src AampGrowableBuffer to transfer
 */
void AampGrowableBuffer::Replace( AampGrowableBuffer *src )
{
	// Decrement for destination if it has capacity
	if( buffer.capacity() > 0 )
	{
		NETMEMORY_MINUS();
	}

	buffer = std::move(src->buffer);

	// Decrement for source since we're clearing it
	if( src->buffer.capacity() > 0 )  // check capacity after move
	{
		NETMEMORY_MINUS();
	}
	src->buffer.clear();
	src->buffer.shrink_to_fit();

	// Increment for destination which now has the moved buffer
	if( buffer.capacity() > 0 )
	{
		NETMEMORY_PLUS();
	}
}

/**
 * @brief Extract the internal vector for ownership transfer to external code (e.g., GStreamer)
 * @return vector object (moved) containing the buffer data
 * @note The internal buffer is moved out and the AampGrowableBuffer is reset to known empty state
 */
std::vector<uint8_t> AampGrowableBuffer::ExtractVector( void )
{
	assert( !buffer.empty() );

	if( buffer.capacity() > 0 )
	{
		NETMEMORY_MINUS();
		if( gbEnableLogging )
		{
			printf("AampGrowableBuffer::%s(%s:%d)\n", "ExtractVector",name,gNetMemoryCount);
		}
	}

	// Move our data into a temporary vector for return
	std::vector<uint8_t> extracted(std::move(buffer));

	// Explicitly clear to ensure known empty state (not just moved-from state)
	buffer.clear();
	buffer.shrink_to_fit();

	return extracted; // Move constructor will be used for efficient return
}
