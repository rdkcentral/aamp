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


void AampGrowableBuffer::EnableLogging( bool enable )
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
		AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::Free: this=%p bufferData=%p size=%zu capacity=%zu", 
			this, buffer.data(), buffer.size(), buffer.capacity());
		NETMEMORY_MINUS();
		if( gbEnableLogging )
		{
			printf("AampGrowableBuffer::%s(%s:%d)\n", "Free",name,gNetMemoryCount);
		}
		buffer.clear();
		buffer.shrink_to_fit();  // Release the allocated memory
		AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::Free: AFTER clear/shrink: size=%zu capacity=%zu",
			buffer.size(), buffer.capacity());
	}
	else
	{
		AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::Free: this=%p buffer already empty (size=%zu capacity=%zu)", 
			this, buffer.size(), buffer.capacity());
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
		// CRITICAL FIX: Must resize, not just reserve!
		// TSB Read expects to write directly into the buffer, so we need actual size, not just capacity
		//buffer.resize(numBytes);
		buffer.reserve(numBytes);
		AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::ReserveBytes: this=%p numBytes=%zu AFTER resize: bufferData=%p size=%zu capacity=%zu",
			this, numBytes, buffer.data(), buffer.size(), buffer.capacity());
	}
}

void AampGrowableBuffer::AppendBytes( const void *srcPtr, size_t srcLen )
{
	AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::AppendBytes: this=%p srcPtr=%p srcLen=%zu BEFORE: bufferData=%p size=%zu cap=%zu",
		this, srcPtr, srcLen, buffer.data(), buffer.size(), buffer.capacity());
	
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
	
	AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::AppendBytes: AFTER: this=%p bufferData=%p size=%zu cap=%zu",
		this, buffer.data(), buffer.size(), buffer.capacity());
	
	// Log first 32 bytes of buffer after append
	if (!buffer.empty()) {
		size_t bytesToLog = (buffer.size() < 32) ? buffer.size() : 32;
		char hexStr[97] = {0}; // 32 bytes * 3 chars per byte + null terminator
		for (size_t i = 0; i < bytesToLog; i++) {
			snprintf(hexStr + (i * 3), 4, "%02x ", buffer[i]);
		}
		AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::AppendBytes: First %zu bytes after append: %s",
			bytesToLog, hexStr);
	}
}

/**
 * @brief replace contents of AampGrowableBuffer
 * @param srcPtr pointer to memory (may be subset of existing AampGrowableBuffer)
 * @param srcLen new logical size for AampGrowableBuffer reflecting memory being copied/moved
 */
void AampGrowableBuffer::MoveBytes( const void *srcPtr, size_t srcLen )
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
	
	AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::Replace: this=%p (size=%zu cap=%zu) src=%p (data=%p size=%zu cap=%zu)",
		this, buffer.size(), buffer.capacity(), src, src->buffer.data(), src->buffer.size(), src->buffer.capacity());
	
	buffer = std::move(src->buffer);
	
	AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::Replace: AFTER MOVE: this=%p (data=%p size=%zu cap=%zu)",
		this, buffer.data(), buffer.size(), buffer.capacity());
	
	// Ensure source is in known empty state (not just moved-from)
	src->buffer.clear();
	src->buffer.shrink_to_fit();
	
	AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::Replace: src after clear: %p (size=%zu cap=%zu)",
		src, src->buffer.size(), src->buffer.capacity());
}

/**
 * @brief called when internal memory is transferred (i.e. as part of GStreamer injection)
 * @note Returns the buffer data and size via GetPtr/GetLen before calling this.
 *       This method prepares the buffer for external ownership transfer.
 */
void AampGrowableBuffer::Transfer( void )
{
	assert( !buffer.empty() );
	
	AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::Transfer: this=%p data=%p size=%zu capacity=%zu (ownership transferred)",
		this, buffer.data(), buffer.size(), buffer.capacity());
	
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
	
	AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::Transfer: AFTER clear: this=%p size=%zu capacity=%zu",
		this, buffer.size(), buffer.capacity());
}

/**
 * @brief Extract the internal vector for ownership transfer to external code (e.g., GStreamer)
 * @return pointer to new vector that caller must delete
 * @note The internal buffer is moved out and the AampGrowableBuffer is reset to known empty state
 */
std::vector<uint8_t>* AampGrowableBuffer::ExtractVector( void )
{
	assert( !buffer.empty() );
	
	// DIAGNOSTIC: Log state BEFORE extraction
	AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::ExtractVector: BEFORE: this=%p bufferData=%p size=%zu capacity=%zu",
		this, buffer.data(), buffer.size(), buffer.capacity());
	if (!buffer.empty() && buffer.size() >= 8) {
		AAMPLOG_WARN("[DIAGNOSTIC-VEC] ExtractVector: First 8 bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
			buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
	}
	
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
	
	AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::ExtractVector: AFTER MOVE: extracted=%p extractedData=%p size=%zu capacity=%zu",
		extracted, extracted->data(), extracted->size(), extracted->capacity());
	AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::ExtractVector: AFTER MOVE: this=%p bufferData=%p size=%zu capacity=%zu (moved-from state)",
		this, buffer.data(), buffer.size(), buffer.capacity());
	
	// Explicitly clear to ensure known empty state (not just moved-from state)
	buffer.clear();
	buffer.shrink_to_fit();
	
	AAMPLOG_WARN("[DIAGNOSTIC-VEC] AampGrowableBuffer::ExtractVector: AFTER CLEAR: this=%p size=%zu capacity=%zu",
		this, buffer.size(), buffer.capacity());
	
	return extracted;
}
