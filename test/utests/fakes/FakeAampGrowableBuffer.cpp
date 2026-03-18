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

// Flag to enable copying behavior for tests that need it
static bool g_enableMemoryCopying = false;

// Storage for buffer data using vectors - more memory safe than raw pointers
static std::unordered_map<void*, std::vector<char>> g_bufferStorage;

void AampGrowableBuffer_EnableMemoryCopying(bool enable)
{
	g_enableMemoryCopying = enable;
}

void AampGrowableBuffer_ClearGlobalStorage()
{
	g_bufferStorage.clear();
}

AampGrowableBuffer::~AampGrowableBuffer( void )
{
	if (g_mockAampGrowableBuffer)
	{
		g_mockAampGrowableBuffer->dtor();
	}
	// Clean up vector storage if we allocated it
	if (g_enableMemoryCopying && this->ptr) {
		auto it = g_bufferStorage.find(this->ptr);
		if (it != g_bufferStorage.end()) {
			g_bufferStorage.erase(it);
		}
	}
}

/**
 * @brief release any resource associated with AampGrowableBuffer, resetting back to constructed state
 */
void AampGrowableBuffer::Free( void )
{
	// Clean up vector storage if we allocated it
	if (g_enableMemoryCopying && this->ptr) {
		auto it = g_bufferStorage.find(this->ptr);
		if (it != g_bufferStorage.end()) {
			g_bufferStorage.erase(it);
		}
	}
	
	// Reset to default state
	this->ptr = nullptr;
	this->len = 0;
	this->avail = 0;
}

void AampGrowableBuffer::ReserveBytes( size_t numBytes )
{
	if (g_enableMemoryCopying) {
		// Reserve new buffer if needed
		if (numBytes > 0) {
			auto& buffer = g_bufferStorage[this];
			buffer.reserve(numBytes);
			this->ptr = buffer.data();
			this->avail = numBytes;
		} else {
			// Clear existing buffer
			auto it = g_bufferStorage.find(this);
			if (it != g_bufferStorage.end()) {
				g_bufferStorage.erase(it);
			}
			this->ptr = nullptr;
			this->avail = 0;
		}
		// Don't change len - ReserveBytes only affects available capacity
	} else {
		// Old behavior for backward compatibility
		this->avail = numBytes;
	}
}

void AampGrowableBuffer::AppendBytes( const void *srcPtr, size_t srcLen )
{
	if (g_enableMemoryCopying) {
		if (srcPtr && srcLen > 0) {
			// Find or create buffer for this AampGrowableBuffer instance
			auto& buffer = g_bufferStorage[this];
			size_t oldLen = buffer.size();
			buffer.resize(oldLen + srcLen);
			
			// Append new data
			std::memcpy(buffer.data() + oldLen, srcPtr, srcLen);
			
			// Update ptr and len
			this->ptr = buffer.data();
			this->len = buffer.size();
		}
	} else {
		// Old behavior for backward compatibility
		this->ptr = (void*)srcPtr;
		this->len = srcLen;
	}
}

void AampGrowableBuffer::MoveBytes( const void *ptr, size_t len )
{
}

void AampGrowableBuffer::Clear( void )
{
	if (g_enableMemoryCopying) {
		// Clear should reset length but keep allocated capacity
		auto it = g_bufferStorage.find(this);
		if (it != g_bufferStorage.end()) {
			it->second.clear(); // Clear vector content but keep capacity
			this->ptr = it->second.data(); // Update pointer (may be nullptr now)
		}
		this->len = 0;
	}
}

void AampGrowableBuffer::Replace( AampGrowableBuffer *src )
{
}

void AampGrowableBuffer::Transfer( void )
{
}

void AampGrowableBuffer::EnableLogging( bool enable )
{
}
