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

#ifndef __AAMP_GROWABLE_BUFFER_H__
#define __AAMP_GROWABLE_BUFFER_H__

#include <stddef.h>
#include <cstring>
#include <utility>
#include <assert.h>
#include <stdio.h>

class AampGrowableBuffer
{
public:
	AampGrowableBuffer( const char *name="?" ):ptr(NULL),len(0),avail(0),name(name){}
	~AampGrowableBuffer();

	// Copy constructor
	AampGrowableBuffer(const AampGrowableBuffer & other)
	 : ptr(nullptr),
	len{other.len},
	avail(0),name{other.name}
	{ // never reached/used?
		ReserveBytes(len); // allocate the pointer and set avail
		std::memcpy(ptr, other.ptr, len); // populate
	}
	// Copy assignment
	AampGrowableBuffer& operator=(const AampGrowableBuffer & other)
	{ // never reached/used?
		Free();
		len = other.len;
		ReserveBytes(len);
		std::memcpy(ptr, other.ptr, len);
		return *this;
	}

	// Move constructor
	AampGrowableBuffer(AampGrowableBuffer && other) noexcept
		: ptr {other.ptr},
		len {other.len},
		avail{other.avail},
		name{other.name}
	{ // never reached/used
		other.ptr = nullptr;
		other.len = 0;
		other.avail = 0;
	}
	// Move assignment
	AampGrowableBuffer& operator=(AampGrowableBuffer && other) noexcept
	{ // never reached/used
		Free();
		std::swap(ptr, other.ptr);
		std::swap(len, other.len);
		std::swap(avail, other.avail);
		return *this;
	}

	void Free( void );
	void ReserveBytes( size_t len );
	void AppendBytes( const void *ptr, size_t len ); // append passed binary data to end of growable buffer, increasing underlying storage if required
	void MoveBytes( const void *ptr, size_t len );
	void Clear( void ); // sets logical buffer size back to zero, without releasing available pre-allocated memory; allows a growable buffer to be recycled
	void Replace( AampGrowableBuffer *src );
	void Transfer( void );
	
	char *GetPtr( void ) { return (char *)ptr; } // accessor function for current growable buffer binary payload
	const char *GetPtr( void ) const { return static_cast<const char *>(ptr); } // accessor function for current growable buffer binary payload
	size_t GetLen( void ) const { return len; } // accessor function for current logical growable buffer size
	size_t GetAvail( void ) const { return avail; } // should be opaque, but used in logging
	void SetLen( size_t l ) { assert(l<=avail); len = l; }

    static void EnableLogging( bool enable );
    
private:
    const char *name;
	void *ptr;      /**< Pointer to buffer's memory location (gpointer) */
	size_t len;     /**< Subset of allocated buffer that is populated and in use */
	size_t avail;   /**< Available buffer size */
	
    static bool gbEnableLogging;
	static int gNetMemoryCount;
	static int gNetMemoryHighWatermark;
    
	static void NETMEMORY_PLUS( void )
	{
		gNetMemoryCount++;
		if( gNetMemoryCount>gNetMemoryHighWatermark )
		{
			gNetMemoryHighWatermark = gNetMemoryCount;
			printf( "***gNetMemoryHighWatermark=%d\n", gNetMemoryHighWatermark );
		}
	}

	/**
	 * @brief subtracts from memory count
	 */
	static void NETMEMORY_MINUS( void )
	{
		if( gNetMemoryCount > 0 )
		{
			gNetMemoryCount--;
			if( gNetMemoryCount == 0)
			{
				printf("***gNetMemoryCount=0\n");
			}
		}
		else
		{
			printf("gNetMemoryCount is already 0");
		}
		assert( gNetMemoryCount >= 0 );
	}
};

#endif /* __AAMP_GROWABLE_BUFFER_H__ */
