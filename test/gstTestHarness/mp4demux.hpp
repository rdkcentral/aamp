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
#ifndef parsemp4_hpp
#define parsemp4_hpp

#include "initializationheaderinfo.hpp"
#include <cstdint>
#include <stddef.h>
#include <vector>
#include <assert.h>
#include <inttypes.h>
#include <cstdio>
#include <glib.h>

struct Mp4Sample
{
	const uint8_t *ptr;
	size_t len;
	double pts;
	double dts;
	double duration;
};

class Mp4Demux
{
public:
	InitializationHeaderInfo info;

private:
	std::vector<Mp4Sample> samples;
	const uint8_t *moof_ptr; // base address for sample data
	const uint8_t *ptr; // parsing state

	uint8_t version;
	uint32_t flags;
	uint64_t baseMediaDecodeTime;
	uint32_t fragment_duration;
	uint32_t track_id;
	uint64_t base_data_offset;
	uint32_t default_sample_description_index;
	uint32_t default_sample_duration;
	uint32_t default_sample_size;
	uint32_t default_sample_flags;
	uint64_t creation_time;
	uint64_t modification_time;
	uint32_t timescale;
	uint32_t duration;
	uint32_t rate;
	uint32_t volume;
	int32_t matrix[9];
	uint16_t layer;
	uint16_t alternate_group;
	uint32_t width;
	uint32_t height;
	uint16_t language;
	
	uint64_t ReadBytes( int n )
	{
		uint64_t rc = 0;
		for( int i=0; i<n; i++ )
		{
			rc <<= 8;
			rc |= *ptr++;
		}
		return rc;
	}
	uint16_t ReadU16()
	{
		return (uint16_t)ReadBytes(2);
	}
	uint32_t ReadU32()
	{
		return (uint32_t)ReadBytes(4);
	}
	int32_t ReadI32()
	{
		return (int32_t)ReadBytes(4);
	}
	uint64_t ReadU64()
	{
		return ReadBytes(8);
	}
	void ReadHeader( void )
	{
		version = *ptr++;
		flags = (uint32_t)ReadBytes(3);
	}
	void SkipBytes( size_t len )
	{
		printf( "skipping %zu bytes\n", len );
		while( len-- )
		{
			printf( " %02x", (unsigned char)*ptr++ );
		}
		printf( "\n" );
	}

	void parseMovieFragmentHeaderBox( void )
	{
		ReadHeader();
		uint32_t sequence_number = ReadU32();
		printf( "sequence_number=%" PRIu32 "\n", sequence_number );
	}
	
	void parseTrackFragmentHeaderBox( void )
	{ // TODO: use these defaults if not explicitly defined per sample
		ReadHeader();
		track_id = ReadU32();
		printf( "track_id=%" PRIu32 "\n", track_id );
		if (flags & 0x00001)
		{
			base_data_offset = ReadU64();
			printf( "base_data_offset=%" PRIu64 "\n", base_data_offset );
		}
		if (flags & 0x00002)
		{
			default_sample_description_index = ReadU32();
			printf( "default_sample_description_index=%" PRIu32 "\n", default_sample_description_index );
		}
		if (flags & 0x00008)
		{
			default_sample_duration = ReadU32();
			printf( "default_sample_duration=%" PRIu32 "\n", default_sample_duration );
		}
		if (flags & 0x00010)
		{
			default_sample_size = ReadU32();
			printf( "default_sample_size=%" PRIu32 "\n", default_sample_size );
		}
		if (flags & 0x00020)
		{
			default_sample_flags = ReadU32();
			printf( "default_sample_flags=%" PRIu32 "\n", default_sample_flags );
		}
	}
	
	void parseTrackFragmentBaseMediaDecodeTimeBox( void  )
	{
		ReadHeader();
		int sz = (version==1)?8:4;
		baseMediaDecodeTime  = ReadBytes(sz);
		printf( "baseMediaDecodeTime: %" PRIu64 "\n", baseMediaDecodeTime );
	}
	
	void parseTrackFragmentRunBox( void )
	{
		ReadHeader();
		uint32_t sample_count = ReadU32();
		printf( "sample_number=%" PRIu32 "\n", sample_count );
		const unsigned char *data_ptr = moof_ptr;
		if( flags & 0x0001 )
		{ // offset from start of Moof box field
			int32_t data_offset = ReadI32();
			printf( "data_offset=%" PRIu32 "\n", data_offset );
			data_ptr += data_offset;
		}
		else
		{ // mandatory field? should never reach here
			assert(0);
		}
		uint32_t sample_flags = 0;
		if(flags & 0x0004)
		{
			sample_flags = ReadU32();
			printf( "first_sample_flags=0x%" PRIx32 "\n", sample_flags );
		}
		uint64_t dts = baseMediaDecodeTime;
		for( unsigned int i=0; i<sample_count; i++ )
		{
			struct Mp4Sample sample;
			sample.ptr = data_ptr;
			sample.len = 0;
			sample.pts = 0.0;
			sample.dts = 0.0;
			sample.duration = 0.0;
			printf( "[FRAME] %d\n", i );
			uint32_t sample_duration = 0;
			if (flags & 0x0100)
			{
				sample_duration = ReadU32();
				printf( "sample_duration=%" PRIu32 "\n", sample_duration );
				sample.duration = sample_duration / (double)timescale;
			}
			if (flags & 0x0200)
			{
				uint32_t sample_size = ReadU32();
				printf( "sample_size=%" PRIu32 "\n", sample_size );
				sample.len = sample_size;
				data_ptr += sample_size;
			}
			if (flags & 0x0400)
			{ // rarely present?
				sample_flags = ReadU32();
				printf( "sample_flags=0x%" PRIx32 "\n", sample_flags );
			}
			int32_t sample_composition_time_offset = 0;
			if (flags & 0x0800)
			{ // for samples were pts and dts differ (overriding 'trex')
				sample_composition_time_offset = ReadI32();
				printf( "sample_composition_time_offset=%" PRIi32 "\n", sample_composition_time_offset );
			}
			sample.dts = dts/(double)timescale;
			sample.pts = (dts+sample_composition_time_offset)/(double)timescale;
			printf( "dts=%f pts=%f\n", sample.dts, sample.pts );
			dts += sample_duration;
			samples.push_back( sample );
		}
	}
	
	void parseMovieHeaderBox( void )
	{
		ReadHeader();
		int sz = (version==1)?8:4;
		creation_time = ReadBytes(sz);
		modification_time = ReadBytes(sz);
		timescale = ReadU32();
		duration = ReadU32();
		rate = ReadU32();
		volume = ReadU32(); // fixed point
		ptr += 8;
		for( int  i=0; i<9; i++ )
		{
			matrix[i] = ReadI32();
		}
	}
	
	void parseMovieExtendsHeader( void )
	{
		ReadHeader();
		fragment_duration = ReadU32();
	}
	
	void parseTrackExtendsBox( void )
	{
		ReadHeader();
		track_id = ReadU32();
		default_sample_description_index = ReadU32();
		default_sample_duration = ReadU32();
		default_sample_size = ReadU32();
		default_sample_flags = ReadU32();
	}
	
	void parseTrackHeader( void )
	{
		ReadHeader();
		int sz = (version==1)?8:4;
		creation_time = ReadBytes(sz);
		modification_time = ReadBytes(sz);
		track_id = ReadU32();
		ptr += 20+sz; // duration, layer, alternate_group, volume
		for( int i=0; i<9; i++ )
		{
			matrix[i] = ReadI32();
		}
		width = ReadU32(); // fixed point
		height = ReadU32(); // fixed point
	}

	void parseMediaHeaderBox( void )
	{
		ReadHeader();
		int sz = (version==1)?8:4;
		creation_time = ReadBytes(sz);
		modification_time = ReadBytes(sz);
		timescale = ReadU32();
		duration = ReadU32();
		language = ReadU16();
	}
	
	char *readPascalString( void )
	{
		int len = *ptr++;
		char *rc = (char *)malloc(len+1);
		if( rc )
		{
			memcpy(rc, ptr, len );
			rc[len] = 0x00;
		}
		ptr += len;
		return rc;
	}

	void parseSampleDescriptionBox( const uint8_t *next, int indent )
	{ // stsd
		ReadHeader();
		uint32_t count = ReadU32();
		assert( count == 1 );
		DemuxHelper(next, indent+1);
	}
		
	void parseStreamFormat( uint32_t type, const uint8_t *next, int indent )
	{
		info.stream_format = type;
		switch( info.stream_format )
		{
			case 'hev1':
			case 'avc1':
			case 'hvc1':
				SkipBytes(4); // always zero?
				info.data_reference_index = ReadU32();
				SkipBytes(16); // always zero?
				info.width = ReadU16();
				info.height = ReadU16();
				info.horizresolution = ReadU32();
				info.vertresolution = ReadU32();
				SkipBytes(4);
				info.frame_count = ReadU16();
				info.compressor_name = readPascalString();
				switch( info.stream_format )
				{
					case 'avc1':
						SkipBytes(31); // ?
						break;
					case 'hvc1':
						SkipBytes(9); // ?
						break;
					case 'hev1': // ?
						SkipBytes(31); // ?
						break;
					default:
						break;
				}
				info.depth = ReadU16();
				SkipBytes(2);
				break;
				
			case 'mp4a':
			case 'ec-3':
				SkipBytes(4);
				info.data_reference_index = ReadU32();
				SkipBytes(8);
				info.channel_count = ReadU16();
				info.samplesize = ReadU16();
				SkipBytes(4);
				info.samplerate = ReadU16();
				SkipBytes(2);
				break;
				
			default:
				printf( "unk stream_format\n" );
				assert(0);
				break;
		}
		DemuxHelper( next, indent+1 );
	}
	
	int readLen( void )
	{
		int rc = 0;
		for(;;)
		{
			unsigned char octet = *ptr++;
			rc <<= 7;
			rc |= octet&0x7f;
			if( (octet&0x80)==0 ) return rc;
		}
	}
	
	void parseCodecConfigHelper( const uint8_t *next )
	{
		while( ptr < next )
		{
			uint32_t tag = *ptr++;
			uint32_t len = readLen();
			const uint8_t *end = ptr + len;
			switch( tag )
			{
				case 0x03:
					printf( "ES_Descriptor: ");
					SkipBytes(3);
					parseCodecConfigHelper( end );
					break;
					
				case 0x04:
					printf( "DecoderConfigDescriptor: ");
					info.object_type_id = *ptr++;
					info.stream_type = *ptr++; // >>2
					info.upStream = *ptr++;
					info.buffer_size = ReadU16();
					info.maxBitrate = ReadU32();
					info.avgBitrate = ReadU32();
					printf( "maxBitrate=%" PRIu32 "\n", info.maxBitrate );
					printf( "avgBitrate=%" PRIu32 "\n", info.avgBitrate );
					
					parseCodecConfigHelper( end );
					break;
					
				case 0x05:
					printf( "DecodeSpecificInfo: ") ;
					info.codec_data_len = len;
					info.codec_data = (uint8_t *)malloc( len );
					if( info.codec_data )
					{
						memcpy( info.codec_data, ptr, len );
						ptr += len;
					}
					break;
					
				case 0x06:
					printf( "SlConfigDescriptor: ");
					SkipBytes( len );
					break;
					
				default:
					assert(0);
					break;
			}
			assert( ptr == end );
			ptr = end;
		}
	}
	void parseCodecConfiguration( uint32_t type, const uint8_t *next )
	{
		info.codec_type = type;
		if( type == 'esds' )
		{
			SkipBytes(4);
			parseCodecConfigHelper( next );
		}
		else
		{
			info.codec_data_len = next - ptr;
			info.codec_data = (uint8_t *)malloc( info.codec_data_len );
			if( info.codec_data )
			{
				memcpy( info.codec_data, ptr, info.codec_data_len );
			}
		}
	}
	
	void DemuxHelper( const uint8_t *fin, int indent )
	{
		while( ptr < fin )
		{
			uint32_t size = ReadU32();
			//printf( "size=%" PRIu32 "\n", size );
			const uint8_t *next = ptr+size-4;
			uint32_t type = ReadU32();
			for( int i=0; i<indent; i++ )
			{
				printf( "\t" );
			}
			printf( "'%c%c%c%c'\n",
				   (type>>24)&0xff, (type>>16)&0xff, (type>>8)&0xff, type&0xff );
			switch( type )
			{
				case 'hev1':
				case 'hvc1':
				case 'avc1':
				case 'mp4a':
				case 'ec-3':
					parseStreamFormat( type, next, indent );
					break;
					
				case 'hvcC':
				case 'dec3':
				case 'avcC':
				case 'esds':
					parseCodecConfiguration( type, next );
					break;
					
				case 'ftyp': // FileType Box
					/*
					 major_brand // 4 chars
					 minor_version // 4 bytes
					 compatible_brands // 16 bytes, uint32be
					 */
					break;
					
				case 'mfhd':
					parseMovieFragmentHeaderBox();
					break;
					
				case 'tfhd':
					parseTrackFragmentHeaderBox();
					break;
					
				case 'trun':
					parseTrackFragmentRunBox();
					break;
					
				case 'tfdt':
					parseTrackFragmentBaseMediaDecodeTimeBox();
					break;
					
				case 'mvhd':
					parseMovieHeaderBox();
					break;
					
				case 'mehd':
					parseMovieExtendsHeader();
					break;
					
				case 'trex':
					parseTrackExtendsBox();
					break;
					
				case 'tkhd':
					parseTrackHeader();
					break;
					
				case 'mdhd':
					parseMediaHeaderBox();
					break;
					
				case 'hdlr': // Handler Reference Box
					/*
					 handler	vide
					 name	Bento4 Video Handler
					 */
					break;
					
				case 'vmhd': // Video Media Header
					/*
					 graphicsmode	0
					 opcolor	0,0,0
					 */
					break;
					
				case 'smhd': // Sound Media Header
					/*
					 balance	0
					 */
					break;
					
				case 'dref': // Data Reference Box
					/*
					 url
					 */
					break;
					
				case 'stsd': // Sample Description Box
					parseSampleDescriptionBox(next,indent);
					break;
					
				case 'stts': // DecodingTimeToSample
					break;
				case 'stsc': // SampleToChunkBox
					break;
				case 'stsz': // SampleSizeBoxes
					break;
				case 'stco': // ChunkOffsets
					break;
				case 'edts': // Edit Box
					break;
				case 'fiel':
				case 'colr':
				case 'pasp':
				case 'btrt':
					break;
					
					
				case 'mdat': // Movie Data Box
					break;

				case 'moof': // Movie Fragment Box
					moof_ptr = ptr-8;
					DemuxHelper(next, indent+1 ); // walk children
					break;
					
				case 'traf': // Track Fragment Boxes
				case 'moov': // Movie Boxes
				case 'trak': // Track Box
				case 'minf': // Media Information Container
				case 'dinf': // Data Information Box
				case 'mvex': // Movie Extends Box
				case 'mdia': // Media Box
				case 'stbl': // Sample Table Box
					DemuxHelper(next, indent+1 ); // walk children
					break;
										
				default:
					printf( "unknown box type!\n" );
					break;
			}
			ptr = next;
		}
	}

public:
	Mp4Demux( gpointer ptr, size_t len, uint32_t timescale )
	{
		this->ptr = (const uint8_t *)ptr;
		this->moof_ptr = NULL;
		this->timescale = timescale;
		DemuxHelper( &this->ptr[len], 0 );
	}
	
	int count( void )
	{
		return (int)samples.size();
	}
	
	const uint8_t * getPtr( int part )
	{
		return samples[part].ptr;
	}
	
	size_t getLen( int part )
	{
		return samples[part].len;
	}
	
	double getPts( int part )
	{
		return samples[part].pts;
	}
	
	double getDts( int part )
	{
		return samples[part].dts;
	}

	double getDuration( int part )
	{
		return samples[part].duration;
	}

	~Mp4Demux()
	{
	}
	
	Mp4Demux(const Mp4Demux & other)
	{ // stub copy constructor
		assert(0);
	}
	
	Mp4Demux& operator=(const Mp4Demux & other)
	{ // stub move constructor
		assert(0);
	}
};

/**
 * @brief apply adjustment for pts restamping
 */
uint64_t mp4_AdjustMediaDecodeTime( uint8_t *ptr, size_t len, int64_t pts_restamp_delta );

#endif /* parsemp4_hpp */
