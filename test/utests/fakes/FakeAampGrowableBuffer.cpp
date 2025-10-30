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
	// Simplified: Just reset to default state
	this->ptr = nullptr;
	this->len = 0;
	this->avail = 0;
}

/**
 * @brief release any resource associated with AampGrowableBuffer, resetting back to constructed state
 */
void AampGrowableBuffer::Free( void )
{
	// Simplified: Reset to default state - no complex memory management needed
	this->ptr = nullptr;
	this->len = 0;
	this->avail = 0;
}

void AampGrowableBuffer::ReserveBytes( size_t numBytes )
{
	// Simplified: Just update available capacity - tests don't verify actual allocation
	this->avail = numBytes;
}

void AampGrowableBuffer::AppendBytes( const void *srcPtr, size_t srcLen )
{
	// Simplified: Basic pointer assignment - sufficient for CachedFragment tests
	// Tests focus on CachedFragment behavior, not buffer copying behavior
	if (srcPtr && srcLen > 0) {
		this->ptr = (void*)srcPtr;
		this->len = srcLen;
	}
}

void AampGrowableBuffer::Clear( void )
{
	// Simplified: Reset length to 0, preserve capacity
	this->len = 0;
}

// All methods below are no-op since tests don't use them and they have no effect on test outcomes

void AampGrowableBuffer::MoveBytes( const void *ptr, size_t len )
{
	// No-op: Not used by CachedFragment tests
}

void AampGrowableBuffer::Replace( AampGrowableBuffer *src )
{
	// No-op: Not used by CachedFragment tests  
}

void AampGrowableBuffer::Transfer( void )
{
	// No-op: Not used by CachedFragment tests
}

void AampGrowableBuffer::EnableLogging( bool enable )
{
	// No-op: Not used by CachedFragment tests
}
