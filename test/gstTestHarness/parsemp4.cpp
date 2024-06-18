/*
 *   Copyright 2024 RDK Management
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
*/
#include "parsemp4.hpp"
#include <string>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <filesystem>

static uint64_t WriteBytes( uint8_t *ptr, int n, uint64_t value )
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

static uint64_t parse_tfdt( uint8_t *ptr, int64_t pts_offset  )
{
	uint8_t version = READ_VERSION(ptr);
	uint32_t flags  = READ_FLAGS(ptr);
	(void)flags;
	uint64_t media_decode_time;
	if( 1 == version )
	{
		media_decode_time = ReadBytes( ptr, 8 );
		if( pts_offset )
		{
			media_decode_time += pts_offset;
			WriteBytes( (uint8_t *)ptr, 8, media_decode_time );
		}
	}
	else
	{
		media_decode_time = ReadBytes( ptr, 4 );
		if( pts_offset )
		{
			media_decode_time += pts_offset;
			WriteBytes( (uint8_t *)ptr, 4, media_decode_time );
		}
	}
	return media_decode_time;
}

uint64_t parsemp4_ApplyPtsOffset( uint8_t *ptr, size_t len, int64_t pts_offset )
{
	uint64_t rc = 0;
	
	const uint8_t *fin = ptr+len;
	while( ptr < fin )
	{
		uint8_t *base = ptr;
		uint32_t size = READ_U32(ptr);
		uint8_t *next = base+size;
		uint32_t type = READ_U32(ptr);
		if( type == 'tfdt' )
		{
			rc = parse_tfdt( ptr, pts_offset );
			break;
		}
		else
		{
			switch( type )
			{
				case 'moov':
				case 'trak':
				case 'minf':
				case 'dinf':
				case 'stbl':
				case 'mvex':
				case 'moof':
				case 'traf':
				case 'mdia':
					// walk children
					parsemp4_ApplyPtsOffset( ptr, next-ptr, pts_offset );
					break;
					
				default:
					break;
			}
			ptr = next;
		}
	}
	return rc;
}
