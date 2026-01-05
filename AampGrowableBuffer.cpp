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
	if( !buffer.empty() )
	{
		NETMEMORY_MINUS();
		if( gbEnableLogging )
		{
			printf("AampGrowableBuffer::%s(%s:%d)\n", "Free",name,gNetMemoryCount);
		}
		buffer.clear();
		buffer.shrink_to_fit();  // Release the allocated memory
	}
}

void AampGrowableBuffer::ReserveBytes( size_t numBytes )
{
	assert( buffer.empty() && buffer.capacity() == 0 );
	if( numBytes > 0 )
	{
		NETMEMORY_PLUS();
		if( gbEnableLogging )
		{
			printf("AampGrowableBuffer::%s(%s:%d)\n", "ReserveBytes",name,gNetMemoryCount);
		}
		buffer.reserve(numBytes);
	}
}

void AampGrowableBuffer::AppendBytes( const void *srcPtr, size_t srcLen )
{
	if( srcLen == 0 )
	{
		return;
	}

	bool isFirstAllocation = buffer.empty() && buffer.capacity() == 0;
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
 * @brief replace contents of AampGrowableBuffer
 * @param srcPtr pointer to memory (may be subset of existing AampGrowableBuffer)
 * @param srcLen new logical size for AampGrowableBuffer reflecting memory being copied/moved
 */
void AampGrowableBuffer::MoveBytes( const void *srcPtr, size_t srcLen ) //DJH is this ever used ? resize before move is probably wrong
{ // this API assumes AampGrowableBuffer is already big enough to fit
	assert( srcPtr && buffer.capacity() >= srcLen );
	const uint8_t* bytes = static_cast<const uint8_t*>(srcPtr);
	// Must resize before writing to buffer.data()
	buffer.resize(srcLen);
	std::memmove( buffer.data(), bytes, srcLen );
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
	assert( buffer.empty() ); // only replace if empty!

	buffer = std::move(src->buffer);

	// Ensure source is in known empty state (not just moved-from)
	src->buffer.clear();
	src->buffer.shrink_to_fit();
}

/**
 * @brief called when internal memory is transferred (i.e. as part of GStreamer injection)
 * @note Returns the buffer data and size via GetPtr/GetLen before calling this.
 *       This method prepares the buffer for external ownership transfer.
 */
void AampGrowableBuffer::Transfer( void )
{
	assert( !buffer.empty() );

	if( !buffer.empty() )
	{
		NETMEMORY_MINUS();
		if( gbEnableLogging )
		{
			printf("AampGrowableBuffer::%s(%s:%d)\n", "Transfer",name,gNetMemoryCount);
		}
	}
	// Clear the buffer - ownership has been transferred
	buffer.clear();
	buffer.shrink_to_fit();
}

/**
 * @brief Extract the internal vector for ownership transfer to external code (e.g., GStreamer)
 * @return pointer to new vector that caller must delete
 * @note The internal buffer is moved out and the AampGrowableBuffer is reset to known empty state
 */
std::vector<uint8_t>* AampGrowableBuffer::ExtractVector( void )
{
	assert( !buffer.empty() );

	if( !buffer.empty() )
	{
		NETMEMORY_MINUS();
		if( gbEnableLogging )
		{
			printf("AampGrowableBuffer::%s(%s:%d)\n", "ExtractVector",name,gNetMemoryCount);
		}
	}

	// Create new vector and move our data into it
	auto* extracted = new std::vector<uint8_t>(std::move(buffer));

	// Explicitly clear to ensure known empty state (not just moved-from state)
	buffer.clear();
	buffer.shrink_to_fit();

	return extracted;
}
