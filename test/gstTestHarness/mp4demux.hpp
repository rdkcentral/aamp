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
#ifndef parsemp4_hpp
#define parsemp4_hpp

#include <cstdint>
#include <stddef.h>
#include <vector>
#include <assert.h>
#include <inttypes.h>
#include <cstdio>
#include <cstring> // for memcpy
#include <string>
#include <gst/app/gstappsrc.h>

#define INDENT() &"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"[indent]
#define PRINTF(fmt,...) if( verbose ) printf(fmt,##__VA_ARGS__)

// convert multi-character constants like 'cenc' to equivalent 32 bit integer - pass as four character string
#define MultiChar_Constant(TEXT) ( \
(static_cast<uint32_t>(TEXT[0]) << 0x18) | \
(static_cast<uint32_t>(TEXT[1]) << 0x10) | \
(static_cast<uint32_t>(TEXT[2]) << 0x08) | \
(static_cast<uint32_t>(TEXT[3]) << 0x00) )

struct Mp4Sample
{
	const uint8_t *ptr;
	size_t len;
	double pts;
	double dts;
	double duration;
	
	std::string subsamples;
	std::string iv;
};

class Mp4Demux
{
private:
	struct
	{
		uint16_t channel_count;
		uint16_t samplesize;
		uint16_t samplerate;
		uint8_t object_type_id;
		uint8_t stream_type;
		uint8_t upStream;
		uint16_t buffer_size;
		uint32_t maxBitrate;
		uint32_t avgBitrate;
	} audio;
	
	struct
	{
		uint16_t width;
		uint16_t height;
		uint16_t frame_count;
		uint16_t depth;
		uint32_t horizresolution;
		uint32_t vertresolution;
	} video;
	
	uint32_t stream_format;
	uint32_t data_reference_index;
	uint32_t codec_type;
	std::string codec_data;
	uint8_t is_encrypted;
	uint8_t iv_size;
	uint8_t crypt_byte_block;
	uint8_t skip_byte_block;
	uint8_t constant_iv_size;
	std::string constant_iv;
	
	uint32_t timescale;
	std::vector<Mp4Sample> samples;
	
	// encryption-specific data
	std::string default_kid;
	bool got_auxiliary_information_offset;
	uint64_t auxiliary_information_offset;
	uint32_t scheme_type; // 'cenc' or 'cbcs'
	uint32_t scheme_version;
	uint32_t original_media_type;
	std::vector<uint8_t> cenc_aux_info_sizes;
	std::vector<GstEvent *> protectionEvents;
	
	const uint8_t *moof_ptr; // base address for sample data
	const uint8_t *ptr; // parser state
	int indent;
	
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
	uint32_t duration;
	uint32_t rate;
	uint32_t volume;
	int32_t matrix[9];
	uint16_t layer;
	uint16_t alternate_group;
	uint32_t width_fixed;
	uint32_t height_fixed;
	uint16_t language;
	bool verbose;
	
	GstBuffer * _gst_buffer_new_memdup(gconstpointer data, gsize size)
	{
		//return gst_buffer_new_memdup( data, size ); // requires gstreamer 1.20
		
		GstBuffer *buffer = gst_buffer_new_and_alloc( size );
		if( buffer)
		{
			GstMapInfo map;
			gst_buffer_map(buffer, &map, GST_MAP_WRITE);
			memcpy(map.data, data, size );
			gst_buffer_unmap(buffer, &map);
		}
		return buffer;
	}

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
		ptr += len;
	}
	
	void parseOriginalFormat( void )
	{
		original_media_type = ReadU32();
	}
	
	void parseSchemeManagementBox( void )
	{
		ReadHeader();
		scheme_type = ReadU32(); // 'cenc' or 'cbcs'
		scheme_version = ReadU32();
		PRINTF( "%sscheme_version=0x%x\n", INDENT(), scheme_version );
	}
	
	void parseTrackEncryptionBox( void )
	{
		ReadHeader();

		ptr++; // skip
		uint8_t pattern = *ptr++;
		if( scheme_type == MultiChar_Constant("cbcs") )
		{
			crypt_byte_block = (pattern>>4) & 0xf;
			skip_byte_block = pattern & 0xf;
		}
		is_encrypted = *ptr++;
		iv_size = *ptr++;

		default_kid = std::string((char *)ptr,16);
		ptr += 16;
		if( scheme_type == MultiChar_Constant("cbcs") )
		{
			constant_iv_size = *ptr++;
			assert( constant_iv_size==8 || constant_iv_size==16 );
			constant_iv = std::string((char *)ptr,constant_iv_size);
			ptr += constant_iv_size;
		}
	}
	
	/*
	 12 24 "8de6242e-6601-5218-8841-ace2761b413f"	// kid
	 12 24 "2e9b8068-fa3a-c50f-4781-550aae5986ad"	// kid
	 22 13 "6112559918033517163" 					// ContentID
	 */
	void parseProtectionSystemSpecificHeaderBox( const uint8_t *next )
	{
		ReadHeader();
		gchar *system_id = g_strdup_printf( "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
										   ptr[0x0], ptr[0x1], ptr[0x2], ptr[0x3], ptr[0x4], ptr[0x5], ptr[0x6], ptr[0x7],
										   ptr[0x8], ptr[0x9], ptr[0xa], ptr[0xb], ptr[0xc], ptr[0xd], ptr[0xe], ptr[0xf] );
		ptr += 16;
		PRINTF( "%ssystem_id: '%s'\n", INDENT(), system_id );
		
		size_t pssh_size = next - ptr;
		GstBuffer *pssh = _gst_buffer_new_memdup(ptr, pssh_size);
		GstEvent *event = gst_event_new_protection(system_id, pssh, "isobmff/moov" ); // or isobmff/moof
		//g_queue_push_tail (&stream->protection_scheme_event_queue, gst_event_ref (event));
		//gst_pad_push_event (pad, gst_event_ref (event));
		protectionEvents.push_back(event);
		g_free (system_id);
		//gst_event_unref(event);
		gst_buffer_unref(pssh);
	}
	
	void process_auxiliary_information( void )
	{ // redundant with parseSampleEncryptionBox?
		size_t sample_count = cenc_aux_info_sizes.size();
		if( sample_count && got_auxiliary_information_offset )
		{
			PRINTF( "%sauxiliary_information\n", INDENT() );
			const uint8_t *src = moof_ptr + auxiliary_information_offset;
			for( int i=0; i<cenc_aux_info_sizes.size(); i++ )
			{
				int sz = cenc_aux_info_sizes[i];
				if( verbose )
				{
					PRINTF( "%s", INDENT() );
					for( int j=0; j<sz; j++ )
					{
						PRINTF( "%02x", src[j] );
					}
					PRINTF( "\n" );
				}
				src += sz;
			}
		}
	}
	
	void parseSampleAuxiliaryInformationSizes( void )
	{
		ReadHeader();
		// 00 00 00 01
		// 63 65 6e 63 'cenc'
		// 00 00 00 00
		// 00 // default_info_size
		// 00 00 00 4c // sampleCount
		// 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 10 ...
		if( flags&1 )
		{
			parseAuxInfo();
		}
		uint8_t default_info_size = *ptr++;
		uint32_t sampleCount  = ReadU32();
		if( default_info_size )
		{
			for( int i=0; i<sampleCount; i++ )
			{
				cenc_aux_info_sizes.push_back(default_info_size);
			}
		}
		else
		{
			for( int i=0; i<sampleCount; i++ )
			{
				cenc_aux_info_sizes.push_back(ptr[i]);
			}
			ptr += sampleCount;
			if( verbose )
			{
				PRINTF( "%s", INDENT() );
				for( int i=0; i<sampleCount; i++ )
				{
					PRINTF( " %02x", cenc_aux_info_sizes[i] );
				}
				PRINTF( "\n" );
			}
		}
		process_auxiliary_information();
	}
	
	void parseAuxInfo( void )
	{
		uint32_t aux_info_type = ReadU32(); // cenc or cbcs
		assert( aux_info_type == MultiChar_Constant("cenc") || aux_info_type == MultiChar_Constant("cbcs") );
		
		uint32_t aux_info_type_parameter = ReadU32();
		(void)aux_info_type_parameter;
	}
	
	// ISO/IEC 23001-7
	void parseSampleAuxiliaryInformationOffsets( void )
	{ // offsets to auxilliary information for samples or groups of samples
		// 00 00 00 01
		// 63 65 6e 63 'cenc'
		// 00 00 00 00
		// 00 00 00 01
		// 00 00 05 2c
		ReadHeader();
		if( flags&1 )
		{
			parseAuxInfo();
		}
		uint32_t entry_count = ReadU32();
		assert( entry_count == 1 );
		if( version == 0 )
		{
			auxiliary_information_offset = ReadU32();
		}
		else
		{
			auxiliary_information_offset = ReadU64();
		}
		PRINTF( "%sauxiliary_information_offset = 0x%" PRIu64 "\n", INDENT(), auxiliary_information_offset );
		got_auxiliary_information_offset = true;
		process_auxiliary_information();
	}
	
	void parseSampleEncryptionBox( void )
	{
		ReadHeader();
		uint32_t sampleCount = ReadU32();
		assert( samples.size() == sampleCount );
		for( auto iSample=0; iSample<sampleCount; iSample++ )
		{
			if( iv_size )
			{
				samples[iSample].iv = std::string((char *)ptr,iv_size);
				if( verbose )
				{
					PRINTF( "%s", INDENT() );
					for( int i=0; i<iv_size; i++ )
					{
						PRINTF( " %02x", ptr[i] );
					}
					PRINTF("\n");
				}
				ptr += iv_size;
			}
			
			if( flags&2 )
			{ // sub sample encryption
				uint16_t n_subsamples = ReadU16();
				size_t subsamples_size = n_subsamples * 6;
				samples[iSample].subsamples = std::string((char *)ptr,subsamples_size);
				if( verbose )
				{
					PRINTF( "%s", INDENT() );
					for( int i=0; i<subsamples_size; i++ )
					{
						PRINTF( " %02x", ptr[i] );
					}
					PRINTF("\n");
				}
				ptr += subsamples_size;
			}
		}
	}
	
	void parseMovieFragmentHeaderBox( void )
	{
		ReadHeader();
		uint32_t sequence_number = ReadU32();
		PRINTF( "%ssequence_number=%" PRIu32 "\n", INDENT(), sequence_number );
	}
	
	void parseTrackFragmentHeaderBox( void )
	{
		ReadHeader();
		track_id = ReadU32();
		PRINTF( "%strack_id=%" PRIu32 "\n", INDENT(), track_id );
		if (flags & 0x00001)
		{
			base_data_offset = ReadU64();
			PRINTF( "%sbase_data_offset=%" PRIu64 "\n", INDENT(), base_data_offset );
		}
		if (flags & 0x00002)
		{
			default_sample_description_index = ReadU32();
			PRINTF( "%sdefault_sample_description_index=%" PRIu32 "\n", INDENT(), default_sample_description_index );
		}
		if (flags & 0x00008)
		{
			default_sample_duration = ReadU32();
			INDENT();
			PRINTF( "%sdefault_sample_duration=%" PRIu32 "\n", INDENT(), default_sample_duration );
		}
		if (flags & 0x00010)
		{
			default_sample_size = ReadU32();
			PRINTF( "%sdefault_sample_size=%" PRIu32 "\n", INDENT(), default_sample_size );
		}
		if (flags & 0x00020)
		{
			default_sample_flags = ReadU32();
			INDENT();
			PRINTF( "%sdefault_sample_flags=%" PRIu32 "\n", INDENT(), default_sample_flags );
		}
	}
	
	void parseTrackFragmentBaseMediaDecodeTimeBox( void  )
	{
		ReadHeader();
		int sz = (version==1)?8:4;
		baseMediaDecodeTime  = ReadBytes(sz);
		PRINTF( "%sbaseMediaDecodeTime: %" PRIu64 "\n", INDENT(), baseMediaDecodeTime );
	}
	
	void parseTrackFragmentRunBox( void )
	{
		ReadHeader();
		uint32_t sample_count = ReadU32();
		PRINTF( "%ssample_number=%" PRIu32 "\n", INDENT(), sample_count );
		const unsigned char *data_ptr = moof_ptr;
		//0xE01
		if( flags & 0x0001 )
		{ // offset from start of Moof box field
			int32_t data_offset = ReadI32();
			PRINTF( "%sdata_offset=%" PRIu32 "\n", INDENT(), data_offset );
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
			PRINTF( "%sfirst_sample_flags=0x%" PRIx32 "\n", INDENT(), sample_flags );
		}
		uint64_t dts = baseMediaDecodeTime;
		for( unsigned int i=0; i<sample_count; i++ )
		{
			struct Mp4Sample sample;
			sample.ptr = data_ptr;
			sample.len = default_sample_size;
			sample.pts = 0.0;
			sample.dts = 0.0;
			sample.duration = 0.0;
			PRINTF( "%sframe#%03d", INDENT(), i );
			uint32_t sample_duration = default_sample_duration;
			if (flags & 0x0100)
			{
				sample_duration = ReadU32();
				PRINTF( " duration=%" PRIu32, sample_duration );
				sample.duration = sample_duration / (double)timescale;
			}
			if (flags & 0x0200)
			{
				uint32_t sample_size = ReadU32();
				PRINTF( " size=%" PRIu32, sample_size );
				sample.len = sample_size;
			}
			data_ptr += sample.len;
			if (flags & 0x0400)
			{ // rarely present?
				sample_flags = ReadU32();
				PRINTF( " flags=0x%" PRIx32, sample_flags );
			}
			int32_t sample_composition_time_offset = 0;
			if (flags & 0x0800)
			{ // for samples where pts and dts differ (overriding 'trex')
				sample_composition_time_offset = ReadI32();
				PRINTF( " composition_time_offset=%" PRIi32, sample_composition_time_offset );
			}
			sample.dts = dts/(double)timescale;
			sample.pts = (dts+sample_composition_time_offset)/(double)timescale;
			PRINTF( " dts=%f pts=%f", sample.dts, sample.pts );
			dts += sample_duration;
			samples.push_back( std::move(sample) );
			PRINTF( "\n" );
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
	
	void parseTrackHeaderBox( void )
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
		width_fixed = ReadU32();
		height_fixed = ReadU32();
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
	
	void parseSampleDescriptionBox( const uint8_t *next )
	{ // stsd
		ReadHeader();
		uint32_t count = ReadU32();
		assert( count == 1 );
		DemuxHelper(next);
	}
	
	void parseVideoInformation( void )
	{
		SkipBytes(4); // always zero?
		data_reference_index = ReadU32();
		SkipBytes(16); // always zero?
		video.width = ReadU16();
		video.height = ReadU16();
		video.horizresolution = ReadU32();
		video.vertresolution = ReadU32();
		SkipBytes(4);
		video.frame_count = ReadU16();
		SkipBytes(32); // compressor_name
		video.depth = ReadU16();
		int pad = ReadU16();
		assert( pad == 0xffff );
	}
	
	void parseAudioInformation( void )
	{
		SkipBytes(4); // zero
		data_reference_index = ReadU32();
		SkipBytes(8); // zero
		audio.channel_count = ReadU16();
		audio.samplesize = ReadU16();
		SkipBytes(4); // zero
		audio.samplerate = ReadU16();
		SkipBytes(2); // zero
	}
	
	void parseStreamFormatBox( uint32_t type, const uint8_t *next )
	{
		stream_format = type;
		switch( stream_format )
		{
			case MultiChar_Constant("hev1"):
			case MultiChar_Constant("avc1"):
			case MultiChar_Constant("hvc1"):
			case MultiChar_Constant("encv"):
				parseVideoInformation();
				break;
				
			case MultiChar_Constant("mp4a"):
			case MultiChar_Constant("ec-3"):
			case MultiChar_Constant("enca"):
				parseAudioInformation();
				break;
				
			default:
				PRINTF( "%sunknown stream_format\n", INDENT()  );
				assert(0);
				break;
		}
		DemuxHelper( next );
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
					PRINTF( "%sES_Descriptor\n", INDENT() );
					SkipBytes(3);
					parseCodecConfigHelper( end );
					break;
					
				case 0x04:
					PRINTF( "%sDecoderConfigDescriptor\n", INDENT() );
					audio.object_type_id = *ptr++;
					audio.stream_type = *ptr++; // >>2
					audio.upStream = *ptr++;
					audio.buffer_size = ReadU16();
					audio.maxBitrate = ReadU32();
					audio.avgBitrate = ReadU32();
					PRINTF( "%smaxBitrate=%" PRIu32 "\n", INDENT(), audio.maxBitrate );
					PRINTF( "%savgBitrate=%" PRIu32 "\n", INDENT(), audio.avgBitrate );
					parseCodecConfigHelper( end );
					break;
					
				case 0x05:
					PRINTF( "%sDecodeSpecificInfo\n", INDENT() ) ;
					codec_data = std::string((char *)ptr,len);
					ptr += len;
					break;
					
				case 0x06:
					PRINTF( "%sSlConfigDescriptor\n", INDENT());
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
	
	void parseCodecConfigurationBox( uint32_t type, const uint8_t *next )
	{
		codec_type = type;
		if( type == MultiChar_Constant("esds") )
		{
			SkipBytes(4);
			parseCodecConfigHelper( next );
		}
		else
		{
			size_t codec_data_len = next - ptr;
			codec_data = std::string( (char *)ptr,codec_data_len );
		}
	}
	
	void DemuxHelper( const uint8_t *fin )
	{
		indent--; // back up to include one more tab
		while( ptr < fin )
		{
			uint32_t size = ReadU32();
			const uint8_t *next = ptr+size-4;
			uint32_t type = ReadU32();
			PRINTF( "%s '%c%c%c%c' (%" PRIu32 ")\n", INDENT(), (type>>24)&0xff, (type>>16)&0xff, (type>>8)&0xff, type&0xff, size );
			switch( type )
			{
				case MultiChar_Constant("hev1"):
				case MultiChar_Constant("hvc1"):
				case MultiChar_Constant("avc1"):
				case MultiChar_Constant("mp4a"):
				case MultiChar_Constant("ec-3"):
				case MultiChar_Constant("enca"):
				case MultiChar_Constant("encv"):
					parseStreamFormatBox( type, next );
					break;
					
				case MultiChar_Constant("hvcC"):
				case MultiChar_Constant("dec3"):
				case MultiChar_Constant("avcC"):
				case MultiChar_Constant("esds"): // Elementary Stream Descriptor
					parseCodecConfigurationBox( type, next );
					break;
					
				case MultiChar_Constant("pssh"):
					parseProtectionSystemSpecificHeaderBox(next);
					break;
					
				case MultiChar_Constant("saio"): // points to first IV in senc box
					parseSampleAuxiliaryInformationOffsets();
					assert( ptr == next );
					break;
					
				case MultiChar_Constant("saiz"): // defines size of senc entries
					parseSampleAuxiliaryInformationSizes();
					assert( ptr == next );
					break;
					
				case MultiChar_Constant("senc"): // modern, optional
					parseSampleEncryptionBox();
					break;
					
				case MultiChar_Constant("mfhd"):
					parseMovieFragmentHeaderBox();
					break;
					
				case MultiChar_Constant("tfhd"):
					parseTrackFragmentHeaderBox();
					break;
					
				case MultiChar_Constant("trun"):
					parseTrackFragmentRunBox();
					break;
					
				case MultiChar_Constant("tfdt"):
					parseTrackFragmentBaseMediaDecodeTimeBox();
					break;
					
				case MultiChar_Constant("mvhd"):
					parseMovieHeaderBox();
					break;
					
				case MultiChar_Constant("mehd"):
					parseMovieExtendsHeader();
					break;
					
				case MultiChar_Constant("trex"):
					parseTrackExtendsBox();
					break;
					
				case MultiChar_Constant("tkhd"):
					parseTrackHeaderBox();
					break;
					
				case MultiChar_Constant("mdhd"):
					parseMediaHeaderBox();
					break;
					
				case MultiChar_Constant("stsd"): // Sample Description
					parseSampleDescriptionBox(next);
					break;
					
				case MultiChar_Constant("ftyp"): // FileType (major_brand, minor_version, compatible_brands)
				case MultiChar_Constant("hdlr"): // Handler Reference (handler, name)
				case MultiChar_Constant("vmhd"): // Video Media Header (graphicsmode, opcolor)
				case MultiChar_Constant("smhd"): // Sound Media Header (balance)
				case MultiChar_Constant("dref"): // Data Reference (url) (under dinf box)
				case MultiChar_Constant("stts"): // Decoding Time To Sample (under stb boxl)
				case MultiChar_Constant("stsc"): // Sample To Chunk (under stbl box)
				case MultiChar_Constant("stsz"): // Sample Size Boxes (under stbl box)
				case MultiChar_Constant("stco"): // Chunk Offsets (under stbl box)
				case MultiChar_Constant("stss"): // Sync Sample (under stbl box)
				case MultiChar_Constant("prft"): // Producer Reference Time
				case MultiChar_Constant("edts"): // Edit (under trak box)
				case MultiChar_Constant("fiel"): // Field (progressive or interlaced)
				case MultiChar_Constant("colr"): // Color Pattern Atom
				case MultiChar_Constant("pasp"): // Pixel Aspect Ratio (hSpacing, vSpacing)
				case MultiChar_Constant("btrt"): // Buffer Time to Render Time (bufferSizeDB, maxBitrate, avgBitrate)
				case MultiChar_Constant("styp"): // Segment Type (under file box)
				case MultiChar_Constant("sidx"): // Segment Index
				case MultiChar_Constant("udta"): // User Data (can appear under moov, trak, moof, traf)
				case MultiChar_Constant("mdat"): // Movie Data (under file box)
					// TODO - parse if needed
					break;
					
				case MultiChar_Constant("schm"):
					parseSchemeManagementBox();
					break;
					
				case MultiChar_Constant("frma"):
					parseOriginalFormat();
					break;
					
				case MultiChar_Constant("tenc"):
					parseTrackEncryptionBox();
					assert( ptr == next );
					break;
					
				case MultiChar_Constant("moof"):  // Movie Fragment
					moof_ptr = ptr-8;
					DemuxHelper(next );
					break;
					
				case MultiChar_Constant("schi"): // Scheme Information
				case MultiChar_Constant("traf"): // Track Fragment
				case MultiChar_Constant("moov"): // Movie
				case MultiChar_Constant("trak"): // Track
				case MultiChar_Constant("minf"): // Media Information
				case MultiChar_Constant("dinf"): // Data Information
				case MultiChar_Constant("mvex"): // Movie Extends
				case MultiChar_Constant("mdia"): // Media
				case MultiChar_Constant("stbl"): // Sample Table
				case MultiChar_Constant("sinf"): // Protection Scheme Information
					DemuxHelper(next );
					break;
					
				default:
					PRINTF( "unknown box type!\n" );
					break;
			}
			ptr = next;
		}
		indent++;
	}
	
public:
	void Parse( const void *ptr, size_t len )
	{
		// scrub sample data from previous segment, but leave other metadata intact
		samples.clear();
		cenc_aux_info_sizes.clear();
		got_auxiliary_information_offset = false;
		moof_ptr = NULL;
		if( ptr )
		{
			this->ptr = (const uint8_t *)ptr;
			indent = 16+1;
			DemuxHelper( &this->ptr[len] );
		}
	}
	
	Mp4Demux( bool verbose=false ) : audio{}, video{}, stream_format(), data_reference_index(), codec_type(), codec_data(), is_encrypted(), iv_size(), crypt_byte_block(), skip_byte_block(), constant_iv_size(), constant_iv(), timescale(), samples(), default_kid(), got_auxiliary_information_offset(), auxiliary_information_offset(), scheme_type(), scheme_version(), original_media_type(), cenc_aux_info_sizes(), protectionEvents(), moof_ptr(), ptr(), indent(), version(), flags(), baseMediaDecodeTime(), fragment_duration(), track_id(), base_data_offset(), default_sample_description_index(), default_sample_duration(), default_sample_size(), default_sample_flags(), creation_time(), modification_time(), duration(), rate(), volume(), matrix{}, layer(), alternate_group(), width_fixed(), height_fixed(), language(), verbose(verbose)
	{
	}
	
	uint32_t getTimeScale( void )
	{
		return timescale;
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
		for( int i=0; i<protectionEvents.size(); i++ )
		{
			gst_event_unref(protectionEvents[i]);
		}
	}
	
	Mp4Demux(const Mp4Demux & other)
	{ // stub copy constructor
		assert(0);
	}
	
	Mp4Demux& operator=(const Mp4Demux & other)
	{ // stub move constructor
		assert(0);
	}
	
	void setCaps( GstAppSrc *appsrc ) const
	{
		GstCaps * caps = NULL;
		size_t codec_data_len = codec_data.size();
		GstBuffer *buf = gst_buffer_new_and_alloc(codec_data_len);
		gst_buffer_fill(buf, 0, codec_data.c_str(), codec_data_len );
		switch( codec_type )
		{
			case MultiChar_Constant("hvcC"):
				caps = gst_caps_new_simple(
										   "video/x-h265",
										   "stream-format", G_TYPE_STRING, "hvc1",
										   "alignment", G_TYPE_STRING, "au",
										   "codec_data", GST_TYPE_BUFFER, buf,
										   "width", G_TYPE_INT, video.width,
										   "height", G_TYPE_INT, video.height,
										   "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
										   NULL );
				break;
				
			case MultiChar_Constant("avcC"):
				caps = gst_caps_new_simple(
										   "video/x-h264",
										   "stream-format", G_TYPE_STRING, "avc",
										   "alignment", G_TYPE_STRING, "au",
										   "codec_data", GST_TYPE_BUFFER, buf,
										   "width", G_TYPE_INT, video.width,
										   "height", G_TYPE_INT, video.height,
										   "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
										   NULL );
				break;
				
			case MultiChar_Constant("esds"):
				caps = gst_caps_new_simple(
										   "audio/mpeg",
										   "mpegversion",G_TYPE_INT,4,
										   "framed", G_TYPE_BOOLEAN, TRUE,
										   "stream-format",G_TYPE_STRING,"raw", // FIXME
										   "codec_data", GST_TYPE_BUFFER, buf,
										   NULL );
				break;
				
			case MultiChar_Constant("dec3"):
				caps = gst_caps_new_simple(
										   "audio/x-eac3",
										   "framed", G_TYPE_BOOLEAN, TRUE,
										   "rate", G_TYPE_INT, audio.samplerate,
										   "channels", G_TYPE_INT, audio.channel_count,
										   NULL );
				break;
				
			default:
				g_print( "unk codec_type: %" PRIu32 "\n", codec_type );
				return;
		}
		gst_app_src_set_caps(appsrc, caps);
		gst_caps_unref(caps);
		gst_buffer_unref (buf);
	}
	
	size_t getNumProtectionEvents( void )
	{
		return protectionEvents.size();
	}
	
	GstEvent *getProtectionEvent( int which )
	{
		return protectionEvents[which];
	}
	
	GstStructure *getDrmMetadata( int sampleIndex )
	{
		GstStructure *metadata = NULL;
		if( is_encrypted )
		{
			char original_media_type_string[5] =
			{
				(char)(original_media_type>>0x18),
				(char)(original_media_type>>0x10),
				(char)(original_media_type>>0x08),
				(char)(original_media_type>>0x00),
				0x00
			};
			char cipher_mode_string[5] =
			{
				(char)(scheme_type>>0x18),
				(char)(scheme_type>>0x10),
				(char)(scheme_type>>0x08),
				(char)(scheme_type>>0x00),
				0x00
			};
			
			GstBuffer *kid_buf = _gst_buffer_new_memdup( (gpointer)default_kid.c_str(), (gsize)default_kid.size() );
			metadata = gst_structure_new(
										 "application/x-cenc",
										 "encrypted", G_TYPE_BOOLEAN, TRUE,
										 "kid", GST_TYPE_BUFFER, kid_buf,
										 "original-media-type", G_TYPE_STRING, original_media_type_string,
										 "cipher-mode", G_TYPE_STRING, cipher_mode_string,
										 NULL);
			gst_buffer_unref( kid_buf );
			
			const Mp4Sample &sample = samples[sampleIndex];
			size_t iv_size = sample.iv.size();
			if( iv_size )
			{
				GstBuffer *iv_buf = _gst_buffer_new_memdup( (gpointer)sample.iv.c_str(), (gsize)iv_size );
				gst_structure_set(metadata,
								  "iv_size", G_TYPE_UINT, iv_size,
								  "iv", GST_TYPE_BUFFER, iv_buf,
								  NULL);
				gst_buffer_unref(iv_buf);
			}
			
			size_t subsamples_size = sample.subsamples.size();
			if( subsamples_size )
			{
				GstBuffer *subsamples_buf = _gst_buffer_new_memdup( (gpointer)sample.subsamples.c_str(), (gsize)subsamples_size);
				gst_structure_set(metadata,
								  "subsample_count", G_TYPE_UINT, subsamples_size/6,
								  "subsamples", GST_TYPE_BUFFER, subsamples_buf,
								  NULL);
				gst_buffer_unref(subsamples_buf);
			}
			
			if( scheme_type == MultiChar_Constant("cbcs") )
			{
				GstBuffer *constant_iv_buf = _gst_buffer_new_memdup( (gpointer)constant_iv.c_str(), (gsize)constant_iv_size);
				gst_structure_set(metadata,
								  "iv", GST_TYPE_BUFFER, constant_iv_buf,
								  "constant_iv_size", G_TYPE_UINT, constant_iv_size,
								  "crypt_byte_block", G_TYPE_UINT, crypt_byte_block,
								  "skip_byte_block", G_TYPE_UINT, skip_byte_block,
								  NULL );
				gst_buffer_unref( constant_iv_buf );
			}
		}
		
		if( metadata )
		{ // serialize and print the metadata
			gchar *structure_string = gst_structure_to_string( metadata );
			g_print("metadata: %s\n", structure_string);
			g_free(structure_string);

			// gst_structure_free( metadata );
			// metadata = NULL;
		}
		
		return metadata;
	}
	
	/**
	 * @brief apply adjustment for pts restamping - used for invasive pts restamping (gst test harness dai2 test)
     */
	static uint64_t AdjustMediaDecodeTime( uint8_t *ptr, size_t len, int64_t pts_restamp_delta )
	{
		uint64_t baseMediaDecodeTime = 0;
		const uint8_t *fin = &ptr[len];
		while( ptr < fin && !baseMediaDecodeTime )
		{
			uint32_t size = (uint32_t)(ptr[0]<<24)|(ptr[1]<<16)|(ptr[2]<<8)|ptr[3];
			uint8_t *next = ptr + size;
			ptr += 4;
			uint32_t type = (ptr[0]<<24)|(ptr[1]<<16)|(ptr[2]<<8)|ptr[3];
			ptr += 4;
			if( type == MultiChar_Constant("tfdt") ) // Track Fragment Base Media Decode Time Box
			{
				uint8_t version = *ptr++;
				int sz = (version==1)?8:4;
				ptr += 3; // skip flags
				baseMediaDecodeTime = 0;
				for( int i=0; i<sz; i++ )
				{
					baseMediaDecodeTime <<= 8;
					baseMediaDecodeTime |= ptr[i];
				}
				baseMediaDecodeTime += pts_restamp_delta;
				for( int i=0; i<sz; i++ )
				{
					ptr[i] = (baseMediaDecodeTime>>((sz-1-i)*8))&0xff;
				}
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
					case MultiChar_Constant("mvex"): // Movie Extends Box
					case MultiChar_Constant("moof"): // Movie Fragment Boxes
					case MultiChar_Constant("mdia"): // Media Box
						baseMediaDecodeTime = AdjustMediaDecodeTime( ptr, next-ptr, pts_restamp_delta );
						break;
						
					default:
						break;
				}
			}
			ptr = next;
		}
		return baseMediaDecodeTime;
	}
};

#endif /* parsemp4_hpp */
