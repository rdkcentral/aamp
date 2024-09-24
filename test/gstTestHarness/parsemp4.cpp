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

static void parse_mfhd( const uint8_t *ptr )
{ // Movie Fragment Header Box
	uint8_t version = READ_VERSION(ptr);
	assert( version == 0 );
	uint32_t flags  = READ_FLAGS(ptr);
	(void)flags;
	
	uint32_t sequence_number = READ_U32(ptr);
	printf( "sequence_number=%" PRIu32 "\n", sequence_number );
}

static void parse_tfhd( const uint8_t *ptr )
{ // Track Fragment Header Box
	const uint32_t TFHD_FLAG_BASE_DATA_OFFSET_PRESENT               = 0x00001;
	const uint32_t TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT       = 0x00002;
	const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT        = 0x00008;
	const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT            = 0x00010;
	const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_FLAGS_PRESENT           = 0x00020;
	
	uint8_t version = READ_VERSION(ptr);
	assert( version == 0 );
	uint32_t flags  = READ_FLAGS(ptr);
	(void)flags;
	
	uint32_t track_id = READ_U32(ptr);
	printf( "track_id=%" PRIu32 "\n", track_id );
	
	if (flags & TFHD_FLAG_BASE_DATA_OFFSET_PRESENT)
	{
		uint64_t base_data_offset = READ_U64(ptr);
		printf( "base_data_offset=%" PRIu64 "\n", base_data_offset );
	}
	if (flags & TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT)
	{
		uint32_t default_sample_description_index = READ_U32(ptr);
		printf( "default_sample_description_index=%" PRIu32 "\n", default_sample_description_index );
	}
	if (flags & TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT)
	{
		uint32_t default_sample_duration = READ_U32(ptr);
		printf( "default_sample_duration=%" PRIu32 "\n", default_sample_duration );
	}
	if (flags & TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT)
	{
		uint32_t default_sample_size = READ_U32(ptr);
		printf( "default_sample_size=%" PRIu32 "\n", default_sample_size );
	}
	if (flags & TFHD_FLAG_DEFAULT_SAMPLE_FLAGS_PRESENT)
	{
		uint32_t default_sample_flags = READ_U32(ptr);
		printf( "default_sample_flags=%" PRIu32 "\n", default_sample_flags );
	}
}

static void parse_tfdt( uint8_t *ptr, int64_t pts_restamp_delta  )
{ // TrackFragmentBaseMediaDecodeTimeBox
	uint8_t version = READ_VERSION(ptr);
	uint32_t flags  = READ_FLAGS(ptr);
	(void)flags;
	uint64_t baseMediaDecodeTime;
	if( 1 == version )
	{
		baseMediaDecodeTime = ReadBytes( ptr, 8 );
		if( pts_restamp_delta )
		{
			baseMediaDecodeTime += pts_restamp_delta;
			WriteBytes( (uint8_t *)ptr, 8, baseMediaDecodeTime );
		}
	}
	else
	{
		baseMediaDecodeTime = ReadBytes( ptr, 4 );
		if( pts_restamp_delta )
		{
			baseMediaDecodeTime += pts_restamp_delta;
			WriteBytes( (uint8_t *)ptr, 4, baseMediaDecodeTime );
		}
	}
	printf( "baseMediaDecodeTime: %" PRIu64 "\n", baseMediaDecodeTime );
}

static void parse_trun( const uint8_t *ptr, uint32_t timeScale )
{ // Track Fragment Run Box
	const uint32_t TRUN_FLAG_DATA_OFFSET_PRESENT                    = 0x0001;
	const uint32_t TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT             = 0x0004;
	const uint32_t TRUN_FLAG_SAMPLE_DURATION_PRESENT                = 0x0100;
	const uint32_t TRUN_FLAG_SAMPLE_SIZE_PRESENT                    = 0x0200;
	const uint32_t TRUN_FLAG_SAMPLE_FLAGS_PRESENT                   = 0x0400;
	const uint32_t TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT = 0x0800;
	
	uint8_t version = READ_VERSION(ptr);
	(void)version; // can be 1 or 0; not clear significance
	uint32_t flags  = READ_FLAGS(ptr);
	uint32_t sample_count = READ_U32(ptr);
	printf( "sample_number=%" PRIu32 "\n", sample_count );
	size_t base_data_offset = 1047; // hack - byte delta from start of test mp4, to match ffmpeg result
	if( flags & TRUN_FLAG_DATA_OFFSET_PRESENT )
	{ // offset to data in media data box ('mdat')
		uint32_t data_offset = READ_U32(ptr);
		printf( "data_offset=%" PRIu32 "\n", data_offset );
		base_data_offset += data_offset;
	}
	else
	{
		assert(0);
	}
	uint32_t sample_flags = 0;
	if(flags & TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT)
	{
		sample_flags = READ_U32(ptr);
		printf( "first_sample_flags=0x%" PRIx32 "\n", sample_flags );
	}
	uint64_t dts = 0;
	for( unsigned int i=0; i<sample_count; i++ )
	{
		printf( "[FRAME] %d\n", i );
		uint32_t sample_duration = 0;
		if (flags & TRUN_FLAG_SAMPLE_DURATION_PRESENT)
		{
			sample_duration = READ_U32(ptr);
			printf( "duration=%" PRIu32 "\n", sample_duration );
		}
		if (flags & TRUN_FLAG_SAMPLE_SIZE_PRESENT)
		{
			uint32_t sample_size = READ_U32(ptr);
			printf( "pkt_pos=%zu\n", base_data_offset );
			base_data_offset += sample_size;
			printf( "pkt_size=%" PRIu32 "\n", sample_size );
		}
		if (flags & TRUN_FLAG_SAMPLE_FLAGS_PRESENT)
		{ // rarely present?
			sample_flags = READ_U32(ptr);
			printf( "sample_flags = 0x%" PRIx32 "\n", sample_flags );
		}
		int32_t sample_composition_time_offset = 0;
		if (flags & TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT)
		{ // for samples were pts and dts differ (overriding 'trex'
			sample_composition_time_offset = (int32_t)READ_U32(ptr);
			printf( "sample_composition_time_offset=%" PRIi32 "\n", sample_composition_time_offset );
		}
		printf( "key_frame=%d\n", (sample_flags & 0x2000000)?1:0 );
		sample_flags = 0;
		
		printf( "dts=%f pts=%f\n",
			   dts/(double)timeScale,
			   (dts+sample_composition_time_offset)/(double)timeScale );
		dts += sample_duration;
	}
}

static void DemuxHelper( uint8_t *ptr, const uint8_t *start, const uint8_t *fin, int64_t pts_restamp_delta, uint32_t timeScale )
{
	while( ptr < fin )
	{
		uint8_t *base = ptr;
		uint32_t size = READ_U32(ptr);
		uint8_t *next = base+size;
		uint32_t type = READ_U32(ptr);
		printf( "%zu '%c%c%c%c'\n", ptr-start, (type>>24)&0xff, (type>>16)&0xff, (type>>8)&0xff, type&0xff );
		switch( type )
		{
			case 'mfhd':
				parse_mfhd( ptr );
				break;
				
			case 'tfhd':
				parse_tfhd( ptr );
				break;
				
			case 'trun':
				parse_trun( ptr, timeScale );
				break;
				
			case 'tfdt':
				parse_tfdt( ptr, pts_restamp_delta );
				break;
				
			case 'traf':
			case 'moov':
			case 'trak':
			case 'minf':
			case 'dinf':
			case 'stbl':
			case 'mvex':
			case 'moof':
			case 'mdia':
				DemuxHelper( ptr, start, fin, pts_restamp_delta, timeScale ); // walk children
				break;
				
			default:
				break;
		}
		ptr = next;
	}
}

void mp4demux( uint8_t *ptr, size_t len, int64_t pts_restamp_delta, uint32_t timeScale )
{
	DemuxHelper(
				ptr,
				ptr, // start
				ptr+len, // fin
				pts_restamp_delta,
				timeScale );
}
