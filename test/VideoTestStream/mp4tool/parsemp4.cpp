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

unsigned int mTimeScale = 48000;

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

static bool HasChildren( uint32_t type )
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
			return true;
		default:
			return false;
	}
}

static void parse_sidx( const uint8_t *ptr )
{
	unsigned int version = READ_U32(ptr);
	unsigned int reference_ID = READ_U32(ptr);
	unsigned int timescale = READ_U32(ptr);
	uint64_t earliest_presentation_time;
	uint64_t first_offset;
	if( version==0 )
	{
		earliest_presentation_time = READ_U32(ptr);
		first_offset = READ_U32(ptr);
	}
	else
	{
		earliest_presentation_time = READ_U64(ptr);
		first_offset = READ_U64(ptr);
	}
	unsigned int reserved = READ_U16(ptr);
	unsigned int reference_count = READ_U16(ptr);
	for( int segmentIndex=0;segmentIndex<reference_count; segmentIndex++ )
	{
		//start += 12*segmentIndex;
		auto referenced_size = READ_U32(ptr);
		// top bit is "reference_type"
		
		auto referenced_duration = READ_U32(ptr);
		//printf( "duration=%d / timescale=%d = %f\n", referenced_duration, timescale, referenced_duration/(float)timescale );
		
		auto flags = READ_U32(ptr);
		// starts_with_SAP (1 bit)
		// SAP_type (3 bits)
		// SAP_delta_time (28 bits)
	}
}

static void parse_mvhd( const uint8_t *ptr )
{
	uint8_t version = READ_VERSION(ptr);
	uint32_t flags  = READ_FLAGS(ptr);
	uint32_t tScale;

	uint32_t skip = sizeof(uint32_t)*2;
	if (1 == version)
	{
		//Skipping creation_time &modification_time
		skip = sizeof(uint64_t)*2;
	}
	ptr += skip;

	mTimeScale = READ_U32(ptr);
	// printf( "\t\tTimeScale=%d\n", mTimeScale );
}

static void parse_mfhd( const uint8_t *ptr )
{ // sequence_number for integrity check
}

static void parse_tfhd( const uint8_t *ptr )
{
	//	flags
	//	base_data_offset
	uint8_t version = READ_VERSION(ptr); // 0
	uint32_t flags  = READ_FLAGS(ptr); // 0x20020

	const uint32_t TFHD_FLAG_BASE_DATA_OFFSET_PRESENT            = 0x00001;
	const uint32_t TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT    = 0x00002;
	const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT     = 0x00008;
	const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT         = 0x00010;
	const uint32_t TFHD_FLAG_DEFAULT_SAMPLE_FLAGS_PRESENT        = 0x00020;
	const uint32_t TFHD_FLAG_DURATION_IS_EMPTY                   = 0x10000;
	const uint32_t TFHD_FLAG_DEFAULT_BASE_IS_MOOF                = 0x20000;
	uint32_t TrackId = READ_U32(ptr);
	if (flags & TFHD_FLAG_BASE_DATA_OFFSET_PRESENT) {
		uint32_t BaseDataOffset = (uint32_t)READ_U64(ptr);
		//printf( "\t\t\tBaseDataOffset=%d\n", BaseDataOffset );
	}
	if (flags & TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT) {
		uint32_t SampleDescriptionIndex = READ_U32(ptr);
		//printf( "\t\t\tSampleDescriptionIndex=%d\n", SampleDescriptionIndex );
	}
	if (flags & TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT) {
		uint32_t DefaultSampleDuration = READ_U32(ptr);
		//printf( "\t\t\tDefaultSampleDuration=%d\n", DefaultSampleDuration );
	}
	if (flags & TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT) {
		uint32_t DefaultSampleSize = READ_U32(ptr);
		//printf( "\t\t\tDefaultSampleSize=%d\n", DefaultSampleSize );
	}
	if (flags & TFHD_FLAG_DEFAULT_SAMPLE_FLAGS_PRESENT) {
		uint32_t DefaultSampleFlags = READ_U32(ptr);
		//printf( "\t\t\tDefaultSampleFlags=0x%x\n", DefaultSampleFlags );
		// 0xAA00000
	}
}

static void parse_tfdt( const uint8_t *ptr )
{
	uint8_t version = READ_VERSION(ptr);
	uint32_t flags  = READ_FLAGS(ptr);
	uint64_t mdt; // base media decode time
	if (1 == version)
	{
		mdt = READ_U64(ptr);
	}
	else
	{
		mdt = (uint32_t)READ_U32(ptr);
	}
	//printf( "\t\t\tmdt=%llu\n", mdt );//, mdt/(double)mTimeScale );
}

static void parse_trun( const uint8_t *ptr )
{
	const uint32_t TRUN_FLAG_DATA_OFFSET_PRESENT                    = 0x0001; // !
	const uint32_t TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT             = 0x0004;
	const uint32_t TRUN_FLAG_SAMPLE_DURATION_PRESENT                = 0x0100; // !
	const uint32_t TRUN_FLAG_SAMPLE_SIZE_PRESENT                    = 0x0200; // !
	const uint32_t TRUN_FLAG_SAMPLE_FLAGS_PRESENT                   = 0x0400;
	const uint32_t TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT = 0x0800; // !
	uint8_t version = READ_VERSION(ptr); // 0x01
	uint32_t flags  = READ_FLAGS(ptr); // 0x0b01
	//printf( "\t\t\tflags=0x%x\n", flags );
	uint32_t sample_duration = 0;
	uint32_t sample_size = 0;
	uint32_t sample_flags = 0;
	uint32_t sample_composition_time_offset = 0;
	uint32_t firstSampleFlags=0;
	uint32_t dataOffset = 0;
	uint64_t totalSampleDuration = 0;
	uint32_t sample_count = READ_U32(ptr);
	if(flags & TRUN_FLAG_DATA_OFFSET_PRESENT)
	{
		dataOffset = READ_U32(ptr); // 112
	}
	if(flags & TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT)
	{
		firstSampleFlags = READ_U32(ptr);
	}
	for (unsigned int i=0; i<sample_count; i++)
	{
		if (flags & TRUN_FLAG_SAMPLE_DURATION_PRESENT)
		{
			sample_duration = READ_U32(ptr); // 345600
			//printf( "\t\t\tduration=%d (%f)\n", sample_duration, sample_duration/(double)mTimeScale );
			totalSampleDuration += sample_duration; // 345600
		}
		if (flags & TRUN_FLAG_SAMPLE_SIZE_PRESENT)
		{
			sample_size = READ_U32(ptr);
		}
		if (flags & TRUN_FLAG_SAMPLE_FLAGS_PRESENT)
		{
			sample_flags = READ_U32(ptr);
		}
		if (flags & TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT)
		{
			sample_composition_time_offset = READ_U32(ptr); // 0
		}
	}
}

static int mCursorPos;

static void LogHex( uint64_t number, int numBytes )
{
	if( numBytes>0 )
	{
		LogHex( number>>8, numBytes-1 );
		printf( "0x%02x,", (int)(number&0xff) );
		mCursorPos++;
	}
}

static void LogIndent( int indent )
{
	mCursorPos += indent;
	for( int i=0; i<indent; i++ )
	{
		printf("     " );
	}
}

static void PadMargin( void )
{
	LogIndent( 18 - mCursorPos );
	mCursorPos = 0;
}

void ParseBox( const uint8_t *ptr, const uint8_t *fin, int indent )
{
	while( ptr < fin )
	{
		const uint8_t *base = ptr;
		uint32_t size = READ_U32(ptr);
		const uint8_t *next = base+size;
		uint32_t type = READ_U32(ptr);
		
		{
			printf("\n");
			mCursorPos = 0;
			LogIndent( indent );
			LogHex( size, 4 );
			LogHex( type, 4 );
			PadMargin();
			printf( "// %c%c%c%c (%d)\n",
				   (char)(type>>24),
				   (char)(type>>16),
				   (char)(type>>8),
				   (char)(type>>0),
				   size );
		}
		switch( type )
		{
			case 'mvhd':
				parse_mvhd( ptr );
				break;
			case 'sidx':
				parse_sidx( ptr );
				break;
			case 'mfhd':
				parse_mfhd( ptr );
				break;
			case 'tfhd':
				parse_tfhd( ptr );
				break;
			case 'tfdt':
				parse_tfdt( ptr );
				break;
			case 'trun':
				parse_trun( ptr );
				break;
			case 'tkhd':
				// track_id
				break;
			default:
				break;
		}
		if( HasChildren(type) )
		{
			ParseBox( ptr, next, indent+1 );
		}
		else
		{
			int span = 8;
			if( type=='mdat' )
			{
				span = 16;
			}
			
			{
				std::string comment;
				int i = 0;
				for(;;)
				{
					if( (i%span)==0 || ptr==next )
					{
						if( i )
						{
							PadMargin();
							printf( "// %s\n", comment.c_str() );
							comment.clear();
						}
						LogIndent(indent+1);
					}
					if( ptr == next )
					{
						break;
					}
					int c = *ptr++;
					printf( "0x%02x,", c );
					mCursorPos++;
					if( c>=' ' && c<128 )
					{
						comment += (char)c;
					}
					else
					{
						comment += '?';
					}
					i++;
				}
			}
		}
		ptr = next;
	}
}
