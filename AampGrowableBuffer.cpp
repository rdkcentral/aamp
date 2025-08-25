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
 * @file AampGrowableBuffer.h
 * @brief Header file of helper functions for Growable Buffer class
 */

#include "AampGrowableBuffer.h"
#include "AampConfig.h"
#include <assert.h>
#include <glib.h>

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
	if( ptr )
	{
		NETMEMORY_MINUS();
        if( gbEnableLogging )
        {
            printf("AampGrowableBuffer::%s(%s:%d)\n", "Free",name,gNetMemoryCount);
        }
		g_free( ptr );
		ptr = NULL;
	}
	len = 0;
	avail = 0;
}

void AampGrowableBuffer::ReserveBytes( size_t numBytes )
{
	assert( ptr==NULL && avail == 0 );
	ptr = (char *)g_malloc( numBytes );
	if( ptr )
	{
		NETMEMORY_PLUS();
        if( gbEnableLogging )
        {
            printf("AampGrowableBuffer::%s(%s:%d)\n", "ReserveBytes",name,gNetMemoryCount);
        }
		avail = numBytes;
	}
}

void AampGrowableBuffer::AppendBytes( const void *srcPtr, size_t srcLen )
{
	size_t required = len + srcLen;
	if( avail < required )
	{ // more memory needed - grow
		size_t numBytes = avail*2; // first try doubling size of existing reserved memory
		if( numBytes < required )
		{ // if still not enough, reallocate based on required
			numBytes = required*2;
		}
		gpointer mem = g_realloc(ptr, numBytes );
		if( mem )
		{
			if( !ptr )
			{ // first allocation
				NETMEMORY_PLUS();
                if( gbEnableLogging )
                {
                    printf("AampGrowableBuffer::%s(%s:%d)\n", "AppendBytes",name,gNetMemoryCount);
                }
			}
			ptr = mem;
			avail = numBytes;
		}
		else if (numBytes != 0)
		{
			AAMPLOG_ERR("Memory re-allocation failed!! Requested numBytes: %zu", numBytes);
		}
	}
	if( ptr )
	{
		memcpy( len + (char *)ptr, srcPtr, srcLen);
		len = required;
	}
}

/**
 * @brief replace contents of AampGrowableBuffer
 * @param srcPtr pointer to memory (may be subset of existing AampGrowableBuffer)
 * @param srcLen new logical size for AampGrowableBuffer reflecting memory being copied/moved
 */
void AampGrowableBuffer::MoveBytes( const void *srcPtr, size_t srcLen )
{ // this API assumes AampGrowableBuffer is already big enough to fit
	assert( ptr && srcPtr && avail >= srcLen );
	memmove( ptr, srcPtr, srcLen );
	len = srcLen;
}

/**
 * @brief reset AampGrowableBuffer logical length without releasing reserved memory
 */
void AampGrowableBuffer::Clear( void )
{
	len = 0;
}

/**
 * @brief transfer content of one AampGrowableBuffer into another
 * @param src AampGrowableBuffer to transfer
 */
void AampGrowableBuffer::Replace( AampGrowableBuffer *src )
{
	assert( ptr == NULL ); // only replace if empty!
	ptr = src->GetPtr();
	len = src->GetLen();
	avail = src->GetAvail();
	
	src->ptr = NULL;
	src->len = 0;
	src->avail = 0;
}

/**
 * @brief called when internal memory is transferred (i.e. as part of GStreamer injection)
 */
void AampGrowableBuffer::Transfer( void )
{
	assert( ptr );
	if( ptr )
	{
		NETMEMORY_MINUS();
        if( gbEnableLogging )
        {
            printf("AampGrowableBuffer::%s(%s:%d)\n", "Transfer",name,gNetMemoryCount);
        }
	}
	ptr = NULL;
	len = 0;
	avail = 0;
}
