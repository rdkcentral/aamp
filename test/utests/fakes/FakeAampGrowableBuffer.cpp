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

MockAampGrowableBuffer *g_mockAampGrowableBuffer;

// Flag to enable copying behavior for tests that need it
static bool g_enableMemoryCopying = false;

void AampGrowableBuffer_EnableMemoryCopying(bool enable)
{
	g_enableMemoryCopying = enable;
}

AampGrowableBuffer::~AampGrowableBuffer( void )
{
	if (g_mockAampGrowableBuffer)
	{
		g_mockAampGrowableBuffer->dtor();
	}
	// Only clean up if we allocated the memory ourselves
	if (g_enableMemoryCopying && this->ptr && this->len > 0) {
		delete[] static_cast<char*>(this->ptr);
	}
}

/**
 * @brief release any resource associated with AampGrowableBuffer, resetting back to constructed state
 */
void AampGrowableBuffer::Free( void )
{
	// Only clean up if we allocated the memory ourselves
	if (g_enableMemoryCopying && this->ptr && this->len > 0) {
		delete[] static_cast<char*>(this->ptr);
	}
	
	// Reset to default state
	this->ptr = nullptr;
	this->len = 0;
	this->avail = 0;
}

void AampGrowableBuffer::ReserveBytes( size_t numBytes )
{
	if (g_enableMemoryCopying) {
		// Clean up existing buffer if any
		if (this->ptr && this->len > 0) {
			delete[] static_cast<char*>(this->ptr);
		}
		
		// Allocate new buffer if needed
		if (numBytes > 0) {
			this->ptr = new char[numBytes];
			this->avail = numBytes;
		} else {
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
		// New behavior: Actually copy data for tests that need proper copying
		if (this->ptr == nullptr) {
			// First append - allocate buffer and copy
			this->len = srcLen;
			if (srcLen > 0) {
				char* newBuffer = new char[srcLen];
				if (srcPtr) {
					std::memcpy(newBuffer, srcPtr, srcLen);
				}
				this->ptr = newBuffer;
			}
		} else {
			// Subsequent append - extend buffer
			size_t oldLen = this->len;
			size_t newLen = oldLen + srcLen;
			char* newBuffer = new char[newLen];
			
			// Copy old data
			if (oldLen > 0) {
				std::memcpy(newBuffer, this->ptr, oldLen);
			}
			
			// Append new data
			if (srcPtr && srcLen > 0) {
				std::memcpy(newBuffer + oldLen, srcPtr, srcLen);
			}
			
			// Clean up old buffer if it was allocated
			if (this->ptr && this->len > 0) {
				delete[] static_cast<char*>(this->ptr);
			}
			
			this->ptr = newBuffer;
			this->len = newLen;
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
		this->len = 0;
		// Don't free the buffer, just reset length
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
