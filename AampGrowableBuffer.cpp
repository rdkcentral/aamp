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

AampGrowableBuffer::~AampGrowableBuffer( void )
{
	Free();
}

/**
 * @brief release any resource associated with AampGrowableBuffer, resetting back to constructed state
 */
void AampGrowableBuffer::Free( void )
{
	buffer.clear();
	buffer.shrink_to_fit();  // Release the allocated memory
}

bool AampGrowableBuffer::ReserveBytes( size_t numBytes )
{
	if( !buffer.empty() || buffer.capacity() != 0 )
	{
		AAMPLOG_ERR("ReserveBytes called on non-empty buffer (size=%zu capacity=%zu); ignoring", buffer.size(), buffer.capacity());
		return false;
	}
	if( numBytes > 0 )
	{
		try {
			buffer.reserve(numBytes);
		}
		catch (const std::exception &e)
		{
			AAMPLOG_ERR("Memory allocation failed!! Requested capacity: %zu (%s)", numBytes, e.what());
			return false;
		}
	}
	return true;
}

bool AampGrowableBuffer::AppendBytes( const void *srcPtr, size_t srcLen )
{
	if( srcLen == 0 )
	{
		return true;
	}

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
		}
		catch (const std::exception &e)
		{
			AAMPLOG_ERR("Memory re-allocation failed!! Requested capacity: %zu (%s)", newCapacity, e.what());
			return false;
		}
	}

	// Append the data (reserve guarantees this won't throw or reallocate)
	const uint8_t* bytes = static_cast<const uint8_t*>(srcPtr);
	buffer.insert(buffer.end(), bytes, bytes + srcLen);
	return true;
}

/**
 * @brief transfer content of one AampGrowableBuffer into another
 * @param src AampGrowableBuffer to transfer
 */
void AampGrowableBuffer::Replace( AampGrowableBuffer *src )
{
	buffer = std::move(src->buffer);
	src->buffer.clear();
	src->buffer.shrink_to_fit();
}

/**
 * @brief Extract the internal vector for ownership transfer to external code (e.g., GStreamer)
 * @return vector object (moved) containing the buffer data
 * @note The internal buffer is moved out and the AampGrowableBuffer is reset to known empty state
 */
std::vector<uint8_t> AampGrowableBuffer::ExtractVector( void )
{
	if( buffer.empty() )
	{
		AAMPLOG_ERR("ExtractVector called on empty buffer");

		// Ensure buffer is in a known empty state (release any reserved capacity)
		buffer.clear();
		buffer.shrink_to_fit();

		return std::vector<uint8_t>();
	}
	// Move our data into a temporary vector for return
	std::vector<uint8_t> extracted(std::move(buffer));

	// Explicitly clear to ensure known empty state (not just moved-from state)
	buffer.clear();
	buffer.shrink_to_fit();

	return extracted; // Move constructor will be used for efficient return
}
