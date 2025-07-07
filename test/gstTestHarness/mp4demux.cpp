/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#include "mp4demux.hpp"
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>

static void WriteBytes( uint8_t *ptr, int n, uint64_t value )
{
	while( n>0 )
	{
		ptr[--n] = value&0xff;
		value>>=8;
	}
}

static uint64_t ReadBytes( const uint8_t *ptr, int n )
{
	uint64_t rc = 0;
	for( int i=0; i<n; i++ )
	{
		rc <<= 8;
		rc |= *ptr++;
	}
	return rc;
}

#define READ_U16(buf) (unsigned int)ReadBytes( buf, 2 ); buf+=2;
#define READ_U32(buf) (unsigned int)ReadBytes( buf, 4 ); buf+=4;
#define READ_U64(buf) ReadBytes( buf, 8 ); buf+=8;

#define READ_VERSION(buf) buf[0]; buf++;
#define READ_FLAGS(buf) (uint32_t)ReadBytes(buf,3); buf+=3;

uint64_t mp4_AdjustMediaDecodeTime( uint8_t *ptr, size_t len, int64_t pts_restamp_delta )
{
	uint64_t baseMediaDecodeTime = 0;
	const uint8_t *fin = &ptr[len];
	while( ptr < fin && !baseMediaDecodeTime )
	{
		uint8_t *next = ptr + READ_U32(ptr);
		uint32_t type = READ_U32(ptr);
		if( type == MultiChar_Constant("tfdt") ) // Track Fragment Base Media Decode Time Box
		{
			uint8_t version = READ_VERSION(ptr);
			int sz = (version==1)?8:4;
			uint32_t flags  = READ_FLAGS(ptr);
			(void)flags;
			baseMediaDecodeTime = ReadBytes( ptr, sz );
			baseMediaDecodeTime += pts_restamp_delta;
			WriteBytes( (uint8_t *)ptr, sz, baseMediaDecodeTime );
			//printf( "baseMediaDecodeTime: %" PRIu64 "\n", baseMediaDecodeTime );
			break;
		}
		else
		{ // walk children
			switch( type )
			{
				case MultiChar_Constant("traf"): // Track Fragment Box
				case MultiChar_Constant("moov"): // Movie Box
				case MultiChar_Constant("trak"): // Track Box
				case MultiChar_Constant("minf"): // Media Information Box
				case MultiChar_Constant("dinf"): // Data Information Box
				case MultiChar_Constant("stbl"): // Sample Table Box
				case MultiChar_Constant("mvex"):// Movie Extends Box
				case MultiChar_Constant("moof"): // Movie Fragment Boxes
				case MultiChar_Constant("mdia"): // Media Box
					baseMediaDecodeTime = mp4_AdjustMediaDecodeTime( ptr, next-ptr, pts_restamp_delta );
					break;
					
				default:
					break;
			}
		}
		ptr = next;
	}
	return baseMediaDecodeTime;
}
