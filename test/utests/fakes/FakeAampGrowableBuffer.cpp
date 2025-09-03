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

MockAampGrowableBuffer *g_mockAampGrowableBuffer;

AampGrowableBuffer::~AampGrowableBuffer( void )
{
	if (g_mockAampGrowableBuffer)
	{
		g_mockAampGrowableBuffer->dtor();
	}
}

/**
 * @brief release any resource associated with AampGrowableBuffer, resetting back to constructed state
 */
void AampGrowableBuffer::Free( void )
{
}

void AampGrowableBuffer::ReserveBytes( size_t numBytes )
{
	this->avail = numBytes;
}

void AampGrowableBuffer::AppendBytes( const void *srcPtr, size_t srcLen )
{
	this->ptr = (void*)srcPtr;
	this->len = srcLen;
}

void AampGrowableBuffer::MoveBytes( const void *ptr, size_t len )
{
}

void AampGrowableBuffer::Clear( void )
{
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
